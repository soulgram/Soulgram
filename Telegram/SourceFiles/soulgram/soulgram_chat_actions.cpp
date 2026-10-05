/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_chat_actions.h"

#include "apiwrap.h"
#include "base/timer.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "dialogs/dialogs_key.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "menu/menu_mark_as_read.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_settings.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"

namespace Soulgram {
namespace {

// The server gives at most this many messages per search request.
constexpr auto kSearchLimit = 100;

// Chats that are still not loaded by then are not read.
constexpr auto kReadAllLoadTimeout = 60 * crl::time(1000);

// Telegram was launched in August 2013, no message is older.
[[nodiscard]] QDate StartDate() {
	return QDate(2013, 8, 1);
}

struct Progress {
	int found = 0;
	int deleted = 0;
	bool counted = false; // The first search told how many there are.
	bool stopping = false;
	bool finished = false;

	friend inline bool operator==(const Progress &, const Progress &)
		= default;
};

// Finds own messages with messages.search and deletes them page by page.
// Requests go through Data::Histories, so they don't race with the history
// requests of the chat, and FLOOD_WAIT is waited out by the MTP layer.
class Deleter final : public base::has_weak_ptr {
public:
	Deleter(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer);

	void start();
	void stop();

	[[nodiscard]] Progress progress() const;
	[[nodiscard]] rpl::producer<Progress> progressValue() const;

private:
	void searchNext();
	void searched(const MTPmessages_Messages &result);
	void deleteNext(QVector<MTPint> ids, MsgId nextOffsetId);
	void deleted(int count, MsgId nextOffsetId);
	void failed(const QString &error);
	void nextPart();
	void finish(const QString &error = QString());

	const base::weak_ptr<Window::SessionController> _controller;
	const not_null<PeerData*> _peer;

	// The group and the legacy group it was upgraded from.
	std::vector<not_null<History*>> _parts;
	int _part = 0;
	MsgId _offsetId = 0;
	bool _stopped = false;
	rpl::variable<Progress> _progress;

};

using DeletersMap = base::flat_map<
	not_null<PeerData*>,
	std::unique_ptr<Deleter>>;

[[nodiscard]] DeletersMap &Deleters(not_null<Main::Session*> session) {
	static auto result = base::flat_map<
		not_null<Main::Session*>,
		DeletersMap>();
	const auto i = result.find(session);
	if (i != end(result)) {
		return i->second;
	}
	session->lifetime().add([=] {
		result.remove(session);
	});
	return result.emplace(session, DeletersMap()).first->second;
}

[[nodiscard]] Deleter *FindDeleter(not_null<PeerData*> peer) {
	auto &deleters = Deleters(&peer->session());
	const auto i = deleters.find(peer);
	return (i != end(deleters)) ? i->second.get() : nullptr;
}

// The messages are gone on the server, the loaded ones go away here, so
// that they are not kept as deleted by somebody else.
void DestroyLocal(not_null<History*> history, const QVector<MTPint> &ids) {
	auto &owner = history->owner();
	auto items = std::vector<not_null<HistoryItem*>>();
	for (const auto &id : ids) {
		if (const auto item = owner.message(history->peer->id, id.v)) {
			items.push_back(item);
		}
	}
	if (items.empty()) {
		return;
	}
	owner.notifyItemsAboutToBeDestroyed(items);
	for (const auto &item : items) {
		item->destroy();
	}
}

Deleter::Deleter(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer)
: _controller(controller)
, _peer(peer) {
}

void Deleter::start() {
	auto &owner = _peer->owner();
	_parts.push_back(owner.history(_peer));
	if (const auto from = _peer->migrateFrom()) {
		_parts.push_back(owner.history(from));
	}
	searchNext();
}

void Deleter::stop() {
	if (_stopped) {
		return;
	}
	_stopped = true;
	auto progress = _progress.current();
	progress.stopping = true;
	_progress = progress;
}

Progress Deleter::progress() const {
	return _progress.current();
}

rpl::producer<Progress> Deleter::progressValue() const {
	return _progress.value();
}

void Deleter::searchNext() {
	if (_stopped) {
		finish();
		return;
	}
	const auto history = _parts[_part];
	const auto peer = history->peer;
	const auto api = &peer->session().api();
	const auto offsetId = _offsetId;
	const auto weak = base::make_weak(this);
	using Type = Data::Histories::RequestType;
	history->owner().histories().sendRequest(history, Type::History, [=](
			Fn<void()> finishRequest) {
		using Flag = MTPmessages_Search::Flag;
		return api->request(MTPmessages_Search(
			MTP_flags(Flag::f_from_id),
			peer->input(),
			MTP_string(QString()),
			MTP_inputPeerSelf(),
			MTP_inputPeerEmpty(), // saved_peer_id
			MTP_vector<MTPReaction>(), // saved_reaction
			MTP_int(0), // top_msg_id
			MTP_inputMessagesFilterEmpty(),
			MTP_int(0), // min_date
			MTP_int(0), // max_date
			MTP_int(offsetId),
			MTP_int(0), // add_offset
			MTP_int(kSearchLimit),
			MTP_int(0), // max_id
			MTP_int(0), // min_id
			MTP_long(0) // hash
		)).done([=](const MTPmessages_Messages &result) {
			finishRequest();
			if (const auto strong = weak.get()) {
				strong->searched(result);
			}
		}).fail([=](const MTP::Error &error) {
			finishRequest();
			if (const auto strong = weak.get()) {
				strong->failed(error.type());
			}
		}).send();
	});
}

void Deleter::searched(const MTPmessages_Messages &result) {
	const auto history = _parts[_part];
	auto &owner = history->owner();
	auto count = -1;
	auto list = (const QVector<MTPMessage>*)nullptr;
	result.match([&](const MTPDmessages_messages &data) {
		owner.processUsers(data.vusers());
		owner.processChats(data.vchats());
		list = &data.vmessages().v;
	}, [&](const MTPDmessages_messagesSlice &data) {
		owner.processUsers(data.vusers());
		owner.processChats(data.vchats());
		list = &data.vmessages().v;
		count = data.vcount().v;
	}, [&](const MTPDmessages_channelMessages &data) {
		owner.processUsers(data.vusers());
		owner.processChats(data.vchats());
		if (const auto channel = history->peer->asChannel()) {
			channel->ptsReceived(data.vpts().v);
		}
		list = &data.vmessages().v;
		count = data.vcount().v;
	}, [](const MTPDmessages_messagesNotModified &) {
	});
	if (!list || list->isEmpty()) {
		nextPart();
		return;
	}
	if (!_offsetId) {
		auto progress = _progress.current();
		progress.found += (count >= 0) ? count : int(list->size());
		progress.counted = true;
		_progress = progress;
	}

	// Service messages, like "joined the group", can't be deleted.
	auto ids = QVector<MTPint>();
	auto minId = MsgId(std::numeric_limits<int64>::max());
	for (const auto &message : *list) {
		const auto id = IdFromMessage(message);
		minId = std::min(minId, id);
		if (message.type() == mtpc_message) {
			ids.push_back(MTP_int(id));
		}
	}
	if (_offsetId && minId >= _offsetId) {
		nextPart(); // The server repeats the page, nothing older is left.
	} else if (ids.isEmpty()) {
		_offsetId = minId;
		searchNext();
	} else {
		deleteNext(std::move(ids), minId);
	}
}

void Deleter::deleteNext(QVector<MTPint> ids, MsgId nextOffsetId) {
	if (_stopped) {
		finish();
		return;
	}
	const auto history = _parts[_part];
	const auto peer = history->peer;
	const auto channel = peer->asChannel();
	const auto api = &peer->session().api();
	const auto weak = base::make_weak(this);
	using Type = Data::Histories::RequestType;
	history->owner().histories().sendRequest(history, Type::Delete, [=](
			Fn<void()> finishRequest) {
		const auto done = [=](const MTPmessages_AffectedMessages &result) {
			finishRequest();
			DestroyLocal(history, ids);
			api->applyAffectedMessages(peer, result);
			history->requestChatListMessage();
			const auto count = result.match([](
					const MTPDmessages_affectedMessages &data) {
				return data.vpts_count().v;
			});
			if (const auto strong = weak.get()) {
				strong->deleted(count, nextOffsetId);
			}
		};
		const auto fail = [=](const MTP::Error &error) {
			finishRequest();
			if (const auto strong = weak.get()) {
				if (error.type() == u"MESSAGE_DELETE_FORBIDDEN"_q) {
					strong->deleted(0, nextOffsetId); // Skip this page.
				} else {
					strong->failed(error.type());
				}
			}
		};
		if (channel) {
			return api->request(MTPchannels_DeleteMessages(
				channel->inputChannel(),
				MTP_vector<MTPint>(ids)
			)).done(done).fail(fail).send();
		}
		using Flag = MTPmessages_DeleteMessages::Flag;
		return api->request(MTPmessages_DeleteMessages(
			MTP_flags(Flag::f_revoke),
			MTP_vector<MTPint>(ids)
		)).done(done).fail(fail).send();
	});
}

void Deleter::deleted(int count, MsgId nextOffsetId) {
	auto progress = _progress.current();
	progress.deleted += count;
	_progress = progress;
	_offsetId = nextOffsetId;
	searchNext();
}

void Deleter::failed(const QString &error) {
	if (_part > 0) {
		nextPart(); // The legacy group may be unavailable, it's fine.
	} else {
		finish(error);
	}
}

void Deleter::nextPart() {
	if (++_part < int(_parts.size())) {
		_offsetId = 0;
		searchNext();
	} else {
		finish();
	}
}

void Deleter::finish(const QString &error) {
	auto progress = _progress.current();
	progress.finished = true;
	_progress = progress;

	const auto text = !error.isEmpty()
		? Tr(Phrase::DeleteMyFailed).arg(error)
		: _stopped
		? Tr(Phrase::DeleteMyStopped).arg(progress.deleted)
		: progress.deleted
		? Tr(Phrase::DeleteMyDone).arg(progress.deleted)
		: Tr(Phrase::DeleteMyNothing);
	if (const auto strong = _controller.get()) {
		strong->showToast(text);
	}

	// This object is destroyed after the current callback returns.
	const auto peer = _peer;
	const auto session = &peer->session();
	crl::on_main(session, [=] {
		Deleters(session).remove(peer);
	});
}

[[nodiscard]] QString ProgressText(const Progress &progress) {
	if (progress.stopping) {
		return Tr(Phrase::DeleteMyStopping).arg(progress.deleted);
	} else if (!progress.counted) {
		return Tr(Phrase::DeleteMySearching);
	}
	return Tr(Phrase::DeleteMyProgress)
		.arg(progress.deleted)
		.arg(std::max(progress.found, progress.deleted));
}

void ProgressBox(not_null<Ui::GenericBox*> box, not_null<PeerData*> peer) {
	box->setTitle(TrValue(Phrase::DeleteMyTitle));
	box->setWidth(st::boxWideWidth);

	const auto deleter = FindDeleter(peer);
	auto text = rpl::producer<QString>();
	if (deleter) {
		text = deleter->progressValue() | rpl::map(ProgressText);
	} else {
		text = rpl::single(Tr(Phrase::DeleteMyNothing));
	}
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		std::move(text),
		st::boxLabel));
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			TrValue(Phrase::DeleteMyBackground),
			st::boxDividerLabel),
		st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, 0));

	if (deleter) {
		deleter->progressValue(
		) | rpl::filter([](const Progress &progress) {
			return progress.finished;
		}) | rpl::take(1) | rpl::on_next([=] {
			box->closeBox();
		}, box->lifetime());

		box->addButton(TrValue(Phrase::DeleteMyStop), [=] {
			if (const auto deleter = FindDeleter(peer)) {
				deleter->stop();
			}
			box->closeBox();
		}, st::attentionBoxButton);
	}
	box->addButton(TrValue(Phrase::DeleteMyHide), [=] {
		box->closeBox();
	});
}

// Loads the whole chats list and the archive, then reads every chat.
class AllReader final : public base::has_weak_ptr {
public:
	explicit AllReader(not_null<Window::SessionController*> controller);

	void start();

private:
	[[nodiscard]] Data::Folder *archive() const;
	[[nodiscard]] bool loaded() const;
	void requestMore();
	void readChats();
	void readNextExtra();
	void readMentions(not_null<PeerData*> peer);
	void readReactions(not_null<PeerData*> peer);
	void finish();

	const base::weak_ptr<Window::SessionController> _controller;
	const not_null<Main::Session*> _session;
	std::vector<not_null<PeerData*>> _mentions;
	std::vector<not_null<PeerData*>> _reactions;
	base::Timer _timeout;
	bool _chatsRead = false;
	rpl::lifetime _loadLifetime;

};

using ReadersMap = base::flat_map<
	not_null<Main::Session*>,
	std::unique_ptr<AllReader>>;

[[nodiscard]] ReadersMap &Readers() {
	static auto result = ReadersMap();
	return result;
}

AllReader::AllReader(not_null<Window::SessionController*> controller)
: _controller(controller)
, _session(&controller->session())
, _timeout([=] { readChats(); }) {
}

void AllReader::start() {
	if (loaded()) {
		readChats();
		return;
	}
	if (const auto strong = _controller.get()) {
		strong->showToast(Tr(Phrase::ReadAllLoading));
	}
	auto &owner = _session->data();
	rpl::merge(
		owner.chatsListChanges(),
		owner.chatsListLoadedEvents()
	) | rpl::on_next([=] {
		if (loaded()) {
			readChats();
		} else {
			requestMore();
		}
	}, _loadLifetime);
	_timeout.callOnce(kReadAllLoadTimeout);
	requestMore();
}

Data::Folder *AllReader::archive() const {
	return _session->data().folderLoaded(Data::Folder::kId);
}

bool AllReader::loaded() const {
	auto &owner = _session->data();
	const auto folder = archive();
	return owner.chatsListLoaded(nullptr)
		&& (!folder || owner.chatsListLoaded(folder));
}

void AllReader::requestMore() {
	auto &owner = _session->data();
	auto &api = _session->api();
	if (!owner.chatsListLoaded(nullptr)) {
		api.requestDialogs(nullptr);
	}
	// The archive is loaded by Telegram only when it is opened.
	if (const auto folder = archive()) {
		if (!owner.chatsListLoaded(folder)) {
			api.requestDialogs(folder);
		}
	}
}

void AllReader::readChats() {
	if (_chatsRead) {
		return;
	}
	_chatsRead = true;
	_loadLifetime.destroy();
	_timeout.cancel();

	auto histories = std::vector<not_null<History*>>();
	const auto collect = [&](not_null<Dialogs::MainList*> list) {
		for (const auto &row : list->indexed()->all()) {
			if (const auto history = row->history()) {
				histories.push_back(history);
			}
		}
	};
	collect(_session->data().chatsList());
	if (const auto folder = archive()) {
		collect(folder->chatsList());
	}

	// In ghost mode the chats are read only locally, like any other.
	const auto ghost = Current().ghostHideRead;
	using Muted = MarkAsReadMenu::MarkAsReadMuted;
	for (const auto &history : histories) {
		const auto state = history->chatListUnreadState();
		MarkAsReadMenu::MarkAsReadThread(history, Muted::Include);
		if (ghost) {
			continue;
		} else if (state.mentions > 0) {
			_mentions.push_back(history->peer);
		}
		if (state.reactions > 0 || state.reactionsMuted > 0) {
			_reactions.push_back(history->peer);
		}
	}
	readNextExtra();
}

void AllReader::readNextExtra() {
	if (!_mentions.empty()) {
		const auto peer = _mentions.back();
		_mentions.pop_back();
		readMentions(peer);
	} else if (!_reactions.empty()) {
		const auto peer = _reactions.back();
		_reactions.pop_back();
		readReactions(peer);
	} else {
		finish();
	}
}

void AllReader::readMentions(not_null<PeerData*> peer) {
	const auto weak = base::make_weak(this);
	using Flag = MTPmessages_ReadMentions::Flag;
	_session->api().request(MTPmessages_ReadMentions(
		MTP_flags(Flag(0)),
		peer->input(),
		MTP_int(0) // top_msg_id
	)).done([=](const MTPmessages_AffectedHistory &result) {
		const auto offset = peer->session().api().applyAffectedHistory(
			peer,
			result);
		if (offset <= 0) {
			peer->owner().history(peer)->clearUnreadMentionsFor(MsgId());
		}
		if (const auto strong = weak.get()) {
			if (offset > 0) {
				strong->readMentions(peer);
			} else {
				strong->readNextExtra();
			}
		}
	}).fail([=] {
		if (const auto strong = weak.get()) {
			strong->readNextExtra();
		}
	}).send();
}

void AllReader::readReactions(not_null<PeerData*> peer) {
	const auto weak = base::make_weak(this);
	using Flag = MTPmessages_ReadReactions::Flag;
	_session->api().request(MTPmessages_ReadReactions(
		MTP_flags(Flag(0)),
		peer->input(),
		MTP_int(0), // top_msg_id
		MTPInputPeer() // saved_peer_id
	)).done([=](const MTPmessages_AffectedHistory &result) {
		const auto offset = peer->session().api().applyAffectedHistory(
			peer,
			result);
		if (offset <= 0) {
			peer->owner().history(peer)->clearUnreadReactionsFor(
				MsgId(),
				nullptr);
		}
		if (const auto strong = weak.get()) {
			if (offset > 0) {
				strong->readReactions(peer);
			} else {
				strong->readNextExtra();
			}
		}
	}).fail([=] {
		if (const auto strong = weak.get()) {
			strong->readNextExtra();
		}
	}).send();
}

void AllReader::finish() {
	if (const auto strong = _controller.get()) {
		strong->showToast(Tr(Phrase::ReadAllDone));
	}

	// This object is destroyed after the current callback returns.
	const auto session = _session.get();
	crl::on_main(session, [=] {
		Readers().remove(session);
	});
}

void StartReading(not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	auto &readers = Readers();
	if (readers.contains(session)) {
		return;
	}
	session->lifetime().add([=] {
		Readers().remove(session);
	});
	readers.emplace(
		session,
		std::make_unique<AllReader>(controller)
	).first->second->start();
}

void StartDeleting(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	if (FindDeleter(peer)) {
		return;
	}
	const auto deleter = Deleters(&peer->session()).emplace(
		peer,
		std::make_unique<Deleter>(controller, peer)
	).first->second.get();
	controller->show(Box(ProgressBox, peer));
	deleter->start();
}

} // namespace

void JumpToStart(
		not_null<Window::SessionController*> controller,
		Dialogs::Key key) {
	const auto thread = key.thread();
	if (!thread) {
		return;
	}
	const auto viaThread = key.topic() || key.sublist();
	const auto weak = base::make_weak(controller);
	const auto weakThread = base::make_weak(thread);
	const auto open = [=](not_null<PeerData*> peer, MsgId id) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		} else if (!viaThread) {
			// The peer is the legacy group if the message is there.
			strong->showPeerHistory(
				peer,
				Window::SectionShow::Way::Forward,
				id);
		} else if (const auto strongThread = weakThread.get()) {
			strong->showThread(
				strongThread,
				id,
				Window::SectionShow::Way::Forward);
		}
	};
	controller->session().api().resolveJumpToDate(key, StartDate(), open);
}

bool CanDeleteMyMessages(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return chat->amIn();
	} else if (const auto megagroup = peer->asMegagroup()) {
		return megagroup->amIn();
	}
	return false;
}

QString DeletingMyMessagesText(not_null<PeerData*> peer) {
	const auto deleter = FindDeleter(peer);
	if (!deleter || deleter->progress().finished) {
		return QString();
	}
	const auto progress = deleter->progress();
	return Tr(Phrase::DeleteMyRunning)
		.arg(progress.deleted)
		.arg(std::max(progress.found, progress.deleted));
}

void ReadAll(not_null<Window::SessionController*> controller) {
	if (Readers().contains(&controller->session())) {
		controller->showToast(Tr(Phrase::ReadAllRunning));
		return;
	}
	auto text = Tr(Phrase::ReadAllText);
	if (Current().ghostHideRead) {
		text += u"\n\n"_q + Tr(Phrase::ReadAllGhost);
	}
	const auto weak = base::make_weak(controller);
	controller->show(Ui::MakeConfirmBox({
		.text = text,
		.confirmed = [=](Fn<void()> close) {
			close();
			if (const auto strong = weak.get()) {
				StartReading(strong);
			}
		},
		.confirmText = TrValue(Phrase::ReadAllConfirm),
		.title = TrValue(Phrase::ReadAll),
	}));
}

void DeleteMyMessages(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	if (FindDeleter(peer)) {
		controller->show(Box(ProgressBox, peer));
		return;
	}
	const auto weak = base::make_weak(controller);
	controller->show(Ui::MakeConfirmBox({
		.text = Tr(Phrase::DeleteMyText).arg(peer->name()),
		.confirmed = [=](Fn<void()> close) {
			close();
			if (const auto strong = weak.get()) {
				StartDeleting(strong, peer);
			}
		},
		.confirmText = tr::lng_box_delete(),
		.confirmStyle = &st::attentionBoxButton,
		.title = TrValue(Phrase::DeleteMyTitle),
	}));
}

} // namespace Soulgram

/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_forward.h"

#include "api/api_sending.h"
#include "apiwrap.h"
#include "base/base_file_utilities.h"
#include "base/timer.h"
#include "core/application.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_messages.h"
#include "soulgram/soulgram_premium.h"
#include "soulgram/soulgram_settings.h"
#include "soulgram/soulgram_storage.h"
#include "storage/localimageloader.h"
#include "storage/storage_media_prepare.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/image/image.h"
#include "ui/layers/show.h"
#include "window/window_controller.h"
#include "styles/style_boxes.h"

namespace Soulgram {
namespace {

// Media files are copied here before they are sent again.
constexpr auto kFolder = "forward/";

// Older copies are removed when copying starts.
constexpr auto kKeepFiles = 7 * 24 * 60 * 60;

constexpr auto kCheckInterval = crl::time(300);

// How long a sent file may be prepared before the next one is sent.
constexpr auto kPrepareTimeout = 120 * crl::time(1000);

[[nodiscard]] bool NeedsCopy(not_null<const HistoryItem*> item) {
	const auto media = item->media();
	return IsDeleted(item)
		|| item->forbidsForward()
		|| !item->history()->peer->allowsForwarding()
		|| (media && media->ttlSeconds());
}

[[nodiscard]] bool Copyable(not_null<const HistoryItem*> item) {
	if (!item->isRegular() || item->isService()) {
		return false;
	}
	const auto media = item->media();
	return !media
		|| media->webpage()
		|| dynamic_cast<const Data::MediaPhoto*>(media)
		|| dynamic_cast<const Data::MediaFile*>(media);
}

// A link preview is sent again by the server from the text.
[[nodiscard]] PhotoData *PhotoOf(not_null<const HistoryItem*> item) {
	const auto media = item->media();
	return (media && !media->webpage()) ? media->photo() : nullptr;
}

[[nodiscard]] DocumentData *DocumentOf(not_null<const HistoryItem*> item) {
	const auto media = item->media();
	return (media && !media->webpage()) ? media->document() : nullptr;
}

[[nodiscard]] TextWithTags TextOf(not_null<const HistoryItem*> item) {
	const auto &text = item->originalText();
	return {
		text.text,
		TextUtilities::ConvertEntitiesToTextTags(text.entities),
	};
}

[[nodiscard]] bool WriteBytes(const QString &path, const QByteArray &bytes) {
	QDir().mkpath(QFileInfo(path).absolutePath());
	auto file = QFile(path);
	return file.open(QIODevice::WriteOnly)
		&& (file.write(bytes) == bytes.size());
}

void RemoveOldFiles(const QString &folder) {
	const auto old = QDateTime::currentDateTime().addSecs(-kKeepFiles);
	auto dir = QDir(folder);
	const auto filters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;
	for (const auto &info : dir.entryInfoList(filters)) {
		if (info.lastModified() >= old) {
			continue;
		} else if (info.isDir()) {
			QDir(info.absoluteFilePath()).removeRecursively();
		} else {
			QFile::remove(info.absoluteFilePath());
		}
	}
}

// One message, or all messages of an album.
struct Part {
	std::vector<FullMsgId> ids;
};

// A media file to send again.
struct File {
	FullMsgId itemId;
	std::shared_ptr<Data::PhotoMedia> photo;
	std::shared_ptr<Data::DocumentMedia> media;
	DocumentData *document = nullptr;
	QString path;
	bool started = false; // Our own download was started.
	bool ready = false;
	bool failed = false;
};

class Copier final : public base::has_weak_ptr {
public:
	Copier(
		not_null<Main::Session*> session,
		const std::vector<not_null<HistoryItem*>> &items,
		std::vector<Api::SendAction> actions,
		TextWithTags comment,
		Data::ForwardOptions options,
		std::shared_ptr<Ui::Show> show,
		Fn<void()> done);

	void start();

private:
	struct Waiting {
		not_null<History*> history;
		int expected = 0;
		std::vector<FullMsgId> ids;
		crl::time started = 0;
	};

	void addFile(not_null<HistoryItem*> item);
	void checkFiles();
	void checkFile(File &file);
	void sendNext();
	[[nodiscard]] int sendPart(const Part &part, const Api::SendAction &action);
	[[nodiscard]] int sendText(
		TextWithTags text,
		const Api::SendAction &action);
	[[nodiscard]] int sendFiles(
		const std::vector<not_null<HistoryItem*>> &items,
		const Api::SendAction &action);
	[[nodiscard]] File *findFile(FullMsgId itemId);
	void checkWaiting();
	void finish();
	void toast(const QString &text);

	const not_null<Main::Session*> _session;
	const QString _folder;
	std::vector<Part> _parts;
	std::vector<Api::SendAction> _actions;
	TextWithTags _comment;
	const Data::ForwardOptions _options;
	const std::shared_ptr<Ui::Show> _show;
	Fn<void()> _done;

	std::vector<File> _files;
	bool _sending = false;
	int _action = 0;
	int _part = 0;
	bool _commentSent = false;
	int _sent = 0;
	int _failed = 0;
	std::optional<Waiting> _waiting;
	base::Timer _timer;
	rpl::lifetime _lifetime;

};

using CopiersMap = base::flat_map<
	not_null<Main::Session*>,
	std::vector<std::unique_ptr<Copier>>>;

[[nodiscard]] CopiersMap &Copiers() {
	static auto result = CopiersMap();
	return result;
}

void RemoveCopier(not_null<Main::Session*> session, not_null<Copier*> copier) {
	auto &map = Copiers();
	const auto i = map.find(session);
	if (i == end(map)) {
		return;
	}
	auto &list = i->second;
	list.erase(
		ranges::remove(list, copier.get(), &std::unique_ptr<Copier>::get),
		end(list));
	if (list.empty()) {
		map.erase(i);
	}
}

Copier::Copier(
	not_null<Main::Session*> session,
	const std::vector<not_null<HistoryItem*>> &items,
	std::vector<Api::SendAction> actions,
	TextWithTags comment,
	Data::ForwardOptions options,
	std::shared_ptr<Ui::Show> show,
	Fn<void()> done)
: _session(session)
, _folder(AccountFolder(session) + QString::fromLatin1(kFolder))
, _actions(std::move(actions))
, _comment(std::move(comment))
, _options(options)
, _show(std::move(show))
, _done(std::move(done))
, _timer([=] { checkFiles(); checkWaiting(); }) {
	auto groupId = MessageGroupId();
	for (const auto &item : items) {
		const auto group = item->groupId();
		if (group && group == groupId && !_parts.empty()) {
			_parts.back().ids.push_back(item->fullId());
		} else {
			_parts.push_back({ { item->fullId() } });
		}
		groupId = group;
		addFile(item);
	}
}

void Copier::start() {
	RemoveOldFiles(_folder);

	_session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		checkFiles();
	}, _lifetime);
	_session->data().documentLoadProgress(
	) | rpl::on_next([=] {
		checkFiles();
	}, _lifetime);
	_session->data().newItemAdded(
	) | rpl::on_next([=](not_null<HistoryItem*> item) {
		if (_waiting
			&& item->history() == _waiting->history
			&& IsClientMsgId(item->id)) {
			_waiting->ids.push_back(item->fullId());
		}
	}, _lifetime);
	_session->data().itemIdChanged(
	) | rpl::on_next([=] {
		checkWaiting();
	}, _lifetime);

	_timer.callEach(kCheckInterval);
	if (ranges::any_of(_files, [](const File &file) { return !file.ready; })) {
		toast(Tr(Phrase::CopyForwardPreparing));
	}
	checkFiles();
}

void Copier::addFile(not_null<HistoryItem*> item) {
	const auto document = DocumentOf(item);
	const auto photo = PhotoOf(item);
	if (document && document->sticker()) {
		return; // Sent by itself, not as a file.
	} else if (!document && !photo) {
		return;
	}
	const auto origin = Data::FileOrigin(item->fullId());
	const auto name = QString::number(item->history()->peer->id.value)
		+ '_'
		+ QString::number(item->id.bare);
	auto file = File{ .itemId = item->fullId() };
	if (photo) {
		file.path = _folder + name + u".jpg"_q;
		file.photo = photo->createMediaView();
		file.photo->wanted(Data::PhotoSize::Large, origin);
	} else {
		// The file keeps its name, the receiver sees it.
		const auto filename = base::FileNameFromUserString(
			document->filename());
		file.path = _folder
			+ name
			+ '/'
			+ (filename.isEmpty() ? u"file"_q : filename);
		file.document = document;
		file.media = document->createMediaView();
	}
	_files.push_back(std::move(file));
}

void Copier::checkFiles() {
	if (_sending) {
		return;
	}
	for (auto &file : _files) {
		if (!file.ready && !file.failed) {
			checkFile(file);
		}
	}
	const auto waiting = ranges::any_of(_files, [](const File &file) {
		return !file.ready && !file.failed;
	});
	if (!waiting) {
		_sending = true;
		sendNext();
	}
}

void Copier::checkFile(File &file) {
	if (const auto photo = file.photo.get()) {
		if (photo->owner()->failed(Data::PhotoSize::Large)) {
			file.failed = true;
			return;
		} else if (!photo->loaded()) {
			return;
		}
		constexpr auto large = Data::PhotoSize::Large;
		const auto bytes = photo->imageBytes(large);
		const auto image = photo->image(large);
		const auto written = !bytes.isEmpty()
			? WriteBytes(file.path, bytes)
			: (image
				&& QDir().mkpath(QFileInfo(file.path).absolutePath())
				&& image->original().save(file.path, "JPG"));
		(written ? file.ready : file.failed) = true;
		return;
	}
	const auto document = file.document;
	const auto complete = [&](const QString &path) {
		const auto info = QFileInfo(path);
		return info.isFile()
			&& (document->size <= 0 || info.size() == document->size);
	};
	if (complete(file.path)) {
		file.ready = true;
		return;
	}
	const auto existing = document->filepath(true);
	if (!existing.isEmpty() && complete(existing)) {
		file.path = existing; // Already downloaded, sent from there.
		file.ready = true;
		return;
	} else if (document->loading()) {
		// Somebody else's download would be cancelled by ours, wait.
		return;
	} else if (const auto bytes = file.media->bytes(); !bytes.isEmpty()) {
		(WriteBytes(file.path, bytes) ? file.ready : file.failed) = true;
		return;
	} else if (file.started) {
		file.failed = true; // Our download failed or was cancelled.
		return;
	}
	file.started = true;
	QDir().mkpath(QFileInfo(file.path).absolutePath());
	document->save(Data::FileOrigin(file.itemId), file.path);
	if (complete(file.path)) {
		file.ready = true;
	} else if (!document->loading()) {
		file.failed = true;
	}
}

File *Copier::findFile(FullMsgId itemId) {
	const auto i = ranges::find(_files, itemId, &File::itemId);
	return (i != end(_files) && i->ready) ? &*i : nullptr;
}

void Copier::sendNext() {
	while (_action < int(_actions.size())) {
		const auto &action = _actions[_action];
		auto expected = 0;
		if (!_commentSent) {
			_commentSent = true;
			expected = sendText(base::take(_comment), action);
		} else if (_part < int(_parts.size())) {
			expected = sendPart(_parts[_part++], action);
		} else {
			++_action;
			_part = 0;
			continue;
		}
		// The send set _waiting up before it started: some sends add
		// their messages right away, others after preparing the files.
		// Scheduled messages don't appear in the history, nothing to wait.
		const auto scheduled = action.options.scheduled
			|| action.options.shortcutId;
		if (expected > 0 && _waiting && !scheduled) {
			_waiting->expected = expected;
			checkWaiting();
			return;
		}
		_waiting = std::nullopt;
	}
	finish();
}

int Copier::sendText(TextWithTags text, const Api::SendAction &action) {
	if (text.text.isEmpty()) {
		return 0;
	}
	auto message = Api::MessageToSend(action);
	message.textWithTags = std::move(text);
	_waiting = Waiting{ .history = action.history, .started = crl::now() };
	_session->api().sendMessage(std::move(message));
	return 1;
}

int Copier::sendPart(const Part &part, const Api::SendAction &action) {
	auto items = std::vector<not_null<HistoryItem*>>();
	for (const auto &id : part.ids) {
		if (const auto item = _session->data().message(id)) {
			items.push_back(item);
		}
	}
	if (items.empty()) {
		return 0;
	}
	const auto first = items.front();
	const auto document = DocumentOf(first);
	if (items.size() == 1 && document && document->sticker()) {
		_waiting = Waiting{ .history = action.history, .started = crl::now() };
		Api::SendExistingDocument(Api::MessageToSend(action), document);
		++_sent;
		return 1;
	} else if (items.size() == 1
		&& document
		&& (document->isVoiceMessage() || document->isVideoMessage())) {
		if (const auto file = findFile(first->fullId())) {
			auto source = QFile(file->path);
			if (source.open(QIODevice::ReadOnly)) {
				const auto voice = document->voice();
				_waiting = Waiting{
					.history = action.history,
					.started = crl::now(),
				};
				_session->api().sendVoiceMessage(
					source.readAll(),
					voice ? voice->waveform : VoiceWaveform(),
					document->duration(),
					document->isVideoMessage(),
					action);
				++_sent;
				return 1;
			}
		}
		++_failed;
		return 0;
	}
	if (!PhotoOf(first) && !document) {
		const auto sent = sendText(TextOf(first), action);
		_sent += sent;
		return sent;
	}
	return sendFiles(items, action);
}

int Copier::sendFiles(
		const std::vector<not_null<HistoryItem*>> &items,
		const Api::SendAction &action) {
	const auto withCaption = (_options
		!= Data::ForwardOptions::NoNamesAndCaptions);
	auto paths = QStringList();
	auto captions = std::vector<TextWithTags>();
	auto asMedia = false;
	auto lost = TextWithTags();
	for (const auto &item : items) {
		const auto file = findFile(item->fullId());
		if (!file) {
			++_failed;
			if (lost.text.isEmpty() && withCaption) {
				lost = TextOf(item);
			}
			continue;
		}
		paths.push_back(file->path);
		captions.push_back(withCaption ? TextOf(item) : TextWithTags());
		const auto document = DocumentOf(item);
		if (PhotoOf(item)
			|| (document
				&& (document->isVideoFile()
					|| document->isGifv()
					|| document->isAnimation()))) {
			asMedia = true;
		}
	}
	if (paths.isEmpty()) {
		// Not even the media of this message could be copied.
		return sendText(std::move(lost), action);
	}
	auto list = Storage::PrepareMediaList(
		paths,
		st::sendMediaPreviewSize,
		ServerPremium(_session));
	if (list.error != Ui::PreparedList::Error::None
		|| list.files.size() != paths.size()) {
		_failed += paths.size();
		return sendText(std::move(lost), action);
	}
	for (auto i = 0, count = int(list.files.size()); i != count; ++i) {
		list.files[i].caption = std::move(captions[i]);
	}
	auto way = Ui::SendFilesWay();
	way.setGroupFiles(true);
	way.setSendImagesAsPhotos(asMedia);
	const auto type = asMedia ? SendMediaType::Photo : SendMediaType::File;
	auto groups = Ui::DivideByGroups(
		std::move(list),
		way,
		action.history->peer->slowmodeApplied());
	auto expected = 0;
	_waiting = Waiting{ .history = action.history, .started = crl::now() };
	for (auto &group : groups) {
		const auto album = (group.type != Ui::AlbumType::None)
			? std::make_shared<SendingAlbum>()
			: nullptr;
		expected += int(group.list.files.size());
		_session->api().sendFiles(std::move(group.list), type, album, action);
	}
	_sent += expected;
	return expected;
}

void Copier::checkWaiting() {
	if (!_waiting) {
		return;
	}
	const auto &waiting = *_waiting;
	if (int(waiting.ids.size()) < waiting.expected
		&& crl::now() - waiting.started < kPrepareTimeout) {
		return; // The files are still being prepared.
	}
	for (const auto &id : waiting.ids) {
		// A sent message gets another id, so it is not found.
		const auto item = _session->data().message(id);
		if (item && item->isSending() && !item->hasFailed()) {
			return;
		}
	}
	_waiting = std::nullopt;
	sendNext();
}

void Copier::finish() {
	_timer.cancel();
	_lifetime.destroy();
	if (_failed) {
		toast(Tr(Phrase::CopyForwardFailed).arg(_failed));
	} else if (!_show && _sent) {
		toast(Tr(Phrase::CopyForwardDone).arg(_sent));
	}
	if (const auto done = base::take(_done)) {
		done();
	}

	// This object is destroyed after the current callback returns.
	const auto session = _session.get();
	const auto copier = this;
	crl::on_main(session, [=] {
		RemoveCopier(session, copier);
	});
}

void Copier::toast(const QString &text) {
	if (_show && _show->valid()) {
		_show->showToast(text);
	} else if (const auto window = Core::App().activeWindow()) {
		window->showToast(text);
	}
}

} // namespace

bool CanCopyForward(not_null<const HistoryItem*> item) {
	return Copyable(item)
		&& NeedsCopy(item)
		&& (IsDeleted(item) || Current().copyProtected);
}

bool CopyForward(
		const std::vector<not_null<HistoryItem*>> &items,
		std::vector<Api::SendAction> actions,
		TextWithTags comment,
		Data::ForwardOptions options,
		std::shared_ptr<Ui::Show> show,
		Fn<void()> done) {
	const auto needed = [](not_null<HistoryItem*> item) {
		return CanCopyForward(item);
	};
	if (items.empty()
		|| actions.empty()
		|| ranges::none_of(items, needed)) {
		return false;
	}
	for (auto &action : actions) {
		action.clearDraft = false;
		action.generateLocal = true;
	}
	const auto session = &items.front()->history()->session();
	if (!Copiers().contains(session)) {
		session->lifetime().add([=] {
			Copiers().remove(session);
		});
	}
	auto &list = Copiers()[session];
	list.push_back(std::make_unique<Copier>(
		session,
		items,
		std::move(actions),
		std::move(comment),
		options,
		std::move(show),
		std::move(done)));
	list.back()->start();
	return true;
}

} // namespace Soulgram

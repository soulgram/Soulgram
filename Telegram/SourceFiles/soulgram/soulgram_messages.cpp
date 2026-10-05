/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_messages.h"

#include "api/api_common.h"
#include "api/api_text_entities.h"
#include "base/base_file_utilities.h"
#include "base/unixtime.h"
#include "core/file_location.h"
#include "core/version.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "history/view/history_view_element.h"
#include "main/main_session.h"
#include "soulgram/soulgram_filters.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_settings.h"
#include "soulgram/soulgram_storage.h"
#include "storage/cache/storage_cache_database.h"
#include "storage/serialize_common.h"
#include "storage/serialize_peer.h"
#include "ui/image/image.h"

#include <unordered_map>

namespace Soulgram {
namespace {

// Bigger files of deleted messages are not copied to disk.
constexpr auto kMaxSavedFileSize = int64(300) * 1024 * 1024;

[[nodiscard]] bool CanKeep(not_null<HistoryItem*> item) {
	return item->isRegular()
		&& !item->isService()
		&& !item->history()->peer->isSelf();
}

using RawMap = std::unordered_map<const HistoryItem*, QByteArray>;

// Items of a destroyed session are deleted without being unregistered,
// so the copies are kept per session.
[[nodiscard]] RawMap &Raw(not_null<Main::Session*> session) {
	static auto result = base::flat_map<not_null<Main::Session*>, RawMap>();
	const auto i = result.find(session);
	if (i != end(result)) {
		return i->second;
	}
	session->lifetime().add([=] {
		result.remove(session);
	});
	return result.emplace(session, RawMap()).first->second;
}

[[nodiscard]] RawMap &Raw(not_null<const HistoryItem*> item) {
	return Raw(&item->history()->session());
}

[[nodiscard]] QByteArray SerializeMessage(const MTPMessage &data) {
	auto buffer = mtpBuffer();
	data.write(buffer);
	return QByteArray(
		reinterpret_cast<const char*>(buffer.constData()),
		buffer.size() * sizeof(mtpPrime));
}

[[nodiscard]] std::optional<MTPMessage> ParseMessage(const QByteArray &bytes) {
	if (bytes.isEmpty() || (bytes.size() % sizeof(mtpPrime))) {
		return std::nullopt;
	}
	auto buffer = mtpBuffer(bytes.size() / sizeof(mtpPrime));
	memcpy(buffer.data(), bytes.constData(), bytes.size());
	auto from = buffer.constData();
	const auto end = from + buffer.size();
	auto result = MTPMessage();
	if (!result.read(from, end) || from != end) {
		return std::nullopt;
	}
	return result;
}

// Sender and the original author of a forward, they may be unknown after
// a restart, when the server doesn't send them with the history.
[[nodiscard]] std::vector<not_null<PeerData*>> PeersToKeep(
		not_null<HistoryItem*> item) {
	auto result = std::vector<not_null<PeerData*>>();
	const auto add = [&](PeerData *peer) {
		const auto known = [&](not_null<PeerData*> already) {
			return (already == peer);
		};
		if (peer
			&& peer != item->history()->peer
			&& ranges::none_of(result, known)) {
			result.push_back(peer);
		}
	};
	add(item->from());
	add(item->originalSender());
	add(item->viaBot());
	return result;
}

[[nodiscard]] QByteArray SerializePeers(
		const std::vector<not_null<PeerData*>> &peers) {
	if (peers.empty()) {
		return QByteArray();
	}
	auto size = int(sizeof(quint32));
	for (const auto &peer : peers) {
		size += Serialize::peerSize(peer);
	}
	auto stream = Serialize::ByteArrayWriter(size);
	stream << quint32(peers.size());
	for (const auto &peer : peers) {
		Serialize::writePeer(stream, peer);
	}
	return std::move(stream).result();
}

void ApplyPeers(
		not_null<Main::Session*> session,
		const QByteArray &serialized,
		int appVersion) {
	if (serialized.isEmpty()) {
		return;
	}
	auto stream = Serialize::ByteArrayReader(serialized);
	auto count = quint32();
	stream >> count;
	for (auto i = 0; stream.ok() && i != int(count); ++i) {
		// Applies the data only to peers that are not loaded.
		if (!Serialize::readPeer(session, appVersion, stream)) {
			break;
		}
	}
}

[[nodiscard]] QString MediaDescription(not_null<HistoryItem*> item) {
	const auto media = item->media();
	if (!media) {
		return QString();
	}
	const auto text = media->notificationText().text;
	return text.isEmpty() ? Tr(Phrase::MediaMessage) : text;
}

[[nodiscard]] DeletedRecord Serialize(
		not_null<HistoryItem*> item,
		TimeId deletedAt) {
	const auto from = item->from();
	const auto &raw = Raw(item);
	const auto i = raw.find(item.get());
	return {
		.peerId = item->history()->peer->id,
		.msgId = item->id,
		.fromId = from->id,
		.fromName = from->name(),
		.date = item->date(),
		.deletedAt = deletedAt,
		.text = item->originalText().text,
		.media = MediaDescription(item),
		.out = item->out(),
		.message = (i != end(raw)) ? i->second : QByteArray(),
		.layer = int(MTP::details::kCurrentLayer),
		.sender = SerializePeers(PeersToKeep(item)),
		.senderAppVersion = AppVersion,
	};
}

// Only letters and digits, the name comes from the sender.
[[nodiscard]] bool GoodExtension(const QString &extension) {
	if (extension.isEmpty() || extension.size() > 5) {
		return false;
	}
	for (const auto ch : extension) {
		const auto code = ch.unicode();
		const auto good = (code >= 'a' && code <= 'z')
			|| (code >= 'A' && code <= 'Z')
			|| (code >= '0' && code <= '9');
		if (!good) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] QString Extension(not_null<DocumentData*> document) {
	const auto name = document->filename();
	const auto dot = name.lastIndexOf('.');
	if (dot >= 0) {
		const auto extension = name.mid(dot + 1);
		if (GoodExtension(extension)) {
			return u"."_q + extension.toLower();
		}
	}
	const auto mime = document->mimeString().toLower();
	if (document->isVoiceMessage() || mime.contains(u"ogg"_q)) {
		return u".ogg"_q;
	} else if (mime.startsWith(u"image/"_q)) {
		return mime.contains(u"webp"_q) ? u".webp"_q : u".jpg"_q;
	} else if (mime.startsWith(u"audio/"_q)) {
		return u".mp3"_q;
	} else if (mime.startsWith(u"video/"_q)) {
		return u".mp4"_q;
	} else if (mime.contains(u"tgsticker"_q)) {
		return u".tgs"_q;
	}
	return u".bin"_q;
}

[[nodiscard]] bool WriteBytes(const QString &path, const QByteArray &bytes) {
	auto file = QFile(path);
	return file.open(QIODevice::WriteOnly)
		&& (file.write(bytes) == bytes.size())
		&& file.flush();
}

// The file the document was downloaded to, if it is still there.
[[nodiscard]] QString DownloadedPath(not_null<DocumentData*> document) {
	const auto &location = document->location(true);
	return location.check() ? location.name() : QString();
}

[[nodiscard]] bool Complete(
		not_null<DocumentData*> document,
		const QString &path) {
	const auto info = QFileInfo(path);
	if (!info.exists()) {
		return false;
	} else if (document->size > 0) {
		return (info.size() == document->size);
	}
	return !document->loading() || (document->loadingFilePath() != path);
}

// Copies the media of deleted messages to <account>/deleted_media/.
// It never takes over a download somebody else started, because that
// would cancel it: it waits for it to finish and copies the file.
// Files are written off the main thread, they can be up to 300 MB.
class MediaSaver final : public base::has_weak_ptr {
public:
	explicit MediaSaver(not_null<Main::Session*> session);

	void save(not_null<HistoryItem*> item);

private:
	struct Pending {
		uint64 id = 0;
		FullMsgId itemId;
		std::shared_ptr<Data::PhotoMedia> photo;
		std::shared_ptr<Data::DocumentMedia> media;
		DocumentData *document = nullptr;
		QString relative;
		bool started = false; // Our own download was started.
		bool waited = false; // A download of somebody else was running.
		bool writing = false; // The file is written in the background.
		bool copyFailed = false;
	};
	using Writer = Fn<bool(const QString &path)>;

	void scheduleCheck();
	void check();

	// True when the pending copy is saved or can't be saved any more.
	[[nodiscard]] bool finished(Pending &pending);
	[[nodiscard]] bool finishedPhoto(Pending &pending, const QString &path);
	[[nodiscard]] bool finishedDocument(
		Pending &pending,
		const QString &path);

	// Writes in the background, a failed copy is retried another way.
	void write(
		Pending &pending,
		const QString &path,
		bool copy,
		Writer writer);
	void written(uint64 id, bool copy, bool success);
	void store(const Pending &pending);

	const not_null<Main::Session*> _session;
	std::vector<Pending> _pending;
	uint64 _lastId = 0;
	bool _checkScheduled = false;
	rpl::lifetime _lifetime;

};

MediaSaver::MediaSaver(not_null<Main::Session*> session)
: _session(session) {
	// Copies that a previous run didn't finish.
	const auto folder = AccountFolder(_session) + u"deleted_media/"_q;
	const auto parts = QDir(folder).entryList(
		QStringList(u"*.part"_q),
		QDir::Files);
	for (const auto &name : parts) {
		QFile::remove(folder + name);
	}

	_session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		scheduleCheck();
	}, _lifetime);

	// A cancelled or failed download doesn't finish a downloader task.
	_session->data().documentLoadProgress(
	) | rpl::filter([=](not_null<DocumentData*> document) {
		const auto i = ranges::find(
			_pending,
			document.get(),
			&Pending::document);
		return (i != end(_pending));
	}) | rpl::on_next([=] {
		scheduleCheck();
	}, _lifetime);
}

void MediaSaver::save(not_null<HistoryItem*> item) {
	const auto media = item->media();
	if (!media) {
		return;
	}
	const auto folder = u"deleted_media/"_q;
	const auto name = QString::number(item->history()->peer->id.value)
		+ '_'
		+ QString::number(item->id.bare);
	const auto origin = Data::FileOrigin(item->fullId());
	auto pending = Pending{ .id = ++_lastId, .itemId = item->fullId() };
	if (const auto photo = media->photo()) {
		pending.relative = folder + name + u".jpg"_q;
		pending.photo = photo->createMediaView();
		pending.photo->wanted(Data::PhotoSize::Large, origin);
	} else if (const auto document = media->document()) {
		if (document->size > kMaxSavedFileSize) {
			return;
		}
		pending.relative = folder
			+ base::FileNameFromUserString(name + Extension(document));
		pending.document = document;
		pending.media = document->createMediaView();
	} else {
		return;
	}
	const auto root = AccountFolder(_session);
	if (!QDir().mkpath(root + folder)) {
		LOG(("Soulgram Error: could not create '%1'.").arg(root + folder));
		return;
	}
	_pending.push_back(std::move(pending));
	check();
}

void MediaSaver::scheduleCheck() {
	if (_checkScheduled) {
		return;
	}
	_checkScheduled = true;
	crl::on_main(this, [=] {
		_checkScheduled = false;
		check();
	});
}

bool MediaSaver::finished(Pending &pending) {
	if (pending.writing) {
		return false;
	}
	const auto path = AccountFolder(_session) + pending.relative;
	const auto document = pending.document;
	const auto &id = pending.itemId;
	if (!LookupDeleted(_session, id.peer, id.msg)) {
		// The kept message was forgotten, its copy is not needed.
		if (document
			&& document->loading()
			&& document->loadingFilePath() == path) {
			return false; // Remove the file when our download ends.
		}
		QFile::remove(path);
		return true;
	} else if (pending.photo) {
		return finishedPhoto(pending, path);
	} else if (document) {
		return finishedDocument(pending, path);
	}
	return true;
}

bool MediaSaver::finishedPhoto(Pending &pending, const QString &path) {
	const auto photo = pending.photo.get();
	if (!photo->loaded()) {
		return false;
	}
	// Not saveToFile(): it prefers the video of a live photo.
	constexpr auto large = Data::PhotoSize::Large;
	const auto bytes = photo->imageBytes(large);
	const auto image = photo->image(large);
	if (!bytes.isEmpty()) {
		write(pending, path, false, [=](const QString &to) {
			return WriteBytes(to, bytes);
		});
	} else if (image) {
		const auto original = image->original();
		write(pending, path, false, [=](const QString &to) {
			return original.save(to, "JPG");
		});
	} else {
		LOG(("Soulgram Error: could not save '%1'.").arg(path));
		return true;
	}
	return false;
}

bool MediaSaver::finishedDocument(Pending &pending, const QString &path) {
	const auto document = pending.document;
	if (Complete(document, path)) {
		store(pending);
		return true;
	} else if (document->loading()) {
		if (document->loadingFilePath() != path) {
			pending.waited = true;
		}
		return false;
	}
	const auto source = DownloadedPath(document);
	if (!pending.copyFailed
		&& !source.isEmpty()
		&& QFileInfo(source) != QFileInfo(path)) {
		const auto size = document->size;
		write(pending, path, true, [=](const QString &to) {
			return QFile::copy(source, to)
				&& (size <= 0 || QFileInfo(to).size() == size);
		});
		return false;
	}
	if (const auto bytes = pending.media->bytes(); !bytes.isEmpty()) {
		write(pending, path, false, [=](const QString &to) {
			return WriteBytes(to, bytes);
		});
		return false;
	} else if (pending.started || (pending.waited && document->cancelled())) {
		QFile::remove(path); // The download failed or was cancelled.
		return true;
	} else if (!document->filepath(true).isEmpty()) {
		// DocumentData::save() would copy it on the main thread.
		return true;
	}
	pending.started = true;
	document->save(Data::FileOrigin(pending.itemId), path);
	if (Complete(document, path)) {
		store(pending);
		return true;
	} else if (document->loading()) {
		return false;
	}
	QFile::remove(path);
	return true;
}

void MediaSaver::write(
		Pending &pending,
		const QString &path,
		bool copy,
		Writer writer) {
	pending.writing = true;
	const auto id = pending.id;

	// Record the name now: the session may end before the copy does.
	store(pending);
	crl::async([=, weak = base::make_weak(this)] {
		// The file gets its name only when it is complete.
		const auto part = path + u".part"_q;
		QFile::remove(part);
		auto success = writer(part);
		if (success) {
			QFile::remove(path);
			success = QFile::rename(part, path);
		}
		if (!success) {
			QFile::remove(part);
		}
		crl::on_main(weak, [=] {
			written(id, copy, success);
		});
	});
}

void MediaSaver::written(uint64 id, bool copy, bool success) {
	const auto i = ranges::find(_pending, id, &Pending::id);
	if (i == end(_pending)) {
		return;
	}
	i->writing = false;
	if (success) {
		store(*i); // It removes the file if the message was forgotten.
		_pending.erase(i);
		return;
	}
	LOG(("Soulgram Error: could not save '%1'.").arg(i->relative));
	if (copy) {
		i->copyFailed = true;
		scheduleCheck();
	} else {
		_pending.erase(i);
	}
}

void MediaSaver::store(const Pending &pending) {
	StoreDeletedFile(
		_session,
		pending.itemId.peer,
		pending.itemId.msg,
		pending.relative);
}

void MediaSaver::check() {
	for (auto i = begin(_pending); i != end(_pending);) {
		if (finished(*i)) {
			i = _pending.erase(i);
		} else {
			++i;
		}
	}
}

[[nodiscard]] not_null<MediaSaver*> SaverFor(
		not_null<Main::Session*> session) {
	static auto savers = base::flat_map<
		not_null<Main::Session*>,
		std::unique_ptr<MediaSaver>>();
	const auto i = savers.find(session);
	if (i != end(savers)) {
		return i->second.get();
	}
	session->lifetime().add([=] {
		savers.remove(session);
	});
	return savers.emplace(
		session,
		std::make_unique<MediaSaver>(session)
	).first->second.get();
}

void AttachLocalCopy(
		not_null<HistoryItem*> item,
		const DeletedRecord &record) {
	const auto media = item->media();
	if (!media || record.file.isEmpty()) {
		return;
	}
	const auto session = &item->history()->session();
	const auto path = AccountFolder(session) + record.file;
	if (!QFile::exists(path)) {
		return;
	}
	if (const auto document = media->document()) {
		const auto size = document->size;
		if (size > 0 && QFileInfo(path).size() != size) {
			return; // An interrupted download into the final name.
		} else if (document->filepath(true).isEmpty()) {
			document->setLocation(Core::FileLocation(path));
		}
	} else if (const auto photo = media->photo()) {
		// The photo is loaded from the cache, put the copy there.
		const auto &location = photo->location(Data::PhotoSize::Large);
		if (!location.valid()) {
			return;
		}
		auto file = QFile(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return;
		}
		session->data().cache().putIfEmpty(
			location.file().cacheKey(),
			Storage::Cache::Database::TaggedValue(
				file.readAll(),
				Data::kImageCacheTag));
	}
}

[[nodiscard]] std::optional<std::pair<MsgId, MsgId>> LoadedRange(
		not_null<History*> history) {
	auto first = MsgId();
	auto last = MsgId();
	for (const auto &block : history->blocks) {
		for (const auto &view : block->messages) {
			const auto id = view->data()->id;
			if (IsServerMsgId(id)) {
				if (!first) {
					first = id;
				}
				last = id;
			}
		}
	}
	if (!first) {
		return std::nullopt;
	}
	return std::make_pair(first, last);
}

} // namespace

void KeepDeleted(std::vector<not_null<HistoryItem*>> &items) {
	if (items.empty() || !Current().saveDeletedMessages) {
		return;
	}
	const auto now = base::unixtime::now();
	auto destroy = std::vector<not_null<HistoryItem*>>();
	auto kept = std::vector<not_null<HistoryItem*>>();
	destroy.reserve(items.size());
	for (const auto &item : items) {
		if (IsDeleted(item)) {
			continue; // Already kept, nothing to do.
		} else if (!CanKeep(item)) {
			destroy.push_back(item);
			continue;
		}
		item->soulgramMarkDeleted(now);
		kept.push_back(item);
	}
	items = std::move(destroy);

	for (const auto &item : kept) {
		const auto session = &item->history()->session();
		StoreDeleted(session, Serialize(item, now));
		Raw(session).erase(item.get());
		SaverFor(session)->save(item);
		session->data().requestItemViewRefresh(item);
	}
}

bool KeepExpired(not_null<HistoryItem*> item) {
	auto items = std::vector<not_null<HistoryItem*>>{ item };
	KeepDeleted(items);
	return items.empty();
}

void RememberMessage(not_null<HistoryItem*> item, const MTPMessage &data) {
	if (data.type() != mtpc_message
		|| !Current().saveDeletedMessages
		|| !CanKeep(item)
		|| IsDeleted(item)) {
		return;
	}
	Raw(item)[item.get()] = SerializeMessage(data);
}

void RememberExisting(
		not_null<Main::Session*> session,
		const MTPMessage &data) {
	if (data.type() != mtpc_message || !Current().saveDeletedMessages) {
		return;
	}
	const auto &fields = data.c_message();
	const auto item = session->data().message(
		peerFromMTP(fields.vpeer_id()),
		fields.vid().v);
	if (item) {
		RememberMessage(item, data);
	}
}

void RememberSentMessage(
		not_null<HistoryItem*> item,
		const MTPDupdateShortSentMessage &data) {
	if (!Current().saveDeletedMessages
		|| !CanKeep(item)
		|| IsDeleted(item)
		|| Raw(item).contains(item.get())) { // The full data came already.
		return;
	}
	// Built the same way as ScheduledMessages::sendNowSimpleMessage.
	const auto history = item->history();
	auto action = Api::SendAction(history);
	action.replyTo = item->replyTo();
	const auto media = data.vmedia();
	const auto &text = item->originalText();
	using Flag = MTPDmessage::Flag;
	const auto flags = Flag::f_entities
		| Flag::f_from_id
		| (action.replyTo ? Flag::f_reply_to : Flag(0))
		| (data.vttl_period() ? Flag::f_ttl_period : Flag(0))
		| (item->out() ? Flag::f_out : Flag(0))
		| (media ? Flag::f_media : Flag(0))
		| (item->effectId() ? Flag::f_effect : Flag(0));
	RememberMessage(item, MTP_message(
		MTP_flags(flags),
		data.vid(),
		peerToMTP(item->from()->id),
		MTPint(), // from_boosts_applied
		MTPstring(), // from_rank
		peerToMTP(history->peer->id),
		MTPPeer(), // saved_peer_id
		MTPMessageFwdHeader(),
		MTPlong(), // via_bot_id
		MTPlong(), // via_business_bot_id
		MTPPeer(), // guestchat_via_from
		NewMessageReplyHeader(action),
		data.vdate(),
		MTP_string(text.text),
		media ? *media : MTPMessageMedia(MTP_messageMediaEmpty()),
		MTPReplyMarkup(),
		Api::EntitiesToMTP(&history->session(), text.entities),
		MTPint(), // views
		MTPint(), // forwards
		MTPMessageReplies(),
		MTPint(), // edit_date
		MTPstring(), // post_author
		MTPlong(), // grouped_id
		MTPMessageReactions(),
		MTPVector<MTPRestrictionReason>(),
		MTP_int(data.vttl_period().value_or_empty()),
		MTPint(), // quick_reply_shortcut_id
		MTP_long(item->effectId()),
		MTPFactCheck(),
		MTPint(), // report_delivery_until_date
		MTPlong(), // paid_message_stars
		MTPSuggestedPost(),
		MTPint(), // schedule_repeat_period
		MTPstring(), // summary_from_language
		MTPRichMessage()));
}

void ForgetMessage(not_null<HistoryItem*> item) {
	Raw(item).erase(item.get());
	ForgetFiltered(item);
}

std::optional<QVector<MTPMessage>> WithDeleted(
		not_null<History*> history,
		const QVector<MTPMessage> &slice,
		bool older) {
	const auto peer = history->peer;
	const auto session = &history->session();
	if (!Current().saveDeletedMessages
		|| peer->isSelf()
		|| !HasDeleted(session, peer->id)) {
		return std::nullopt;
	}
	const auto kMax = MsgId(std::numeric_limits<int64>::max());
	auto ids = base::flat_set<MsgId>();
	auto minId = kMax;
	auto maxId = MsgId(0);
	for (const auto &message : slice) {
		const auto id = IdFromMessage(message);
		ids.emplace(id);
		minId = std::min(minId, id);
		maxId = std::max(maxId, id);
	}
	const auto loaded = LoadedRange(history);
	const auto last = history->lastServerMessage();
	const auto toBottom = slice.isEmpty() || (last && last->id <= maxId);

	// Messages between after and before are inside the loaded range.
	auto after = MsgId(0);
	auto before = kMax;
	if (older) {
		after = slice.isEmpty() ? MsgId(0) : minId;
		before = loaded ? loaded->first : toBottom ? kMax : maxId;
	} else {
		after = loaded ? loaded->second : slice.isEmpty() ? MsgId(0) : minId;
		before = toBottom ? kMax : maxId;
	}
	if (const auto channel = peer->asChannel()) {
		// The history was cleared up to it, maybe on another device.
		after = std::max(after, channel->availableMinId());
	}
	if (after >= before) {
		LOG(("Soulgram: restore peer %1, %2 slice %3 [%4..%5], empty range "
			"(%6..%7), last %8").arg(peer->id.value
			).arg(older ? u"older"_q : u"newer"_q
			).arg(slice.size()
			).arg(slice.isEmpty() ? 0 : minId.bare
			).arg(maxId.bare
			).arg(after.bare
			).arg(before.bare
			).arg(last ? last->id.bare : 0));
		return std::nullopt;
	}
	auto added = QVector<MTPMessage>();
	auto skipped = QStringList();
	for (const auto &record : DeletedInRange(session, peer->id, after, before)) {
		if (ids.contains(record.msgId)) {
			continue;
		}
		const auto message = ParseMessage(record.message);
		if (!message || IdFromMessage(*message) != record.msgId) {
			skipped.push_back(QString::number(record.msgId.bare));
			continue;
		}
		ApplyPeers(session, record.sender, record.senderAppVersion);
		added.push_back(*message);
	}
	LOG(("Soulgram: restore peer %1, %2 slice %3 [%4..%5], loaded %6..%7, "
		"last %8 (known %9), range (%10..%11), added %12, no data: %13"
		).arg(peer->id.value
		).arg(older ? u"older"_q : u"newer"_q
		).arg(slice.size()
		).arg(slice.isEmpty() ? 0 : minId.bare
		).arg(maxId.bare
		).arg(loaded ? loaded->first.bare : 0
		).arg(loaded ? loaded->second.bare : 0
		).arg(last ? last->id.bare : 0
		).arg(history->lastServerMessageKnown() ? 1 : 0
		).arg(after.bare
		).arg(before == kMax ? -1 : before.bare
		).arg(added.size()
		).arg(skipped.join(',')));
	if (added.isEmpty()) {
		return std::nullopt;
	}
	auto result = slice;
	result.append(added);
	ranges::stable_sort(result, ranges::greater(), [](const MTPMessage &m) {
		return IdFromMessage(m);
	});
	return result;
}

void MarkRestored(const std::vector<not_null<HistoryItem*>> &items) {
	if (items.empty() || !Current().saveDeletedMessages) {
		return;
	}
	const auto history = items.front()->history();
	const auto session = &history->session();
	if (!HasDeleted(session, history->peer->id)) {
		return;
	}
	for (const auto &item : items) {
		if (IsDeleted(item) || !item->isRegular()) {
			continue;
		}
		const auto record = LookupDeleted(
			session,
			item->history()->peer->id,
			item->id);
		if (!record) {
			continue;
		}
		item->soulgramMarkDeleted(record->deletedAt);
		Raw(session).erase(item.get());
		AttachLocalCopy(item, *record);
		LOG(("Soulgram: restored %1 in peer %2, view %3"
			).arg(item->id.bare
			).arg(history->peer->id.value
			).arg(item->mainView() ? 1 : 0));
	}
}

bool ForgetDeletedByUser(not_null<HistoryItem*> item) {
	if (!IsDeleted(item)) {
		return false;
	}
	ForgetDeleted(
		&item->history()->session(),
		item->history()->peer->id,
		item->id);
	return true;
}

void ForgetDeletedHistory(not_null<History*> history) {
	ForgetAllDeleted(&history->session(), history->peer->id);

	// Clearing the history destroys only the messages the server knows.
	auto kept = base::flat_set<not_null<HistoryItem*>>();
	for (const auto &block : history->blocks) {
		for (const auto &view : block->messages) {
			if (IsDeleted(view->data())) {
				kept.emplace(view->data());
			}
		}
	}
	if (const auto last = history->lastMessage(); last && IsDeleted(last)) {
		kept.emplace(last);
	}
	if (kept.empty()) {
		return;
	}
	const auto remove = std::vector<not_null<HistoryItem*>>(
		begin(kept),
		end(kept));
	history->owner().notifyItemsAboutToBeDestroyed(remove);
	for (const auto &item : remove) {
		item->destroy();
	}
}

void ForgetDeletedByDates(
		not_null<History*> history,
		TimeId minDate,
		TimeId maxDate) {
	const auto session = &history->session();
	const auto peerId = history->peer->id;
	for (const auto &record : LoadDeleted(session, peerId)) {
		if (record.date > minDate && record.date < maxDate) {
			ForgetDeleted(session, peerId, record.msgId);
		}
	}
}

bool IsDeleted(not_null<const HistoryItem*> item) {
	return item->soulgramDeletedAt() != 0;
}

QString BottomInfoText(not_null<const HistoryItem*> item) {
	auto parts = QStringList();
	const auto media = item->media();
	if (const auto ttl = media ? media->ttlSeconds() : crl::time()) {
		if (media->ttlSecondsSingleView()) {
			const auto document = media->document();
			parts.push_back(Tr((document && document->isVoiceMessage())
				? Phrase::ViewOnceListen
				: Phrase::ViewOnceView));
		} else {
			parts.push_back(Tr(Phrase::TimerSeconds).arg(ttl));
		}
	}
	if (IsDeleted(item)) {
		parts.push_back(QString::fromUtf8("\xF0\x9F\x97\x91\xEF\xB8\x8F")); // 🗑️
	}
	return parts.join(u" · "_q);
}

QString EditedMark() {
	return QString::fromUtf8("\xE2\x9C\x8F\xEF\xB8\x8F"); // ✏️
}

} // namespace Soulgram

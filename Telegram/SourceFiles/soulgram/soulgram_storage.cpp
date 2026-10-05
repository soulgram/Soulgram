/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_storage.h"

#include "main/main_session.h"
#include "soulgram/soulgram_settings.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

// Records are stored as JSON lines, one object per line, appended as they
// happen. Big ids are written as strings, because JSON numbers are doubles.
// Each chat has its own file deleted/<peer id>.jsonl, read on its first use.
// Lines without "type" are records, "file" attaches a media copy to a record
// and "forget" removes it. A file with such lines is rewritten when read.

namespace Soulgram {
namespace {

constexpr auto kDeletedFolder = "deleted/";
constexpr auto kLegacyDeletedFile = "deleted.jsonl";
constexpr auto kLinesExtension = ".jsonl";

using PeerRecords = std::map<MsgId, DeletedRecord>;

struct DeletedIndex {
	// Chats that have a file, it is read when the chat is used.
	base::flat_set<PeerId> files;

	// Read chats, std::map keeps references valid while chats are added.
	std::map<PeerId, PeerRecords> loaded;
};

[[nodiscard]] QString IdToString(uint64 value) {
	return QString::number(value);
}

[[nodiscard]] uint64 IdFromValue(const QJsonValue &value) {
	return value.isString()
		? value.toString().toULongLong()
		: uint64(value.toDouble());
}

[[nodiscard]] PeerId PeerFromValue(const QJsonValue &value) {
	return PeerId(PeerIdHelper(IdFromValue(value)));
}

[[nodiscard]] MsgId MsgFromValue(const QJsonValue &value) {
	return MsgId(int64(IdFromValue(value)));
}

[[nodiscard]] QString FilePath(
		not_null<Main::Session*> session,
		const char *name) {
	return AccountFolder(session) + QString::fromLatin1(name);
}

[[nodiscard]] QString PeerPath(
		not_null<Main::Session*> session,
		PeerId peerId) {
	return FilePath(session, kDeletedFolder)
		+ QString::number(peerId.value)
		+ QString::fromLatin1(kLinesExtension);
}

// The line starts on a new line even if the previous write was torn.
bool AppendLine(const QString &path, const QJsonObject &object) {
	QDir().mkpath(QFileInfo(path).absolutePath());
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadWrite)) {
		LOG(("Soulgram Error: could not open '%1' for append.").arg(path));
		return false;
	}
	const auto size = file.size();
	auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
	bytes.append('\n');
	auto last = char();
	if (size > 0
		&& !(file.seek(size - 1) && file.getChar(&last) && last == '\n')) {
		bytes.prepend('\n');
	}
	if (!file.seek(size)
		|| file.write(bytes) != bytes.size()
		|| !file.flush()) {
		LOG(("Soulgram Error: could not append to '%1'.").arg(path));
		file.resize(size);
		return false;
	}
	return true;
}

// Returns the count of non-empty lines, broken ones too, or -1 on error.
// Lines the filter rejects are not parsed.
template <typename Filter, typename Callback>
int ReadLines(const QString &path, Filter &&filter, Callback &&callback) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return -1;
	}
	auto lines = 0;
	auto broken = 0;
	while (!file.atEnd()) {
		const auto line = file.readLine().trimmed();
		if (line.isEmpty()) {
			continue;
		}
		++lines;
		if (!filter(line)) {
			continue;
		}
		const auto document = QJsonDocument::fromJson(line);
		if (document.isObject()) {
			callback(document.object());
		} else {
			++broken;
		}
	}
	if (broken) {
		LOG(("Soulgram Error: skipped %1 broken lines in '%2'."
			).arg(broken
			).arg(path));
	}
	return (file.error() == QFileDevice::NoError) ? lines : -1;
}

template <typename Callback>
int ReadLines(const QString &path, Callback &&callback) {
	return ReadLines(path, [](const QByteArray &) {
		return true;
	}, std::forward<Callback>(callback));
}

// Replaces the whole file, so it is never left half written.
bool WriteFile(const QString &path, const QByteArray &bytes) {
	QDir().mkpath(QFileInfo(path).absolutePath());
	auto file = QSaveFile(path);
	if (!file.open(QIODevice::WriteOnly)
		|| file.write(bytes) != bytes.size()
		|| !file.commit()) {
		LOG(("Soulgram Error: could not write '%1'.").arg(path));
		return false;
	}
	return true;
}

[[nodiscard]] QByteArray BytesFromValue(const QJsonValue &value) {
	return QByteArray::fromBase64(value.toString().toLatin1());
}

[[nodiscard]] QString BytesToString(const QByteArray &bytes) {
	return QString::fromLatin1(bytes.toBase64());
}

[[nodiscard]] DeletedRecord ParseDeleted(const QJsonObject &o) {
	return {
		.peerId = PeerFromValue(o.value(u"peer"_q)),
		.msgId = MsgFromValue(o.value(u"msg"_q)),
		.fromId = PeerFromValue(o.value(u"from"_q)),
		.fromName = o.value(u"from_name"_q).toString(),
		.date = TimeId(o.value(u"date"_q).toDouble()),
		.deletedAt = TimeId(o.value(u"deleted_at"_q).toDouble()),
		.text = o.value(u"text"_q).toString(),
		.media = o.value(u"media"_q).toString(),
		.out = o.value(u"out"_q).toBool(),
		.message = BytesFromValue(o.value(u"message"_q)),
		.layer = o.value(u"layer"_q).toInt(),
		.sender = BytesFromValue(o.value(u"sender"_q)),
		.senderAppVersion = o.value(u"sender_version"_q).toInt(),
		.file = o.value(u"file"_q).toString(),
	};
}

[[nodiscard]] QJsonObject RecordObject(const DeletedRecord &record) {
	auto object = QJsonObject();
	object.insert(u"peer"_q, IdToString(record.peerId.value));
	object.insert(u"msg"_q, IdToString(uint64(record.msgId.bare)));
	object.insert(u"from"_q, IdToString(record.fromId.value));
	object.insert(u"from_name"_q, record.fromName);
	object.insert(u"date"_q, double(record.date));
	object.insert(u"deleted_at"_q, double(record.deletedAt));
	object.insert(u"text"_q, record.text);
	object.insert(u"media"_q, record.media);
	object.insert(u"out"_q, record.out);
	if (!record.message.isEmpty()) {
		object.insert(u"message"_q, BytesToString(record.message));
		object.insert(u"layer"_q, record.layer);
	}
	if (!record.sender.isEmpty()) {
		object.insert(u"sender"_q, BytesToString(record.sender));
		object.insert(u"sender_version"_q, record.senderAppVersion);
	}
	if (!record.file.isEmpty()) {
		object.insert(u"file"_q, record.file);
	}
	return object;
}

[[nodiscard]] QJsonObject KeyObject(
		const QString &type,
		PeerId peerId,
		MsgId msgId) {
	auto object = QJsonObject();
	object.insert(u"type"_q, type);
	object.insert(u"peer"_q, IdToString(peerId.value));
	object.insert(u"msg"_q, IdToString(uint64(msgId.bare)));
	return object;
}

void ApplyLine(PeerRecords &records, const QJsonObject &o) {
	const auto msgId = MsgFromValue(o.value(u"msg"_q));
	if (!msgId) {
		return;
	}
	const auto type = o.value(u"type"_q).toString();
	if (type.isEmpty()) {
		records.emplace(msgId, ParseDeleted(o));
	} else if (type == u"file"_q) {
		const auto i = records.find(msgId);
		if (i != end(records)) {
			i->second.file = o.value(u"file"_q).toString();
		}
	} else if (type == u"forget"_q) {
		records.erase(msgId);
	}
}

// Removes a saved media copy together with its unfinished ".part" file.
void RemoveMediaFile(
		not_null<Main::Session*> session,
		const QString &file) {
	if (!file.isEmpty()) {
		const auto path = AccountFolder(session) + file;
		QFile::remove(path);
		QFile::remove(path + u".part"_q);
	}
}

// Writes only the records, an empty chat has no file.
bool WriteRecords(const QString &path, const PeerRecords &records) {
	if (records.empty()) {
		return QFile::remove(path) || !QFile::exists(path);
	}
	auto bytes = QByteArray();
	for (const auto &entry : records) {
		bytes.append(QJsonDocument(
			RecordObject(entry.second)
		).toJson(QJsonDocument::Compact));
		bytes.append('\n');
	}
	return WriteFile(path, bytes);
}

[[nodiscard]] PeerRecords ReadPeer(const QString &path) {
	auto result = PeerRecords();
	const auto lines = ReadLines(path, [&](const QJsonObject &o) {
		ApplyLine(result, o);
	});
	if (lines >= 0 && (result.empty() || lines != int(result.size()))) {
		WriteRecords(path, result); // Drop forgotten records and the rest.
	}
	return result;
}

[[nodiscard]] PeerRecords *FindRecords(
		not_null<Main::Session*> session,
		not_null<DeletedIndex*> index,
		PeerId peerId) {
	const auto i = index->loaded.find(peerId);
	if (i != end(index->loaded)) {
		return &i->second;
	} else if (!index->files.contains(peerId)) {
		return nullptr;
	}
	auto &result = index->loaded[peerId];
	result = ReadPeer(PeerPath(session, peerId));
	if (result.empty()) {
		index->files.remove(peerId);
	}
	return &result;
}

[[nodiscard]] PeerRecords &EnsureRecords(
		not_null<Main::Session*> session,
		not_null<DeletedIndex*> index,
		PeerId peerId) {
	if (const auto result = FindRecords(session, index, peerId)) {
		return *result;
	}
	return index->loaded[peerId];
}

[[nodiscard]] DeletedRecord *FindRecord(
		not_null<Main::Session*> session,
		not_null<DeletedIndex*> index,
		PeerId peerId,
		MsgId msgId) {
	const auto records = FindRecords(session, index, peerId);
	if (!records) {
		return nullptr;
	}
	const auto i = records->find(msgId);
	return (i != end(*records)) ? &i->second : nullptr;
}

// Older versions kept all the chats in one file.
void MigrateLegacy(
		not_null<Main::Session*> session,
		not_null<DeletedIndex*> index) {
	const auto path = FilePath(session, kLegacyDeletedFile);
	if (!QFile::exists(path)) {
		return;
	}
	auto peers = std::map<PeerId, PeerRecords>();
	auto lines = std::map<PeerId, QByteArray>();
	const auto read = ReadLines(path, [&](const QJsonObject &o) {
		if (const auto peerId = PeerFromValue(o.value(u"peer"_q))) {
			ApplyLine(peers[peerId], o);
			auto &raw = lines[peerId];
			raw.append(QJsonDocument(o).toJson(QJsonDocument::Compact));
			raw.append('\n');
		}
	});
	if (read < 0) {
		return; // Unreadable now, try again next time.
	}
	auto failed = QByteArray();
	for (auto &[peerId, legacy] : peers) {
		if (legacy.empty()) {
			continue;
		}
		auto merged = PeerRecords();
		if (const auto existing = FindRecords(session, index, peerId)) {
			merged = *existing;
		}
		for (auto &entry : legacy) {
			merged.emplace(entry.first, std::move(entry.second));
		}
		if (!WriteRecords(PeerPath(session, peerId), merged)) {
			failed.append(lines[peerId]);
			continue;
		}
		index->loaded[peerId] = std::move(merged);
		index->files.emplace(peerId);
	}
	// WHY: merging the old file again would bring back records that
	// were forgotten since, so it keeps only the chats that failed.
	const auto done = failed.isEmpty()
		? QFile::remove(path)
		: WriteFile(path, failed);
	if (!done) {
		const auto old = path + u".old"_q;
		QFile::remove(old);
		if (!QFile::rename(path, old)) {
			LOG(("Soulgram Error: could not remove '%1'.").arg(path));
		}
	}
}

[[nodiscard]] not_null<DeletedIndex*> Index(
		not_null<Main::Session*> session) {
	static auto indices = base::flat_map<
		not_null<Main::Session*>,
		std::unique_ptr<DeletedIndex>>();
	const auto i = indices.find(session);
	if (i != end(indices)) {
		return i->second.get();
	}
	session->lifetime().add([=] {
		indices.remove(session);
	});
	const auto result = indices.emplace(
		session,
		std::make_unique<DeletedIndex>()).first->second.get();
	const auto extension = QString::fromLatin1(kLinesExtension);
	const auto names = QDir(FilePath(session, kDeletedFolder)).entryList(
		QStringList(u"*"_q + extension),
		QDir::Files);
	for (const auto &name : names) {
		auto ok = false;
		const auto value = name.chopped(extension.size()).toULongLong(&ok);
		if (ok && value) {
			result->files.emplace(PeerId(PeerIdHelper(value)));
		}
	}
	MigrateLegacy(session, result);
	return result;
}

} // namespace

QString AccountFolder(not_null<Main::Session*> session) {
	return DataFolder()
		+ QString::number(session->userId().bare)
		+ '/';
}

void StoreDeleted(
		not_null<Main::Session*> session,
		const DeletedRecord &record) {
	const auto index = Index(session);
	auto &records = EnsureRecords(session, index, record.peerId);
	if (!records.emplace(record.msgId, record).second) {
		return;
	}
	index->files.emplace(record.peerId);
	AppendLine(PeerPath(session, record.peerId), RecordObject(record));
}

void StoreDeletedFile(
		not_null<Main::Session*> session,
		PeerId peerId,
		MsgId msgId,
		const QString &file) {
	if (file.isEmpty()) {
		return;
	}
	const auto record = FindRecord(session, Index(session), peerId, msgId);
	if (!record) {
		// The kept message was forgotten while its media was saved.
		RemoveMediaFile(session, file);
		return;
	} else if (record->file == file) {
		return;
	}
	record->file = file;
	auto object = KeyObject(u"file"_q, peerId, msgId);
	object.insert(u"file"_q, file);
	AppendLine(PeerPath(session, peerId), object);
}

void ForgetDeleted(
		not_null<Main::Session*> session,
		PeerId peerId,
		MsgId msgId) {
	const auto index = Index(session);
	const auto records = FindRecords(session, index, peerId);
	if (!records) {
		return;
	}
	const auto i = records->find(msgId);
	if (i == end(*records)) {
		return;
	}
	RemoveMediaFile(session, i->second.file);
	records->erase(i);
	if (records->empty()) {
		QFile::remove(PeerPath(session, peerId));
		index->files.remove(peerId);
	} else {
		AppendLine(
			PeerPath(session, peerId),
			KeyObject(u"forget"_q, peerId, msgId));
	}
}

void ForgetAllDeleted(not_null<Main::Session*> session, PeerId peerId) {
	const auto index = Index(session);
	if (const auto records = FindRecords(session, index, peerId)) {
		for (const auto &entry : *records) {
			RemoveMediaFile(session, entry.second.file);
		}
		records->clear();
	}
	QFile::remove(PeerPath(session, peerId));
	index->files.remove(peerId);
}

std::vector<DeletedRecord> LoadDeleted(
		not_null<Main::Session*> session,
		PeerId peerId) {
	return DeletedInRange(
		session,
		peerId,
		MsgId(std::numeric_limits<int64>::min()),
		MsgId(std::numeric_limits<int64>::max()));
}

const DeletedRecord *LookupDeleted(
		not_null<Main::Session*> session,
		PeerId peerId,
		MsgId msgId) {
	return FindRecord(session, Index(session), peerId, msgId);
}

std::vector<DeletedRecord> DeletedInRange(
		not_null<Main::Session*> session,
		PeerId peerId,
		MsgId after,
		MsgId before) {
	auto result = std::vector<DeletedRecord>();
	const auto records = FindRecords(session, Index(session), peerId);
	if (!records) {
		return result;
	}
	for (auto j = records->upper_bound(after); j != end(*records); ++j) {
		if (j->first >= before) {
			break;
		}
		result.push_back(j->second);
	}
	return result;
}

bool HasDeleted(not_null<Main::Session*> session, PeerId peerId) {
	const auto records = FindRecords(session, Index(session), peerId);
	return records && !records->empty();
}

} // namespace Soulgram

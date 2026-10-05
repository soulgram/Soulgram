/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Soulgram {

struct DeletedRecord {
	PeerId peerId = 0;
	MsgId msgId = 0;
	PeerId fromId = 0;
	QString fromName;
	TimeId date = 0;
	TimeId deletedAt = 0;
	QString text;
	QString media;
	bool out = false;

	// Serialized MTPMessage and the API layer it was serialized with.
	QByteArray message;
	int layer = 0;

	// Serialized sender, see Serialize::writePeer.
	QByteArray sender;
	int senderAppVersion = 0;

	// Copy of the media, relative to the account folder.
	QString file;
};

// Per-account folder: <working dir>/soulgram/<user id>/
[[nodiscard]] QString AccountFolder(not_null<Main::Session*> session);

void StoreDeleted(
	not_null<Main::Session*> session,
	const DeletedRecord &record);

// Attaches the saved media copy, it is removed if the record is forgotten.
void StoreDeletedFile(
	not_null<Main::Session*> session,
	PeerId peerId,
	MsgId msgId,
	const QString &file);

// The user deleted the kept message, it won't be restored any more.
void ForgetDeleted(
	not_null<Main::Session*> session,
	PeerId peerId,
	MsgId msgId);

// The user cleared the chat, all its kept messages are forgotten.
void ForgetAllDeleted(not_null<Main::Session*> session, PeerId peerId);

[[nodiscard]] std::vector<DeletedRecord> LoadDeleted(
	not_null<Main::Session*> session,
	PeerId peerId);
[[nodiscard]] const DeletedRecord *LookupDeleted(
	not_null<Main::Session*> session,
	PeerId peerId,
	MsgId msgId);

// Records with after < msgId < before, sorted by msgId.
[[nodiscard]] std::vector<DeletedRecord> DeletedInRange(
	not_null<Main::Session*> session,
	PeerId peerId,
	MsgId after,
	MsgId before);
[[nodiscard]] bool HasDeleted(
	not_null<Main::Session*> session,
	PeerId peerId);

} // namespace Soulgram

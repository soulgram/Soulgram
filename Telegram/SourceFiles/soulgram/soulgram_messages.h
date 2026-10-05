/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class History;
class HistoryItem;

namespace Main {
class Session;
} // namespace Main

namespace Soulgram {

// Called with the items the server deleted, right before they would be
// destroyed. Items that Soulgram keeps get the "deleted" mark and are
// removed from the list, so the caller destroys only the rest.
void KeepDeleted(std::vector<not_null<HistoryItem*>> &items);

// The auto-delete timer of the item fired, returns true if it is kept.
[[nodiscard]] bool KeepExpired(not_null<HistoryItem*> item);

// Keeps the server data of loaded messages, so that a deleted message
// can be restored in the chat after a restart.
void RememberMessage(not_null<HistoryItem*> item, const MTPMessage &data);

// The same for a loaded message the server sends again, like an own one.
void RememberExisting(
	not_null<Main::Session*> session,
	const MTPMessage &data);

// The same for an own message the server confirmed only with the id.
void RememberSentMessage(
	not_null<HistoryItem*> item,
	const MTPDupdateShortSentMessage &data);

// Called when the item is destroyed.
void ForgetMessage(not_null<HistoryItem*> item);

// Adds the kept deleted messages that belong between the loaded ones.
// The slice is sorted by id descending, as the server sends it.
[[nodiscard]] std::optional<QVector<MTPMessage>> WithDeleted(
	not_null<History*> history,
	const QVector<MTPMessage> &slice,
	bool older);

// Marks the restored deleted messages and attaches local media copies.
void MarkRestored(const std::vector<not_null<HistoryItem*>> &items);

// The user deletes a kept message: it is forgotten and there is nothing
// to delete on the server. Returns true for such messages.
bool ForgetDeletedByUser(not_null<HistoryItem*> item);

// The user clears or deletes the chat: its kept messages are forgotten.
void ForgetDeletedHistory(not_null<History*> history);

// The user deletes the messages of these dates, the bounds are excluded.
void ForgetDeletedByDates(
	not_null<History*> history,
	TimeId minDate,
	TimeId maxDate);

[[nodiscard]] bool IsDeleted(not_null<const HistoryItem*> item);

// Marks shown before the time: deleted, "view once", "10 s".
[[nodiscard]] QString BottomInfoText(not_null<const HistoryItem*> item);

// Shown instead of the "edited" word before the time.
[[nodiscard]] QString EditedMark();

} // namespace Soulgram

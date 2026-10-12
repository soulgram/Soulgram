/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class History;
class HistoryItem;
class PeerData;
class UserData;

namespace Ui {
class Show;
} // namespace Ui

namespace Soulgram {

// In ghost mode sending a message or a reaction reads the chat on the
// server, as the other side sees the answer anyway.
void ReadOnAction(not_null<History*> history);

enum class SendKind {
	Sticker,
	Gif,
	Voice,
	Round,
};

// Asks before sending, if it is turned on for this kind.
void ConfirmSending(
	std::shared_ptr<Ui::Show> show,
	SendKind kind,
	Fn<void()> send);

[[nodiscard]] bool HideReactions(not_null<PeerData*> peer);

// Time of a message, with seconds if it is turned on.
[[nodiscard]] QString MessageTime(const QTime &time);

// Kept deleted messages may be painted translucent.
[[nodiscard]] float64 MessageOpacity(not_null<const HistoryItem*> item);

// The formatted phone, "Number hidden" for the own one if it is turned on.
[[nodiscard]] QString PhoneText(not_null<UserData*> user);

} // namespace Soulgram

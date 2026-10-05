/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class PeerData;

namespace Dialogs {
class Key;
} // namespace Dialogs

namespace Window {
class SessionController;
} // namespace Window

namespace Soulgram {

// Opens the chat, the topic or the sublist at its very first message.
void JumpToStart(
	not_null<Window::SessionController*> controller,
	Dialogs::Key key);

// "Delete my messages" is offered in groups.
[[nodiscard]] bool CanDeleteMyMessages(not_null<PeerData*> peer);

// "Deleted 10 of 250" while the deletion runs in the chat, else empty.
[[nodiscard]] QString DeletingMyMessagesText(not_null<PeerData*> peer);

// Asks for a confirmation, or shows the progress if it already runs.
void DeleteMyMessages(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer);

// Marks all chats of the account as read, archived and muted ones too,
// after a confirmation. Unread mentions and reactions are read as well.
void ReadAll(not_null<Window::SessionController*> controller);

} // namespace Soulgram

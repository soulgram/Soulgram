/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
class PeerData;

namespace Dialogs {
class Key;
} // namespace Dialogs

namespace Ui {
class GenericBox;
class PopupMenu;
class Show;
} // namespace Ui

namespace Ui::Menu {
struct MenuCallback;
} // namespace Ui::Menu

namespace Window {
class SessionController;
} // namespace Window

namespace Soulgram {

void DeletedMessagesBox(
	not_null<Ui::GenericBox*> box,
	not_null<PeerData*> peer);

// "Soulgram" submenu with all its chat features, in the "three dots" menu
// of a chat or a topic and in the context menu of the chats list.
void FillPeerMenu(
	not_null<Window::SessionController*> controller,
	Dialogs::Key key,
	const Ui::Menu::MenuCallback &addAction);

// Message context menu.
void AddMessageActions(
	not_null<Ui::PopupMenu*> menu,
	HistoryItem *item,
	not_null<Window::SessionController*> controller);

// Bot API style id: 123 for users, -123 for groups, -100123 for channels.
[[nodiscard]] QString PeerIdString(not_null<PeerData*> peer);

// "DC2 · Amsterdam" from the profile photo, empty while it is unknown.
[[nodiscard]] rpl::producer<QString> DataCenterValue(
	not_null<PeerData*> peer);
[[nodiscard]] Fn<void()> CopyTextCallback(
	std::shared_ptr<Ui::Show> show,
	QString text);

} // namespace Soulgram

/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class UserData;

namespace Main {
class Session;
} // namespace Main

namespace Soulgram {

// Local Premium: the client always treats own accounts as Premium ones.
[[nodiscard]] bool LocalPremium(not_null<const UserData*> user);

// Telegram loads the own profile photo together with its video only when
// the profile is opened, animated userpics in chats waited for that.
void PreloadOwnVideoUserpic(not_null<Main::Session*> session);

// Premium as the server sees it, for the checks the server enforces.
[[nodiscard]] bool ServerPremium(not_null<const Main::Session*> session);
[[nodiscard]] rpl::producer<bool> ServerPremiumValue(
	not_null<Main::Session*> session);

} // namespace Soulgram

/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;

namespace Soulgram {

// Warns before opening a link from someone else's message or profile.
// Returns true if the click was taken over: `open` runs after confirming.
[[nodiscard]] bool GuardLink(
	const QString &url,
	const QVariant &context,
	Fn<void()> open);

// True while a confirmed link is being opened, so that Telegram doesn't
// ask for the same link a second time.
[[nodiscard]] bool LinkConfirmed();

void ForgetTrustedSites();

// Fires when the link warning is turned on or off.
[[nodiscard]] rpl::producer<> LinkWarningChanges();

// True if the link warning decides whether this message shows its links.
[[nodiscard]] bool LinksFollowWarning(not_null<const HistoryItem*> item);

} // namespace Soulgram

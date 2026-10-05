/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Soulgram {

struct Settings {
	// Messages.
	bool saveDeletedMessages = true;
	bool translucentDeleted = true;

	// Self-destructing ("view once") media.
	bool keepSelfDestructing = true;
	bool saveSelfDestructingToDisk = true;

	// Ghost mode.
	bool ghostHideRead = false;
	bool ghostHideContentsRead = false;
	bool ghostHideOnline = false;
	bool ghostHideTyping = false;
	bool ghostHideStoriesRead = false;

	// In ghost mode sending a message or a reaction reads the chat.
	bool ghostReadOnAction = true;

	// Links from other people.
	bool linkWarning = true;

	// Sponsored messages in channels, bots and search.
	bool hideAds = true;

	// Copying and saving from chats with protected content.
	bool copyProtected = true;

	// Soulgram windows are excluded from screen capture.
	bool streamerMode = false;

	// Word filters.
	bool wordFilters = true;
	bool wordFiltersInChannels = false;
	QStringList filterWords;

	// Shadow ban: messages of these users and channels are hidden.
	QStringList shadowBan;
	bool hideFromBlocked = false;

	// Confirmation before sending.
	bool confirmStickers = false;
	bool confirmGifs = false;
	bool confirmVoice = false;
	bool confirmRound = false;

	// Interface.
	bool hideReactionsInChannels = false;
	bool hideReactionsInGroups = false;
	bool hideReactionsInPrivate = false;
	bool disableStories = false;
	bool showMessageSeconds = false;
	bool disableGreetingSticker = false;
	bool hidePremiumStatuses = false;
	bool disableNotificationsDelay = false;

	// Message shot, the theme is 0 for the current one or EmbeddedType + 1.
	int shotTheme = 0;
	bool shotShowBackground = true;
	bool shotShowDate = false;
	bool shotShowReactions = false;
	bool shotShowTags = true;
	bool shotColorfulReplies = true;
	bool shotRevealSpoilers = true;
};

[[nodiscard]] const Settings &Current();
void Change(Fn<void(Settings&)> modify);
[[nodiscard]] rpl::producer<> Changes();

[[nodiscard]] rpl::producer<bool> StreamerModeValue();

// Ghost mode is on when all of its options are on.
[[nodiscard]] bool GhostMode();
[[nodiscard]] rpl::producer<bool> GhostModeValue();
void SetGhostMode(bool enabled);

// Folder next to "tdata" where Soulgram keeps its own files.
[[nodiscard]] QString DataFolder();

} // namespace Soulgram

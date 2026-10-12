/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Soulgram {

// Soulgram updates itself from its GitHub releases: the installer of a
// newer version is downloaded in the background and run when asked.
enum class UpdateState {
	Disabled,
	Idle,
	Checking,
	Latest,
	Downloading,
	Ready,
	Failed,
};

[[nodiscard]] rpl::producer<UpdateState> UpdateStateValue();
[[nodiscard]] rpl::producer<bool> UpdateReadyValue();

// The version that is downloaded or ready to be installed.
[[nodiscard]] QString UpdateVersion();

void CheckForUpdates();

// Returns false if this copy wasn't installed by the installer.
bool InstallUpdate();

} // namespace Soulgram

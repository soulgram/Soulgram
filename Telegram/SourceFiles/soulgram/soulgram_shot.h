/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

struct FullMsgId;

namespace Window {
class SessionController;
} // namespace Window

namespace Soulgram {

// Message shot: the selected messages are painted into one image.
// While it is painted the message views skip some of their parts.
enum class ShotPart {
	Date,
	Reactions,
	Tags,
};

[[nodiscard]] bool TakingShot();
[[nodiscard]] bool ShotHides(ShotPart part);
[[nodiscard]] bool ShotRevealsSpoilers();
[[nodiscard]] bool ShotPlainReplies();

// Calls done after the shot was copied or saved.
void ShowMessageShot(
	not_null<Window::SessionController*> controller,
	const std::vector<FullMsgId> &ids,
	Fn<void()> done);

} // namespace Soulgram

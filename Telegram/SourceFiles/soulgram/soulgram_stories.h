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

namespace Data {
class Story;
struct StoriesContext;
} // namespace Data

namespace Window {
class SessionController;
} // namespace Window

namespace Soulgram {

// WHY: the server marks every story up to the newest read one, so after
// an invisible view the author's stories stay invisible until it expires,
// unless the user explicitly opens them normally and accepts the warning.
[[nodiscard]] bool KeepStoryReadLocal(not_null<Data::Story*> story);

// True once after local-only reads of the author: the next normal read
// must reach the server even if the story already looks read locally.
[[nodiscard]] bool ForceStoryReadRequest(not_null<Data::Story*> story);

// Opens the stories viewer after asking whether to watch invisibly,
// unless the setting already says so. The answer lasts for that viewer.
void OpenStory(
	not_null<Window::SessionController*> controller,
	FullStoryId id,
	const Data::StoriesContext &context);

// The stories viewer of the session closed, its answer must not stay.
void StoryViewerClosed(not_null<Main::Session*> session);

} // namespace Soulgram

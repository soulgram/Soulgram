/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;

namespace Main {
class Session;
} // namespace Main

namespace Soulgram {

// Self-destructing media is not cleared when its timer expires.
[[nodiscard]] bool KeepSelfDestructingMedia();

// Called when a message with self-destructing media is created,
// saves a copy of the media to disk if enabled.
void OnSelfDestructingMedia(not_null<HistoryItem*> item);

[[nodiscard]] QString SavedMediaFolder(not_null<Main::Session*> session);

} // namespace Soulgram

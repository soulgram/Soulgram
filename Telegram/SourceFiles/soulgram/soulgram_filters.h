/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class HistoryItem;
class PeerData;

namespace Soulgram {

// Messages in groups (and optionally channels) hidden by the word filters.
// Called very often while painting, the result is cached per item.
[[nodiscard]] bool IsFiltered(not_null<const HistoryItem*> item);

// Drops the cached result, when the item is destroyed.
void ForgetFiltered(not_null<const HistoryItem*> item);

// Shadow ban hides messages of a user or a channel everywhere, except
// the private chat with them, without blocking them.
[[nodiscard]] bool CanShadowBan(not_null<PeerData*> peer);
[[nodiscard]] bool IsShadowBanned(not_null<PeerData*> peer);
void ToggleShadowBan(not_null<PeerData*> peer);

// One word or phrase per line, /pattern/ is a regular expression.
[[nodiscard]] QStringList ParseFilterWords(const QString &text);

} // namespace Soulgram

/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "api/api_common.h"

class HistoryItem;

namespace Ui {
class Show;
} // namespace Ui

namespace Soulgram {

// Telegram doesn't forward messages from chats with protected content,
// kept deleted messages and view-once media, Soulgram sends their copies.
[[nodiscard]] bool CanCopyForward(not_null<const HistoryItem*> item);

// Sends copies of the items to every chat of the actions, in order, if
// some of the items can't be forwarded. The comment goes first. Returns
// false if the items should be forwarded the usual way.
bool CopyForward(
	const std::vector<not_null<HistoryItem*>> &items,
	std::vector<Api::SendAction> actions,
	TextWithTags comment,
	Data::ForwardOptions options,
	std::shared_ptr<Ui::Show> show,
	Fn<void()> done);

} // namespace Soulgram

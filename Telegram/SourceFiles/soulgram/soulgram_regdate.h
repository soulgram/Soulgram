/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class PeerData;
class UserData;

namespace Soulgram {

// Telegram doesn't tell when an account was created. Ids grow with time,
// so the date is estimated from the id and the dates of known accounts.
[[nodiscard]] QString EstimatedRegistration(not_null<UserData*> user);

// The exact date, if the private chat starts with "X joined Telegram".
// Called back only if such a message is found.
void RequestJoinedTelegram(
	not_null<UserData*> user,
	Fn<void(QString)> done);

struct PeerDate {
	QString label;
	QString date;
};

// When a group or a channel was created or joined, known locally.
[[nodiscard]] PeerDate ChatDate(not_null<PeerData*> peer);

} // namespace Soulgram

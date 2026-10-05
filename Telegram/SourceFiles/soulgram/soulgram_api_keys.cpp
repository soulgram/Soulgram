/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_api_keys.h"

#include <QtCore/QtGlobal>

#if !defined TDESKTOP_API_ID || !defined TDESKTOP_API_HASH
// How to obtain your own 'api_id' and 'api_hash' is described here:
// https://core.telegram.org/api/obtaining_api_id
#error You are required to provide API_ID and API_HASH.
#endif // !TDESKTOP_API_ID || !TDESKTOP_API_HASH

extern const int ApiId = TDESKTOP_API_ID;
extern const char ApiHash[] = QT_STRINGIFY(TDESKTOP_API_HASH);

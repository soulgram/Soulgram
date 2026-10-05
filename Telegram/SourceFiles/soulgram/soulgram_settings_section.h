/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "settings/settings_type.h"

namespace Soulgram {

// Soulgram page in Settings, each group of features has its own page.
[[nodiscard]] ::Settings::Type SettingsSectionId();

} // namespace Soulgram

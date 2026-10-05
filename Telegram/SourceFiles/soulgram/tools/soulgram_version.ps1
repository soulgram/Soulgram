# This file is part of Soulgram,
# an unofficial fork of Telegram Desktop.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

# Prints SoulgramVersionStr from core/version.h, like 0.1.0.
$header = Join-Path $PSScriptRoot '..\..\core\version.h'
$match = Select-String -Path $header -Pattern 'SoulgramVersionStr = "([0-9.]+)"'
if (-not $match) {
	exit 1
}
Write-Output $match.Matches[0].Groups[1].Value

# Soulgram

Soulgram is an unofficial Telegram client for Windows based on
[Telegram Desktop](https://github.com/telegramdesktop/tdesktop) and the Telegram API.
It is not affiliated with Telegram. Use it at your own risk.

## Features

- **Ghost mode**: optionally don't send read receipts, "online", "typing...",
  story views and "listened/opened" marks. One switch in the main menu.
- **Deleted messages** stay in the chat with a "deleted" mark and a little dimmed,
  with photos and videos, also after a restart.
- **View-once and timer media** are not destroyed and are saved to disk.
- **Protected chats**: copying, saving and forwarding (as copies) is allowed.
- **Message shot**: selected messages as one image, to the clipboard or a PNG file.
- **No ads** in channels, bots and search.
- **Streamer mode**: Soulgram windows are hidden from screen capture.
- **Word filters and shadow ban** hide spam and chosen users in groups.
- **Chat actions** (the "Soulgram" item of the chat menu): jump to the first
  message, deleted messages list, delete all my messages in a group.
- **Read all**: marks every chat of the account as read.
- **Profile**: id, data center and an estimated registration date.
- Interface options: seconds in message time, confirmations before sending
  stickers / GIFs / voice / video messages, hiding reactions, stories and
  premium statuses.

All options are in Settings → Soulgram.

Limitations: only messages the client received while it was running can be kept.
Telegram Desktop has no secret chats.

## Where the data is

Everything is stored next to the program, in the `soulgram` folder:

- `soulgram/settings.json`: settings
- `soulgram/<account id>/deleted/`: deleted messages, `deleted_media/`: their files
- `soulgram/<account id>/view_once/`: saved view-once media

## Source layout

- `Telegram/SourceFiles/soulgram/`: all Soulgram code (namespace `Soulgram`)
- `Telegram/cmake/soulgram.cmake`: the list of Soulgram sources
- Changes in upstream files are marked with `// Soulgram`, to keep merges simple.
- `soulgram/tools/make_icons.ps1`: generates the application icons.
- `Telegram/SourceFiles/soulgram/tools/`: the installer script and its images.

## Building on Windows

Follow the upstream [build instructions](../docs/building-win.md) with MSVC 14.44,
then build the `Telegram` target; the result is `Soulgram.exe`. Pass your own
`TDESKTOP_API_ID` and `TDESKTOP_API_HASH` to `configure.bat`.
The installer is built by Inno Setup from
`Telegram/SourceFiles/soulgram/tools/soulgram_setup.iss`.

## License

GPLv3 with the OpenSSL exception, same as Telegram Desktop. See [LEGAL](../LEGAL).

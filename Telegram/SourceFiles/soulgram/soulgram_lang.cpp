/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_lang.h"

#include "lang/lang_instance.h"

namespace Soulgram {
namespace {

struct Entry {
	Phrase phrase;
	const char *en = nullptr;
	const char *ru = nullptr;
};

const Entry kEntries[] = {
	{
		Phrase::SettingsButton,
		"Soulgram",
		"Soulgram",
	},
	{
		Phrase::SettingsTitle,
		"Soulgram",
		"Soulgram",
	},
	{
		Phrase::AboutText,
		"Unofficial Telegram client based on\n"
		"Telegram Desktop and the Telegram API.",
		"Неофициальный клиент Telegram на основе\n"
		"Telegram Desktop и Telegram API.",
	},
	{
		Phrase::SectionMessages,
		"Deleted messages",
		"Удалённые сообщения",
	},
	{
		Phrase::SaveDeleted,
		"Keep deleted messages",
		"Сохранять удалённые сообщения",
	},
	{
		Phrase::MessagesAbout,
		"Deleted messages stay in the chat with a \"deleted\" mark, "
		"also after a restart, together with photos and videos: Soulgram "
		"keeps a copy of them (files up to 300 MB). Only messages received "
		"while Soulgram was running can be kept. The full list is in the "
		"chat menu, \"Deleted messages\".",
		"Удалённые сообщения остаются в чате с пометкой «удалено» — "
		"и после перезапуска, вместе с фото и видео: Soulgram сохраняет "
		"их копию (файлы до 300 МБ). Сохраняется только то, что Soulgram "
		"успел получить, пока был запущен. Полный список — в меню чата, "
		"«Удалённые сообщения».",
	},
	{
		Phrase::SectionViewOnce,
		"View-once media",
		"Одноразовые медиа",
	},
	{
		Phrase::KeepViewOnce,
		"Don't destroy view-once and timer media",
		"Не удалять одноразовые и медиа с таймером",
	},
	{
		Phrase::SaveViewOnceToDisk,
		"Save a copy to disk on arrival",
		"Сохранять копию на диск при получении",
	},
	{
		Phrase::OpenSavedFolder,
		"Open folder with saved files",
		"Открыть папку с сохранёнными файлами",
	},
	{
		Phrase::ViewOnceAbout,
		"Photos, videos, voice and video messages are saved to the "
		"\"soulgram\" folder next to the program right after they arrive, "
		"even if you never open them here.",
		"Фото, видео, голосовые и кружки сохраняются в папку «soulgram» "
		"рядом с программой сразу при получении, даже если вы их здесь "
		"не открываете.",
	},
	{
		Phrase::SectionGhost,
		"Ghost mode",
		"Режим призрака",
	},
	{
		Phrase::GhostRead,
		"Don't send read receipts",
		"Не отправлять отметку о прочтении",
	},
	{
		Phrase::GhostContentsRead,
		"Don't mark voice, video messages and view-once media as opened",
		"Не отмечать голосовые, кружки и одноразовые как просмотренные",
	},
	{
		Phrase::GhostOnline,
		"Don't show \"online\"",
		"Не показывать «в сети»",
	},
	{
		Phrase::GhostTyping,
		"Don't show \"typing...\"",
		"Не показывать «печатает…»",
	},
	{
		Phrase::GhostStories,
		"Don't mark stories as viewed",
		"Не отмечать истории как просмотренные",
	},
	{
		Phrase::GhostAbout,
		"Sending messages or reactions still shows you online: "
		"that is decided by Telegram servers.",
		"Отправка сообщений и реакций всё равно показывает вас в сети — "
		"это решают серверы Telegram.",
	},
	{
		Phrase::DeletedMessages,
		"Deleted messages",
		"Удалённые сообщения",
	},
	{
		Phrase::DeletedMessagesEmpty,
		"No deleted messages were saved in this chat yet.",
		"В этом чате пока нет сохранённых удалённых сообщений.",
	},
	{
		Phrase::DeletedAt,
		"deleted",
		"удалено",
	},
	{
		Phrase::MediaMessage,
		"Media",
		"Медиа",
	},
	{
		Phrase::ViewOnceView,
		"view once",
		"1 просмотр",
	},
	{
		Phrase::ViewOnceListen,
		"listen once",
		"1 прослушивание",
	},
	{
		Phrase::TimerSeconds,
		"%1 s",
		"%1 сек",
	},
	{
		Phrase::IdCopied,
		"ID copied to clipboard.",
		"ID скопирован.",
	},
	{
		Phrase::CopyId,
		"Copy ID",
		"Копировать ID",
	},
	{
		Phrase::StoriesGhostText,
		"Watch the story invisibly? The author won't see that you watched it.",
		"Посмотреть историю незаметно? Автор не увидит, что вы её смотрели.",
	},
	{
		Phrase::StoriesGhostHint,
		"To stop asking, turn on \"Don't mark stories as viewed\" "
		"in Soulgram settings.",
		"Чтобы не спрашивать каждый раз, включите «Не отмечать истории "
		"как просмотренные» в настройках Soulgram.",
	},
	{
		Phrase::StoriesGhostYes,
		"Invisibly",
		"Незаметно",
	},
	{
		Phrase::StoriesGhostNo,
		"Normally",
		"Обычно",
	},
	{
		Phrase::StoriesGhostWarning,
		"You already watched this person's stories invisibly. If you open "
		"them normally, they will see those views too.",
		"Вы уже смотрели истории этого человека незаметно. Если открыть "
		"обычно, он увидит и те просмотры.",
	},
	{
		Phrase::SectionLinks,
		"Link protection",
		"Защита от ссылок",
	},
	{
		Phrase::LinkWarning,
		"Warn before opening links",
		"Предупреждать перед открытием ссылок",
	},
	{
		Phrase::ForgetTrustedSites,
		"Forget trusted sites",
		"Забыть доверенные сайты",
	},
	{
		Phrase::TrustedSitesForgotten,
		"Soulgram will ask again for every site.",
		"Soulgram снова будет спрашивать для всех сайтов.",
	},
	{
		Phrase::LinksAbout,
		"Before opening a link from someone else's message or profile, "
		"Soulgram shows where it leads and reminds you to be careful. "
		"Links from strangers, which Telegram shows as plain text, "
		"become clickable too.",
		"Перед открытием ссылки из чужого сообщения или профиля Soulgram "
		"покажет, куда она ведёт, и напомнит об осторожности. Ссылки "
		"от незнакомцев, которые Telegram показывает обычным текстом, "
		"тоже станут кликабельными.",
	},
	{
		Phrase::LinkWarningTitle,
		"Open this link?",
		"Открыть ссылку?",
	},
	{
		Phrase::LinkWarningExternal,
		"Be careful: scammers copy the sites of Telegram, banks and shops. "
		"Never enter your Telegram login code, passwords or card details "
		"on sites you don't know, and don't download programs from them.",
		"Будьте осторожны: мошенники подделывают сайты Telegram, банков "
		"и магазинов. Не вводите на незнакомых сайтах код входа в Telegram, "
		"пароли и данные карты и не скачивайте с них программы.",
	},
	{
		Phrase::LinkWarningTelegram,
		"The link opens in Telegram: a chat, channel, bot or mini app. "
		"Never tell anyone your login code and don't confirm logins "
		"that you didn't start.",
		"Ссылка откроется в Telegram: это может быть чат, канал, бот или "
		"мини-приложение. Никому не сообщайте код входа и не подтверждайте "
		"вход, который начали не вы.",
	},
	{
		Phrase::LinkWarningSuspicious,
		"The address contains unusual characters: this is how well-known "
		"sites are faked.",
		"В адресе есть необычные символы — так подделывают известные сайты.",
	},
	{
		Phrase::LinkTrustSite,
		"Don't ask again for %1",
		"Больше не спрашивать для %1",
	},
	{
		Phrase::LinkWarningHint,
		"You can turn this warning off in Soulgram settings.",
		"Отключить это предупреждение можно в настройках Soulgram.",
	},
	{
		Phrase::SectionAds,
		"Ads and protected content",
		"Реклама и защищённый контент",
	},
	{
		Phrase::HideAds,
		"Hide sponsored messages",
		"Скрывать рекламу",
	},
	{
		Phrase::CopyProtected,
		"Allow copying from protected chats",
		"Снять запрет копирования",
	},
	{
		Phrase::AdsAbout,
		"Sponsored messages in channels, bots and search are not loaded. "
		"In chats and channels that forbid copying you can copy text, "
		"save photos, videos and files and take screenshots of media. "
		"Forwarding stays forbidden: Telegram servers don't allow it.",
		"Рекламные сообщения в каналах, ботах и поиске не загружаются. "
		"В чатах и каналах с запретом копирования можно копировать текст, "
		"сохранять фото, видео и файлы и делать скриншоты медиа. "
		"Пересылка остаётся запрещённой — её не пропускают серверы "
		"Telegram.",
	},
	{
		Phrase::SectionStreamer,
		"Streamer mode",
		"Режим стримера",
	},
	{
		Phrase::StreamerMode,
		"Hide Soulgram from screen capture",
		"Скрывать Soulgram от захвата экрана",
	},
	{
		Phrase::StreamerAbout,
		"Soulgram windows are not visible in OBS, screen recordings and "
		"screenshots, while you see them as usual. Native Windows "
		"notifications are not hidden: turn them off in the notification "
		"settings if needed.",
		"Окна Soulgram не видны в OBS, на записи экрана и скриншотах, "
		"а вы видите их как обычно. Системные уведомления Windows "
		"не скрываются — при необходимости отключите их в настройках "
		"уведомлений.",
	},
	{
		Phrase::SectionFilters,
		"Filters and shadow ban",
		"Фильтры и теневой бан",
	},
	{
		Phrase::WordFilters,
		"Hide messages with these words",
		"Скрывать сообщения с этими словами",
	},
	{
		Phrase::WordFiltersInChannels,
		"Also in channels",
		"Также в каналах",
	},
	{
		Phrase::FilterWordsButton,
		"Words and phrases: %1",
		"Слова и фразы: %1",
	},
	{
		Phrase::FiltersAbout,
		"Messages in groups that contain any of the words are hidden "
		"together with their notifications. Your own messages are never "
		"hidden.",
		"Сообщения в группах, где встречается любое из слов, скрываются "
		"вместе с уведомлениями. Ваши сообщения не скрываются.",
	},
	{
		Phrase::FilterWordsTitle,
		"Word filters",
		"Фильтры по словам",
	},
	{
		Phrase::FilterWordsPlaceholder,
		"One word or phrase per line",
		"Одно слово или фраза на строку",
	},
	{
		Phrase::FilterWordsAbout,
		"Case doesn't matter. A line like /pattern/ is a regular "
		"expression, for example /t\\.me\\/\\w+/ hides links to "
		"channels.",
		"Регистр не важен. Строка вида /шаблон/ — регулярное выражение, "
		"например /t\\.me\\/\\w+/ скрывает ссылки на каналы.",
	},
	{
		Phrase::DownloadPathDefault,
		"\"downloads\" folder next to the program",
		"Папка «downloads» рядом с программой",
	},
	{
		Phrase::DataCenter,
		"Data center",
		"Дата-центр",
	},
	{
		Phrase::DcMiami,
		"Miami, USA",
		"Майами, США",
	},
	{
		Phrase::DcAmsterdam,
		"Amsterdam, Netherlands",
		"Амстердам, Нидерланды",
	},
	{
		Phrase::DcSingapore,
		"Singapore",
		"Сингапур",
	},
	{
		Phrase::JumpToStart,
		"To the beginning",
		"В начало",
	},
	{
		Phrase::DeleteMyMessages,
		"Delete my messages",
		"Удалить мои сообщения",
	},
	{
		Phrase::DeleteMyTitle,
		"Delete my messages",
		"Удалить мои сообщения",
	},
	{
		Phrase::DeleteMyText,
		"Soulgram will find all your messages in «%1» and delete them "
		"for everyone. This can't be undone.",
		"Soulgram найдёт все ваши сообщения в «%1» и удалит их у всех "
		"участников. Отменить это нельзя.",
	},
	{
		Phrase::DeleteMySearching,
		"Looking for your messages…",
		"Ищу ваши сообщения…",
	},
	{
		Phrase::DeleteMyProgress,
		"Deleted %1 of %2",
		"Удалено %1 из %2",
	},
	{
		Phrase::DeleteMyStopping,
		"Stopping… Deleted: %1",
		"Останавливаю… Удалено: %1",
	},
	{
		Phrase::DeleteMyBackground,
		"You can close this window, the deletion goes on in the "
		"background. With many messages Telegram may slow it down.",
		"Окно можно закрыть — удаление продолжится в фоне. Если "
		"сообщений много, Telegram может его притормаживать.",
	},
	{
		Phrase::DeleteMyStop,
		"Stop",
		"Остановить",
	},
	{
		Phrase::DeleteMyHide,
		"Hide",
		"Скрыть",
	},
	{
		Phrase::DeleteMyRunning,
		"Deleting my messages: %1 of %2",
		"Удаление моих сообщений: %1 из %2",
	},
	{
		Phrase::DeleteMyDone,
		"Deleted your messages: %1",
		"Удалено ваших сообщений: %1",
	},
	{
		Phrase::DeleteMyNothing,
		"You have no messages here.",
		"Ваших сообщений здесь нет.",
	},
	{
		Phrase::DeleteMyStopped,
		"Stopped. Deleted messages: %1",
		"Удаление остановлено. Удалено сообщений: %1",
	},
	{
		Phrase::DeleteMyFailed,
		"Could not delete the messages: %1",
		"Не удалось удалить сообщения: %1",
	},
	{
		Phrase::ReadAll,
		"Read all",
		"Прочитать все",
	},
	{
		Phrase::ReadAllText,
		"Mark all messages in all chats as read, including the archive "
		"and muted chats? This can't be undone.",
		"Отметить прочитанными все сообщения во всех чатах, включая "
		"архив и чаты без звука? Отменить это нельзя.",
	},
	{
		Phrase::ReadAllGhost,
		"Ghost mode is on: the chats are read only here, read receipts "
		"are not sent.",
		"Включён режим призрака: чаты прочитаются только у вас, отметки "
		"о прочтении не отправятся.",
	},
	{
		Phrase::ReadAllConfirm,
		"Read",
		"Прочитать",
	},
	{
		Phrase::ReadAllLoading,
		"Loading the list of chats…",
		"Загружаю список чатов…",
	},
	{
		Phrase::ReadAllDone,
		"All chats are read.",
		"Все чаты прочитаны.",
	},
	{
		Phrase::ReadAllRunning,
		"Chats are being read already…",
		"Чаты уже читаются…",
	},
	{
		Phrase::CopyForwardPreparing,
		"Copying the messages…",
		"Копирую сообщения…",
	},
	{
		Phrase::CopyForwardDone,
		"Sent as copies: %1",
		"Отправлено копиями: %1",
	},
	{
		Phrase::CopyForwardFailed,
		"Some files were not copied, they did not download: %1",
		"Часть файлов не скопировалась — они не скачались: %1",
	},
	{
		Phrase::LocalPremiumTitle,
		"Local Telegram Premium",
		"Локальный Telegram Premium",
	},
	{
		Phrase::LocalPremiumAbout,
		"Premium is on only in Soulgram on this device. What Soulgram "
		"does by itself works. What Telegram servers check, like bigger "
		"limits, voice to text and faster downloads, needs a real "
		"subscription.",
		"Premium включён только в Soulgram на этом устройстве. Работает "
		"то, что Soulgram делает сам. То, что проверяют серверы "
		"Telegram, — повышенные лимиты, расшифровка голосовых, быстрая "
		"загрузка — требует настоящей подписки.",
	},
	{
		Phrase::ConfirmStickerText,
		"Send this sticker?",
		"Отправить этот стикер?",
	},
	{
		Phrase::ConfirmGifText,
		"Send this GIF?",
		"Отправить эту GIF?",
	},
	{
		Phrase::ConfirmVoiceText,
		"Send this voice message?",
		"Отправить это голосовое сообщение?",
	},
	{
		Phrase::ConfirmRoundText,
		"Send this video message?",
		"Отправить этот кружок?",
	},
	{
		Phrase::GhostReadOnAction,
		"Read the chat when I reply",
		"Читать чат, когда я отвечаю",
	},
	{
		Phrase::GhostModeAbout,
		"The switch in the main menu turns all of these on or off at "
		"once. With \"Read the chat when I reply\" sending a message or a "
		"reaction marks the chat as read: the other side sees your "
		"answer anyway.",
		"Переключатель в главном меню включает и выключает всё это "
		"сразу. С «Читать чат, когда я отвечаю» отправка сообщения или "
		"реакции отмечает чат прочитанным — собеседник всё равно видит "
		"ваш ответ.",
	},
	{
		Phrase::TranslucentDeleted,
		"Dim deleted messages",
		"Затемнять удалённые сообщения",
	},
	{
		Phrase::ShadowBan,
		"Shadow ban",
		"Теневой бан",
	},
	{
		Phrase::ShadowBanRemove,
		"Remove shadow ban",
		"Снять теневой бан",
	},
	{
		Phrase::ShadowBanAuthor,
		"Shadow ban: %1",
		"Теневой бан: %1",
	},
	{
		Phrase::ShadowBanAuthorRemove,
		"Remove shadow ban: %1",
		"Снять теневой бан: %1",
	},
	{
		Phrase::ShadowBanList,
		"Shadow banned: %1",
		"В теневом бане: %1",
	},
	{
		Phrase::ShadowBanEmpty,
		"Nobody is shadow banned.",
		"В теневом бане никого нет.",
	},
	{
		Phrase::ShadowBanAdded,
		"%1 is shadow banned.",
		"%1 — в теневом бане.",
	},
	{
		Phrase::ShadowBanRemoved,
		"%1 is out of the shadow ban.",
		"%1 — больше не в теневом бане.",
	},
	{
		Phrase::HideFromBlocked,
		"Hide messages of blocked users",
		"Скрывать сообщения заблокированных",
	},
	{
		Phrase::ShadowBanAbout,
		"Messages of shadow banned users and channels are hidden in "
		"groups and comments, forwards from them too. Unlike a block, "
		"they don't know about it, and the private chat with them "
		"stays. Shadow ban is in the Soulgram menu of a chat and of a "
		"message.",
		"Сообщения пользователей и каналов из теневого бана скрываются "
		"в группах и комментариях, пересланные от них — тоже. В отличие "
		"от блокировки, они об этом не узнают, а личный чат с ними "
		"остаётся. Теневой бан — в меню Soulgram у чата и у сообщения.",
	},
	{
		Phrase::SectionSending,
		"Sending",
		"Отправка",
	},
	{
		Phrase::ConfirmStickers,
		"Ask before sending stickers",
		"Спрашивать перед отправкой стикеров",
	},
	{
		Phrase::ConfirmGifs,
		"Ask before sending GIFs",
		"Спрашивать перед отправкой GIF",
	},
	{
		Phrase::ConfirmVoice,
		"Ask before sending voice messages",
		"Спрашивать перед отправкой голосовых",
	},
	{
		Phrase::ConfirmRound,
		"Ask before sending video messages",
		"Спрашивать перед отправкой кружков",
	},
	{
		Phrase::SendingAbout,
		"Protects from sending something by a misclick.",
		"Защищает от случайной отправки по ошибочному клику.",
	},
	{
		Phrase::SectionInterface,
		"Interface",
		"Интерфейс",
	},
	{
		Phrase::HideReactionsChannels,
		"Hide reactions in channels",
		"Скрывать реакции в каналах",
	},
	{
		Phrase::HideReactionsGroups,
		"Hide reactions in groups",
		"Скрывать реакции в группах",
	},
	{
		Phrase::HideReactionsPrivate,
		"Hide reactions in private chats",
		"Скрывать реакции в личных чатах",
	},
	{
		Phrase::DisableStories,
		"Hide stories",
		"Скрывать истории",
	},
	{
		Phrase::ShowMessageSeconds,
		"Show seconds in the time of messages",
		"Показывать секунды во времени сообщений",
	},
	{
		Phrase::DisableGreetingSticker,
		"No greeting sticker in empty chats",
		"Без приветственного стикера в пустых чатах",
	},
	{
		Phrase::HidePremiumStatuses,
		"Hide premium statuses",
		"Скрывать премиум-статусы",
	},
	{
		Phrase::DisableNotificationsDelay,
		"Notifications without delay",
		"Уведомления без задержки",
	},
	{
		Phrase::InterfaceAbout,
		"Some of these apply to newly shown messages and chats; restart "
		"Soulgram to apply them everywhere. Without the delay "
		"notifications come at once, even while you are active on "
		"another device.",
		"Часть настроек применяется к заново показанным сообщениям и "
		"чатам — перезапустите Soulgram, чтобы применить везде. Без "
		"задержки уведомления приходят сразу, даже если вы активны на "
		"другом устройстве.",
	},
	{
		Phrase::MessageDetails,
		"Message details",
		"Подробнее о сообщении",
	},
	{
		Phrase::DetailsMessageId,
		"Message ID",
		"ID сообщения",
	},
	{
		Phrase::DetailsChat,
		"Chat",
		"Чат",
	},
	{
		Phrase::DetailsSender,
		"Sender",
		"Отправитель",
	},
	{
		Phrase::DetailsDate,
		"Date",
		"Дата",
	},
	{
		Phrase::DetailsEdited,
		"Edited",
		"Изменено",
	},
	{
		Phrase::DetailsForwardedFrom,
		"Forwarded from",
		"Переслано от",
	},
	{
		Phrase::DetailsForwardedDate,
		"Original date",
		"Исходная дата",
	},
	{
		Phrase::DetailsViews,
		"Views",
		"Просмотры",
	},
	{
		Phrase::DetailsForwards,
		"Shares",
		"Пересылки",
	},
	{
		Phrase::DetailsFile,
		"File",
		"Файл",
	},
	{
		Phrase::DetailsSize,
		"Size",
		"Размер",
	},
	{
		Phrase::DetailsType,
		"Type",
		"Тип",
	},
	{
		Phrase::DetailsResolution,
		"Resolution",
		"Разрешение",
	},
	{
		Phrase::DetailsDuration,
		"Duration",
		"Длительность",
	},
	{
		Phrase::DetailsPath,
		"On disk",
		"На диске",
	},
	{
		Phrase::DetailsDeleted,
		"Deleted",
		"Удалено",
	},
	{
		Phrase::ClearDeleted,
		"Clear deleted messages",
		"Очистить удалённые",
	},
	{
		Phrase::ClearDeletedText,
		"Forget all kept deleted messages of this chat together with "
		"their saved files?",
		"Забыть все сохранённые удалённые сообщения этого чата вместе с "
		"их файлами?",
	},
	{
		Phrase::ClearDeletedConfirm,
		"Clear",
		"Очистить",
	},
	{
		Phrase::Shot,
		"Shot",
		"Снимок",
	},
	{
		Phrase::ShotPreview,
		"Preview",
		"Превью",
	},
	{
		Phrase::ShotSettings,
		"Settings",
		"Настройки",
	},
	{
		Phrase::ShotTheme,
		"Theme",
		"Тема",
	},
	{
		Phrase::ShotThemeDefault,
		"Default",
		"По умолчанию",
	},
	{
		Phrase::ShotShowBackground,
		"Show background",
		"Показывать фон",
	},
	{
		Phrase::ShotShowDate,
		"Show date",
		"Показывать дату",
	},
	{
		Phrase::ShotShowReactions,
		"Show reactions",
		"Показывать реакции",
	},
	{
		Phrase::ShotShowTags,
		"Show tags",
		"Показывать теги",
	},
	{
		Phrase::ShotColorfulReplies,
		"Show colorful replies",
		"Показывать цветные ответы",
	},
	{
		Phrase::ShotRevealSpoilers,
		"Reveal spoilers",
		"Раскрыть спойлеры",
	},
	{
		Phrase::ShotCopy,
		"Copy",
		"В буфер",
	},
	{
		Phrase::ShotSave,
		"Save",
		"Сохранить",
	},
	{
		Phrase::ShotCopied,
		"The shot is copied to the clipboard.",
		"Снимок скопирован в буфер обмена.",
	},
	{
		Phrase::ShotSaved,
		"The shot is saved.",
		"Снимок сохранён.",
	},
	{
		Phrase::RegistrationDate,
		"Registration date",
		"Дата регистрации",
	},
	{
		Phrase::RegistrationApproximately,
		"\u2248 %1",
		"\u2248 %1",
	},
	{
		Phrase::RegistrationLater,
		"later than %1",
		"позже %1",
	},
	{
		Phrase::RegistrationExact,
		"%1 (exact)",
		"%1 (точно)",
	},
	{
		Phrase::CreatedDate,
		"Created",
		"Создан",
	},
	{
		Phrase::JoinedDate,
		"You joined",
		"Вы вступили",
	},
	{
		Phrase::SoulgramChannel,
		"Telegram channel",
		"Telegram-канал",
	},
	{
		Phrase::SoulgramGithub,
		"GitHub",
		"GitHub",
	},
	{
		Phrase::UpdateSoulgram,
		"Update Soulgram",
		"Обновить Soulgram",
	},
	{
		Phrase::AutoUpdate,
		"Update automatically",
		"Обновлять автоматически",
	},
	{
		Phrase::CheckUpdates,
		"Check for updates",
		"Проверить обновления",
	},
	{
		Phrase::UpdateChecking,
		"Checking...",
		"Проверяю...",
	},
	{
		Phrase::UpdateLatest,
		"Latest version",
		"Последняя версия",
	},
	{
		Phrase::UpdateDownloading,
		"Downloading %1...",
		"Скачиваю %1...",
	},
	{
		Phrase::UpdateReady,
		"Version %1 is ready",
		"Версия %1 готова",
	},
	{
		Phrase::UpdateFailed,
		"Could not check",
		"Не удалось проверить",
	},
	{
		Phrase::UpdateInstalledOnly,
		"Updates work in the installed Soulgram.",
		"Обновления работают в установленном Soulgram.",
	},
};

[[nodiscard]] bool UseRussian() {
	const auto &instance = Lang::GetInstance();
	const auto russian = [](const QString &id) {
		return id.startsWith(u"ru"_q, Qt::CaseInsensitive);
	};
	return russian(instance.id()) || russian(instance.baseId());
}

} // namespace

QString Tr(Phrase phrase) {
	const auto ru = UseRussian();
	for (const auto &entry : kEntries) {
		if (entry.phrase == phrase) {
			return QString::fromUtf8(ru ? entry.ru : entry.en);
		}
	}
	return QString();
}

rpl::producer<QString> TrValue(Phrase phrase) {
	return rpl::single(
		Tr(phrase)
	) | rpl::then(Lang::GetInstance().updated(
	) | rpl::map([=] {
		return Tr(phrase);
	}));
}

} // namespace Soulgram

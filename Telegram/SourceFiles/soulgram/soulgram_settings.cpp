/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_settings.h"

#include "settings.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

namespace Soulgram {
namespace {

struct Field {
	const char *key = nullptr;
	bool Settings::*member = nullptr;
};

const Field kFields[] = {
	{ "save_deleted_messages", &Settings::saveDeletedMessages },
	{ "keep_self_destructing", &Settings::keepSelfDestructing },
	{ "save_self_destructing_to_disk", &Settings::saveSelfDestructingToDisk },
	{ "ghost_hide_read", &Settings::ghostHideRead },
	{ "ghost_hide_contents_read", &Settings::ghostHideContentsRead },
	{ "ghost_hide_online", &Settings::ghostHideOnline },
	{ "ghost_hide_typing", &Settings::ghostHideTyping },
	{ "ghost_hide_stories_read", &Settings::ghostHideStoriesRead },
	{ "link_warning", &Settings::linkWarning },
	{ "hide_ads", &Settings::hideAds },
	{ "copy_protected", &Settings::copyProtected },
	{ "streamer_mode", &Settings::streamerMode },
	{ "word_filters", &Settings::wordFilters },
	{ "word_filters_in_channels", &Settings::wordFiltersInChannels },
	{ "dim_deleted_messages", &Settings::translucentDeleted },
	{ "ghost_read_on_action", &Settings::ghostReadOnAction },
	{ "hide_from_blocked", &Settings::hideFromBlocked },
	{ "confirm_stickers", &Settings::confirmStickers },
	{ "confirm_gifs", &Settings::confirmGifs },
	{ "confirm_voice", &Settings::confirmVoice },
	{ "confirm_round", &Settings::confirmRound },
	{ "hide_reactions_in_channels", &Settings::hideReactionsInChannels },
	{ "hide_reactions_in_groups", &Settings::hideReactionsInGroups },
	{ "hide_reactions_in_private", &Settings::hideReactionsInPrivate },
	{ "disable_stories", &Settings::disableStories },
	{ "show_message_seconds", &Settings::showMessageSeconds },
	{ "disable_greeting_sticker", &Settings::disableGreetingSticker },
	{ "hide_premium_statuses", &Settings::hidePremiumStatuses },
	{ "disable_notifications_delay", &Settings::disableNotificationsDelay },
	{ "shot_show_background", &Settings::shotShowBackground },
	{ "shot_show_date", &Settings::shotShowDate },
	{ "shot_show_reactions", &Settings::shotShowReactions },
	{ "shot_show_tags", &Settings::shotShowTags },
	{ "shot_colorful_replies", &Settings::shotColorfulReplies },
	{ "shot_reveal_spoilers", &Settings::shotRevealSpoilers },
	{ "auto_update", &Settings::autoUpdate },
};

struct IntField {
	const char *key = nullptr;
	int Settings::*member = nullptr;
};

const IntField kIntFields[] = {
	{ "shot_theme", &Settings::shotTheme },
};

struct ListField {
	const char *key = nullptr;
	QStringList Settings::*member = nullptr;
};

const ListField kListFields[] = {
	{ "filter_words", &Settings::filterWords },
	{ "shadow_ban", &Settings::shadowBan },
};

struct State {
	Settings settings;
	bool loaded = false;
	rpl::event_stream<> changes;
};

[[nodiscard]] State &Instance() {
	static auto result = State();
	return result;
}

[[nodiscard]] QString SettingsPath() {
	return DataFolder() + u"settings.json"_q;
}

void Load(State &state) {
	state.loaded = true;

	auto file = QFile(SettingsPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		LOG(("Soulgram Error: could not parse settings.json: %1"
			).arg(error.errorString()));
		return;
	}
	const auto object = document.object();
	for (const auto &field : kFields) {
		const auto value = object.value(QString::fromLatin1(field.key));
		if (value.isBool()) {
			state.settings.*(field.member) = value.toBool();
		}
	}
	for (const auto &field : kIntFields) {
		const auto value = object.value(QString::fromLatin1(field.key));
		if (value.isDouble()) {
			state.settings.*(field.member) = value.toInt();
		}
	}
	for (const auto &field : kListFields) {
		const auto list = object.value(QString::fromLatin1(field.key));
		for (const auto &value : list.toArray()) {
			if (value.isString()) {
				(state.settings.*(field.member)).push_back(value.toString());
			}
		}
	}
}

void Save(const State &state) {
	auto object = QJsonObject();
	for (const auto &field : kFields) {
		object.insert(
			QString::fromLatin1(field.key),
			state.settings.*(field.member));
	}
	for (const auto &field : kIntFields) {
		object.insert(
			QString::fromLatin1(field.key),
			state.settings.*(field.member));
	}
	for (const auto &field : kListFields) {
		object.insert(
			QString::fromLatin1(field.key),
			QJsonArray::fromStringList(state.settings.*(field.member)));
	}
	QDir().mkpath(DataFolder());
	auto file = QSaveFile(SettingsPath());
	if (!file.open(QIODevice::WriteOnly)) {
		LOG(("Soulgram Error: could not open settings.json for writing."));
		return;
	}
	file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
	if (!file.commit()) {
		LOG(("Soulgram Error: could not save settings.json."));
	}
}

} // namespace

const Settings &Current() {
	auto &state = Instance();
	if (!state.loaded) {
		Load(state);
	}
	return state.settings;
}

void Change(Fn<void(Settings&)> modify) {
	auto &state = Instance();
	if (!state.loaded) {
		Load(state);
	}
	modify(state.settings);
	Save(state);
	state.changes.fire({});
}

rpl::producer<> Changes() {
	return Instance().changes.events();
}

bool GhostMode() {
	const auto &settings = Current();
	return settings.ghostHideRead
		&& settings.ghostHideContentsRead
		&& settings.ghostHideOnline
		&& settings.ghostHideTyping
		&& settings.ghostHideStoriesRead;
}

rpl::producer<bool> GhostModeValue() {
	return rpl::single(
		rpl::empty
	) | rpl::then(
		Changes()
	) | rpl::map([] {
		return GhostMode();
	}) | rpl::distinct_until_changed();
}

void SetGhostMode(bool enabled) {
	if (GhostMode() == enabled) {
		return;
	}
	Change([&](Settings &settings) {
		settings.ghostHideRead = enabled;
		settings.ghostHideContentsRead = enabled;
		settings.ghostHideOnline = enabled;
		settings.ghostHideTyping = enabled;
		settings.ghostHideStoriesRead = enabled;
	});
}

rpl::producer<bool> StreamerModeValue() {
	return rpl::single(
		rpl::empty
	) | rpl::then(
		Changes()
	) | rpl::map([] {
		return Current().streamerMode;
	}) | rpl::distinct_until_changed();
}

QString DataFolder() {
	return cWorkingDir() + u"soulgram/"_q;
}

} // namespace Soulgram

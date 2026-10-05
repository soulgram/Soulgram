/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_filters.h"

#include "data/data_groups.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "soulgram/soulgram_settings.h"

#include <QtCore/QRegularExpression>

#include <unordered_map>

namespace Soulgram {
namespace {

struct Part {
	const HistoryItem *item = nullptr; // only compared, may be destroyed
	QString text;
};

// WHY: a relayout in the middle of an edition caches the old text, and
// any new text replaces the shared data, so its pointer tells the change.
struct Cached {
	Main::Session *session = nullptr;
	int version = 0;
	bool filtered = false;
	QString text;
	std::vector<Part> album;
};

struct State {
	bool started = false;
	bool active = false;
	bool wordsActive = false;
	bool enabled = false;
	bool inChannels = false;
	bool hideFromBlocked = false;
	QStringList source;
	QStringList shadowSource;
	base::flat_set<PeerId> shadowBan;
	QStringList words;
	std::vector<QRegularExpression> patterns;
	int version = 1;
	std::unordered_map<const HistoryItem*, Cached> cache;
	base::flat_set<not_null<Main::Session*>> sessions;
	rpl::lifetime lifetime;
};

[[nodiscard]] State &Instance() {
	static auto result = State();
	return result;
}

[[nodiscard]] bool IsPattern(const QString &word) {
	return (word.size() > 2) && word.startsWith('/') && word.endsWith('/');
}

void Compile(State &state) {
	const auto &settings = Current();
	state.source = settings.filterWords;
	state.enabled = settings.wordFilters;
	state.words.clear();
	state.patterns.clear();
	for (const auto &word : settings.filterWords) {
		if (IsPattern(word)) {
			auto pattern = QRegularExpression(
				word.mid(1, word.size() - 2),
				(QRegularExpression::CaseInsensitiveOption
					| QRegularExpression::UseUnicodePropertiesOption));
			if (pattern.isValid()) {
				state.patterns.push_back(std::move(pattern));
				continue;
			}
		}
		state.words.push_back(word);
	}
	state.inChannels = settings.wordFiltersInChannels;
	state.wordsActive = settings.wordFilters
		&& (!state.words.isEmpty() || !state.patterns.empty());

	state.shadowSource = settings.shadowBan;
	state.shadowBan.clear();
	for (const auto &id : settings.shadowBan) {
		if (const auto value = id.toULongLong()) {
			state.shadowBan.emplace(PeerId(PeerIdHelper(value)));
		}
	}
	state.hideFromBlocked = settings.hideFromBlocked;
	state.active = state.wordsActive
		|| !state.shadowBan.empty()
		|| state.hideFromBlocked;
}

// The sender, the author of a forward or the bot it came via are hidden.
// A private chat with such a user stays as it is.
[[nodiscard]] bool SenderHidden(
		const State &state,
		not_null<const HistoryItem*> item) {
	const auto chat = item->history()->peer;
	const auto hidden = [&](PeerData *sender) {
		if (!sender || sender == chat) {
			return false;
		} else if (state.shadowBan.contains(sender->id)) {
			return true;
		} else if (const auto user = sender->asUser()) {
			return state.hideFromBlocked && user->isBlocked();
		}
		return false;
	};
	if (hidden(item->from())) {
		return true;
	} else if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
		if (hidden(forwarded->originalSender)) {
			return true;
		}
	}
	return hidden(item->viaBot());
}

[[nodiscard]] bool Compute(
		const State &state,
		not_null<const HistoryItem*> item) {
	if (item->out() || item->isService() || !item->isRegular()) {
		return false;
	} else if (SenderHidden(state, item)) {
		return true;
	} else if (!state.wordsActive) {
		return false;
	}
	const auto peer = item->history()->peer;
	const auto group = peer->isChat() || peer->isMegagroup();
	if (!group && !(state.inChannels && peer->isBroadcast())) {
		return false;
	}
	const auto &text = item->originalText().text;
	if (text.isEmpty()) {
		return false;
	}
	for (const auto &word : state.words) {
		if (text.contains(word, Qt::CaseInsensitive)) {
			return true;
		}
	}
	for (const auto &pattern : state.patterns) {
		if (pattern.match(text).hasMatch()) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] const QString &TextOf(not_null<const HistoryItem*> item) {
	return item->originalText().text;
}

// The view of the first part shows the captions of the whole album.
[[nodiscard]] const Data::Group *FindAlbum(
		not_null<const HistoryItem*> item) {
	return item->history()->owner().groups().find(item);
}

[[nodiscard]] bool Actual(
		const State &state,
		const Cached &cached,
		not_null<const HistoryItem*> item,
		const Data::Group *album) {
	if (cached.version != state.version) {
		return false;
	} else if (!album) {
		return cached.album.empty() && cached.text.isSharedWith(TextOf(item));
	}
	const auto &items = album->items;
	if (cached.album.size() != items.size()) {
		return false;
	}
	for (auto i = 0, count = int(items.size()); i != count; ++i) {
		const auto &part = cached.album[i];
		if (part.item != items[i].get()
			|| !part.text.isSharedWith(TextOf(items[i]))) {
			return false;
		}
	}
	return true;
}

void Fill(
		const State &state,
		Cached &cached,
		not_null<const HistoryItem*> item,
		const Data::Group *album) {
	cached.version = state.version;
	cached.filtered = false;
	cached.album.clear();
	if (!album) {
		cached.text = TextOf(item);
		cached.filtered = state.active && Compute(state, item);
		return;
	}
	cached.text = QString();
	cached.album.reserve(album->items.size());
	for (const auto &part : album->items) {
		cached.album.push_back({ part.get(), TextOf(part) });
		if (state.active && !cached.filtered && Compute(state, part)) {
			cached.filtered = true;
		}
	}
}

void Refresh(State &state) {
	const auto &settings = Current();
	if (state.source == settings.filterWords
		&& state.enabled == settings.wordFilters
		&& state.inChannels == settings.wordFiltersInChannels
		&& state.shadowSource == settings.shadowBan
		&& state.hideFromBlocked == settings.hideFromBlocked) {
		return;
	}
	Compile(state);
	++state.version;

	// Refreshing a view asks IsFiltered() again, collect the items first.
	auto changed = base::flat_set<not_null<HistoryItem*>>();
	for (auto &[item, cached] : state.cache) {
		const auto was = cached.filtered;
		const auto album = FindAlbum(item);
		Fill(state, cached, item, album);
		if (cached.filtered == was) {
			continue;
		} else if (album) {
			for (const auto &part : album->items) {
				changed.emplace(part);
			}
		} else {
			changed.emplace(const_cast<HistoryItem*>(item));
		}
	}
	for (const auto &item : changed) {
		item->history()->owner().requestItemViewRefresh(item);
	}
}

// Items of a destroyed session are deleted without being unregistered.
[[nodiscard]] not_null<Main::Session*> Watch(
		State &state,
		not_null<const HistoryItem*> item) {
	const auto session = &item->history()->session();
	if (state.sessions.emplace(session).second) {
		session->lifetime().add([=, &state] {
			state.sessions.remove(session);
			for (auto i = begin(state.cache); i != end(state.cache);) {
				if (i->second.session == session) {
					i = state.cache.erase(i);
				} else {
					++i;
				}
			}
		});
	}
	return session;
}

void Start(State &state) {
	state.started = true;
	Compile(state);
	Changes() | rpl::on_next([&state] {
		Refresh(state);
	}, state.lifetime);
}

} // namespace

bool IsFiltered(not_null<const HistoryItem*> item) {
	auto &state = Instance();
	if (!state.started) {
		Start(state);
	}
	// Items are tracked even while no filter is active, so that their
	// views can be refreshed when a filter appears.
	auto &cached = state.cache[item.get()];
	const auto album = FindAlbum(item);
	if (!Actual(state, cached, item, album)) {
		if (!cached.session) {
			cached.session = Watch(state, item);
		}
		Fill(state, cached, item, album);
	}
	return cached.filtered;
}

void ForgetFiltered(not_null<const HistoryItem*> item) {
	Instance().cache.erase(item.get());
}

bool CanShadowBan(not_null<PeerData*> peer) {
	if (const auto user = peer->asUser()) {
		return !user->isSelf();
	}
	return peer->isBroadcast();
}

bool IsShadowBanned(not_null<PeerData*> peer) {
	return Current().shadowBan.contains(QString::number(peer->id.value));
}

void ToggleShadowBan(not_null<PeerData*> peer) {
	const auto id = QString::number(peer->id.value);
	Change([&](Settings &settings) {
		if (!settings.shadowBan.removeOne(id)) {
			settings.shadowBan.push_back(id);
		}
	});
}

QStringList ParseFilterWords(const QString &text) {
	auto result = QStringList();
	for (const auto &line : text.split('\n')) {
		const auto word = line.trimmed();
		if (!word.isEmpty() && !result.contains(word)) {
			result.push_back(word);
		}
	}
	return result;
}

} // namespace Soulgram

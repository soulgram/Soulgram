/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_stories.h"

#include "base/unixtime.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_stories.h"
#include "data/data_story.h"
#include "main/main_session.h"
#include "media/view/media_view_open_common.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_settings.h"
#include "soulgram/soulgram_storage.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

namespace Soulgram {
namespace {

// Authors whose stories were watched invisibly -> latest expiry of those.
using GhostAuthors = base::flat_map<PeerId, TimeId>;

struct SessionState {
	GhostAuthors authors;
	base::flat_set<PeerId> localOnlyReads;

	// Answer for the stories viewer open now, it is reset when that closes.
	bool viewerGhost = false;
};

[[nodiscard]] QString GhostAuthorsPath(not_null<Main::Session*> session) {
	return AccountFolder(session) + u"ghost_stories.json"_q;
}

[[nodiscard]] GhostAuthors Load(not_null<Main::Session*> session) {
	auto result = GhostAuthors();
	auto file = QFile(GhostAuthorsPath(session));
	if (!file.open(QIODevice::ReadOnly)) {
		return result;
	}
	const auto object = QJsonDocument::fromJson(file.readAll()).object();
	for (auto i = object.begin(); i != object.end(); ++i) {
		const auto peer = PeerId(PeerIdHelper(i.key().toULongLong()));
		result.emplace(peer, TimeId(i.value().toDouble()));
	}
	return result;
}

void Save(not_null<Main::Session*> session, const GhostAuthors &authors) {
	auto object = QJsonObject();
	for (const auto &[peer, expires] : authors) {
		object.insert(QString::number(peer.value), double(expires));
	}
	const auto path = GhostAuthorsPath(session);
	QDir().mkpath(QFileInfo(path).absolutePath());
	auto file = QSaveFile(path);
	if (!file.open(QIODevice::WriteOnly)) {
		LOG(("Soulgram Error: could not open '%1' for writing.").arg(path));
		return;
	}
	file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
	if (!file.commit()) {
		LOG(("Soulgram Error: could not save '%1'.").arg(path));
	}
}

[[nodiscard]] base::flat_map<
	not_null<Main::Session*>,
	SessionState> &AllStates() {
	static auto result = base::flat_map<
		not_null<Main::Session*>,
		SessionState>();
	return result;
}

// Returns the state with the expired ghost authors already dropped.
[[nodiscard]] SessionState &State(not_null<Main::Session*> session) {
	auto &all = AllStates();
	auto i = all.find(session);
	if (i == end(all)) {
		session->lifetime().add([=] {
			AllStates().remove(session);
		});
		i = all.emplace(session, SessionState{ .authors = Load(session) }).first;
	}
	auto &authors = i->second.authors;
	const auto now = base::unixtime::now();
	auto changed = false;
	for (auto j = begin(authors); j != end(authors);) {
		if (j->second <= now) {
			j = authors.erase(j);
			changed = true;
		} else {
			++j;
		}
	}
	if (changed) {
		Save(session, authors);
	}
	return i->second;
}

// A viewer opened on own stories from the list goes on to unread ones.
[[nodiscard]] bool MayLeaveSelf(
		not_null<PeerData*> peer,
		const Data::StoriesContext &context) {
	const auto list = std::get_if<Data::StorySourcesList>(&context.data);
	if (!list) {
		return false;
	}
	const auto &sources = peer->owner().stories().sources(*list);
	return ranges::any_of(sources, [&](const Data::StoriesSourceInfo &info) {
		return (info.id != peer->id) && (info.unreadCount > 0);
	});
}

} // namespace

bool KeepStoryReadLocal(not_null<Data::Story*> story) {
	const auto peer = story->peer();
	const auto session = &peer->session();
	auto &state = State(session);
	const auto local = Current().ghostHideStoriesRead
		|| state.viewerGhost
		|| state.authors.contains(peer->id);
	if (!local) {
		return false;
	}
	state.localOnlyReads.emplace(peer->id);
	if (!story->expired()) {
		auto &expires = state.authors[peer->id];
		if (expires < story->expires()) {
			expires = story->expires();
			Save(session, state.authors);
		}
	}
	return true;
}

bool ForceStoryReadRequest(not_null<Data::Story*> story) {
	const auto peer = story->peer();
	return State(&peer->session()).localOnlyReads.remove(peer->id);
}

void OpenStory(
		not_null<Window::SessionController*> controller,
		FullStoryId id,
		const Data::StoriesContext &context) {
	const auto session = &controller->session();
	const auto peer = session->data().peer(id.peer);
	const auto open = crl::guard(controller, [=] {
		// The story may be gone while the question was shown.
		const auto story = session->data().stories().lookup(id);
		if (story) {
			controller->window().openInMediaView(
				Media::View::OpenRequest(controller, *story, context));
		}
	});
	const auto setting = Current().ghostHideStoriesRead;
	if (setting || (peer->isSelf() && !MayLeaveSelf(peer, context))) {
		State(session).viewerGhost = setting;
		open();
		return;
	}
	const auto watchedInvisibly = State(session).authors.contains(peer->id);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(TrValue(Phrase::SectionGhost));
		box->setWidth(st::boxWideWidth);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			TrValue(Phrase::StoriesGhostText),
			st::boxLabel));
		const auto hint = watchedInvisibly
			? Phrase::StoriesGhostWarning
			: Phrase::StoriesGhostHint;
		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				TrValue(hint),
				st::boxDividerLabel),
			st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, 0));
		box->addButton(TrValue(Phrase::StoriesGhostYes), [=] {
			State(session).viewerGhost = true;
			box->closeBox();
			open();
		});
		box->addButton(TrValue(Phrase::StoriesGhostNo), [=] {
			auto &state = State(session);
			state.viewerGhost = false;
			if (state.authors.remove(peer->id)) {
				Save(session, state.authors);
			}
			box->closeBox();
			open();
		});
	}));
}

void StoryViewerClosed(not_null<Main::Session*> session) {
	auto &all = AllStates();
	const auto i = all.find(session);
	if (i != end(all)) {
		i->second.viewerGhost = false;
	}
}

} // namespace Soulgram

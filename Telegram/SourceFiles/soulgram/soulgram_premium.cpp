/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_premium.h"

#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_file_origin.h"
#include "data/data_peer_values.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "soulgram/soulgram_storage.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

namespace Soulgram {
namespace {

struct LocalStatus {
	DocumentId document = 0;
	TimeId until = 0;
};

[[nodiscard]] QString StatusPath(not_null<Main::Session*> session) {
	return AccountFolder(session) + u"emoji_status.json"_q;
}

// Read once for each account.
[[nodiscard]] LocalStatus &StatusFor(not_null<Main::Session*> session) {
	static auto statuses = std::map<uint64, LocalStatus>();
	const auto key = session->uniqueId();
	auto i = statuses.find(key);
	if (i == end(statuses)) {
		auto status = LocalStatus();
		auto file = QFile(StatusPath(session));
		if (file.open(QIODevice::ReadOnly)) {
			const auto object = QJsonDocument::fromJson(
				file.readAll()).object();
			status.document = object.value(
				u"document"_q).toString().toULongLong();
			status.until = TimeId(object.value(u"until"_q).toDouble());
		}
		i = statuses.emplace(key, status).first;
	}
	return i->second;
}

void SaveStatus(not_null<Main::Session*> session, LocalStatus status) {
	StatusFor(session) = status;
	const auto path = StatusPath(session);
	if (!status.document) {
		QFile::remove(path);
		return;
	}
	QDir().mkpath(AccountFolder(session));
	auto object = QJsonObject();
	object.insert(u"document"_q, QString::number(status.document));
	object.insert(u"until"_q, double(status.until));
	auto file = QSaveFile(path);
	if (!file.open(QIODevice::WriteOnly)) {
		LOG(("Soulgram Error: could not write '%1'.").arg(path));
		return;
	}
	file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
	if (!file.commit()) {
		LOG(("Soulgram Error: could not save '%1'.").arg(path));
	}
}

} // namespace

bool LocalPremium(not_null<const UserData*> user) {
	return user->isSelf();
}

void PreloadOwnVideoUserpic(not_null<Main::Session*> session) {
	struct State {
		std::shared_ptr<Data::PhotoMedia> media;
		PhotoId loaded = 0;
		PhotoId requested = 0;
		bool waited = false;
	};
	const auto user = session->user();
	const auto state = session->lifetime().make_state<State>();
	const auto check = [=] {
		const auto id = user->userpicPhotoId();
		if (!id || !user->userpicHasVideo()) {
			*state = State();
			return;
		} else if (state->loaded == id) {
			return;
		}
		const auto photo = session->data().photo(id);
		if (photo->isNull() || !photo->hasVideo()) {
			state->waited = true;
			if (state->requested != id) {
				state->requested = id;
				user->updateFullForced();
			}
			return;
		}
		state->loaded = id;
		state->media = photo->createMediaView();
		state->media->videoWanted(
			Data::PhotoSize::Small,
			user->userpicPhotoOrigin());
		if (base::take(state->waited)) {
			// Userpics that saw the photo before it was loaded look again.
			session->changes().peerUpdated(
				user,
				Data::PeerUpdate::Flag::Photo);
		}
	};
	session->changes().peerFlagsValue(
		user,
		(Data::PeerUpdate::Flag::Photo | Data::PeerUpdate::Flag::FullInfo)
	) | rpl::on_next(check, session->lifetime());
}

bool ServerPremium(not_null<const Main::Session*> session) {
	return (session->user()->flags() & UserDataFlag::Premium);
}

rpl::producer<bool> ServerPremiumValue(not_null<Main::Session*> session) {
	return Data::PeerFlagValue(session->user().get(), UserDataFlag::Premium);
}

bool KeepLocalEmojiStatus(
		not_null<PeerData*> peer,
		const EmojiStatusId &id,
		TimeId until) {
	if (!peer->isSelf()) {
		return false;
	}
	const auto session = &peer->session();
	if (ServerPremium(session) || id.collectible) {
		// The server keeps it, a collectible one needs the gift there.
		if (StatusFor(session).document) {
			SaveStatus(session, {});
		}
		return false;
	}
	SaveStatus(session, { .document = id.documentId, .until = until });
	return true;
}

void ApplyLocalEmojiStatus(
		not_null<PeerData*> peer,
		EmojiStatusId &id,
		TimeId &until) {
	if (!peer->isSelf()) {
		return;
	}
	const auto session = &peer->session();
	const auto status = StatusFor(session);
	if (!status.document || ServerPremium(session)) {
		return;
	} else if (status.until && status.until <= base::unixtime::now()) {
		SaveStatus(session, {});
		return;
	}
	id = EmojiStatusId{ .documentId = status.document };
	until = status.until;
}

void RestoreLocalEmojiStatus(not_null<Main::Session*> session) {
	if (StatusFor(session).document && !ServerPremium(session)) {
		const auto user = session->user();
		user->setEmojiStatus(user->emojiStatusId());
	}
}

} // namespace Soulgram

/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_premium.h"

#include "data/data_changes.h"
#include "data/data_file_origin.h"
#include "data/data_peer_values.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"

namespace Soulgram {

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

} // namespace Soulgram

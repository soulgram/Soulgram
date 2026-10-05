/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_extras.h"

#include "apiwrap.h"
#include "data/data_channel.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_messages.h"
#include "soulgram/soulgram_settings.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/show.h"

namespace Soulgram {
namespace {

constexpr auto kDeletedOpacity = 0.7;

// The last message each chat was read till, so it isn't sent again.
[[nodiscard]] std::map<std::pair<uint64, PeerId>, MsgId> &ReadTill() {
	static auto result = std::map<std::pair<uint64, PeerId>, MsgId>();
	return result;
}

} // namespace

void ReadOnAction(not_null<History*> history) {
	const auto &settings = Current();
	if (!settings.ghostHideRead || !settings.ghostReadOnAction) {
		return;
	}
	const auto last = history->lastServerMessage();
	if (!last || !IsServerMsgId(last->id)) {
		return;
	}
	const auto peer = history->peer;
	const auto tillId = last->id;
	auto &till = ReadTill()[{ history->session().uniqueId(), peer->id }];
	if (till >= tillId) {
		return;
	}
	till = tillId;
	const auto api = &history->session().api();
	using Type = Data::Histories::RequestType;
	history->owner().histories().sendRequest(history, Type::ReadInbox, [=](
			Fn<void()> finish) {
		if (const auto channel = peer->asChannel()) {
			return api->request(MTPchannels_ReadHistory(
				channel->inputChannel(),
				MTP_int(tillId)
			)).done(finish).fail(finish).send();
		}
		return api->request(MTPmessages_ReadHistory(
			peer->input(),
			MTP_int(tillId)
		)).done([=](const MTPmessages_AffectedMessages &result) {
			api->applyAffectedMessages(peer, result);
			finish();
		}).fail(finish).send();
	});
}

void ConfirmSending(
		std::shared_ptr<Ui::Show> show,
		SendKind kind,
		Fn<void()> send) {
	const auto &settings = Current();
	const auto needed = [&] {
		switch (kind) {
		case SendKind::Sticker: return settings.confirmStickers;
		case SendKind::Gif: return settings.confirmGifs;
		case SendKind::Voice: return settings.confirmVoice;
		case SendKind::Round: return settings.confirmRound;
		}
		return false;
	}();
	if (!needed || !show || !show->valid()) {
		send();
		return;
	}
	const auto phrase = [&] {
		switch (kind) {
		case SendKind::Sticker: return Phrase::ConfirmStickerText;
		case SendKind::Gif: return Phrase::ConfirmGifText;
		case SendKind::Voice: return Phrase::ConfirmVoiceText;
		case SendKind::Round: return Phrase::ConfirmRoundText;
		}
		return Phrase::ConfirmStickerText;
	}();
	show->showBox(Ui::MakeConfirmBox({
		.text = Tr(phrase),
		.confirmed = [=](Fn<void()> close) {
			send();
			close();
		},
		.confirmText = tr::lng_send_button(),
	}));
}

bool HideReactions(not_null<PeerData*> peer) {
	const auto &settings = Current();
	if (peer->isUser()) {
		return settings.hideReactionsInPrivate;
	} else if (peer->isBroadcast()) {
		return settings.hideReactionsInChannels;
	}
	return settings.hideReactionsInGroups;
}

QString MessageTime(const QTime &time) {
	const auto locale = QLocale();
	if (!Current().showMessageSeconds) {
		return locale.toString(time, QLocale::ShortFormat);
	}
	const auto twelveHours = locale.timeFormat(
		QLocale::ShortFormat
	).contains(u"AP"_q, Qt::CaseInsensitive);
	return locale.toString(
		time,
		twelveHours ? u"h:mm:ss AP"_q : u"HH:mm:ss"_q);
}

float64 MessageOpacity(not_null<const HistoryItem*> item) {
	return (Current().translucentDeleted && IsDeleted(item))
		? kDeletedOpacity
		: 1.;
}

} // namespace Soulgram

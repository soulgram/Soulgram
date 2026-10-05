/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_regdate.h"

#include "apiwrap.h"
#include "base/unixtime.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "soulgram/soulgram_lang.h"

namespace Soulgram {
namespace {

// The first messages of a chat are checked for "X joined Telegram".
constexpr auto kFirstMessages = 3;

struct KnownAccount {
	uint64 id = 0;
	int date = 0; // yyyymmdd
};

// Known accounts from github.com/jobians/telegram-id-age (2013 - 2025),
// MIT License, Copyright (c) 2025 JOBIANSTECHIE.
constexpr KnownAccount kKnownAccounts[] = {
	{ 0ULL, 20130814 }, { 2768409ULL, 20131101 }, { 7679610ULL, 20131231 },
	{ 11538514ULL, 20140201 }, { 15835244ULL, 20140220 },
	{ 23646077ULL, 20140226 }, { 38015510ULL, 20140301 },
	{ 44634663ULL, 20140506 }, { 46145305ULL, 20140515 },
	{ 54845238ULL, 20140920 }, { 63263518ULL, 20141027 },
	{ 101260938ULL, 20150306 }, { 101323197ULL, 20150313 },
	{ 103151531ULL, 20150603 }, { 103258382ULL, 20150527 },
	{ 109393468ULL, 20150808 }, { 111220210ULL, 20150421 },
	{ 112594714ULL, 20150815 }, { 116812045ULL, 20150723 },
	{ 122600695ULL, 20150724 }, { 124872445ULL, 20150817 },
	{ 125828524ULL, 20151005 }, { 130029930ULL, 20150903 },
	{ 133909606ULL, 20151007 }, { 143445125ULL, 20151201 },
	{ 148670295ULL, 20160108 }, { 152079341ULL, 20160122 },
	{ 157242073ULL, 20151106 }, { 171295414ULL, 20160309 },
	{ 181783990ULL, 20160410 }, { 222021233ULL, 20160608 },
	{ 225034354ULL, 20160618 }, { 278941742ULL, 20160910 },
	{ 285253072ULL, 20161018 }, { 294851037ULL, 20161119 },
	{ 297621225ULL, 20161216 }, { 328594461ULL, 20170128 },
	{ 337808429ULL, 20170221 }, { 341546272ULL, 20170222 },
	{ 352940995ULL, 20170224 }, { 369669043ULL, 20170331 },
	{ 400169472ULL, 20170731 }, { 805158066ULL, 20190715 },
	{ 1974255900ULL, 20211012 }, { 5022636255ULL, 20211210 },
	{ 5031711230ULL, 20211206 }, { 5045293264ULL, 20220113 },
	{ 5047148663ULL, 20220405 }, { 5070164216ULL, 20220119 },
	{ 5106451106ULL, 20220305 }, { 5124771193ULL, 20220222 },
	{ 5144324763ULL, 20220406 }, { 5149590651ULL, 20220122 },
	{ 5153900870ULL, 20220322 }, { 5155903109ULL, 20220322 },
	{ 5159326926ULL, 20220616 }, { 5166844465ULL, 20220509 },
	{ 5169485538ULL, 20220301 }, { 5170390109ULL, 20220228 },
	{ 5177789190ULL, 20220124 }, { 5179102906ULL, 20220312 },
	{ 5196353812ULL, 20220215 }, { 5207110227ULL, 20220203 },
	{ 5210565134ULL, 20220209 }, { 5244529493ULL, 20220419 },
	{ 5259159476ULL, 20220506 }, { 5260388619ULL, 20220302 },
	{ 5268253519ULL, 20220322 }, { 5271530336ULL, 20220430 },
	{ 5288930461ULL, 20220127 }, { 5308260177ULL, 20220425 },
	{ 5340744210ULL, 20221026 }, { 5349830748ULL, 20220429 },
	{ 5351497367ULL, 20220524 }, { 5363536419ULL, 20220801 },
	{ 5394432429ULL, 20220523 }, { 5396515972ULL, 20220421 },
	{ 5428357996ULL, 20220711 }, { 5433708969ULL, 20220528 },
	{ 5434011049ULL, 20220629 }, { 5442755368ULL, 20220723 },
	{ 5451256696ULL, 20220806 }, { 5468433192ULL, 20220720 },
	{ 5468950164ULL, 20220714 }, { 5472518401ULL, 20221102 },
	{ 5488407539ULL, 20220529 }, { 5505809357ULL, 20220527 },
	{ 5515826405ULL, 20221003 }, { 5519218712ULL, 20220814 },
	{ 5542245357ULL, 20220707 }, { 5546930145ULL, 20220724 },
	{ 5558980075ULL, 20221030 }, { 5559594088ULL, 20221110 },
	{ 5567880858ULL, 20230125 }, { 5595045952ULL, 20221023 },
	{ 5596032583ULL, 20220616 }, { 5598262640ULL, 20220611 },
	{ 5601951167ULL, 20221002 }, { 5627539474ULL, 20221202 },
	{ 5681900282ULL, 20221017 }, { 5694365966ULL, 20220828 },
	{ 5705427359ULL, 20221006 }, { 5721138769ULL, 20220923 },
	{ 5735455201ULL, 20221007 }, { 5738347976ULL, 20230425 },
	{ 5744374534ULL, 20221010 }, { 5765259845ULL, 20221030 },
	{ 5795660441ULL, 20221106 }, { 5802659303ULL, 20230321 },
	{ 5804028268ULL, 20230305 }, { 5806925457ULL, 20230112 },
	{ 5815100469ULL, 20221229 }, { 5839137822ULL, 20221216 },
	{ 5854845236ULL, 20230427 }, { 5859861622ULL, 20221119 },
	{ 5862080962ULL, 20221213 }, { 5869978651ULL, 20230324 },
	{ 5891297818ULL, 20230504 }, { 5904140174ULL, 20230520 },
	{ 5931294587ULL, 20221119 }, { 5964221956ULL, 20230109 },
	{ 5983753471ULL, 20221223 }, { 5994561143ULL, 20230516 },
	{ 6000582627ULL, 20230530 }, { 6001287799ULL, 20230510 },
	{ 6074830852ULL, 20230501 }, { 6108395402ULL, 20230518 },
	{ 6135597783ULL, 20230524 }, { 6175817126ULL, 20230507 },
	{ 6180394472ULL, 20230529 }, { 6188508923ULL, 20230622 },
	{ 6254094947ULL, 20230227 }, { 6271031786ULL, 20230212 },
	{ 6277658932ULL, 20230317 }, { 6326011828ULL, 20230707 },
	{ 6401027363ULL, 20231125 }, { 6451891234ULL, 20231202 },
	{ 6513268158ULL, 20231202 }, { 6514802524ULL, 20231116 },
	{ 6523424924ULL, 20230802 }, { 6536173556ULL, 20240111 },
	{ 6545049031ULL, 20231219 }, { 6559717847ULL, 20240225 },
	{ 6606876583ULL, 20240317 }, { 6651640269ULL, 20240518 },
	{ 6670760749ULL, 20240114 }, { 6684986493ULL, 20230925 },
	{ 6703731755ULL, 20240505 }, { 6715889959ULL, 20240205 },
	{ 6720229740ULL, 20240317 }, { 6732829831ULL, 20240211 },
	{ 6749492866ULL, 20231122 }, { 6765129195ULL, 20231102 },
	{ 6827058708ULL, 20231106 }, { 6829119388ULL, 20231202 },
	{ 6854829938ULL, 20240201 }, { 6872061796ULL, 20240525 },
	{ 6903333095ULL, 20240131 }, { 6926984452ULL, 20240109 },
	{ 6947316117ULL, 20231215 }, { 7002435197ULL, 20240406 },
	{ 7078066115ULL, 20240908 }, { 7085776398ULL, 20240510 },
	{ 7104310277ULL, 20240419 }, { 7224009547ULL, 20240802 },
	{ 7242296450ULL, 20240529 }, { 7243375923ULL, 20240825 },
	{ 7254607307ULL, 20240610 }, { 7273085448ULL, 20241121 },
	{ 7280136256ULL, 20240704 }, { 7293965553ULL, 20240616 },
	{ 7342300216ULL, 20250116 }, { 7357703634ULL, 20240910 },
	{ 7363299295ULL, 20240725 }, { 7409259451ULL, 20240620 },
	{ 7450316621ULL, 20241202 }, { 7458668365ULL, 20240802 },
	{ 7591351660ULL, 20250321 }, { 7664959631ULL, 20241219 },
	{ 7708562823ULL, 20250510 }, { 7747102337ULL, 20241106 },
	{ 7793034911ULL, 20240923 }, { 7817256746ULL, 20250618 },
	{ 7825518194ULL, 20250116 }, { 7829910989ULL, 20250511 },
	{ 7831448272ULL, 20241111 }, { 7832006200ULL, 20240919 },
	{ 7834356221ULL, 20250901 }, { 7852083588ULL, 20250602 },
	{ 7870888707ULL, 20250608 }, { 7899152800ULL, 20250408 },
	{ 7912577935ULL, 20250915 }, { 7915901421ULL, 20250707 },
	{ 7964511972ULL, 20250326 }, { 8017192943ULL, 20251005 },
	{ 8044853035ULL, 20250320 }, { 8096742229ULL, 20250831 },
	{ 8117852491ULL, 20251014 }, { 8135088730ULL, 20250305 },
	{ 8173852075ULL, 20250221 }, { 8179125032ULL, 20250709 },
	{ 8200159552ULL, 20250912 }, { 8209194945ULL, 20251026 },
	{ 8238766847ULL, 20250731 }, { 8325327280ULL, 20251013 },
	{ 8343786378ULL, 20250808 }, { 8369442459ULL, 20250808 },
	{ 8384648263ULL, 20251023 }, { 8393200797ULL, 20251025 },
	{ 8461579295ULL, 20250911 }, { 8480708838ULL, 20251105 },
	{ 8559682245ULL, 20251111 },
};

struct CurvePoint {
	uint64 id = 0;
	float64 day = 0.;
};

[[nodiscard]] QDate ToDate(int value) {
	return QDate(value / 10000, (value / 100) % 100, value % 100);
}

// Ids are not given out strictly in order, so the dates of the known
// accounts are made non-decreasing (pool adjacent violators).
[[nodiscard]] const std::vector<CurvePoint> &Curve() {
	static const auto result = [] {
		struct Block {
			float64 sum = 0.;
			int count = 0;
		};
		auto blocks = std::vector<Block>();
		for (const auto &account : kKnownAccounts) {
			blocks.push_back({
				float64(ToDate(account.date).toJulianDay()),
				1,
			});
			while (blocks.size() > 1) {
				const auto last = blocks.back();
				const auto previous = blocks[blocks.size() - 2];
				if (previous.sum * last.count <= last.sum * previous.count) {
					break;
				}
				blocks.pop_back();
				blocks.back() = Block{
					previous.sum + last.sum,
					previous.count + last.count,
				};
			}
		}
		auto points = std::vector<CurvePoint>();
		points.reserve(std::size(kKnownAccounts));
		auto index = 0;
		for (const auto &block : blocks) {
			for (auto i = 0; i != block.count; ++i) {
				points.push_back({
					kKnownAccounts[index++].id,
					block.sum / block.count,
				});
			}
		}
		return points;
	}();
	return result;
}

// Exact dates found in the chats, zero if there is no such message.
[[nodiscard]] std::map<std::pair<uint64, UserId>, TimeId> &Joined() {
	static auto result = std::map<std::pair<uint64, UserId>, TimeId>();
	return result;
}

[[nodiscard]] QString ExactText(TimeId date) {
	return Tr(Phrase::RegistrationExact).arg(
		langDayOfMonthFull(base::unixtime::parse(date).date()));
}

[[nodiscard]] QString FormatDate(TimeId date) {
	return langDayOfMonthFull(base::unixtime::parse(date).date());
}

} // namespace

QString EstimatedRegistration(not_null<UserData*> user) {
	const auto id = peerToUser(user->id).bare;
	const auto &curve = Curve();
	const auto &newest = kKnownAccounts[std::size(kKnownAccounts) - 1];
	if (id > newest.id) {
		return Tr(Phrase::RegistrationLater).arg(
			langDayOfMonthFull(ToDate(newest.date)));
	}
	const auto upper = ranges::lower_bound(
		curve,
		id,
		ranges::less(),
		&CurvePoint::id);
	auto day = upper->day;
	if (upper != begin(curve) && upper->id != id) {
		const auto lower = upper - 1;
		const auto ratio = float64(id - lower->id)
			/ float64(upper->id - lower->id);
		day = lower->day + ratio * (upper->day - lower->day);
	}
	const auto date = QDate::fromJulianDay(qint64(std::round(day)));
	return Tr(Phrase::RegistrationApproximately).arg(
		langMonthOfYearFull(date.month(), date.year()));
}

void RequestJoinedTelegram(
		not_null<UserData*> user,
		Fn<void(QString)> done) {
	if (user->isSelf()) {
		return;
	}
	const auto session = &user->session();
	const auto key = std::make_pair(session->uniqueId(), peerToUser(user->id));
	if (const auto i = Joined().find(key); i != end(Joined())) {
		if (i->second) {
			done(ExactText(i->second));
		}
		return;
	}
	session->api().request(MTPmessages_GetHistory(
		user->input(),
		MTP_int(1), // From the very first message of the chat.
		MTP_int(0),
		MTP_int(-kFirstMessages),
		MTP_int(kFirstMessages),
		MTP_int(0),
		MTP_int(0),
		MTP_long(0)
	)).done([=](const MTPmessages_Messages &result) {
		auto date = TimeId();
		result.match([](const MTPDmessages_messagesNotModified &) {
		}, [&](const auto &data) {
			for (const auto &message : data.vmessages().v) {
				message.match([&](const MTPDmessageService &service) {
					const auto type = service.vaction().type();
					if (type == mtpc_messageActionContactSignUp) {
						date = service.vdate().v;
					}
				}, [](const auto &) {
				});
			}
		});
		Joined()[key] = date;
		if (date) {
			done(ExactText(date));
		}
	}).send();
}

PeerDate ChatDate(not_null<PeerData*> peer) {
	if (const auto channel = peer->asChannel()) {
		if (channel->amIn()) {
			// For members the date is when they joined.
			const auto date = channel->inviteDate
				? channel->inviteDate
				: TimeId(channel->date);
			if (date) {
				return { Tr(Phrase::JoinedDate), FormatDate(date) };
			}
		} else if (channel->date) {
			return { Tr(Phrase::CreatedDate), FormatDate(channel->date) };
		}
	} else if (const auto chat = peer->asChat()) {
		if (chat->date) {
			return { Tr(Phrase::CreatedDate), FormatDate(chat->date) };
		}
	}
	return {};
}

} // namespace Soulgram

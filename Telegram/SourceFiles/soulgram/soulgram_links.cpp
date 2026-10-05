/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_links.h"

#include "core/application.h"
#include "core/click_handler_types.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "iv/iv_instance.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_settings.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/show.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_calls.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSaveFile>

namespace Soulgram {
namespace {

// How long Instant View may take before it opens the link in the browser.
constexpr auto kConfirmedIvTimeout = 2 * 60 * crl::time(1000);

bool Confirmed = false;

// A confirmed link that Instant View may still open in the browser.
QString ConfirmedIvLink;
crl::time ConfirmedIvAt = 0;

// The browser fallback of Instant View opens a link the user confirmed.
[[nodiscard]] bool TakeConfirmedIvLink(const QString &link) {
	if (ConfirmedIvLink.isEmpty() || ConfirmedIvLink != link) {
		return false;
	}
	ConfirmedIvLink = QString();
	return (crl::now() - ConfirmedIvAt < kConfirmedIvTimeout);
}

[[nodiscard]] QString TrustedSitesPath() {
	return DataFolder() + u"trusted_sites.json"_q;
}

[[nodiscard]] base::flat_set<QString> &TrustedSites() {
	static auto result = [] {
		auto result = base::flat_set<QString>();
		auto file = QFile(TrustedSitesPath());
		if (file.open(QIODevice::ReadOnly)) {
			const auto document = QJsonDocument::fromJson(file.readAll());
			for (const auto &value : document.array()) {
				if (const auto site = value.toString(); !site.isEmpty()) {
					result.emplace(site);
				}
			}
		}
		return result;
	}();
	return result;
}

void SaveTrustedSites() {
	auto list = QJsonArray();
	for (const auto &site : TrustedSites()) {
		list.append(site);
	}
	QDir().mkpath(DataFolder());
	auto file = QSaveFile(TrustedSitesPath());
	if (!file.open(QIODevice::WriteOnly)) {
		LOG(("Soulgram Error: could not open trusted_sites.json "
			"for writing."));
		return;
	}
	file.write(QJsonDocument(list).toJson(QJsonDocument::Indented));
	if (!file.commit()) {
		LOG(("Soulgram Error: could not save trusted_sites.json."));
	}
}

[[nodiscard]] QUrl ParseUrl(const QString &url) {
	return url.startsWith(u"tonsite://"_q, Qt::CaseInsensitive)
		? QUrl(url)
		: QUrl::fromUserInput(url);
}

[[nodiscard]] bool IsTelegramLink(const QUrl &url) {
	if (url.scheme().compare(u"tg"_q, Qt::CaseInsensitive) == 0) {
		return true;
	}
	const auto host = url.host().toLower();
	const auto domains = { u"t.me"_q, u"telegram.me"_q, u"telegram.dog"_q };
	for (const auto &domain : domains) {
		if (host == domain || host.endsWith('.' + domain)) {
			return true;
		}
	}
	return false;
}

// What "don't ask again" remembers: the host without "www.".
[[nodiscard]] QString SiteKey(const QUrl &url) {
	if (IsTelegramLink(url)) {
		return u"t.me"_q;
	}
	auto host = url.host().toLower();
	if (host.startsWith(u"www."_q)) {
		host = host.mid(4);
	}
	return host;
}

[[nodiscard]] bool FromSomeoneElse(const ClickHandlerContext &my) {
	if (const auto peer = my.peer) {
		return !peer->isSelf();
	} else if (!my.itemId) {
		return false;
	}
	const auto controller = my.sessionWindow.get();
	const auto item = controller
		? controller->session().data().message(my.itemId)
		: nullptr;
	return !item || !item->out() || item->Has<HistoryMessageForwarded>();
}

[[nodiscard]] TextWithEntities BoldHost(
		const QString &shown,
		const QString &host) {
	auto result = TextWithEntities{ .text = shown };
	const auto bold = host.startsWith(u"www."_q) ? host.mid(4) : host;
	const auto position = bold.isEmpty()
		? -1
		: shown.indexOf(bold, 0, Qt::CaseInsensitive);
	if (position >= 0) {
		result.entities.push_back(
			EntityInText(EntityType::Bold, position, bold.size()));
	}
	return result;
}

void LinkWarningBox(
		not_null<Ui::GenericBox*> box,
		const QString &url,
		const QVariant &context,
		bool dark,
		Fn<void()> open) {
	const auto &label = dark ? st::groupCallBoxLabel : st::boxLabel;
	const auto &hint = dark ? st::groupCallBoxLabel : st::boxDividerLabel;
	const auto &checkbox = dark
		? st::groupCallCheckbox
		: st::defaultBoxCheckbox;
	const auto parsed = ParseUrl(url);
	const auto displayed = parsed.isValid() ? parsed.toDisplayString() : url;
	const auto suspicious = UrlClickHandler::IsSuspicious(displayed);
	const auto shown = !suspicious
		? displayed
		: parsed.isValid()
		? QString::fromUtf8(parsed.toEncoded())
		: UrlClickHandler::ShowEncoded(displayed);
	const auto host = parsed.host(suspicious
		? QUrl::EncodeUnicode
		: QUrl::PrettyDecoded);
	const auto site = suspicious ? QString() : SiteKey(parsed);
	const auto padding = st::boxRowPadding
		+ QMargins(0, st::boxLittleSkip, 0, 0);

	box->setTitle(TrValue(Phrase::LinkWarningTitle));
	box->setWidth(st::boxWideWidth);

	const auto address = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		rpl::single(BoldHost(shown, host)),
		label));
	address->setSelectable(true);
	address->setContextCopyText(tr::lng_context_copy_link(tr::now));

	if (suspicious) {
		const auto text = Tr(Phrase::LinkWarningSuspicious);
		box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(Ui::Text::Bold(text)),
				label),
			padding);
	}
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			TrValue(IsTelegramLink(parsed)
				? Phrase::LinkWarningTelegram
				: Phrase::LinkWarningExternal),
			label),
		padding);
	const auto trust = site.isEmpty()
		? nullptr
		: box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				Tr(Phrase::LinkTrustSite).arg(site),
				false,
				checkbox),
			padding);
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			TrValue(Phrase::LinkWarningHint),
			hint),
		padding);

	box->addButton(tr::lng_open_link(), [=] {
		if (trust && trust->checked()) {
			TrustedSites().emplace(site);
			SaveTrustedSites();
		}
		box->closeBox();
		// Remember the link only if an Instant View attempt will follow.
		const auto my = context.value<ClickHandlerContext>();
		if (!my.ignoreIv && my.sessionWindow.get() && Iv::PreferForUri(url)) {
			ConfirmedIvLink = url;
			ConfirmedIvAt = crl::now();
		} else {
			ConfirmedIvLink = QString();
		}
		Confirmed = true;
		open();
		Confirmed = false;
	});
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

} // namespace

bool GuardLink(
		const QString &url,
		const QVariant &context,
		Fn<void()> open) {
	if (!Current().linkWarning) {
		return false;
	}
	auto link = url.trimmed();
	const auto external = UrlClickHandler::ExternalUrlFromInternalUrl(link);
	if (!external.isEmpty()) {
		link = external;
	}
	if (link.isEmpty()
		|| UrlClickHandler::IsEmail(link)
		|| link.startsWith(u"mailto:"_q, Qt::CaseInsensitive)
		|| link.startsWith(u"internal:"_q, Qt::CaseInsensitive)) {
		return false;
	}
	const auto my = context.value<ClickHandlerContext>();
	// The Instant View fallback also consumes the remembered link.
	if (my.ignoreIv && TakeConfirmedIvLink(link)) {
		return false;
	} else if (Confirmed || !FromSomeoneElse(my)) {
		return false;
	}
	const auto parsed = ParseUrl(link);
	const auto displayed = parsed.isValid() ? parsed.toDisplayString() : link;
	if (!UrlClickHandler::IsSuspicious(displayed)
		&& TrustedSites().contains(SiteKey(parsed))) {
		return false;
	}
	auto box = Box(LinkWarningBox, link, context, my.dark, std::move(open));
	if (my.show) {
		my.show->showBox(std::move(box));
		return true;
	}
	const auto controller = my.sessionWindow.get();
	const auto window = controller
		? &controller->window()
		: Core::App().activeWindow();
	if (!window) {
		return false;
	}
	Core::App().hideMediaView();
	window->show(std::move(box));
	window->activate();
	return true;
}

bool LinkConfirmed() {
	return Confirmed;
}

void ForgetTrustedSites() {
	TrustedSites().clear();
	SaveTrustedSites();
}

rpl::producer<> LinkWarningChanges() {
	return rpl::single(
		rpl::empty
	) | rpl::then(
		Changes()
	) | rpl::map([] {
		return Current().linkWarning;
	}) | rpl::distinct_until_changed(
	) | rpl::skip(1) | rpl::to_empty;
}

bool LinksFollowWarning(not_null<const HistoryItem*> item) {
	if (item->hasHiddenLinks()) {
		return true;
	} else if (item->out() || item->isService()) {
		return false;
	}
	return item->history()->peer->hideLinks();
}

} // namespace Soulgram

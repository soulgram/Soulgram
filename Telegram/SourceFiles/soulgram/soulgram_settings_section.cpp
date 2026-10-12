/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_settings_section.h"

#include "core/click_handler_types.h"
#include "core/version.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "settings/settings_common_session.h"
#include "soulgram/soulgram_filters.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_links.h"
#include "soulgram/soulgram_settings.h"
#include "soulgram/soulgram_updates.h"
#include "soulgram/soulgram_view_once.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/vertical_list.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <QtGui/QDesktopServices>

namespace Soulgram {
namespace {

constexpr auto kChannelUrl = "https://t.me/official_soulgram";
constexpr auto kGithubUrl = "https://github.com/soulgram/Soulgram";

enum class Category {
	Messages,
	ViewOnce,
	Ghost,
	Ads,
	Streamer,
	Filters,
	Sending,
	Interface,
	Links,
};

struct CategoryInfo {
	Category category = Category();
	Phrase title = Phrase();
	const style::icon *icon = nullptr;
};

[[nodiscard]] const std::vector<CategoryInfo> &Categories() {
	static const auto result = std::vector<CategoryInfo>{
		{ Category::Messages, Phrase::SectionMessages, &st::menuIconRestore },
		{ Category::ViewOnce, Phrase::SectionViewOnce, &st::menuIconTTL },
		{ Category::Ghost, Phrase::SectionGhost, &st::menuIconStealth },
		{ Category::Ads, Phrase::SectionAds, &st::menuIconBlock },
		{ Category::Streamer, Phrase::SectionStreamer, &st::menuIconStartStream },
		{ Category::Filters, Phrase::SectionFilters, &st::menuIconAntispam },
		{ Category::Sending, Phrase::SectionSending, &st::menuIconSend },
		{ Category::Interface, Phrase::SectionInterface, &st::menuIconPalette },
		{ Category::Links, Phrase::SectionLinks, &st::menuIconLink },
	};
	return result;
}

[[nodiscard]] Phrase TitleOf(Category category) {
	for (const auto &info : Categories()) {
		if (info.category == category) {
			return info.title;
		}
	}
	return Phrase::SettingsTitle;
}

void AddToggle(
		not_null<Ui::VerticalLayout*> container,
		Phrase phrase,
		bool Settings::*member) {
	const auto button = container->add(object_ptr<Ui::SettingsButton>(
		container,
		TrValue(phrase),
		st::settingsButtonNoIcon));
	button->toggleOn(rpl::single(Current().*member));
	button->toggledChanges(
	) | rpl::on_next([=](bool value) {
		Change([&](Settings &settings) {
			settings.*member = value;
		});
	}, button->lifetime());
}

not_null<Ui::SettingsButton*> AddButton(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text) {
	return container->add(object_ptr<Ui::SettingsButton>(
		container,
		std::move(text),
		st::settingsButtonNoIcon));
}

void AddAbout(not_null<Ui::VerticalLayout*> container, Phrase phrase) {
	Ui::AddSkip(container);
	Ui::AddDividerText(container, TrValue(phrase));
}

void FilterWordsBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(TrValue(Phrase::FilterWordsTitle));
	box->setWidth(st::boxWideWidth);

	const auto fieldStyle = box->lifetime().make_state<style::InputField>(
		st::newGroupDescription);
	fieldStyle->heightMax = st::newGroupDescription.heightMax * 3;
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		*fieldStyle,
		Ui::InputField::Mode::MultiLine,
		TrValue(Phrase::FilterWordsPlaceholder),
		Current().filterWords.join('\n')));
	field->setSubmitSettings(Ui::InputField::SubmitSettings::CtrlEnter);
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			TrValue(Phrase::FilterWordsAbout),
			st::boxDividerLabel),
		st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, 0));

	const auto save = [=] {
		const auto words = ParseFilterWords(field->getLastText());
		Change([&](Settings &settings) {
			settings.filterWords = words;
		});
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=](Qt::KeyboardModifiers) {
		save();
	}, field->lifetime());
	box->setFocusCallback([=] {
		field->setFocusFast();
	});
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

// Each row removes its user or channel from the shadow ban.
void ShadowBanBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session) {
	box->setTitle(TrValue(Phrase::ShadowBan));
	box->setWidth(st::boxWideWidth);

	const auto ids = Current().shadowBan;
	if (ids.isEmpty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			Tr(Phrase::ShadowBanEmpty),
			st::boxLabel));
	}
	const auto container = box->verticalLayout();
	for (const auto &id : ids) {
		const auto peer = session->data().peerLoaded(
			PeerId(PeerIdHelper(id.toULongLong())));
		const auto name = peer ? peer->name() : (u"ID "_q + id);
		const auto row = container->add(
			object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
				container,
				object_ptr<Ui::SettingsButton>(
					container,
					rpl::single(name),
					st::settingsButtonNoIcon)));
		row->entity()->setClickedCallback([=] {
			Change([&](Settings &settings) {
				settings.shadowBan.removeAll(id);
			});
			row->hide(anim::type::normal);
			box->uiShow()->showToast(Tr(Phrase::ShadowBanRemoved).arg(name));
		});
	}
	Ui::AddSkip(container);
	Ui::AddDividerText(container, TrValue(Phrase::ShadowBanAbout));
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void FillMessages(not_null<Ui::VerticalLayout*> container) {
	AddToggle(container, Phrase::SaveDeleted, &Settings::saveDeletedMessages);
	AddToggle(
		container,
		Phrase::TranslucentDeleted,
		&Settings::translucentDeleted);
	AddAbout(container, Phrase::MessagesAbout);
}

void FillViewOnce(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller) {
	AddToggle(container, Phrase::KeepViewOnce, &Settings::keepSelfDestructing);
	AddToggle(
		container,
		Phrase::SaveViewOnceToDisk,
		&Settings::saveSelfDestructingToDisk);
	const auto session = &controller->session();
	AddButton(
		container,
		TrValue(Phrase::OpenSavedFolder)
	)->setClickedCallback([=] {
		const auto folder = SavedMediaFolder(session);
		QDir().mkpath(folder);
		QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
	});
	AddAbout(container, Phrase::ViewOnceAbout);
}

void FillGhost(not_null<Ui::VerticalLayout*> container) {
	AddToggle(container, Phrase::GhostRead, &Settings::ghostHideRead);
	AddToggle(
		container,
		Phrase::GhostContentsRead,
		&Settings::ghostHideContentsRead);
	AddToggle(container, Phrase::GhostOnline, &Settings::ghostHideOnline);
	AddToggle(container, Phrase::GhostTyping, &Settings::ghostHideTyping);
	AddToggle(container, Phrase::GhostStories, &Settings::ghostHideStoriesRead);
	AddToggle(
		container,
		Phrase::GhostReadOnAction,
		&Settings::ghostReadOnAction);
	AddAbout(container, Phrase::GhostModeAbout);
	AddAbout(container, Phrase::GhostAbout);
}

void FillAds(not_null<Ui::VerticalLayout*> container) {
	AddToggle(container, Phrase::HideAds, &Settings::hideAds);
	AddToggle(container, Phrase::CopyProtected, &Settings::copyProtected);
	AddAbout(container, Phrase::AdsAbout);
}

void FillStreamer(not_null<Ui::VerticalLayout*> container) {
	AddToggle(container, Phrase::StreamerMode, &Settings::streamerMode);
	AddAbout(container, Phrase::StreamerAbout);
}

void FillFilters(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller) {
	AddToggle(container, Phrase::WordFilters, &Settings::wordFilters);
	AddToggle(
		container,
		Phrase::WordFiltersInChannels,
		&Settings::wordFiltersInChannels);
	auto wordsText = rpl::combine(
		TrValue(Phrase::FilterWordsButton),
		rpl::single(rpl::empty) | rpl::then(Changes())
	) | rpl::map([](const QString &text, const auto &) {
		return text.arg(Current().filterWords.size());
	});
	AddButton(container, std::move(wordsText))->setClickedCallback([=] {
		controller->show(Box(FilterWordsBox));
	});
	AddAbout(container, Phrase::FiltersAbout);

	Ui::AddSkip(container);
	AddToggle(container, Phrase::HideFromBlocked, &Settings::hideFromBlocked);
	auto bannedText = rpl::combine(
		TrValue(Phrase::ShadowBanList),
		rpl::single(rpl::empty) | rpl::then(Changes())
	) | rpl::map([](const QString &text, const auto &) {
		return text.arg(Current().shadowBan.size());
	});
	const auto session = &controller->session();
	AddButton(container, std::move(bannedText))->setClickedCallback([=] {
		controller->show(Box(ShadowBanBox, session));
	});
	AddAbout(container, Phrase::ShadowBanAbout);
}

void FillSending(not_null<Ui::VerticalLayout*> container) {
	AddToggle(container, Phrase::ConfirmStickers, &Settings::confirmStickers);
	AddToggle(container, Phrase::ConfirmGifs, &Settings::confirmGifs);
	AddToggle(container, Phrase::ConfirmVoice, &Settings::confirmVoice);
	AddToggle(container, Phrase::ConfirmRound, &Settings::confirmRound);
	AddAbout(container, Phrase::SendingAbout);
}

void FillInterface(not_null<Ui::VerticalLayout*> container) {
	AddToggle(
		container,
		Phrase::HideReactionsChannels,
		&Settings::hideReactionsInChannels);
	AddToggle(
		container,
		Phrase::HideReactionsGroups,
		&Settings::hideReactionsInGroups);
	AddToggle(
		container,
		Phrase::HideReactionsPrivate,
		&Settings::hideReactionsInPrivate);
	AddToggle(container, Phrase::DisableStories, &Settings::disableStories);
	AddToggle(
		container,
		Phrase::ShowMessageSeconds,
		&Settings::showMessageSeconds);
	AddToggle(
		container,
		Phrase::DisableGreetingSticker,
		&Settings::disableGreetingSticker);
	AddToggle(
		container,
		Phrase::HidePremiumStatuses,
		&Settings::hidePremiumStatuses);
	AddToggle(
		container,
		Phrase::DisableNotificationsDelay,
		&Settings::disableNotificationsDelay);
	AddAbout(container, Phrase::InterfaceAbout);
}

void FillLinks(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller) {
	AddToggle(container, Phrase::LinkWarning, &Settings::linkWarning);
	AddButton(
		container,
		TrValue(Phrase::ForgetTrustedSites)
	)->setClickedCallback([=] {
		ForgetTrustedSites();
		controller->showToast(Tr(Phrase::TrustedSitesForgotten));
	});
	AddAbout(container, Phrase::LinksAbout);
}

void FillCategory(
		Category category,
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller) {
	Ui::AddSkip(container);
	switch (category) {
	case Category::Messages: FillMessages(container); break;
	case Category::ViewOnce: FillViewOnce(container, controller); break;
	case Category::Ghost: FillGhost(container); break;
	case Category::Ads: FillAds(container); break;
	case Category::Streamer: FillStreamer(container); break;
	case Category::Filters: FillFilters(container, controller); break;
	case Category::Sending: FillSending(container); break;
	case Category::Interface: FillInterface(container); break;
	case Category::Links: FillLinks(container, controller); break;
	}
}

template <Category kCategory>
class CategorySection final
	: public ::Settings::Section<CategorySection<kCategory>> {
public:
	CategorySection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

};

template <Category kCategory>
CategorySection<kCategory>::CategorySection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: ::Settings::Section<CategorySection<kCategory>>(parent, controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	FillCategory(kCategory, content, controller);
	Ui::ResizeFitChild(this, content);
}

template <Category kCategory>
rpl::producer<QString> CategorySection<kCategory>::title() {
	return TrValue(TitleOf(kCategory));
}

[[nodiscard]] ::Settings::Type CategoryId(Category category) {
	switch (category) {
	case Category::Messages:
		return CategorySection<Category::Messages>::Id();
	case Category::ViewOnce:
		return CategorySection<Category::ViewOnce>::Id();
	case Category::Ghost: return CategorySection<Category::Ghost>::Id();
	case Category::Ads: return CategorySection<Category::Ads>::Id();
	case Category::Streamer:
		return CategorySection<Category::Streamer>::Id();
	case Category::Filters: return CategorySection<Category::Filters>::Id();
	case Category::Sending: return CategorySection<Category::Sending>::Id();
	case Category::Interface:
		return CategorySection<Category::Interface>::Id();
	case Category::Links: return CategorySection<Category::Links>::Id();
	}
	Unexpected("Category in Soulgram::CategoryId.");
}

class MainSection final : public ::Settings::Section<MainSection> {
public:
	MainSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

};

MainSection::MainSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	Ui::AddSkip(content);
	for (const auto &info : Categories()) {
		const auto category = info.category;
		::Settings::AddButtonWithIcon(
			content,
			TrValue(info.title),
			st::settingsButton,
			{ info.icon }
		)->setClickedCallback([=] {
			showOther(CategoryId(category));
		});
	}
	Ui::AddSkip(content);
	Ui::AddDivider(content);
	Ui::AddSkip(content);
	const auto weak = base::make_weak(controller);
	::Settings::AddButtonWithIcon(
		content,
		TrValue(Phrase::SoulgramChannel),
		st::settingsButton,
		{ &st::menuIconChannel }
	)->setClickedCallback([=] {
		UrlClickHandler::Open(
			QString::fromLatin1(kChannelUrl),
			QVariant::fromValue(ClickHandlerContext{
				.sessionWindow = weak,
			}));
	});
	::Settings::AddButtonWithIcon(
		content,
		TrValue(Phrase::SoulgramGithub),
		st::settingsButton,
		{ &st::menuIconLink }
	)->setClickedCallback([=] {
		UrlClickHandler::Open(QString::fromLatin1(kGithubUrl));
	});
	::Settings::AddButtonWithIcon(
		content,
		TrValue(Phrase::AutoUpdate),
		st::settingsButton,
		{ &st::menuIconSettings }
	)->toggleOn(
		rpl::single(Current().autoUpdate)
	)->toggledChanges(
	) | rpl::on_next([=](bool value) {
		Change([&](Settings &settings) {
			settings.autoUpdate = value;
		});
	}, content->lifetime());
	auto updateLabel = UpdateStateValue(
	) | rpl::map([](UpdateState state) {
		switch (state) {
		case UpdateState::Checking: return Tr(Phrase::UpdateChecking);
		case UpdateState::Latest: return Tr(Phrase::UpdateLatest);
		case UpdateState::Downloading:
			return Tr(Phrase::UpdateDownloading).arg(UpdateVersion());
		case UpdateState::Ready:
			return Tr(Phrase::UpdateReady).arg(UpdateVersion());
		case UpdateState::Failed: return Tr(Phrase::UpdateFailed);
		case UpdateState::Disabled:
		case UpdateState::Idle: return QString();
		}
		return QString();
	});
	::Settings::AddButtonWithLabel(
		content,
		TrValue(Phrase::CheckUpdates),
		std::move(updateLabel),
		st::settingsButton,
		{ &st::menuIconDownload }
	)->setClickedCallback([=] {
		if (!InstallUpdate()) {
			CheckForUpdates();
		}
	});
	Ui::AddSkip(content);
	Ui::AddDividerText(
		content,
		rpl::single(u"Soulgram %1 · Telegram Desktop %2"_q.arg(
			QString::fromLatin1(SoulgramVersionStr),
			QString::fromLatin1(AppVersionStr))));
	Ui::ResizeFitChild(this, content);
}

rpl::producer<QString> MainSection::title() {
	return TrValue(Phrase::SettingsTitle);
}

} // namespace

::Settings::Type SettingsSectionId() {
	return MainSection::Id();
}

} // namespace Soulgram

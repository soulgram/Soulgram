/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_ui.h"

#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "dialogs/dialogs_key.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "soulgram/soulgram_chat_actions.h"
#include "soulgram/soulgram_filters.h"
#include "soulgram/soulgram_messages.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_settings.h"
#include "soulgram/soulgram_storage.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/show.h"
#include "ui/boxes/confirm_box.h"
#include "ui/text/format_values.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "ui/widgets/popup_menu.h"
#include "ui/vertical_list.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace Soulgram {
namespace {

// Chats can keep thousands of deleted messages, they are shown in pages.
constexpr auto kDeletedPerPage = 100;

[[nodiscard]] QString FormatDate(TimeId date) {
	return QLocale().toString(
		base::unixtime::parse(date),
		QLocale::ShortFormat);
}

// Justified rows don't make the layout recount its natural width.
void AddMessage(
		not_null<Ui::VerticalLayout*> container,
		const QString &header,
		const QString &text) {
	container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			rpl::single(Ui::Text::Bold(header)),
			st::boxDividerLabel),
		st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, 0),
		style::al_justify);
	const auto label = container->add(
		object_ptr<Ui::FlatLabel>(container, text, st::boxLabel),
		st::boxRowPadding,
		style::al_justify);
	label->setSelectable(true);
}

void AddDeletedMessage(
		not_null<Ui::VerticalLayout*> container,
		const DeletedRecord &record) {
	const auto header = record.fromName
		+ u", "_q
		+ FormatDate(record.date)
		+ u" ("_q
		+ Tr(Phrase::DeletedAt)
		+ ' '
		+ FormatDate(record.deletedAt)
		+ ')';
	auto text = record.text;
	if (!record.media.isEmpty() && record.media != record.text) {
		text = u"["_q + record.media + u"]"_q
			+ (text.isEmpty() ? QString() : (u"\n"_q + text));
	}
	AddMessage(container, header, text);
}

// The full date and time, with seconds.
[[nodiscard]] QString FormatFullDate(TimeId date) {
	return QLocale().toString(
		base::unixtime::parse(date),
		u"dd.MM.yyyy HH:mm:ss"_q);
}

void ConfirmClearDeleted(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer) {
	controller->show(Ui::MakeConfirmBox({
		.text = Tr(Phrase::ClearDeletedText),
		.confirmed = [=](Fn<void()> close) {
			ForgetDeletedHistory(peer->owner().history(peer));
			close();
		},
		.confirmText = TrValue(Phrase::ClearDeletedConfirm),
		.confirmStyle = &st::attentionBoxButton,
		.title = TrValue(Phrase::ClearDeleted),
	}));
}

// With the name, for the menu of a message of somebody in a group.
void AddShadowBanAction(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> peer,
		const Ui::Menu::MenuCallback &addAction,
		bool withName) {
	const auto banned = IsShadowBanned(peer);
	const auto name = peer->name();
	const auto text = withName
		? Tr(banned
			? Phrase::ShadowBanAuthorRemove
			: Phrase::ShadowBanAuthor).arg(name)
		: Tr(banned ? Phrase::ShadowBanRemove : Phrase::ShadowBan);
	addAction(text, [=] {
		ToggleShadowBan(peer);
		controller->showToast(Tr(banned
			? Phrase::ShadowBanRemoved
			: Phrase::ShadowBanAdded).arg(name));
	}, banned ? &st::menuIconShowInChat : &st::menuIconStealth);
}

void AddDetail(
		not_null<Ui::VerticalLayout*> container,
		Phrase label,
		const QString &value) {
	if (value.isEmpty()) {
		return;
	}
	container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			rpl::single(Ui::Text::Bold(Tr(label))),
			st::boxDividerLabel),
		st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, 0),
		style::al_justify);
	const auto text = container->add(
		object_ptr<Ui::FlatLabel>(container, value, st::boxLabel),
		st::boxRowPadding,
		style::al_justify);
	text->setSelectable(true);
}

[[nodiscard]] QString PeerLine(not_null<PeerData*> peer) {
	return peer->name() + u" \u00b7 "_q + PeerIdString(peer);
}

void MessageDetailsBox(
		not_null<Ui::GenericBox*> box,
		not_null<HistoryItem*> item) {
	box->setTitle(TrValue(Phrase::MessageDetails));
	box->setWidth(st::boxWideWidth);

	const auto container = box->verticalLayout();
	const auto peer = item->history()->peer;
	AddDetail(container, Phrase::DetailsMessageId, QString::number(item->id.bare));
	AddDetail(container, Phrase::DetailsChat, PeerLine(peer));
	if (const auto from = item->from(); from != peer) {
		AddDetail(container, Phrase::DetailsSender, PeerLine(from));
	}
	AddDetail(container, Phrase::DetailsDate, FormatFullDate(item->date()));
	if (const auto edited = item->Get<HistoryMessageEdited>()) {
		if (edited->date) {
			AddDetail(
				container,
				Phrase::DetailsEdited,
				FormatFullDate(edited->date));
		}
	}
	if (const auto forwarded = item->Get<HistoryMessageForwarded>()) {
		const auto original = forwarded->originalSender;
		const auto hidden = forwarded->originalHiddenSenderInfo.get();
		AddDetail(
			container,
			Phrase::DetailsForwardedFrom,
			(original
				? PeerLine(original)
				: hidden
				? hidden->name
				: QString()));
		if (forwarded->originalDate) {
			AddDetail(
				container,
				Phrase::DetailsForwardedDate,
				FormatFullDate(forwarded->originalDate));
		}
	}
	if (const auto views = item->Get<HistoryMessageViews>()) {
		if (views->views.count >= 0) {
			AddDetail(
				container,
				Phrase::DetailsViews,
				QString::number(views->views.count));
		}
		if (views->forwardsCount > 0) {
			AddDetail(
				container,
				Phrase::DetailsForwards,
				QString::number(views->forwardsCount));
		}
	}
	const auto media = item->media();
	if (const auto document = media ? media->document() : nullptr) {
		AddDetail(container, Phrase::DetailsFile, document->filename());
		if (document->size > 0) {
			AddDetail(
				container,
				Phrase::DetailsSize,
				Ui::FormatSizeText(document->size));
		}
		AddDetail(container, Phrase::DetailsType, document->mimeString());
		const auto dimensions = document->dimensions;
		if (!dimensions.isEmpty()) {
			AddDetail(
				container,
				Phrase::DetailsResolution,
				u"%1\u00d7%2"_q.arg(dimensions.width()).arg(dimensions.height()));
		}
		if (const auto duration = document->duration(); duration > 0) {
			AddDetail(
				container,
				Phrase::DetailsDuration,
				Ui::FormatDurationText(duration / 1000));
		}
		AddDetail(container, Phrase::DetailsPath, document->filepath(true));
	} else if (const auto photo = media ? media->photo() : nullptr) {
		if (const auto size = photo->size(Data::PhotoSize::Large)) {
			AddDetail(
				container,
				Phrase::DetailsResolution,
				u"%1\u00d7%2"_q.arg(size->width()).arg(size->height()));
		}
		if (const auto bytes = photo->imageByteSize(Data::PhotoSize::Large)) {
			AddDetail(container, Phrase::DetailsSize, Ui::FormatSizeText(bytes));
		}
	}
	if (const auto deleted = item->soulgramDeletedAt()) {
		AddDetail(container, Phrase::DetailsDeleted, FormatFullDate(deleted));
	}
	Ui::AddSkip(container);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void FillSoulgramMenu(
		not_null<Window::SessionController*> controller,
		Dialogs::Key key,
		const Ui::Menu::MenuCallback &addAction) {
	const auto peer = key.peer();
	addAction(Tr(Phrase::JumpToStart), [=] {
		JumpToStart(controller, key);
	}, &st::menuIconShowInChat);
	if (key.topic() || key.sublist()) {
		return;
	}
	if (!peer->isSelf()) {
		addAction(Tr(Phrase::DeletedMessages), [=] {
			controller->show(Box(DeletedMessagesBox, peer));
		}, &st::menuIconRestore);
	}
	if (HasDeleted(&peer->session(), peer->id)) {
		addAction(Tr(Phrase::ClearDeleted), [=] {
			ConfirmClearDeleted(controller, peer);
		}, &st::menuIconClear);
	}
	if (CanShadowBan(peer)) {
		AddShadowBanAction(controller, peer, addAction, false);
	}
	if (CanDeleteMyMessages(peer)) {
		const auto running = DeletingMyMessagesText(peer);
		addAction({
			.text = (running.isEmpty()
				? Tr(Phrase::DeleteMyMessages)
				: running),
			.handler = [=] { DeleteMyMessages(controller, peer); },
			.icon = &st::menuIconDeleteAttention,
			.isAttention = running.isEmpty(),
		});
	}
}

} // namespace

void DeletedMessagesBox(
		not_null<Ui::GenericBox*> box,
		not_null<PeerData*> peer) {
	box->setTitle(TrValue(Phrase::DeletedMessages));
	box->setWidth(st::boxWideWidth);

	struct State {
		std::vector<DeletedRecord> records;
		int shown = 0;
	};
	const auto state = box->lifetime().make_state<State>();
	state->records = LoadDeleted(&peer->session(), peer->id);
	if (state->records.empty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			Tr(Phrase::DeletedMessagesEmpty),
			st::boxLabel));
	} else {
		const auto list = box->addRow(
			object_ptr<Ui::VerticalLayout>(box),
			style::margins(),
			style::al_justify);
		const auto more = box->addRow(
			object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
				box,
				object_ptr<Ui::SettingsButton>(
					box,
					tr::lng_channels_your_more(),
					st::powerSavingButtonNoIcon)),
			style::margins(),
			style::al_justify);
		const auto showMore = [=] {
			const auto &records = state->records;
			const auto count = int(records.size());
			const auto till = std::min(state->shown + kDeletedPerPage, count);
			for (; state->shown != till; ++state->shown) {
				// Records are sorted by id, the newest go first.
				AddDeletedMessage(list, records[count - 1 - state->shown]);
			}
			if (const auto width = list->width()) {
				list->resizeToWidth(width);
			}
			more->toggle(state->shown < count, anim::type::instant);
		};
		more->entity()->setClickedCallback(showMore);
		showMore();
	}
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void FillPeerMenu(
		not_null<Window::SessionController*> controller,
		Dialogs::Key key,
		const Ui::Menu::MenuCallback &addAction) {
	const auto peer = key.peer();
	if (!peer || !key.thread()) {
		return;
	}
	addAction({
		.text = Tr(Phrase::SettingsButton),
		.handler = nullptr,
		.icon = &st::menuIconStealth,
		.fillSubmenu = [=](not_null<Ui::PopupMenu*> menu) {
			FillSoulgramMenu(
				controller,
				key,
				Ui::Menu::CreateAddActionCallback(menu));
		},
	});
}

QString PeerIdString(not_null<PeerData*> peer) {
	if (peer->isUser()) {
		return QString::number(peerToUser(peer->id).bare);
	} else if (peer->isChat()) {
		return u"-"_q + QString::number(peerToChat(peer->id).bare);
	} else if (peer->isChannel()) {
		return u"-100"_q + QString::number(peerToChannel(peer->id).bare);
	}
	return QString::number(peer->id.value);
}

rpl::producer<QString> DataCenterValue(not_null<PeerData*> peer) {
	return peer->session().changes().peerFlagsValue(
		peer,
		Data::PeerUpdate::Flag::Photo
	) | rpl::map([=] {
		const auto location = peer->userpicLocation();
		const auto storage = std::get_if<StorageFileLocation>(
			&location.file().data);
		const auto dc = storage ? storage->dcId() : 0;
		const auto city = (dc == 1 || dc == 3)
			? Tr(Phrase::DcMiami)
			: (dc == 2 || dc == 4)
			? Tr(Phrase::DcAmsterdam)
			: (dc == 5)
			? Tr(Phrase::DcSingapore)
			: QString();
		return city.isEmpty()
			? QString()
			: (u"DC%1 · "_q.arg(dc) + city);
	});
}

Fn<void()> CopyTextCallback(std::shared_ptr<Ui::Show> show, QString text) {
	return [=] {
		QGuiApplication::clipboard()->setText(text);
		show->showToast(Tr(Phrase::IdCopied));
	};
}

void AddMessageActions(
		not_null<Ui::PopupMenu*> menu,
		HistoryItem *item,
		not_null<Window::SessionController*> controller) {
	if (!item || !item->isRegular()) {
		return;
	}
	const auto session = &item->history()->session();
	const auto id = item->fullId();
	menu->addAction(Tr(Phrase::MessageDetails), [=] {
		if (const auto item = session->data().message(id)) {
			controller->show(Box(MessageDetailsBox, item));
		}
	}, &st::menuIconInfo);
	const auto from = item->from();
	if (from != item->history()->peer && !item->out() && CanShadowBan(from)) {
		AddShadowBanAction(
			controller,
			from,
			Ui::Menu::CreateAddActionCallback(menu),
			true);
	}
}

} // namespace Soulgram

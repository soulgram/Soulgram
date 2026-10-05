/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_shot.h"

#include "base/timer.h"
#include "core/file_utilities.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_groups.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "dialogs/ui/dialogs_video_userpic.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "history/view/history_view_element.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "soulgram/soulgram_lang.h"
#include "soulgram/soulgram_settings.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/chat/chat_style.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/userpic_view.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "window/themes/window_theme.h"
#include "window/themes/window_themes_embedded.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>

namespace Soulgram {
namespace {

// Views are laid out wider than any message, empty space is cut off.
constexpr auto kPadding = 12;
constexpr auto kRerenderDelay = crl::time(300);

struct ShotState {
	bool taking = false;
	bool hideDate = false;
	bool hideReactions = false;
	bool hideTags = false;
	bool revealSpoilers = false;
	bool plainReplies = false;
};

[[nodiscard]] ShotState &Shot() {
	static auto result = ShotState();
	return result;
}

// The options apply to the views created and painted while it lives.
class TakingShotGuard final {
public:
	explicit TakingShotGuard(const Settings &settings) {
		Shot() = ShotState{
			.taking = true,
			.hideDate = !settings.shotShowDate,
			.hideReactions = !settings.shotShowReactions,
			.hideTags = !settings.shotShowTags,
			.revealSpoilers = settings.shotRevealSpoilers,
			.plainReplies = !settings.shotColorfulReplies,
		};
	}
	~TakingShotGuard() {
		Shot() = ShotState();
	}

};

// Messages are shown like in a wide chat: all on the left, with userpics.
class Delegate final : public HistoryView::SimpleElementDelegate {
public:
	explicit Delegate(not_null<Window::SessionController*> controller)
	: SimpleElementDelegate(controller, [] {}) {
	}

	HistoryView::Context elementContext() override {
		return HistoryView::Context::AdminLog;
	}
	HistoryView::ElementChatMode elementChatMode() override {
		return HistoryView::ElementChatMode::Wide;
	}
	bool elementAnimationsPaused() override {
		return true;
	}
	bool elementHideReply(
			not_null<const HistoryView::Element*> view) override {
		if (!view->isTopicRootReply()) {
			return false;
		}
		const auto reply = view->data()->Get<HistoryMessageReply>();
		return reply && !reply->fields().manualQuote;
	}

};

struct Userpics {
	base::flat_map<not_null<PeerData*>, Ui::PeerUserpicView> peers;
	base::flat_map<FullMsgId, Ui::PeerUserpicView> hidden;
};

// Media is loaded while the box is open, the shot is painted again.
struct Preload {
	std::vector<std::shared_ptr<Data::PhotoMedia>> photos;
	std::vector<std::shared_ptr<Data::DocumentMedia>> documents;
};

// What the toggles of the box can change in these messages.
struct Features {
	bool reactions = false;
	bool tags = false;
	bool replies = false;
	bool spoilers = false;
};

[[nodiscard]] std::vector<not_null<HistoryItem*>> ResolveItems(
		not_null<Main::Session*> session,
		const std::vector<FullMsgId> &ids) {
	auto result = std::vector<not_null<HistoryItem*>>();
	result.reserve(ids.size());
	for (const auto &id : ids) {
		if (const auto item = session->data().message(id)) {
			result.push_back(item);
		}
	}
	return result;
}

// Whole albums, sorted from the oldest message.
[[nodiscard]] std::vector<FullMsgId> PrepareIds(
		not_null<Main::Session*> session,
		const std::vector<FullMsgId> &ids) {
	auto items = std::vector<not_null<HistoryItem*>>();
	const auto add = [&](not_null<HistoryItem*> item) {
		if (!ranges::contains(items, item)) {
			items.push_back(item);
		}
	};
	for (const auto item : ResolveItems(session, ids)) {
		if (const auto group = session->data().groups().find(item)) {
			for (const auto &part : group->items) {
				add(part);
			}
		} else {
			add(item);
		}
	}
	ranges::stable_sort(items, [](
			not_null<HistoryItem*> a,
			not_null<HistoryItem*> b) {
		return (a->date() != b->date())
			? (a->date() < b->date())
			: (a->id < b->id);
	});
	auto result = std::vector<FullMsgId>();
	result.reserve(items.size());
	for (const auto item : items) {
		result.push_back(item->fullId());
	}
	return result;
}

void PreloadMedia(not_null<HistoryItem*> item, Preload &preload) {
	const auto media = item->media();
	if (!media) {
		return;
	}
	const auto origin = Data::FileOrigin(item->fullId());
	if (const auto photo = media->photo()) {
		auto view = photo->createMediaView();
		if (!view->loaded()) {
			photo->load(origin);
		}
		preload.photos.push_back(std::move(view));
	} else if (const auto document = media->document()) {
		auto view = document->createMediaView();
		if (document->hasThumbnail() && !view->thumbnail()) {
			document->loadThumbnail(origin);
		}
		preload.documents.push_back(std::move(view));
	}
}

[[nodiscard]] Features DetectFeatures(
		const std::vector<not_null<HistoryItem*>> &items) {
	auto result = Features();
	for (const auto item : items) {
		if (!item->reactions().empty()) {
			result.reactions = true;
		}
		const auto peer = item->history()->peer;
		if (peer->isChat() || peer->isMegagroup()) {
			result.tags = true;
		}
		const auto media = item->media();
		if (item->Get<HistoryMessageReply>()
			|| (media && media->webpage())) {
			result.replies = true;
		}
		if (media && (media->hasSpoiler() || item->isMediaSensitive())) {
			result.spoilers = true;
		}
		for (const auto &entity : item->originalText().entities) {
			if (entity.type() == EntityType::Spoiler) {
				result.spoilers = true;
			} else if (entity.type() == EntityType::Blockquote) {
				result.replies = true;
			}
		}
	}
	return result;
}

[[nodiscard]] QString CurrentText(rpl::producer<QString> text) {
	auto result = QString();
	auto lifetime = rpl::lifetime();
	std::move(text) | rpl::on_next([&](const QString &value) {
		result = value;
	}, lifetime);
	return result;
}

struct ThemeOption {
	int id = 0;
	QString name;
};

[[nodiscard]] std::vector<ThemeOption> ThemeOptions() {
	auto result = std::vector<ThemeOption>{
		{ 0, Tr(Phrase::ShotThemeDefault) },
	};
	for (auto &scheme : Window::Theme::EmbeddedThemes()) {
		result.push_back({
			int(scheme.type) + 1,
			CurrentText(std::move(scheme.name)),
		});
	}
	return result;
}

[[nodiscard]] std::unique_ptr<style::palette> ThemePalette(int theme) {
	if (theme <= 0) {
		return nullptr;
	}
	using namespace Window::Theme;
	const auto themes = EmbeddedThemes();
	const auto i = ranges::find(
		themes,
		EmbeddedType(theme - 1),
		&EmbeddedScheme::type);
	if (i == end(themes)) {
		return nullptr;
	}
	auto result = std::make_unique<style::palette>();
	if (i->path.isEmpty()) {
		PreparePaletteCallback(false, std::nullopt)(*result);
		return result;
	}
	auto instance = Window::Theme::Instance();
	if (!LoadFromFile(i->path, &instance, nullptr, nullptr)) {
		return nullptr;
	}
	*result = instance.palette;
	return result;
}

// Replies and quotes keep only their colored line, like in AyuGram.
void MakeRepliesPlain(not_null<const Ui::ChatStyle*> st) {
	const auto clear = [](Ui::Text::QuotePaintCache *cache) {
		if (cache) {
			cache->bg = QColor(0, 0, 0, 0);
		}
	};
	for (const auto selected : { false, true }) {
		for (const auto outbg : { false, true }) {
			const auto &stm = st->messageStyle(outbg, selected);
			for (const auto &cache : stm.quoteCache) {
				clear(cache.get());
			}
			for (const auto &cache : stm.replyCache) {
				clear(cache.get());
			}
		}
		for (auto i = 0; i != Ui::kColorIndexCount; ++i) {
			clear(st->coloredQuoteCache(selected, uint8(i)));
			clear(st->coloredReplyCache(selected, uint8(i)));
		}
	}
}

[[nodiscard]] std::shared_ptr<Ui::ChatStyle> MakeStyle(
		not_null<Window::SessionController*> controller,
		const Settings &settings) {
	auto result = std::make_shared<Ui::ChatStyle>(
		controller->session().colorIndicesValue());
	const auto palette = ThemePalette(settings.shotTheme);
	const auto current = static_cast<const style::palette*>(
		controller->chatStyle().get());
	result->applyCustomPalette(palette ? palette.get() : current);
	if (!settings.shotColorfulReplies) {
		MakeRepliesPlain(result.get());
	}
	return result;
}

void PaintSenderUserpic(
		Painter &p,
		not_null<HistoryView::Element*> view,
		int top,
		int outerWidth,
		Userpics &userpics) {
	const auto item = view->data();
	const auto left = st::historyPhotoLeft;
	const auto size = st::msgPhotoSize;
	if (const auto from = view->displayFrom()) {
		Dialogs::Ui::PaintUserpic(
			p,
			from,
			nullptr,
			userpics.peers[from],
			left,
			top,
			outerWidth,
			size,
			true);
	} else if (const auto info = item->displayHiddenSenderInfo()) {
		if (info->customUserpic.empty()) {
			info->emptyUserpic.paintCircle(p, left, top, outerWidth, size);
		} else {
			auto &userpic = userpics.hidden[item->fullId()];
			const auto valid = info->paintCustomUserpic(
				p,
				userpic,
				left,
				top,
				outerWidth,
				size);
			if (!valid) {
				info->customUserpic.load(
					&item->history()->session(),
					item->fullId());
			}
		}
	}
}

[[nodiscard]] QImage PaintViews(
		not_null<Window::SessionController*> controller,
		const std::vector<not_null<HistoryItem*>> &items,
		not_null<const Ui::ChatStyle*> st,
		Userpics &userpics) {
	const auto guard = TakingShotGuard(Current());

	auto delegate = Delegate(controller);
	auto views = std::vector<std::unique_ptr<HistoryView::Element>>();
	views.reserve(items.size());
	for (const auto item : items) {
		views.push_back(item->createView(&delegate));
	}
	auto previous = (HistoryView::Element*)nullptr;
	for (const auto &view : views) {
		if (view->isHidden()) {
			continue;
		}
		const auto attached = previous
			&& view->computeIsAttachToPrevious(previous);
		view->setAttachToPrevious(attached, previous);
		if (previous) {
			previous->setAttachToNext(attached, view.get());
		}
		previous = view.get();
	}
	if (previous) {
		previous->setAttachToNext(false);
	}

	const auto width = st::msgMaxWidth
		+ 2 * st::msgPhotoSkip
		+ st::msgMargin.left()
		+ st::msgMargin.right();
	auto height = 0;
	for (const auto &view : views) {
		height += view->resizeGetHeight(width);
	}
	if (height <= 0) {
		return QImage();
	}

	const auto ratio = style::DevicePixelRatio();
	auto result = QImage(
		QSize(width, height) * ratio,
		QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(ratio);
	result.fill(Qt::transparent);

	auto p = Painter(&result);
	const auto full = QRect(0, 0, width, height);
	auto top = 0;
	for (const auto &view : views) {
		const auto viewHeight = view->height();
		if (viewHeight > 0) {
			auto context = Ui::ChatPaintContext{
				.st = st,
				.viewport = full,
				.area = full,
				.clip = full,
				.now = crl::now(),
				.outbg = view->hasOutLayout(),
				.paused = true,
			};
			context.translate(0, -top);
			p.translate(0, top);
			view->draw(p, context);
			p.translate(0, -top);
			if (view->displayFromPhoto()) {
				PaintSenderUserpic(
					p,
					view.get(),
					top + viewHeight - view->marginBottom() - st::msgPhotoSize,
					width,
					userpics);
			}
		}
		top += viewHeight;
	}
	p.end();
	return result;
}

// The part of the image that has something painted, in pixels.
[[nodiscard]] QRect PaintedRect(const QImage &image) {
	const auto width = image.width();
	const auto height = image.height();
	auto left = width;
	auto right = -1;
	auto top = height;
	auto bottom = -1;
	for (auto y = 0; y != height; ++y) {
		const auto line = reinterpret_cast<const uint32*>(
			image.constScanLine(y));
		auto first = 0;
		while (first != width && !(line[first] >> 24)) {
			++first;
		}
		if (first == width) {
			continue;
		}
		auto last = width - 1;
		while (!(line[last] >> 24)) {
			--last;
		}
		left = std::min(left, first);
		right = std::max(right, last);
		top = std::min(top, y);
		bottom = y;
	}
	return (right < 0)
		? QRect()
		: QRect(left, top, right - left + 1, bottom - top + 1);
}

[[nodiscard]] QColor BackgroundColor(not_null<const Ui::ChatStyle*> st) {
	const auto color = st->windowBg()->c;
	return st->dark() ? color.lighter(175) : color.darker(110);
}

[[nodiscard]] QImage RenderShot(
		not_null<Window::SessionController*> controller,
		const std::vector<not_null<HistoryItem*>> &items,
		Userpics &userpics) {
	const auto &settings = Current();
	const auto st = MakeStyle(controller, settings);
	const auto painted = PaintViews(controller, items, st.get(), userpics);
	if (painted.isNull()) {
		return QImage();
	}
	const auto rect = PaintedRect(painted);
	if (rect.isEmpty()) {
		return QImage();
	}
	const auto ratio = style::DevicePixelRatio();
	const auto padding = style::ConvertScale(kPadding);
	auto result = QImage(
		rect.size() + QSize(2 * padding, 2 * padding) * ratio,
		QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(ratio);
	if (settings.shotShowBackground) {
		result.fill(BackgroundColor(st.get()));
	} else {
		result.fill(Qt::transparent);
	}
	auto p = QPainter(&result);
	p.drawImage(QPoint(padding, padding), painted, rect);
	p.end();
	return result;
}

class PreviewWidget final : public Ui::RpWidget {
public:
	using RpWidget::RpWidget;

	void setImage(QImage image) {
		_image = std::move(image);
		resizeToWidth(width());
		update();
	}

protected:
	int resizeGetHeight(int newWidth) override {
		return scaledSize(newWidth).height();
	}

	void paintEvent(QPaintEvent *e) override {
		if (_image.isNull()) {
			return;
		}
		auto p = QPainter(this);
		auto hq = PainterHighQualityEnabler(p);
		const auto size = scaledSize(width());
		p.drawImage(
			QRect(QPoint((width() - size.width()) / 2, 0), size),
			_image);
	}

private:
	[[nodiscard]] QSize scaledSize(int available) const {
		if (_image.isNull() || available <= 0) {
			return QSize();
		}
		const auto ratio = _image.devicePixelRatio();
		const auto original = QSize(
			int(_image.width() / ratio),
			int(_image.height() / ratio));
		if (original.width() <= available) {
			return original;
		}
		return QSize(
			available,
			original.height() * available / original.width());
	}

	QImage _image;

};

void ShotBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		std::vector<FullMsgId> ids,
		Fn<void()> done) {
	struct State {
		Userpics userpics;
		Preload preload;
		std::vector<ThemeOption> themes;
		std::vector<QString> themeNames;
		QImage image;
		base::Timer rerender;
	};
	const auto session = &controller->session();
	const auto state = box->lifetime().make_state<State>();
	const auto items = ResolveItems(session, ids);
	for (const auto item : items) {
		PreloadMedia(item, state->preload);
		if (const auto from = item->displayFrom()) {
			from->loadUserpic();
		}
	}
	state->themes = ThemeOptions();
	for (const auto &option : state->themes) {
		state->themeNames.push_back(option.name);
	}

	box->setTitle(TrValue(Phrase::Shot));
	const auto container = box->verticalLayout();
	Ui::AddSubsectionTitle(container, TrValue(Phrase::ShotPreview));
	const auto preview = container->add(
		object_ptr<PreviewWidget>(container),
		st::boxRowPadding);

	const auto render = [=] {
		const auto list = ResolveItems(session, ids);
		if (list.empty()) {
			return;
		}
		state->image = RenderShot(controller, list, state->userpics);
		preview->setImage(state->image);
	};
	state->rerender.setCallback(render);
	session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		state->rerender.callOnce(kRerenderDelay);
	}, box->lifetime());

	Ui::AddSkip(container);
	Ui::AddDivider(container);
	Ui::AddSkip(container);
	Ui::AddSubsectionTitle(container, TrValue(Phrase::ShotSettings));

	const auto themeName = [=] {
		const auto id = Current().shotTheme;
		for (const auto &option : state->themes) {
			if (option.id == id) {
				return option.name;
			}
		}
		return Tr(Phrase::ShotThemeDefault);
	};
	auto themeLabel = rpl::single(
		rpl::empty
	) | rpl::then(
		Changes()
	) | rpl::map(themeName);
	::Settings::AddButtonWithLabel(
		container,
		TrValue(Phrase::ShotTheme),
		std::move(themeLabel),
		st::settingsButtonNoIcon
	)->setClickedCallback([=] {
		const auto id = Current().shotTheme;
		const auto i = ranges::find(state->themes, id, &ThemeOption::id);
		const auto selected = (i == end(state->themes))
			? 0
			: int(i - begin(state->themes));
		controller->show(Box(SingleChoiceBox, SingleChoiceBoxArgs{
			.title = TrValue(Phrase::ShotTheme),
			.options = state->themeNames,
			.initialSelection = selected,
			.callback = crl::guard(box, [=](int index) {
				if (index < 0 || index >= int(state->themes.size())) {
					return;
				}
				Change([&](Settings &settings) {
					settings.shotTheme = state->themes[index].id;
				});
				render();
			}),
		}));
	});

	const auto addToggle = [=](Phrase phrase, bool Settings::*member) {
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
			render();
		}, button->lifetime());
	};
	const auto features = DetectFeatures(items);
	addToggle(Phrase::ShotShowBackground, &Settings::shotShowBackground);
	addToggle(Phrase::ShotShowDate, &Settings::shotShowDate);
	if (features.reactions) {
		addToggle(Phrase::ShotShowReactions, &Settings::shotShowReactions);
	}
	if (features.tags) {
		addToggle(Phrase::ShotShowTags, &Settings::shotShowTags);
	}
	if (features.replies) {
		addToggle(Phrase::ShotColorfulReplies, &Settings::shotColorfulReplies);
	}
	if (features.spoilers) {
		addToggle(Phrase::ShotRevealSpoilers, &Settings::shotRevealSpoilers);
	}
	Ui::AddSkip(container);

	const auto finish = [=](Phrase toast) {
		controller->showToast(Tr(toast));
		box->closeBox();
		if (done) {
			done();
		}
	};
	box->addButton(TrValue(Phrase::ShotSave), [=] {
		const auto image = state->image;
		if (image.isNull()) {
			return;
		}
		FileDialog::GetWritePath(
			box.get(),
			tr::lng_save_file(tr::now),
			u"PNG Image (*.png)"_q,
			filedialogDefaultName(u"shot"_q, u".png"_q),
			crl::guard(box, [=](const QString &path) {
				if (!path.isEmpty() && image.save(path, "PNG")) {
					finish(Phrase::ShotSaved);
				}
			}));
	});
	box->addButton(TrValue(Phrase::ShotCopy), [=] {
		if (state->image.isNull()) {
			return;
		}
		QGuiApplication::clipboard()->setImage(state->image);
		finish(Phrase::ShotCopied);
	});

	render();
	const auto ratio = state->image.isNull()
		? 1
		: state->image.devicePixelRatio();
	const auto imageWidth = int(state->image.width() / ratio);
	box->setWidth(std::clamp(
		imageWidth + st::boxRowPadding.left() + st::boxRowPadding.right(),
		st::boxWideWidth,
		st::boxWideWidth * 2));
}

} // namespace

bool TakingShot() {
	return Shot().taking;
}

bool ShotHides(ShotPart part) {
	const auto &state = Shot();
	switch (part) {
	case ShotPart::Date: return state.taking && state.hideDate;
	case ShotPart::Reactions: return state.taking && state.hideReactions;
	case ShotPart::Tags: return state.taking && state.hideTags;
	}
	return false;
}

bool ShotRevealsSpoilers() {
	const auto &state = Shot();
	return state.taking && state.revealSpoilers;
}

bool ShotPlainReplies() {
	const auto &state = Shot();
	return state.taking && state.plainReplies;
}

void ShowMessageShot(
		not_null<Window::SessionController*> controller,
		const std::vector<FullMsgId> &ids,
		Fn<void()> done) {
	auto list = PrepareIds(&controller->session(), ids);
	if (list.empty()) {
		return;
	}
	controller->show(Box(ShotBox, controller, std::move(list), done));
}

} // namespace Soulgram

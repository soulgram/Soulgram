/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_view_once.h"

#include "base/unixtime.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "soulgram/soulgram_settings.h"
#include "soulgram/soulgram_storage.h"

namespace Soulgram {
namespace {

class Saver final {
public:
	explicit Saver(not_null<Main::Session*> session);

	void save(not_null<HistoryItem*> item);

private:
	struct PendingPhoto {
		std::shared_ptr<Data::PhotoMedia> media;
		QString path;
	};

	void checkPhotos();

	const not_null<Main::Session*> _session;
	std::vector<PendingPhoto> _photos;
	base::flat_set<FullMsgId> _started;
	rpl::lifetime _lifetime;

};

[[nodiscard]] QString SafeFileName(QString name) {
	static const auto kBad = QRegularExpression(u"[\\\\/:*?\"<>|\\x00-\\x1F]"_q);
	name.replace(kBad, u"_"_q);
	name = name.trimmed();
	if (name.size() > 64) {
		name = name.left(64).trimmed();
	}
	return name.isEmpty() ? u"chat"_q : name;
}

[[nodiscard]] QString KindName(not_null<Data::Media*> media) {
	if (media->photo()) {
		return u"photo"_q;
	} else if (const auto document = media->document()) {
		return document->isVoiceMessage()
			? u"voice"_q
			: document->isVideoMessage()
			? u"round"_q
			: u"video"_q;
	}
	return u"media"_q;
}

[[nodiscard]] QString Extension(not_null<DocumentData*> document) {
	const auto name = document->filename();
	const auto dot = name.lastIndexOf('.');
	if (dot >= 0 && dot + 1 < name.size() && name.size() - dot <= 6) {
		return name.mid(dot);
	}
	const auto mime = document->mimeString().toLower();
	if (document->isVoiceMessage() || mime.contains(u"ogg"_q)) {
		return u".ogg"_q;
	} else if (mime.startsWith(u"image/"_q)) {
		return u".jpg"_q;
	} else if (mime.startsWith(u"audio/"_q)) {
		return u".mp3"_q;
	}
	return u".mp4"_q;
}

// "<chat> 2026-09-30 21-15-03 photo id123"
[[nodiscard]] QString BaseName(
		not_null<HistoryItem*> item,
		not_null<Data::Media*> media) {
	const auto date = base::unixtime::parse(item->date());
	return SafeFileName(item->history()->peer->name())
		+ ' '
		+ date.toString(u"yyyy-MM-dd HH-mm-ss"_q)
		+ ' '
		+ KindName(media)
		+ u" id"_q
		+ QString::number(item->id.bare);
}

[[nodiscard]] base::flat_map<
	not_null<Main::Session*>,
	std::unique_ptr<Saver>> &Savers() {
	static auto result = base::flat_map<
		not_null<Main::Session*>,
		std::unique_ptr<Saver>>();
	return result;
}

[[nodiscard]] not_null<Saver*> SaverFor(not_null<Main::Session*> session) {
	auto &savers = Savers();
	const auto i = savers.find(session);
	if (i != end(savers)) {
		return i->second.get();
	}
	session->lifetime().add([=] {
		Savers().remove(session);
	});
	return savers.emplace(
		session,
		std::make_unique<Saver>(session)
	).first->second.get();
}

Saver::Saver(not_null<Main::Session*> session)
: _session(session) {
	_session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		checkPhotos();
	}, _lifetime);
}

void Saver::save(not_null<HistoryItem*> item) {
	const auto media = item->media();
	if (!media || !_started.emplace(item->fullId()).second) {
		return;
	}
	const auto folder = SavedMediaFolder(_session);
	if (!QDir().mkpath(folder)) {
		LOG(("Soulgram Error: could not create '%1'.").arg(folder));
		return;
	}
	const auto base = folder + BaseName(item, media);
	const auto origin = Data::FileOrigin(item->fullId());
	if (const auto photo = media->photo()) {
		const auto path = base + u".jpg"_q;
		if (QFile::exists(path)) {
			return;
		}
		auto view = photo->createMediaView();
		view->wanted(Data::PhotoSize::Large, origin);
		_photos.push_back({ std::move(view), path });
		checkPhotos();
	} else if (const auto document = media->document()) {
		const auto path = base + Extension(document);
		if (QFile::exists(path)) {
			return;
		}
		document->save(origin, path);
	}
}

void Saver::checkPhotos() {
	for (auto i = begin(_photos); i != end(_photos);) {
		if (!i->media->loaded()) {
			++i;
			continue;
		}
		if (!i->media->saveToFile(i->path)) {
			LOG(("Soulgram Error: could not save '%1'.").arg(i->path));
		}
		i = _photos.erase(i);
	}
}

} // namespace

bool KeepSelfDestructingMedia() {
	return Current().keepSelfDestructing;
}

void OnSelfDestructingMedia(not_null<HistoryItem*> item) {
	if (item->out() || !Current().saveSelfDestructingToDisk) {
		return;
	}
	// The item is still being constructed, save it a bit later.
	const auto session = &item->history()->session();
	const auto id = item->fullId();
	crl::on_main(session, [=] {
		const auto item = session->data().message(id);
		const auto media = item ? item->media() : nullptr;
		if (media && media->ttlSeconds()) {
			SaverFor(session)->save(item);
		}
	});
}

QString SavedMediaFolder(not_null<Main::Session*> session) {
	return AccountFolder(session) + u"view_once/"_q;
}

} // namespace Soulgram

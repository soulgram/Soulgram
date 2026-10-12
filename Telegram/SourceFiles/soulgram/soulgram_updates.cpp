/*
This file is part of Soulgram,
an unofficial fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "soulgram/soulgram_updates.h"

#include "base/timer.h"
#include "core/application.h"
#include "core/version.h"
#include "settings.h"
#include "soulgram/soulgram_settings.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Soulgram {
namespace {

constexpr auto kReleaseUrl
	= "https://api.github.com/repos/soulgram/Soulgram/releases/latest";
constexpr auto kInstallerPrefix = "Soulgram-setup-";
constexpr auto kFirstCheckDelay = crl::time(15 * 1000);
constexpr auto kCheckInterval = crl::time(6 * 60 * 60 * 1000);
constexpr auto kRetryDelay = crl::time(30 * 60 * 1000);
constexpr auto kMaxReleaseSize = 1024 * 1024;
constexpr auto kMaxInstallerSize = qint64(512) * 1024 * 1024;

// A test build may pretend to be older to try the whole update.
[[nodiscard]] QString TestVersion() {
	return qEnvironmentVariable("SOULGRAM_UPDATE_TEST_VERSION");
}

[[nodiscard]] QString CurrentVersion() {
	const auto test = TestVersion();
	return test.isEmpty() ? QString::fromLatin1(SoulgramVersionStr) : test;
}

// Only the copy put by the installer is updated by it.
[[nodiscard]] bool Installed() {
	return QFile::exists(cExeDir() + u"unins000.exe"_q);
}

[[nodiscard]] bool Newer(const QString &version, const QString &than) {
	const auto a = version.split('.');
	const auto b = than.split('.');
	for (auto i = 0, count = int(std::max(a.size(), b.size())); i != count; ++i) {
		const auto x = (i < a.size()) ? a[i].toInt() : 0;
		const auto y = (i < b.size()) ? b[i].toInt() : 0;
		if (x != y) {
			return x > y;
		}
	}
	return false;
}

[[nodiscard]] QString UpdateFolder() {
	return cWorkingDir() + u"soulgram/update/"_q;
}

[[nodiscard]] QNetworkRequest MakeRequest(const QString &url) {
	auto result = QNetworkRequest(QUrl(url));
	result.setRawHeader(
		"User-Agent",
		"Soulgram/" + QByteArray(SoulgramVersionStr));
	result.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	return result;
}

class Updater final {
public:
	Updater();

	[[nodiscard]] rpl::producer<UpdateState> stateValue() const {
		return _state.value();
	}
	[[nodiscard]] QString version() const {
		return _version;
	}
	void check();
	bool install();

private:
	[[nodiscard]] bool enabled() const;
	void schedule(crl::time delay);
	void gotRelease(const QByteArray &data);
	void download(const QString &url, const QString &name, qint64 size);
	void downloaded();
	void finish(UpdateState state, crl::time next);
	void clearFolder();

	QNetworkAccessManager _manager;
	base::Timer _timer;
	rpl::variable<UpdateState> _state = UpdateState::Idle;
	QPointer<QNetworkReply> _reply;
	std::unique_ptr<QFile> _file;
	QString _version;
	QString _installer;
	qint64 _size = 0;
	rpl::lifetime _lifetime;

};

Updater::Updater() : _timer([=] { check(); }) {
	if (enabled()) {
		schedule(kFirstCheckDelay);
	} else {
		_state = UpdateState::Disabled;
	}
	Changes() | rpl::on_next([=] {
		const auto state = _state.current();
		if (!enabled()) {
			if (state != UpdateState::Ready) {
				_timer.cancel();
				_state = UpdateState::Disabled;
			}
		} else if (state == UpdateState::Disabled) {
			_state = UpdateState::Idle;
			schedule(kFirstCheckDelay);
		}
	}, _lifetime);
}

bool Updater::enabled() const {
	return Current().autoUpdate && (Installed() || !TestVersion().isEmpty());
}

void Updater::schedule(crl::time delay) {
	_timer.callOnce(delay);
}

void Updater::check() {
	const auto state = _state.current();
	if (state == UpdateState::Checking
		|| state == UpdateState::Downloading
		|| state == UpdateState::Ready) {
		return;
	}
	_timer.cancel();
	_state = UpdateState::Checking;
	const auto reply = _manager.get(MakeRequest(QString::fromLatin1(kReleaseUrl)));
	_reply = reply;
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		reply->deleteLater();
		if (_reply != reply) {
			return;
		}
		_reply = nullptr;
		if (reply->error() != QNetworkReply::NoError) {
			LOG(("Soulgram Error: update check failed, %1."
				).arg(reply->errorString()));
			finish(UpdateState::Failed, kRetryDelay);
			return;
		}
		gotRelease(reply->read(kMaxReleaseSize));
	});
}

void Updater::gotRelease(const QByteArray &data) {
	const auto release = QJsonDocument::fromJson(data).object();
	auto version = release.value(u"tag_name"_q).toString();
	if (version.startsWith('v')) {
		version = version.mid(1);
	}
	if (version.isEmpty()) {
		finish(UpdateState::Failed, kRetryDelay);
		return;
	} else if (!Newer(version, CurrentVersion())) {
		clearFolder();
		finish(UpdateState::Latest, kCheckInterval);
		return;
	}
	const auto prefix = QString::fromLatin1(kInstallerPrefix);
	for (const auto &value : release.value(u"assets"_q).toArray()) {
		const auto asset = value.toObject();
		const auto name = asset.value(u"name"_q).toString();
		if (!name.startsWith(prefix) || !name.endsWith(u".exe"_q)) {
			continue;
		}
		const auto size = qint64(asset.value(u"size"_q).toDouble());
		const auto url = asset.value(u"browser_download_url"_q).toString();
		if (size <= 0 || size > kMaxInstallerSize || url.isEmpty()) {
			break;
		}
		_version = version;
		download(url, name, size);
		return;
	}
	// A release without the installer yet.
	finish(UpdateState::Latest, kCheckInterval);
}

void Updater::download(const QString &url, const QString &name, qint64 size) {
	const auto path = UpdateFolder() + name;
	_installer = path;
	_size = size;
	if (QFileInfo(path).size() == size) {
		_state = UpdateState::Ready;
		return;
	}
	clearFolder();
	QDir().mkpath(UpdateFolder());
	_file = std::make_unique<QFile>(path + u".part"_q);
	if (!_file->open(QIODevice::WriteOnly)) {
		LOG(("Soulgram Error: could not write '%1'.").arg(_file->fileName()));
		_file = nullptr;
		finish(UpdateState::Failed, kRetryDelay);
		return;
	}
	_state = UpdateState::Downloading;
	const auto reply = _manager.get(MakeRequest(url));
	_reply = reply;
	QObject::connect(reply, &QNetworkReply::readyRead, [=] {
		if (_reply == reply && _file) {
			_file->write(reply->readAll());
		}
	});
	QObject::connect(reply, &QNetworkReply::finished, [=] {
		reply->deleteLater();
		if (_reply != reply || !_file) {
			return;
		}
		_reply = nullptr;
		_file->write(reply->readAll());
		if (reply->error() != QNetworkReply::NoError) {
			LOG(("Soulgram Error: update download failed, %1."
				).arg(reply->errorString()));
			_file->close();
			_file->remove();
			_file = nullptr;
			finish(UpdateState::Failed, kRetryDelay);
			return;
		}
		downloaded();
	});
}

void Updater::downloaded() {
	const auto part = _file->fileName();
	_file->close();
	_file = nullptr;
	if (QFileInfo(part).size() != _size) {
		LOG(("Soulgram Error: the downloaded update has a wrong size."));
		QFile::remove(part);
		finish(UpdateState::Failed, kRetryDelay);
		return;
	}
	QFile::remove(_installer);
	if (!QFile::rename(part, _installer)) {
		QFile::remove(part);
		finish(UpdateState::Failed, kRetryDelay);
		return;
	}
	_state = UpdateState::Ready;
}

void Updater::finish(UpdateState state, crl::time next) {
	_state = state;
	if (enabled()) {
		schedule(next);
	}
}

void Updater::clearFolder() {
	if (QDir(UpdateFolder()).exists()) {
		QDir(UpdateFolder()).removeRecursively();
	}
}

bool Updater::install() {
	if (_state.current() != UpdateState::Ready || !Installed()) {
		return false;
	}
	const auto started = QProcess::startDetached(_installer, {
		u"/SILENT"_q,
		u"/SUPPRESSMSGBOXES"_q,
		u"/NORESTART"_q,
	});
	if (!started) {
		LOG(("Soulgram Error: could not run '%1'.").arg(_installer));
		return false;
	}
	Core::Quit();
	return true;
}

// Never destroyed: it lives till the process ends.
[[nodiscard]] Updater &Instance() {
	static const auto result = new Updater();
	return *result;
}

} // namespace

rpl::producer<UpdateState> UpdateStateValue() {
	return Instance().stateValue();
}

rpl::producer<bool> UpdateReadyValue() {
	return Instance().stateValue() | rpl::map([](UpdateState state) {
		return (state == UpdateState::Ready);
	}) | rpl::distinct_until_changed();
}

QString UpdateVersion() {
	return Instance().version();
}

void CheckForUpdates() {
	Instance().check();
}

bool InstallUpdate() {
	return Instance().install();
}

} // namespace Soulgram

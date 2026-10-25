#include "remoteinstaller.h"

#include "paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace {
constexpr qint64 maxDownload = 128LL * 1024 * 1024;
constexpr qint64 maxExtracted = 512LL * 1024 * 1024;

QString safeId(QString id)
{
    id.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("-"));
    return id.isEmpty() ? QStringLiteral("remote") : id;
}

bool runTool(const QString &program, const QStringList &args, QString *output = nullptr, int timeoutMs = 60000)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(5000))
        return false;
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        return false;
    }
    if (output) {
        *output = QString::fromUtf8(process.readAllStandardOutput())
            + QString::fromUtf8(process.readAllStandardError());
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool safeArchivePath(const QString &raw)
{
    QString path = raw.trimmed();
    if (path.isEmpty())
        return true;
    if (path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char('\\')))
        return false;
    if (QRegularExpression(QStringLiteral("^[A-Za-z]:")).match(path).hasMatch())
        return false;

    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (part == QLatin1String(".."))
            return false;
    }
    return true;
}

int candidateRank(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("colors"))
        return 0;
    if (suffix == QLatin1String("json"))
        return 1;
    if (suffix == QLatin1String("txt"))
        return 2;
    return 9;
}

bool isSupportedPaletteFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("colors") || suffix == QLatin1String("json")
        || suffix == QLatin1String("txt");
}
} // namespace

RemoteInstaller::RemoteInstaller(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

void RemoteInstaller::install(const QVariantMap &theme)
{
    if (m_busy) {
        emit failed(QStringLiteral("Another theme download is already running."));
        return;
    }

    m_theme = theme;
    m_name = theme.value(QStringLiteral("name")).toString();
    m_id = safeId(theme.value(QStringLiteral("id")).toString());
    m_root = Paths::cacheDir() + QStringLiteral("/remote/") + m_id;
    m_archive = m_root + QStringLiteral("/download");
    m_finished = false;

    QDir(m_root).removeRecursively();
    if (!QDir().mkpath(m_root)) {
        fail(QStringLiteral("Could not create a download directory."));
        return;
    }

    QString url = theme.value(QStringLiteral("downloadUrl")).toString();
    if (url.isEmpty() && theme.value(QStringLiteral("source")).toString() == QLatin1String("github")) {
        const QString repo = theme.value(QStringLiteral("repo")).toString();
        QString branch = theme.value(QStringLiteral("defaultBranch")).toString();
        if (branch.isEmpty())
            branch = QStringLiteral("main");
        if (repo.isEmpty()) {
            fail(QStringLiteral("This GitHub entry has no repository name."));
            return;
        }
        url = QStringLiteral("https://github.com/%1/archive/refs/heads/%2.zip")
                  .arg(repo, QString::fromLatin1(QUrl::toPercentEncoding(branch)));
    }

    if (url.isEmpty()) {
        fail(QStringLiteral("This entry has no download link. Open its source page instead."));
        return;
    }
    if (url.startsWith(QLatin1String("//")))
        url.prepend(QStringLiteral("https:"));

    startDownload(QUrl(url));
}

void RemoteInstaller::cancel()
{
    if (!m_busy)
        return;
    fail(QStringLiteral("Download cancelled."));
}

void RemoteInstaller::startDownload(const QUrl &url)
{
    if (!url.isValid() || url.scheme().isEmpty()) {
        fail(QStringLiteral("The download link is not a valid URL."));
        return;
    }

    m_file = new QFile(m_archive, this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(QStringLiteral("Could not create the archive file."));
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("HyperThemes/0.1"));

    setBusy(true);
    setStatus(QStringLiteral("Downloading %1…").arg(m_name));
    m_reply = m_manager->get(request);
    m_reply->setReadBufferSize(64 * 1024);

    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (!m_reply || !m_file)
            return;
        const QByteArray chunk = m_reply->readAll();
        if (m_file->size() + chunk.size() > maxDownload) {
            fail(QStringLiteral("The download is larger than 128 MB."));
            return;
        }
        m_file->write(chunk);
    });

    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0)
            setStatus(QStringLiteral("Downloading %1… %2%").arg(m_name).arg(100 * received / total));
    });

    connect(m_reply, &QNetworkReply::finished, this, &RemoteInstaller::finishDownload);
}

void RemoteInstaller::finishDownload()
{
    if (m_finished || !m_reply)
        return;

    const QNetworkReply::NetworkError error = m_reply->error();
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorText = m_reply->errorString();

    if (m_file) {
        m_file->write(m_reply->readAll());
        m_file->flush();
        m_file->close();
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    if (error != QNetworkReply::NoError) {
        fail(QStringLiteral("Download failed: %1").arg(errorText));
        return;
    }
    if (status >= 400) {
        fail(QStringLiteral("Download failed with HTTP %1.").arg(status));
        return;
    }
    if (!m_file || m_file->size() == 0) {
        fail(QStringLiteral("The downloaded file is empty."));
        return;
    }
    if (m_file->size() > maxDownload) {
        fail(QStringLiteral("The download is larger than 128 MB."));
        return;
    }

    processDownload();
}

void RemoteInstaller::processDownload()
{
    const QString tar = QStandardPaths::findExecutable(QStringLiteral("bsdtar"));
    if (tar.isEmpty()) {
        fail(QStringLiteral("bsdtar is required to unpack remote themes."));
        return;
    }

    QString listing;
    const bool isArchive = runTool(tar, {QStringLiteral("-tf"), m_archive}, &listing, 30000);
    if (!isArchive) {
        QString name = m_theme.value(QStringLiteral("downloadName")).toString();
        if (name.isEmpty())
            name = QUrl(m_theme.value(QStringLiteral("downloadUrl")).toString()).fileName();
        if (isSupportedPaletteFile(name)) {
            const QString stableRoot = Paths::themesDir() + QLatin1Char('/') + m_id;
            QDir(stableRoot).removeRecursively();
            QDir().mkpath(stableRoot);
            const QString stable = stableRoot + QLatin1Char('/') + QFileInfo(name).fileName();
            QFile::remove(stable);
            if (!QFile::copy(m_archive, stable)) {
                fail(QStringLiteral("Could not save the downloaded theme file."));
                return;
            }
            finishWithFiles({stable});
            return;
        }
        fail(QStringLiteral("The download is not an archive or a supported palette file."));
        return;
    }

    QStringList lines = listing.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() > 20000) {
        fail(QStringLiteral("The archive contains too many files."));
        return;
    }
    for (const QString &line : std::as_const(lines)) {
        if (!safeArchivePath(line)) {
            fail(QStringLiteral("The archive contains an unsafe path."));
            return;
        }
    }

    QString verbose;
    if (!runTool(tar, {QStringLiteral("-tvf"), m_archive}, &verbose, 30000)) {
        fail(QStringLiteral("The archive could not be inspected safely."));
        return;
    }
    const QStringList entries = verbose.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        if (entry.startsWith(QLatin1Char('l')) || entry.startsWith(QLatin1Char('h'))) {
            fail(QStringLiteral("The archive contains a symbolic link."));
            return;
        }
    }

    const QString unpacked = m_root + QStringLiteral("/unpacked");
    QDir(unpacked).removeRecursively();
    if (!QDir().mkpath(unpacked)) {
        fail(QStringLiteral("Could not create an extraction directory."));
        return;
    }
    if (!runTool(tar, {QStringLiteral("-xf"), m_archive, QStringLiteral("-C"), unpacked}, nullptr, 120000)) {
        fail(QStringLiteral("The archive could not be extracted."));
        return;
    }

    collectFiles(unpacked, Paths::themesDir() + QLatin1Char('/') + m_id);
}

void RemoteInstaller::collectFiles(const QString &root, const QString &stableRoot)
{
    QStringList found;
    qint64 totalSize = 0;
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && found.size() < 200) {
        const QString path = it.next();
        const QFileInfo info = it.fileInfo();
        if (info.isSymLink() || !info.isFile())
            continue;
        totalSize += info.size();
        if (totalSize > maxExtracted) {
            fail(QStringLiteral("The extracted theme is larger than 512 MB."));
            return;
        }
        if (isSupportedPaletteFile(path))
            found << path;
    }

    if (found.isEmpty()) {
        fail(QStringLiteral("No supported palette file was found in the archive."));
        return;
    }

    std::sort(found.begin(), found.end(), [](const QString &a, const QString &b) {
        const int rankA = candidateRank(a);
        const int rankB = candidateRank(b);
        if (rankA != rankB)
            return rankA < rankB;
        return a < b;
    });

    QDir(stableRoot).removeRecursively();
    if (!QDir().mkpath(stableRoot)) {
        fail(QStringLiteral("Could not create the installed theme directory."));
        return;
    }

    QStringList installed;
    for (const QString &source : std::as_const(found)) {
        const QString relative = QDir(root).relativeFilePath(source);
        if (relative.startsWith(QLatin1String("../")))
            continue;
        const QString destination = stableRoot + QLatin1Char('/') + relative;
        QDir().mkpath(QFileInfo(destination).absolutePath());
        QFile::remove(destination);
        if (QFile::copy(source, destination))
            installed << destination;
    }

    finishWithFiles(installed);
}

void RemoteInstaller::finishWithFiles(const QStringList &files)
{
    if (files.isEmpty()) {
        fail(QStringLiteral("No usable palette file was found."));
        return;
    }

    m_finished = true;
    if (m_file) {
        m_file->close();
        m_file->deleteLater();
        m_file = nullptr;
    }
    setBusy(false);
    setStatus(QStringLiteral("Found %1 theme file(s) for %2").arg(files.size()).arg(m_name));
    QDir(m_root).removeRecursively();

    emit finished({
        {QStringLiteral("ok"), true},
        {QStringLiteral("name"), m_name},
        {QStringLiteral("id"), m_id},
        {QStringLiteral("files"), files},
    });
}

void RemoteInstaller::fail(const QString &message)
{
    if (m_finished)
        return;
    m_finished = true;

    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        m_file->close();
        m_file->remove();
        m_file->deleteLater();
        m_file = nullptr;
    }
    QDir(m_root).removeRecursively();

    setBusy(false);
    setStatus(message);
    emit failed(message);
}

void RemoteInstaller::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void RemoteInstaller::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

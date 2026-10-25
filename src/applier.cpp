#include "applier.h"

#include "palette.h"
#include "paths.h"
#include "templaterenderer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

namespace {
QString safeId(QString id)
{
    id.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("-"));
    return id;
}

bool readFile(const QString &path, QString *content)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    *content = QString::fromUtf8(file.readAll());
    return true;
}
} // namespace

Applier::Applier(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(Paths::backupsDir());
}

QString Applier::timestampId(const QString &themeId)
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
        + QLatin1Char('-') + safeId(themeId);
}

QString Applier::sessionDir(const QString &id)
{
    return Paths::backupsDir() + QLatin1Char('/') + id;
}

void Applier::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

bool Applier::run(const QString &program, const QStringList &args, int timeoutMs, QString *output)
{
    if (QStandardPaths::findExecutable(program).isEmpty())
        return false;

    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(5000))
        return false;
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(2000);
        return false;
    }
    if (output)
        *output = QString::fromUtf8(process.readAll());
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool Applier::runPrivileged(const QStringList &args, int timeoutMs, QString *output)
{
    return run(QStringLiteral("pkexec"), args, timeoutMs, output);
}

bool Applier::backupFile(Session &session, const QString &path, bool system)
{
    for (const Entry &entry : std::as_const(session.entries)) {
        if (entry.path == path)
            return true;
    }

    Entry entry;
    entry.path = path;
    entry.existed = QFileInfo::exists(path);
    if (entry.existed) {
        entry.backupRel = QStringLiteral("files/%1").arg(session.entries.size());
        const QString destination = session.dir + QLatin1Char('/') + entry.backupRel;
        if (!QFile::copy(path, destination)) {
            if (!system || !runPrivileged({QStringLiteral("cp"), path, destination}, 60000)) {
                session.warnings << QStringLiteral("Could not back up %1").arg(path);
                return false;
            }
        }
    }
    session.entries.append(entry);
    return true;
}

bool Applier::writeFile(Session &session, const QString &path, const QByteArray &data, bool system)
{
    if (!backupFile(session, path, system))
        return false;

    QDir().mkpath(QFileInfo(path).absolutePath());

    if (system) {
        const QString temporary = QDir::tempPath() + QStringLiteral("/hyperthemes-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        QFile file(temporary);
        if (!file.open(QIODevice::WriteOnly)) {
            session.warnings << QStringLiteral("Could not create a temporary file for %1").arg(path);
            return false;
        }
        if (file.write(data) != data.size() || !file.flush()) {
            file.close();
            QFile::remove(temporary);
            session.warnings << QStringLiteral("Could not write the temporary file for %1").arg(path);
            return false;
        }
        file.close();
        const bool ok = runPrivileged({QStringLiteral("install"), QStringLiteral("-Dm644"), temporary, path}, 60000);
        QFile::remove(temporary);
        if (!ok) {
            session.warnings << QStringLiteral("System write failed for %1").arg(path);
            return false;
        }
    } else {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            session.warnings << QStringLiteral("Could not write %1").arg(path);
            return false;
        }
        if (file.write(data) != data.size() || !file.commit()) {
            session.warnings << QStringLiteral("Could not commit %1").arg(path);
            return false;
        }
    }

    session.applied << path;
    return true;
}

bool Applier::ensureInclude(Session &session, const QString &path, const QString &line)
{
    QString content;
    const bool exists = QFileInfo::exists(path);
    if (exists && !readFile(path, &content)) {
        session.warnings << QStringLiteral("Could not read %1").arg(path);
        return false;
    }
    if (content.contains(line))
        return true;
    content = line + QLatin1Char('\n') + content;
    return writeFile(session, path, content.toUtf8(), false);
}

bool Applier::setQtScheme(Session &session, const QString &path)
{
    if (!backupFile(session, path, false))
        return false;
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QStringLiteral("Appearance/color_scheme"), QStringLiteral("HyperThemes"));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        session.warnings << QStringLiteral("Could not update %1").arg(path);
        return false;
    }
    session.applied << path;
    return true;
}

void Applier::writeManifest(const Session &session) const
{
    QJsonObject root;
    root.insert(QStringLiteral("id"), session.id);
    root.insert(QStringLiteral("theme"), session.themeId);
    root.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("system"), session.system);

    QJsonArray entries;
    for (const Entry &entry : session.entries) {
        QJsonObject object;
        object.insert(QStringLiteral("path"), entry.path);
        object.insert(QStringLiteral("existed"), entry.existed);
        object.insert(QStringLiteral("backup"), entry.backupRel);
        entries.append(object);
    }
    root.insert(QStringLiteral("entries"), entries);
    root.insert(QStringLiteral("applied"), QJsonArray::fromStringList(session.applied));
    root.insert(QStringLiteral("warnings"), QJsonArray::fromStringList(session.warnings));

    QSaveFile file(session.dir + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

QString Applier::makeKde(const Theme &theme) const
{
    if (!theme.installPath.isEmpty() && theme.installPath.endsWith(QStringLiteral(".colors"))) {
        QString content;
        if (readFile(theme.installPath, &content))
            return content;
    }
    return TemplateRenderer::render(QStringLiteral("kde.colors"), theme);
}

QString Applier::makeCaelestia(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("caelestia.json"), theme);
}

QString Applier::makeSpicetify(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("spicetify.ini"), theme);
}

QString Applier::makeVesktop(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("vesktop.css"), theme);
}

QString Applier::makeKonsole(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("konsole.colorscheme"), theme);
}

QString Applier::makeKitty(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("kitty.conf"), theme);
}

QString Applier::makeFoot(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("foot.ini"), theme);
}

void Applier::applyWallpaper(Session &session, const Theme &theme)
{
    if (theme.wallpaper.isEmpty())
        return;
    if (!QFileInfo::exists(theme.wallpaper)) {
        session.warnings << QStringLiteral("Wallpaper not found: %1").arg(theme.wallpaper);
        return;
    }

    if (!QStandardPaths::findExecutable(QStringLiteral("plasma-apply-wallpaperimage")).isEmpty()) {
        if (run(QStringLiteral("plasma-apply-wallpaperimage"), {theme.wallpaper}, 15000))
            session.applied << QStringLiteral("Wallpaper");
        else
            session.warnings << QStringLiteral("Plasma could not set the wallpaper");
        return;
    }

    if (!QStandardPaths::findExecutable(QStringLiteral("swww")).isEmpty()) {
        if (run(QStringLiteral("swww"), {QStringLiteral("img"), theme.wallpaper}, 15000))
            session.applied << QStringLiteral("Wallpaper");
        else
            session.warnings << QStringLiteral("swww could not set the wallpaper");
        return;
    }

    session.warnings << QStringLiteral("No wallpaper backend found; set it manually: %1").arg(theme.wallpaper);
}

void Applier::applyKde(Session &session, const Theme &theme, bool systemWide)
{
    const QString content = makeKde(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("KDE color scheme template produced nothing");
        return;
    }

    const QString path = systemWide
        ? QStringLiteral("/usr/share/color-schemes/HyperThemes.colors")
        : Paths::dataHome() + QStringLiteral("/color-schemes/HyperThemes.colors");

    if (!writeFile(session, path, content.toUtf8(), systemWide))
        return;

    QString output;
    if (!run(QStringLiteral("plasma-apply-colorscheme"), {QStringLiteral("HyperThemes")}, 10000, &output))
        session.warnings << QStringLiteral("KDE color scheme written, but plasma-apply-colorscheme did not accept it");
}

void Applier::applyCaelestia(Session &session, const Theme &theme)
{
    const QString config = Paths::configHome() + QStringLiteral("/caelestia");
    const QString state = Paths::stateHome() + QStringLiteral("/caelestia");
    if (!QFileInfo::exists(config) && !QFileInfo::exists(state)) {
        session.warnings << QStringLiteral("Caelestia/Quickshell was not detected; skipping its scheme");
        return;
    }

    const QString content = makeCaelestia(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("Caelestia scheme template produced nothing");
        return;
    }
    writeFile(session, state + QStringLiteral("/scheme.json"), content.toUtf8(), false);
}

void Applier::applyWaybar(Session &session, const Theme &theme)
{
    const QString config = Paths::configHome() + QStringLiteral("/waybar");
    if (QStandardPaths::findExecutable(QStringLiteral("waybar")).isEmpty() && !QFileInfo::exists(config)) {
        session.warnings << QStringLiteral("Waybar was not detected; skipping its colors");
        return;
    }

    const QString content = TemplateRenderer::render(QStringLiteral("waybar.css"), theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("Waybar template produced nothing");
        return;
    }
    QDir().mkpath(config);
    if (!writeFile(session, config + QStringLiteral("/hyperthemes.css"), content.toUtf8(), false))
        return;
    ensureInclude(session, config + QStringLiteral("/style.css"), QStringLiteral("@import \"hyperthemes.css\";"));

    if (!QStandardPaths::findExecutable(QStringLiteral("waybar")).isEmpty()) {
        QProcess::startDetached(QStringLiteral("pkill"), {QStringLiteral("-USR2"), QStringLiteral("-x"), QStringLiteral("waybar")});
    }
}

void Applier::applyGtk(Session &session, const Theme &theme)
{
    const QString content = TemplateRenderer::render(QStringLiteral("gtk.css"), theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("GTK template produced nothing");
        return;
    }

    for (const QString &folder : {QStringLiteral("gtk-3.0"), QStringLiteral("gtk-4.0")}) {
        const QString dir = Paths::configHome() + QLatin1Char('/') + folder;
        QDir().mkpath(dir);
        if (!writeFile(session, dir + QStringLiteral("/hyperthemes.css"), content.toUtf8(), false))
            continue;
        ensureInclude(session, dir + QStringLiteral("/gtk.css"), QStringLiteral("@import url(\"hyperthemes.css\");"));
    }
}

void Applier::applyQt(Session &session, const Theme &theme)
{
    const QString content = TemplateRenderer::render(QStringLiteral("qt.conf"), theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("Qt template produced nothing");
        return;
    }

    for (const QString &folder : {QStringLiteral("qt5ct"), QStringLiteral("qt6ct")}) {
        const QString dir = Paths::configHome() + QLatin1Char('/') + folder;
        QDir().mkpath(dir + QStringLiteral("/colors"));
        if (!writeFile(session, dir + QStringLiteral("/colors/HyperThemes.conf"), content.toUtf8(), false))
            continue;
        setQtScheme(session, dir + QStringLiteral("/") + folder + QStringLiteral(".conf"));
    }
}

void Applier::applyKonsole(Session &session, const Theme &theme)
{
    const QString content = makeKonsole(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("Konsole template produced nothing");
        return;
    }
    const QString dir = Paths::dataHome() + QStringLiteral("/konsole");
    QDir().mkpath(dir);
    if (writeFile(session, dir + QStringLiteral("/HyperThemes.colorscheme"), content.toUtf8(), false))
        session.warnings << QStringLiteral("Select HyperThemes in Konsole's profile settings to use the scheme");
}

void Applier::applyKitty(Session &session, const Theme &theme)
{
    if (QStandardPaths::findExecutable(QStringLiteral("kitty")).isEmpty()
        && !QFileInfo::exists(Paths::configHome() + QStringLiteral("/kitty"))) {
        session.warnings << QStringLiteral("kitty was not detected; skipping its colors");
        return;
    }

    const QString content = makeKitty(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("kitty template produced nothing");
        return;
    }
    const QString dir = Paths::configHome() + QStringLiteral("/kitty");
    QDir().mkpath(dir);
    if (!writeFile(session, dir + QStringLiteral("/hyperthemes.conf"), content.toUtf8(), false))
        return;
    ensureInclude(session, dir + QStringLiteral("/kitty.conf"), QStringLiteral("include hyperthemes.conf"));
}

void Applier::applyFoot(Session &session, const Theme &theme)
{
    if (QStandardPaths::findExecutable(QStringLiteral("foot")).isEmpty()
        && !QFileInfo::exists(Paths::configHome() + QStringLiteral("/foot"))) {
        session.warnings << QStringLiteral("foot was not detected; skipping its colors");
        return;
    }

    const QString content = makeFoot(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("foot template produced nothing");
        return;
    }
    const QString dir = Paths::configHome() + QStringLiteral("/foot");
    QDir().mkpath(dir);
    if (!writeFile(session, dir + QStringLiteral("/hyperthemes.ini"), content.toUtf8(), false))
        return;
    ensureInclude(session, dir + QStringLiteral("/foot.ini"), QStringLiteral("include=%1/hyperthemes.ini").arg(dir));
}

void Applier::applySpicetify(Session &session, const Theme &theme)
{
    if (QStandardPaths::findExecutable(QStringLiteral("spicetify")).isEmpty()) {
        session.warnings << QStringLiteral("Spicetify is not installed; skipping Spotify");
        return;
    }

    const QString dir = Paths::configHome() + QStringLiteral("/spicetify/Themes/HyperThemes");
    QDir().mkpath(dir);
    writeFile(session, dir + QStringLiteral("/color.ini"), makeSpicetify(theme).toUtf8(), false);
    writeFile(session, dir + QStringLiteral("/user.css"),
              TemplateRenderer::render(QStringLiteral("spicetify-user.css"), theme).toUtf8(), false);

    const QString config = Paths::configHome() + QStringLiteral("/spicetify/config-xpui.ini");
    backupFile(session, config, false);
    QString output;
    if (!run(QStringLiteral("spicetify"),
             {QStringLiteral("config"), QStringLiteral("current_theme"), QStringLiteral("HyperThemes"),
              QStringLiteral("color_scheme"), QStringLiteral("hyperthemes"),
              QStringLiteral("inject_css"), QStringLiteral("1"),
              QStringLiteral("replace_colors"), QStringLiteral("1")},
             30000, &output)) {
        session.warnings << QStringLiteral("Spicetify config failed: %1").arg(output.trimmed());
        return;
    }

    if (run(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("spotify")}, 3000)) {
        session.warnings << QStringLiteral("Spotify is running; close it and run 'spicetify apply' to load the theme");
    } else if (!run(QStringLiteral("spicetify"), {QStringLiteral("apply")}, 120000, &output)) {
        session.warnings << QStringLiteral("Spicetify apply failed: %1").arg(output.trimmed());
    }
    session.warnings << QStringLiteral("Spicetify patches Spotify's client files; use 'spicetify restore' to undo that part");
}

void Applier::applyVesktop(Session &session, const Theme &theme)
{
    const QString flatpakBase = Paths::home() + QStringLiteral("/.var/app/dev.vencord.Vesktop");
    const QString configBase = QFileInfo::exists(flatpakBase)
        ? flatpakBase + QStringLiteral("/config/vesktop")
        : Paths::configHome() + QStringLiteral("/vesktop");

    if (!QFileInfo::exists(flatpakBase)
        && !QFileInfo::exists(configBase)
        && QStandardPaths::findExecutable(QStringLiteral("vesktop")).isEmpty()) {
        session.warnings << QStringLiteral("Vesktop was not detected; skipping Discord");
        return;
    }

    const QString themes = configBase + QStringLiteral("/themes");
    QDir().mkpath(themes);
    const QString content = makeVesktop(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("Vesktop template produced nothing");
        return;
    }
    if (!writeFile(session, themes + QStringLiteral("/hyperthemes.css"), content.toUtf8(), false))
        return;

    const QString settingsPath = configBase + QStringLiteral("/settings/settings.json");
    QJsonObject settings;
    QString existing;
    if (readFile(settingsPath, &existing)) {
        const QJsonDocument document = QJsonDocument::fromJson(existing.toUtf8());
        if (document.isObject())
            settings = document.object();
    }
    QJsonArray enabled = settings.value(QStringLiteral("enabledThemes")).toArray();
    bool found = false;
    for (const QJsonValue &value : std::as_const(enabled)) {
        if (value.toString() == QStringLiteral("hyperthemes.css")) {
            found = true;
            break;
        }
    }
    if (!found)
        enabled.append(QStringLiteral("hyperthemes.css"));
    settings.insert(QStringLiteral("enabledThemes"), enabled);

    QDir().mkpath(configBase + QStringLiteral("/settings"));
    writeFile(session, settingsPath, QJsonDocument(settings).toJson(QJsonDocument::Indented), false);

    if (run(QStringLiteral("pgrep"), {QStringLiteral("-f"), QStringLiteral("vesktop")}, 3000)) {
        session.warnings << QStringLiteral("Vesktop is running; restart it to load hyperthemes.css");
    }
}

QVariantMap Applier::apply(const QVariantMap &themeMap, const QStringList &targets, bool systemWide)
{
    Theme theme = Theme::fromMap(themeMap);
    if (theme.id.isEmpty() || theme.palette.isEmpty())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("No theme palette was supplied")}};
    if (!theme.installable)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("This source is browse-only; open the source page to install it")}};

    Session session;
    session.id = timestampId(theme.id);
    session.dir = sessionDir(session.id);
    session.themeId = theme.id;
    session.system = systemWide;
    if (!QDir().mkpath(session.dir + QStringLiteral("/files")))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Could not create the backup directory")}};

    setStatus(QStringLiteral("Applying %1…").arg(theme.name));

    const QStringList requested = targets.isEmpty() ? theme.targets : targets;
    if (systemWide && (requested.contains(QStringLiteral("gtk")) || requested.contains(QStringLiteral("qt"))
                       || requested.contains(QStringLiteral("waybar")) || requested.contains(QStringLiteral("kitty")))) {
        session.warnings << QStringLiteral("System-wide mode only affects the KDE color scheme; other targets stay in your user config");
    }
    for (const QString &target : requested) {
        if (target == QLatin1String("wallpaper"))
            applyWallpaper(session, theme);
        else if (target == QLatin1String("kde"))
            applyKde(session, theme, systemWide);
        else if (target == QLatin1String("caelestia"))
            applyCaelestia(session, theme);
        else if (target == QLatin1String("waybar"))
            applyWaybar(session, theme);
        else if (target == QLatin1String("gtk"))
            applyGtk(session, theme);
        else if (target == QLatin1String("qt"))
            applyQt(session, theme);
        else if (target == QLatin1String("konsole"))
            applyKonsole(session, theme);
        else if (target == QLatin1String("kitty"))
            applyKitty(session, theme);
        else if (target == QLatin1String("foot"))
            applyFoot(session, theme);
        else if (target == QLatin1String("spicetify"))
            applySpicetify(session, theme);
        else if (target == QLatin1String("vesktop"))
            applyVesktop(session, theme);
        else
            session.warnings << QStringLiteral("Unknown target: %1").arg(target);
    }

    writeManifest(session);
    m_lastBackup = session.id;
    emit lastBackupChanged();

    if (session.applied.isEmpty()) {
        setStatus(QStringLiteral("Nothing was applied"));
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("No target could be applied")},
                {QStringLiteral("warnings"), session.warnings}};
    }

    setStatus(QStringLiteral("Applied %1").arg(theme.name));
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("backup"), session.id},
            {QStringLiteral("applied"), session.applied},
            {QStringLiteral("warnings"), session.warnings},
            {QStringLiteral("system"), systemWide}};
}

QVariantMap Applier::rollback(const QString &backupId)
{
    QString id = backupId;
    if (id.isEmpty()) {
        const QStringList all = backups();
        if (all.isEmpty())
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("No backups yet")}};
        id = all.first();
    }

    const QString dir = sessionDir(id);
    QFile manifestFile(dir + QStringLiteral("/manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Backup not found: %1").arg(id)}};

    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    if (!document.isObject())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Backup manifest is invalid")}};
    const QJsonObject root = document.object();
    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();

    int restored = 0;
    QStringList warnings;
    for (int i = entries.size() - 1; i >= 0; --i) {
        const QJsonObject entry = entries.at(i).toObject();
        const QString path = entry.value(QStringLiteral("path")).toString();
        const bool existed = entry.value(QStringLiteral("existed")).toBool();
        const QString backupRel = entry.value(QStringLiteral("backup")).toString();
        const QString backup = dir + QLatin1Char('/') + backupRel;
        const bool system = path.startsWith(QStringLiteral("/usr/")) || path.startsWith(QStringLiteral("/etc/"));

        if (existed) {
            if (!QFileInfo::exists(backup)) {
                warnings << QStringLiteral("Missing backup for %1").arg(path);
                continue;
            }
            if (system) {
                if (!runPrivileged({QStringLiteral("install"), QStringLiteral("-Dm644"), backup, path}, 60000)) {
                    warnings << QStringLiteral("Could not restore %1").arg(path);
                    continue;
                }
            } else {
                QFile::remove(path);
                QDir().mkpath(QFileInfo(path).absolutePath());
                if (!QFile::copy(backup, path)) {
                    warnings << QStringLiteral("Could not restore %1").arg(path);
                    continue;
                }
            }
        } else {
            if (system) {
                if (!runPrivileged({QStringLiteral("rm"), QStringLiteral("-f"), path}, 60000)) {
                    warnings << QStringLiteral("Could not remove %1").arg(path);
                    continue;
                }
            } else {
                QFile::remove(path);
            }
        }
        ++restored;
    }

    setStatus(QStringLiteral("Rolled back %1 file(s)").arg(restored));
    return {{QStringLiteral("ok"), restored > 0},
            {QStringLiteral("restored"), restored},
            {QStringLiteral("backup"), id},
            {QStringLiteral("warnings"), warnings}};
}

QStringList Applier::backups() const
{
    QStringList result;
    const QDir dir(Paths::backupsDir());
    const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QString &entry : entries) {
        if (QFileInfo::exists(dir.filePath(entry) + QStringLiteral("/manifest.json")))
            result << entry;
    }
    return result;
}

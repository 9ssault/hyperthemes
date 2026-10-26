#include "applier.h"

#include "cursortheme.h"
#include "palette.h"
#include "paths.h"
#include "templaterenderer.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QWebSocket>

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

void Applier::setCursorSize(int size)
{
    const int clamped = CursorTheme::clampSize(size);
    if (m_cursorSize == clamped)
        return;
    m_cursorSize = clamped;
    emit cursorSizeChanged();
}

QVariantList Applier::cursorShapes() const
{
    return CursorTheme::shapes();
}

QString Applier::timestampId(const QString &themeId)
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
        + QLatin1Char('-') + safeId(themeId);
}

QString Applier::uniqueSessionId(const QString &themeId)
{
    const QString base = timestampId(themeId);
    QString id = base;
    int counter = 2;
    while (QFileInfo::exists(sessionDir(id)))
        id = base + QStringLiteral("-%1").arg(counter++);
    return id;
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
    if (session.dryRun) {
        if (!session.applied.contains(path))
            session.applied << path;
        return true;
    }

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

bool Applier::upsertLine(Session &session, const QString &path, const QString &prefix, const QString &line)
{
    QString content;
    const bool exists = QFileInfo::exists(path);
    if (exists && !readFile(path, &content)) {
        session.warnings << QStringLiteral("Could not read %1").arg(path);
        return false;
    }

    QStringList lines = content.split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    const QRegularExpression expression(
        QStringLiteral("^\\s*%1\\b").arg(QRegularExpression::escape(prefix)));
    bool replaced = false;
    for (QString &existing : lines) {
        if (expression.match(existing).hasMatch()) {
            existing = line;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        lines.append(line);

    return writeFile(session, path, (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8(), false);
}

bool Applier::setIniValue(Session &session, const QString &path, const QString &section,
                          const QString &key, const QString &value)
{
    QString content;
    const bool exists = QFileInfo::exists(path);
    if (exists && !readFile(path, &content)) {
        session.warnings << QStringLiteral("Could not read %1").arg(path);
        return false;
    }

    QStringList lines = content.split(QLatin1Char('\n'));
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    const QString header = QStringLiteral("[") + section + QLatin1Char(']');
    int headerIndex = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines.at(i).trimmed().compare(header, Qt::CaseInsensitive) == 0) {
            headerIndex = i;
            break;
        }
    }
    if (headerIndex < 0) {
        if (!lines.isEmpty() && !lines.last().trimmed().isEmpty())
            lines.append(QString());
        lines.append(header);
        headerIndex = lines.size() - 1;
    }

    const QString line = key + QLatin1Char('=') + value;
    bool replaced = false;
    for (int i = headerIndex + 1; i < lines.size(); ++i) {
        const QString existing = lines.at(i).trimmed();
        if (existing.startsWith(QLatin1Char('[')))
            break;
        const QString existingKey = existing.section(QLatin1Char('='), 0, 0).trimmed();
        if (existingKey.compare(key, Qt::CaseInsensitive) == 0) {
            lines[i] = line;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        lines.insert(headerIndex + 1, line);

    return writeFile(session, path, (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8(), false);
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

QString Applier::makeLazyspotify(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("lazyspotify.sh"), theme);
}

QString Applier::makeDiscord(const Theme &theme) const
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

QString Applier::makeBtop(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("btop.theme"), theme);
}

QString Applier::makeZellij(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("zellij.kdl"), theme);
}

QString Applier::makeHyprland(const Theme &theme) const
{
    return TemplateRenderer::render(QStringLiteral("hyprland.conf"), theme);
}

void Applier::applyWallpaper(Session &session, const Theme &theme)
{
    if (theme.wallpaper.isEmpty())
        return;
    if (!QFileInfo::exists(theme.wallpaper)) {
        session.warnings << QStringLiteral("Wallpaper not found: %1").arg(theme.wallpaper);
        return;
    }

    if (session.dryRun) {
        if (!QStandardPaths::findExecutable(QStringLiteral("plasma-apply-wallpaperimage")).isEmpty()
            || !QStandardPaths::findExecutable(QStringLiteral("swww")).isEmpty())
            session.applied << theme.wallpaper;
        else
            session.warnings << QStringLiteral("No wallpaper backend found; set it manually: %1").arg(theme.wallpaper);
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

void Applier::applyKdePackage(Session &session, const Theme &theme)
{
    if (theme.packageId.isEmpty()) {
        session.warnings << QStringLiteral("The KDE package has no plugin id");
        return;
    }

    if (session.dryRun) {
        session.applied << QStringLiteral("KDE package: %1")
                               .arg(theme.packageName.isEmpty() ? theme.packageId : theme.packageName);
        return;
    }

    const QStringList configs = {
        QStringLiteral("kdeglobals"),
        QStringLiteral("plasmarc"),
        QStringLiteral("ksplashrc"),
        QStringLiteral("kwinrc"),
        QStringLiteral("plasma-org.kde.plasma.desktop-appletsrc"),
    };
    for (const QString &name : configs)
        backupFile(session, Paths::configHome() + QLatin1Char('/') + name, false);

    QString program;
    QStringList arguments;
    if (theme.packageStructure == QLatin1String("Plasma/LookAndFeel")) {
        program = QStringLiteral("plasma-apply-lookandfeel");
        arguments = {theme.packageId};
    } else if (theme.packageStructure == QLatin1String("Plasma/DesktopTheme")) {
        program = QStringLiteral("plasma-apply-desktoptheme");
        arguments = {theme.packageId};
    } else if (theme.packageStructure == QLatin1String("Plasma/ColorScheme")) {
        program = QStringLiteral("plasma-apply-colorscheme");
        arguments = {theme.packageName.isEmpty() ? theme.packageId : theme.packageName};
    } else {
        session.warnings << QStringLiteral("The KDE package was installed, but %1 has no automatic apply step").arg(theme.packageStructure);
        return;
    }

    if (QStandardPaths::findExecutable(program).isEmpty()) {
        session.warnings << QStringLiteral("The KDE package is installed; %1 is missing, so apply it from System Settings").arg(program);
        return;
    }

    QString output;
    if (!run(program, arguments, 120000, &output)) {
        session.warnings << QStringLiteral("The KDE package could not be applied: %1").arg(output.trimmed());
        return;
    }

    session.applied << QStringLiteral("KDE package: %1").arg(theme.packageName.isEmpty() ? theme.packageId : theme.packageName);
    session.warnings << QStringLiteral("Rollback restores KDE settings, but leaves the installed package in your user package folder");
}

void Applier::applyKde(Session &session, const Theme &theme, bool systemWide)
{
    if (!theme.packageId.isEmpty()) {
        applyKdePackage(session, theme);
        return;
    }

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
    if (!session.dryRun
        && !run(QStringLiteral("plasma-apply-colorscheme"), {QStringLiteral("HyperThemes")}, 10000, &output))
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

void Applier::applyBtop(Session &session, const Theme &theme)
{
    const QString dir = Paths::configHome() + QStringLiteral("/btop");
    if (QStandardPaths::findExecutable(QStringLiteral("btop")).isEmpty() && !QFileInfo::exists(dir)) {
        session.warnings << QStringLiteral("btop was not detected; skipping its theme");
        return;
    }

    const QString content = makeBtop(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("btop template produced nothing");
        return;
    }

    QDir().mkpath(dir + QStringLiteral("/themes"));
    if (!writeFile(session, dir + QStringLiteral("/themes/hyperthemes.theme"), content.toUtf8(), false))
        return;
    if (!upsertLine(session, dir + QStringLiteral("/btop.conf"), QStringLiteral("color_theme"),
                    QStringLiteral("color_theme = \"hyperthemes\"")))
        return;
    session.warnings << QStringLiteral("Restart btop to load the hyperthemes theme");
}

void Applier::applyZellij(Session &session, const Theme &theme)
{
    const QString dir = Paths::configHome() + QStringLiteral("/zellij");
    if (QStandardPaths::findExecutable(QStringLiteral("zellij")).isEmpty() && !QFileInfo::exists(dir)) {
        session.warnings << QStringLiteral("Zellij was not detected; skipping its theme");
        return;
    }

    const QString content = makeZellij(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("Zellij template produced nothing");
        return;
    }

    QDir().mkpath(dir + QStringLiteral("/themes"));
    if (!writeFile(session, dir + QStringLiteral("/themes/hyperthemes.kdl"), content.toUtf8(), false))
        return;
    if (!upsertLine(session, dir + QStringLiteral("/config.kdl"), QStringLiteral("theme"),
                    QStringLiteral("theme \"hyperthemes\"")))
        return;
    session.warnings << QStringLiteral("Zellij picks up the theme in new sessions");
}

void Applier::applyHyprland(Session &session, const Theme &theme)
{
    const QString dir = Paths::configHome() + QStringLiteral("/hypr");
    const bool hasHyprctl = !QStandardPaths::findExecutable(QStringLiteral("hyprctl")).isEmpty();
    if (!hasHyprctl && !QFileInfo::exists(dir)) {
        session.warnings << QStringLiteral("Hyprland was not detected; skipping its colours");
        return;
    }

    const QString content = makeHyprland(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("Hyprland template produced nothing");
        return;
    }

    QDir().mkpath(dir);
    const QString includePath = dir + QStringLiteral("/hyperthemes.conf");
    if (!writeFile(session, includePath, content.toUtf8(), false))
        return;
    if (!ensureInclude(session, dir + QStringLiteral("/hyprland.conf"),
                       QStringLiteral("source = %1").arg(includePath)))
        return;

    if (!session.dryRun && hasHyprctl && !qEnvironmentVariableIsSet("HYPER_THEMES_NO_RELOAD")) {
        if (!run(QStringLiteral("hyprctl"), {QStringLiteral("reload")}, 8000))
            session.warnings << QStringLiteral("Hyprland colours were written, but hyprctl reload failed");
    }
    session.warnings << QStringLiteral("Hyprland keeps your window rules; only the generated border and shadow colours change");
}

void Applier::applyCursor(Session &session, const Theme &theme)
{
    const QHash<QString, QByteArray> files = CursorTheme::build(theme, m_cursorSize);
    if (files.isEmpty()) {
        session.warnings << QStringLiteral("The pointer theme could not be generated");
        return;
    }

    const QString dir = CursorTheme::installDir();
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        if (!writeFile(session, dir + QLatin1Char('/') + it.key(), it.value(), false))
            return;
    }

    if (!writeFile(session, CursorTheme::defaultThemePath(), CursorTheme::defaultThemeFile(), false))
        return;

    const QString name = CursorTheme::themeName();
    for (const QString &folder : {QStringLiteral("gtk-3.0"), QStringLiteral("gtk-4.0")}) {
        if (!setIniValue(session, Paths::configHome() + QLatin1Char('/') + folder
                                        + QStringLiteral("/settings.ini"),
                         QStringLiteral("Settings"), QStringLiteral("gtk-cursor-theme-name"), name))
            return;
    }
    if (!setIniValue(session, Paths::configHome() + QStringLiteral("/kcminputrc"),
                     QStringLiteral("Mouse"), QStringLiteral("cursorTheme"), name))
        return;
    if (!setIniValue(session, Paths::configHome() + QStringLiteral("/kcminputrc"),
                     QStringLiteral("Mouse"), QStringLiteral("cursorSize"),
                     QString::number(m_cursorSize)))
        return;

    const QString environment = Paths::configHome()
        + QStringLiteral("/environment.d/90-hyperthemes-cursor.conf");
    if (!writeFile(session, environment,
                   QStringLiteral("XCURSOR_THEME=%1\nXCURSOR_SIZE=%2\n")
                       .arg(name, QString::number(m_cursorSize))
                       .toUtf8(),
                   false))
        return;

    if (!session.dryRun && !qEnvironmentVariableIsSet("HYPER_THEMES_NO_RELOAD")) {
        // Each desktop has its own live switch; all of them are best effort.
        run(QStringLiteral("plasma-apply-cursortheme"), {name}, 15000);
        run(QStringLiteral("gsettings"),
            {QStringLiteral("set"), QStringLiteral("org.gnome.desktop.interface"),
             QStringLiteral("cursor-theme"), name},
            10000);
        run(QStringLiteral("gsettings"),
            {QStringLiteral("set"), QStringLiteral("org.gnome.desktop.interface"),
             QStringLiteral("cursor-size"), QString::number(m_cursorSize)},
            10000);
        run(QStringLiteral("hyprctl"),
            {QStringLiteral("setcursor"), name, QString::number(m_cursorSize)}, 8000);
        run(QStringLiteral("swaymsg"),
            {QStringLiteral("seat"), QStringLiteral("*"), QStringLiteral("xcursor_theme"), name,
             QString::number(m_cursorSize)},
            8000);
        if (!run(QStringLiteral("xfconf-query"),
                 {QStringLiteral("-c"), QStringLiteral("xsettings"), QStringLiteral("-p"),
                  QStringLiteral("/Gtk/CursorThemeName"), QStringLiteral("-s"), name},
                 10000)) {
            run(QStringLiteral("xfconf-query"),
                {QStringLiteral("-c"), QStringLiteral("xsettings"), QStringLiteral("-p"),
                 QStringLiteral("/Gtk/CursorThemeName"), QStringLiteral("-n"), QStringLiteral("-t"),
                 QStringLiteral("string"), QStringLiteral("-s"), name},
                10000);
        }
    }

    session.warnings << QStringLiteral("The HyperThemes pointer is selected; restart open apps (or sign out) to refresh their cursor");
}

void Applier::applyLazyspotify(Session &session, const Theme &theme)
{
    const QString wrapper = Paths::home() + QStringLiteral("/.local/bin/lazyspotify-hyperthemes");
    const QString content = makeLazyspotify(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("lazyspotify template produced nothing");
        return;
    }
    if (!writeFile(session, wrapper, content.toUtf8(), false))
        return;
    if (!session.dryRun) {
        QFile::setPermissions(wrapper,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                  | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                  | QFileDevice::ReadOther | QFileDevice::ExeOther);
    }

    const QString desktop = Paths::dataHome()
        + QStringLiteral("/applications/lazyspotify-hyperthemes.desktop");
    const QString entry = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=lazyspotify (HyperThemes)\n"
        "Comment=Terminal Spotify client in the HyperThemes palette\n"
        "Exec=\"%1\"\n"
        "Terminal=false\n"
        "Categories=AudioVideo;Player;\n"
        "Icon=multimedia-player\n").arg(wrapper);
    if (writeFile(session, desktop, entry.toUtf8(), false) && !session.dryRun) {
        QFile::setPermissions(desktop,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }

    if (QStandardPaths::findExecutable(QStringLiteral("lazyspotify")).isEmpty()) {
        session.warnings << QStringLiteral(
            "lazyspotify is missing on this machine; the launcher is ready at "
            "~/.local/bin/lazyspotify-hyperthemes. Install it from "
            "https://github.com/dubeyKartikay/lazyspotify");
    } else {
        session.warnings << QStringLiteral(
            "lazyspotify takes colours from the terminal palette; launch "
            "lazyspotify-hyperthemes (or its desktop entry) for this theme");
    }
}

bool Applier::reloadDiscordTheme(QString *error) const
{
    // Equibop exposes a loopback DevTools port (the same one the Caelestia
    // widget uses). Talking to it lets a running client pick up the new theme
    // without a restart, instead of writing settings that the client would
    // flush back over on exit.
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(
        QNetworkRequest(QUrl(QStringLiteral("http://127.0.0.1:9222/json"))));
    QEventLoop discovery;
    QTimer discoveryTimeout;
    discoveryTimeout.setSingleShot(true);
    QObject::connect(&discoveryTimeout, &QTimer::timeout, &discovery, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &discovery, &QEventLoop::quit);
    discoveryTimeout.start(2000);
    discovery.exec();
    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return fail(QStringLiteral("the DevTools endpoint did not answer"));
    }
    const QByteArray payload = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    reply->deleteLater();
    if (networkError != QNetworkReply::NoError)
        return fail(QStringLiteral("the DevTools endpoint is closed"));

    QString socketUrl;
    const QJsonArray pages = QJsonDocument::fromJson(payload).array();
    for (const QJsonValue &value : pages) {
        const QJsonObject page = value.toObject();
        if (page.value(QStringLiteral("type")).toString() == QLatin1String("page")
            && page.value(QStringLiteral("url")).toString().contains(QStringLiteral("discord.com"))) {
            socketUrl = page.value(QStringLiteral("webSocketDebuggerUrl")).toString();
            break;
        }
    }
    if (socketUrl.isEmpty())
        return fail(QStringLiteral("no Discord page is open"));

    QWebSocket socket;
    QEventLoop connection;
    QTimer connectionTimeout;
    connectionTimeout.setSingleShot(true);
    QObject::connect(&socket, &QWebSocket::connected, &connection, &QEventLoop::quit);
    QObject::connect(&socket, &QWebSocket::errorOccurred, &connection, &QEventLoop::quit);
    QObject::connect(&connectionTimeout, &QTimer::timeout, &connection, &QEventLoop::quit);
    socket.open(QUrl(socketUrl));
    connectionTimeout.start(3000);
    connection.exec();
    if (socket.state() != QAbstractSocket::ConnectedState) {
        socket.abort();
        return fail(QStringLiteral("the client did not accept the connection"));
    }

    // Re-assigning the same list would not notify Vencord, so remove the theme
    // first and add it back a moment later, which reloads the CSS.
    const QString expression = QStringLiteral(
        "(() => {"
        " const V = window.Vencord;"
        " if (!V || !V.Settings) return 'no-vencord';"
        " const current = [...(V.Settings.enabledThemes || [])];"
        " if (current.includes('hyperthemes.css')) {"
        "   V.Settings.enabledThemes = current.filter(t => t !== 'hyperthemes.css');"
        "   setTimeout(() => { V.Settings.enabledThemes = current; }, 60);"
        " } else {"
        "   V.Settings.enabledThemes = [...current, 'hyperthemes.css'];"
        " }"
        " return 'ok';"
        "})()");

    QJsonObject request;
    request.insert(QStringLiteral("id"), 1);
    request.insert(QStringLiteral("method"), QStringLiteral("Runtime.evaluate"));
    request.insert(QStringLiteral("params"),
                   QJsonObject{{QStringLiteral("expression"), expression},
                               {QStringLiteral("returnByValue"), true}});

    bool confirmed = false;
    QEventLoop response;
    QTimer responseTimeout;
    responseTimeout.setSingleShot(true);
    QObject::connect(&socket, &QWebSocket::textMessageReceived, &response,
                     [&confirmed, &response](const QString &message) {
                         const QJsonObject object = QJsonDocument::fromJson(message.toUtf8()).object();
                         if (object.value(QStringLiteral("id")).toInt() != 1)
                             return;
                         const QString value = object.value(QStringLiteral("result"))
                                                   .toObject()
                                                   .value(QStringLiteral("result"))
                                                   .toObject()
                                                   .value(QStringLiteral("value"))
                                                   .toString();
                         confirmed = value == QLatin1String("ok");
                         response.quit();
                     });
    QObject::connect(&responseTimeout, &QTimer::timeout, &response, &QEventLoop::quit);
    socket.sendTextMessage(QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)));
    responseTimeout.start(2000);
    response.exec();
    socket.close();

    if (!confirmed)
        return fail(QStringLiteral("the client did not confirm the reload"));
    return true;
}

void Applier::applyDiscord(Session &session, const Theme &theme)
{
    // Equibop and Vesktop share the Vencord theme format, so the same CSS is
    // written to every client that is installed. Equibop is checked first
    // because it is the client we support going forward.
    struct Client {
        QString name;
        QString flatpakId;
        QString configName;
        QString executable;
    };
    static const Client clients[] = {
        {QStringLiteral("Equibop"), QStringLiteral("org.equicord.equibop"),
         QStringLiteral("equibop"), QStringLiteral("equibop")},
        {QStringLiteral("Vesktop"), QStringLiteral("dev.vencord.Vesktop"),
         QStringLiteral("vesktop"), QStringLiteral("vesktop")},
    };

    const QString content = makeDiscord(theme);
    if (content.isEmpty()) {
        session.warnings << QStringLiteral("Discord template produced nothing");
        return;
    }

    bool detected = false;
    QStringList runningClients;
    for (const Client &client : clients) {
        const QString flatpakBase = Paths::home() + QStringLiteral("/.var/app/") + client.flatpakId;
        const bool flatpak = QFileInfo::exists(flatpakBase);
        const QString configBase = flatpak
            ? flatpakBase + QStringLiteral("/config/") + client.configName
            : Paths::configHome() + QLatin1Char('/') + client.configName;

        // A leftover config folder (for example after a Flatpak uninstall)
        // must not count as an installed client: require a real settings store
        // or an executable on PATH.
        const bool hasStore = QFileInfo::exists(configBase + QStringLiteral("/settings"));
        const bool hasExecutable =
            !QStandardPaths::findExecutable(client.executable).isEmpty();
        if (!flatpak && !hasStore && !hasExecutable)
            continue;

        detected = true;
        const QString themes = configBase + QStringLiteral("/themes");
        QDir().mkpath(themes);
        if (!writeFile(session, themes + QStringLiteral("/hyperthemes.css"), content.toUtf8(), false))
            continue;

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

        // Match the launch wrapper, not the bare client name: pgrep -f would
        // also match this process when the target name is on its command line.
        const QString process = flatpak
            ? QStringLiteral("start") + client.configName
            : client.executable;
        const bool running = !session.dryRun
            && (flatpak ? run(QStringLiteral("pgrep"), {QStringLiteral("-f"), process}, 3000)
                        : run(QStringLiteral("pgrep"), {QStringLiteral("-x"), process}, 3000));
        if (running)
            runningClients << client.name;
    }

    if (!detected) {
        session.warnings << QStringLiteral("Equibop or Vesktop was not detected; skipping Discord");
        return;
    }

    if (!runningClients.isEmpty()) {
        QString error;
        if (!reloadDiscordTheme(&error)) {
            for (const QString &name : std::as_const(runningClients)) {
                session.warnings << QStringLiteral("%1 is running; restart it to load hyperthemes.css (%2)")
                                        .arg(name, error);
            }
        }
    }
}

QVariantMap Applier::setWallpaper(const QString &path)
{
    if (path.isEmpty() || !QFileInfo::exists(path))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Wallpaper not found: %1").arg(path)}};

    Session session;
    session.id = uniqueSessionId(QStringLiteral("wallpaper"));
    session.dir = sessionDir(session.id);
    session.themeId = QStringLiteral("wallpaper");
    QDir().mkpath(session.dir + QStringLiteral("/files"));

    Theme theme;
    theme.name = QStringLiteral("Wallpaper");
    theme.wallpaper = path;
    applyWallpaper(session, theme);

    if (session.applied.isEmpty()) {
        setStatus(QStringLiteral("No wallpaper backend found"));
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("No wallpaper backend found; set it manually: %1").arg(path)},
                {QStringLiteral("warnings"), session.warnings}};
    }

    setStatus(QStringLiteral("Wallpaper updated"));
    return {{QStringLiteral("ok"), true}, {QStringLiteral("warnings"), session.warnings}};
}

QVariantMap Applier::apply(const QVariantMap &themeMap, const QStringList &targets, bool systemWide)
{
    return applyInternal(themeMap, targets, systemWide, false);
}

QVariantMap Applier::dryRun(const QVariantMap &themeMap, const QStringList &targets, bool systemWide)
{
    return applyInternal(themeMap, targets, systemWide, true);
}

QVariantMap Applier::applyInternal(const QVariantMap &themeMap, const QStringList &targets,
                                   bool systemWide, bool dryRun)
{
    Theme theme = Theme::fromMap(themeMap);
    if (theme.id.isEmpty() || (theme.palette.isEmpty() && theme.packageId.isEmpty()))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("No theme palette was supplied")}};
    if (!theme.installable)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("This source is browse-only; open the source page to install it")}};

    Session session;
    session.id = uniqueSessionId(theme.id);
    session.dir = sessionDir(session.id);
    session.themeId = theme.id;
    session.system = systemWide;
    session.dryRun = dryRun;
    if (!dryRun && !QDir().mkpath(session.dir + QStringLiteral("/files")))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Could not create the backup directory")}};

    setStatus(QStringLiteral("Applying %1…").arg(theme.name));

    const QStringList requested = targets.isEmpty() ? theme.targets : targets;
    if (systemWide && (requested.contains(QStringLiteral("gtk")) || requested.contains(QStringLiteral("qt"))
                       || requested.contains(QStringLiteral("waybar")) || requested.contains(QStringLiteral("kitty"))
                       || requested.contains(QStringLiteral("btop")) || requested.contains(QStringLiteral("zellij"))
                       || requested.contains(QStringLiteral("hyprland")))) {
        session.warnings << QStringLiteral("System-wide mode only affects the KDE color scheme; other targets stay in your user config");
    }
    QVariantList results;
    for (const QString &target : requested) {
        const int warningsBefore = session.warnings.size();
        const int appliedBefore = session.applied.size();
        bool known = true;

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
        else if (target == QLatin1String("btop"))
            applyBtop(session, theme);
        else if (target == QLatin1String("zellij"))
            applyZellij(session, theme);
        else if (target == QLatin1String("hyprland"))
            applyHyprland(session, theme);
        else if (target == QLatin1String("cursor"))
            applyCursor(session, theme);
        else if (target == QLatin1String("lazyspotify"))
            applyLazyspotify(session, theme);
        else if (target == QLatin1String("vesktop") || target == QLatin1String("equibop"))
            applyDiscord(session, theme);
        else {
            known = false;
            session.warnings << QStringLiteral("Unknown target: %1").arg(target);
        }

        const QStringList addedWarnings = session.warnings.mid(warningsBefore);
        const QStringList addedFiles = session.applied.mid(appliedBefore);
        QString status = known ? QStringLiteral("ok") : QStringLiteral("unknown");
        if (known && !addedWarnings.isEmpty()) {
            status = QStringLiteral("warning");
            for (const QString &warning : addedWarnings) {
                if (warning.contains(QStringLiteral("skipping"))
                    || warning.contains(QStringLiteral("not detected"))
                    || warning.contains(QStringLiteral("not installed"))
                    || warning.contains(QStringLiteral("was not found"))) {
                    status = QStringLiteral("skipped");
                    break;
                }
            }
        } else if (known && addedFiles.isEmpty()) {
            status = QStringLiteral("unchanged");
        }

        results.append(QVariantMap{
            {QStringLiteral("target"), target},
            {QStringLiteral("status"), status},
            {QStringLiteral("warnings"), addedWarnings},
            {QStringLiteral("files"), addedFiles},
        });
    }

    if (!dryRun) {
        writeManifest(session);
        m_lastBackup = session.id;
        emit lastBackupChanged();
    }

    if (session.applied.isEmpty() && !dryRun) {
        setStatus(QStringLiteral("Nothing was applied"));
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("No target could be applied")},
                {QStringLiteral("warnings"), session.warnings},
                {QStringLiteral("results"), results}};
    }

    setStatus(dryRun ? QStringLiteral("Previewed %1").arg(theme.name)
                     : QStringLiteral("Applied %1").arg(theme.name));
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("backup"), dryRun ? QString() : session.id},
            {QStringLiteral("applied"), session.applied},
            {QStringLiteral("files"), session.applied},
            {QStringLiteral("warnings"), session.warnings},
            {QStringLiteral("results"), results},
            {QStringLiteral("dryRun"), dryRun},
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

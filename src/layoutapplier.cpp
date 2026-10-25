#include "layoutapplier.h"

#include "paths.h"

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
#include <QStandardPaths>
#include <QThread>

namespace {
QString safeId(QString id)
{
    id.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("-"));
    return id;
}

bool runCommand(const QString &program, const QStringList &args, QString *output, int timeoutMs = 20000)
{
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
        *output = QString::fromUtf8(process.readAll()).trimmed();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void reloadShell(const QString &target)
{
    if (qEnvironmentVariableIsSet("HYPER_THEMES_NO_RELOAD"))
        return;

    if (target == QLatin1String("kde")) {
        if (!QStandardPaths::findExecutable(QStringLiteral("systemctl")).isEmpty())
            QProcess::startDetached(QStringLiteral("systemctl"),
                                    {QStringLiteral("--user"), QStringLiteral("restart"),
                                     QStringLiteral("plasma-plasmashell.service")});
        return;
    }

    if (!QStandardPaths::findExecutable(QStringLiteral("caelestia")).isEmpty())
        QProcess::startDetached(QStringLiteral("caelestia"), {QStringLiteral("shell"), QStringLiteral("-r")});
}

bool stopPlasmaShell()
{
    if (QStandardPaths::findExecutable(QStringLiteral("systemctl")).isEmpty())
        return false;
    QString state;
    if (!runCommand(QStringLiteral("systemctl"),
                    {QStringLiteral("--user"), QStringLiteral("is-active"),
                     QStringLiteral("plasma-plasmashell.service")},
                    &state, 5000))
        return false;
    return runCommand(QStringLiteral("systemctl"),
                      {QStringLiteral("--user"), QStringLiteral("stop"),
                       QStringLiteral("plasma-plasmashell.service")},
                      nullptr, 15000);
}

void startPlasmaShell()
{
    if (!QStandardPaths::findExecutable(QStringLiteral("systemctl")).isEmpty())
        QProcess::startDetached(QStringLiteral("systemctl"),
                                {QStringLiteral("--user"), QStringLiteral("start"),
                                 QStringLiteral("plasma-plasmashell.service")});
}

QJsonObject entry(const QString &id, const QString &zone)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("zone"), zone},
        {QStringLiteral("enabled"), true},
    };
}

QJsonArray topEntries()
{
    return {
        entry(QStringLiteral("logo"), QStringLiteral("left")),
        entry(QStringLiteral("workspaces"), QStringLiteral("left")),
        entry(QStringLiteral("greeter"), QStringLiteral("left")),
        entry(QStringLiteral("dock"), QStringLiteral("middle")),
        entry(QStringLiteral("tray"), QStringLiteral("right")),
        entry(QStringLiteral("updateIndicator"), QStringLiteral("right")),
        entry(QStringLiteral("media"), QStringLiteral("right")),
        entry(QStringLiteral("discord"), QStringLiteral("right")),
        entry(QStringLiteral("clock"), QStringLiteral("right")),
        entry(QStringLiteral("statusIcons"), QStringLiteral("right")),
        entry(QStringLiteral("showDesktop"), QStringLiteral("right")),
        entry(QStringLiteral("power"), QStringLiteral("right")),
    };
}

QJsonArray dockEntries()
{
    return {
        entry(QStringLiteral("logo"), QStringLiteral("left")),
        entry(QStringLiteral("workspaces"), QStringLiteral("left")),
        entry(QStringLiteral("dock"), QStringLiteral("middle")),
        entry(QStringLiteral("tray"), QStringLiteral("right")),
        entry(QStringLiteral("media"), QStringLiteral("right")),
        entry(QStringLiteral("clock"), QStringLiteral("right")),
        entry(QStringLiteral("statusIcons"), QStringLiteral("right")),
        entry(QStringLiteral("power"), QStringLiteral("right")),
    };
}

QJsonArray minimalEntries()
{
    return {
        entry(QStringLiteral("logo"), QStringLiteral("left")),
        entry(QStringLiteral("workspaces"), QStringLiteral("left")),
        entry(QStringLiteral("tray"), QStringLiteral("right")),
        entry(QStringLiteral("clock"), QStringLiteral("right")),
        entry(QStringLiteral("statusIcons"), QStringLiteral("right")),
        entry(QStringLiteral("power"), QStringLiteral("right")),
    };
}
} // namespace

LayoutApplier::LayoutApplier(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(Paths::stateDir() + QStringLiteral("/layout-backups"));
}

QVariantList LayoutApplier::presets() const
{
    QVariantList result;
    const auto add = [&result](const QString &id, const QString &name, const QString &target,
                               const QString &targetLabel, const QString &description) {
        result.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("target"), target},
            {QStringLiteral("targetLabel"), targetLabel},
            {QStringLiteral("description"), description},
        });
    };

    add(QStringLiteral("caelestia-top"), QStringLiteral("Caelestia · Top bar"), QStringLiteral("caelestia"),
        QStringLiteral("Quickshell / Caelestia"),
        QStringLiteral("Caelestia house layout: logo, workspaces, dock and a dense right-hand status rail on a top bar."));
    add(QStringLiteral("caelestia-bottom"), QStringLiteral("Caelestia · Bottom dock"), QStringLiteral("caelestia"),
        QStringLiteral("Quickshell / Caelestia"),
        QStringLiteral("A floating bottom bar with a centered dock. Media, tray, clock and power stay on the right."));
    add(QStringLiteral("caelestia-minimal"), QStringLiteral("Caelestia · Minimal top"), QStringLiteral("caelestia"),
        QStringLiteral("Quickshell / Caelestia"),
        QStringLiteral("Only the essentials: logo, workspaces, tray, clock, status icons and power."));
    add(QStringLiteral("kde-top"), QStringLiteral("KDE Plasma · Top bar"), QStringLiteral("kde"),
        QStringLiteral("KDE Plasma"),
        QStringLiteral("Replaces the current panels with one top bar: launcher, pager, tasks, tray and clock."));
    add(QStringLiteral("kde-bottom"), QStringLiteral("KDE Plasma · Auto-hide dock"), QStringLiteral("kde"),
        QStringLiteral("KDE Plasma"),
        QStringLiteral("A bottom panel that hides when windows approach, with a launcher, tasks and tray."));
    add(QStringLiteral("kde-minimal"), QStringLiteral("KDE Plasma · Minimal top"), QStringLiteral("kde"),
        QStringLiteral("KDE Plasma"),
        QStringLiteral("A slim top bar with launcher, pager, tray and clock. Fewer moving parts."));
    return result;
}

QVariantMap LayoutApplier::apply(const QString &id)
{
    QString name;
    QString target;
    const QVariantList all = presets();
    for (const QVariant &value : all) {
        const QVariantMap preset = value.toMap();
        if (preset.value(QStringLiteral("id")).toString() == id) {
            name = preset.value(QStringLiteral("name")).toString();
            target = preset.value(QStringLiteral("target")).toString();
            break;
        }
    }
    if (name.isEmpty())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Unknown layout preset.")}};

    if (target == QLatin1String("kde"))
        return applyKde(id, name);
    return applyCaelestia(id, name);
}

QVariantMap LayoutApplier::applyCaelestia(const QString &id, const QString &name)
{
    const QString path = Paths::configHome() + QStringLiteral("/caelestia/shell.json");
    QJsonObject root;
    if (QFileInfo::exists(path)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Could not read shell.json.")}};
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (!document.isObject() || parseError.error != QJsonParseError::NoError) {
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("shell.json is not valid JSON; refusing to replace it.")}};
        }
        root = document.object();
    }

    QJsonObject bar = root.value(QStringLiteral("bar")).toObject();
    bar.insert(QStringLiteral("position"), id == QLatin1String("caelestia-bottom") ? QStringLiteral("bottom") : QStringLiteral("top"));
    bar.insert(QStringLiteral("persistent"), id != QLatin1String("caelestia-bottom"));
    bar.insert(QStringLiteral("dodgeWindows"), false);
    if (id == QLatin1String("caelestia-bottom")) {
        bar.insert(QStringLiteral("entries"), dockEntries());
        QJsonObject dock = bar.value(QStringLiteral("dock")).toObject();
        dock.insert(QStringLiteral("monitorCenter"), true);
        dock.insert(QStringLiteral("iconSize"), 40);
        bar.insert(QStringLiteral("dock"), dock);
    } else if (id == QLatin1String("caelestia-minimal")) {
        bar.insert(QStringLiteral("entries"), minimalEntries());
    } else {
        bar.insert(QStringLiteral("entries"), topEntries());
    }
    root.insert(QStringLiteral("bar"), bar);

    QString backup;
    QString error;
    if (!backupFile(id, name, QStringLiteral("caelestia"), path, &backup, &error))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Could not write shell.json.")},
                {QStringLiteral("backup"), backup}};
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Could not commit shell.json.")},
                {QStringLiteral("backup"), backup}};
    }

    m_lastBackup = backup;
    emit lastBackupChanged();
    reloadShell(QStringLiteral("caelestia"));

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("backup"), backup},
            {QStringLiteral("target"), QStringLiteral("caelestia")},
            {QStringLiteral("warnings"), QStringList{QStringLiteral("Caelestia bar entries were replaced; the rest of shell.json was preserved.")}}};
}

QVariantMap LayoutApplier::applyKde(const QString &id, const QString &name)
{
    const QString path = Paths::configHome() + QStringLiteral("/plasma-org.kde.plasma.desktop-appletsrc");
    QString backup;
    QString error;
    if (!backupFile(id, name, QStringLiteral("kde"), path, &backup, &error))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};

    const QString program = QStandardPaths::findExecutable(QStringLiteral("qdbus6"));
    if (program.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("qdbus6 is not installed, so Plasma cannot be scripted.")},
                {QStringLiteral("backup"), backup}};
    }

    const QStringList arguments = {QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                                   QStringLiteral("org.kde.PlasmaShell.evaluateScript"), kdeScript(id)};
    QString output;
    bool ok = runCommand(program, arguments, &output, 60000);
    if (!ok) {
        // plasmashell can briefly refuse scripting right after it restarts;
        // one retry covers that without risking the previous panels.
        QThread::msleep(1500);
        output.clear();
        ok = runCommand(program, arguments, &output, 60000);
    }
    if (!ok) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), output.isEmpty() ? QStringLiteral("Plasma rejected the layout script.") : output},
                {QStringLiteral("backup"), backup}};
    }

    m_lastBackup = backup;
    emit lastBackupChanged();
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("backup"), backup},
            {QStringLiteral("target"), QStringLiteral("kde")},
            {QStringLiteral("warnings"), QStringList{QStringLiteral("Your previous Plasma panels were removed. If the new panel does not appear, log out and back in.")}}};
}

QString LayoutApplier::kdeScript(const QString &id) const
{
    if (!id.startsWith(QLatin1String("kde-")))
        return {};

    const bool bottom = id == QLatin1String("kde-bottom");
    const bool minimal = id == QLatin1String("kde-minimal");

    QStringList lines;
    // Build the replacement before touching the old panels so a rejected
    // script cannot leave the desktop without any panel at all.
    lines << QStringLiteral("const previousPanels = panels();")
          << QStringLiteral("let panel = null;")
          << QStringLiteral("try { panel = new Panel(\"org.kde.panel\"); } catch (error) { try { panel = new Panel(); } catch (fallback) { panel = null; } }")
          << QStringLiteral("if (panel === null) { throw new Error(\"Plasma refused to create a panel.\"); }")
          << QStringLiteral("for (let i = 0; i < previousPanels.length; ++i) { try { previousPanels[i].remove(); } catch (error) {} }")
          << QStringLiteral("panel.location = \"%1\";").arg(bottom ? QStringLiteral("bottom") : QStringLiteral("top"))
          << QStringLiteral("panel.height = %1;").arg(bottom ? 52 : (minimal ? 36 : 42));
    if (bottom)
        lines << QStringLiteral("panel.hiding = \"dodgewindows\";");

    lines << QStringLiteral("const known = (typeof knownWidgetTypes !== \"undefined\") ? knownWidgetTypes : [];")
          << QStringLiteral("function addWidget(id) { if (known.length > 0 && known.indexOf(id) === -1) { return false; } try { panel.addWidget(id); return true; } catch (error) { return false; } }")
          << QStringLiteral("function addFirst(ids) { for (let i = 0; i < ids.length; ++i) { if (addWidget(ids[i])) { return true; } } return false; }")
          << QStringLiteral("addFirst([\"org.kde.plasma.kickoff\", \"org.kde.plasma.kickerdash\", \"org.kde.plasma.kicker\"]);");
    if (!minimal)
        lines << QStringLiteral("addFirst([\"org.kde.plasma.pager\", \"org.kde.plasma.activitypager\"]);");
    if (!minimal && !bottom)
        lines << QStringLiteral("addFirst([\"org.kde.plasma.icontasks\", \"org.kde.plasma.taskmanager\", \"org.kde.plasma.windowlist\"]);")
              << QStringLiteral("addFirst([\"org.kde.plasma.marginsseparator\", \"org.kde.plasma.panelspacer\"]);");
    if (bottom)
        lines << QStringLiteral("addFirst([\"org.kde.plasma.icontasks\", \"org.kde.plasma.taskmanager\", \"org.kde.plasma.windowlist\"]);");
    lines << QStringLiteral("addFirst([\"org.kde.plasma.marginsseparator\", \"org.kde.plasma.panelspacer\"]);")
          << QStringLiteral("addFirst([\"org.kde.plasma.systemtray\"]);")
          << QStringLiteral("addFirst([\"org.kde.plasma.digitalclock\", \"org.kde.plasma.fuzzyclock\", \"org.kde.plasma.binaryclock\"]);");
    if (bottom)
        lines << QStringLiteral("addFirst([\"org.kde.plasma.showdesktop\", \"org.kde.plasma.minimizeall\"]);");

    return lines.join(QLatin1Char('\n'));
}

QVariantMap LayoutApplier::restore(const QString &backupId)
{
    QString id = backupId;
    if (id.isEmpty()) {
        const QStringList all = backups();
        if (all.isEmpty())
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("No layout backups yet.")}};
        id = all.first();
    }

    const QString dir = backupDir(id);
    QFile manifestFile(dir + QStringLiteral("/manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Layout backup not found.")}};
    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    if (!document.isObject())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Layout backup is invalid.")}};

    const QJsonObject manifest = document.object();
    const QString path = manifest.value(QStringLiteral("path")).toString();
    const QString target = manifest.value(QStringLiteral("target")).toString();
    const bool existed = manifest.value(QStringLiteral("existed")).toBool();
    const QString backupFileRel = manifest.value(QStringLiteral("file")).toString();
    const QString backup = dir + QLatin1Char('/') + backupFileRel;

    const bool restartKde = target == QLatin1String("kde") && !qEnvironmentVariableIsSet("HYPER_THEMES_NO_RELOAD");
    // Write the file while plasmashell is stopped: a running instance flushes
    // its in-memory panel config on exit and would overwrite the restore.
    const bool stopped = restartKde && stopPlasmaShell();

    if (existed) {
        if (!QFileInfo::exists(backup)) {
            if (stopped)
                startPlasmaShell();
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("The layout backup file is missing.")}};
        }
        QFile::remove(path);
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (!QFile::copy(backup, path)) {
            if (stopped)
                startPlasmaShell();
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Could not restore the layout file.")}};
        }
    } else {
        QFile::remove(path);
    }

    if (restartKde) {
        if (stopped)
            startPlasmaShell();
        else
            reloadShell(target);
    } else {
        reloadShell(target);
    }
    m_lastBackup = id;
    emit lastBackupChanged();
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("backup"), id},
            {QStringLiteral("target"), target},
            {QStringLiteral("warnings"), QStringList{QStringLiteral("The previous layout was restored. The shell was asked to reload.")}}};
}

QStringList LayoutApplier::backups() const
{
    QStringList result;
    const QDir root(Paths::stateDir() + QStringLiteral("/layout-backups"));
    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QString &entry : entries) {
        if (QFileInfo::exists(root.filePath(entry) + QStringLiteral("/manifest.json")))
            result << entry;
    }
    return result;
}

bool LayoutApplier::backupFile(const QString &id, const QString &name, const QString &target, const QString &path,
                               QString *backupId, QString *error)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString backup = timestamp + QLatin1Char('-') + safeId(id);
    const QString dir = backupDir(backup);
    QDir().mkpath(dir + QStringLiteral("/files"));

    const bool existed = QFileInfo::exists(path);
    const QString fileRel = QStringLiteral("files/") + QFileInfo(path).fileName();
    if (existed && !QFile::copy(path, dir + QLatin1Char('/') + fileRel)) {
        if (error)
            *error = QStringLiteral("Could not back up the current layout.");
        return false;
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("id"), backup);
    manifest.insert(QStringLiteral("preset"), id);
    manifest.insert(QStringLiteral("name"), name);
    manifest.insert(QStringLiteral("target"), target);
    manifest.insert(QStringLiteral("path"), path);
    manifest.insert(QStringLiteral("existed"), existed);
    manifest.insert(QStringLiteral("file"), existed ? fileRel : QString());
    manifest.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));

    QSaveFile file(dir + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Could not write the layout backup manifest.");
        return false;
    }
    file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("Could not commit the layout backup manifest.");
        return false;
    }

    *backupId = backup;
    return true;
}

QString LayoutApplier::backupDir(const QString &id) const
{
    return Paths::stateDir() + QStringLiteral("/layout-backups/") + safeId(id);
}

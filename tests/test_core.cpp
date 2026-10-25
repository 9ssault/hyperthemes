#include <QtTest>

#include "applier.h"
#include "catalog.h"
#include "layoutapplier.h"
#include "palette.h"
#include "palettegenerator.h"
#include "paths.h"
#include "remoteinstaller.h"
#include "shellbuilder.h"
#include "templaterenderer.h"
#include "theme.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

namespace {
void writeFile(const QString &path, const QString &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(content.toUtf8());
    file.close();
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QVariantMap sampleTheme()
{
    return {
        {QStringLiteral("id"), QStringLiteral("test-theme")},
        {QStringLiteral("name"), QStringLiteral("Test Theme")},
        {QStringLiteral("mode"), QStringLiteral("dark")},
        {QStringLiteral("flavour"), QStringLiteral("default")},
        {QStringLiteral("variant"), QStringLiteral("tonalspot")},
        {QStringLiteral("targets"), QStringList{QStringLiteral("gtk"), QStringLiteral("qt")}},
        {QStringLiteral("palette"), QVariantMap{
             {QStringLiteral("background"), QStringLiteral("101014")},
             {QStringLiteral("onBackground"), QStringLiteral("f2ecf5")},
             {QStringLiteral("surface"), QStringLiteral("19151e")},
             {QStringLiteral("primary"), QStringLiteral("ff0000")},
             {QStringLiteral("secondary"), QStringLiteral("00ff00")},
             {QStringLiteral("tertiary"), QStringLiteral("0000ff")},
         }},
    };
}
} // namespace

class CoreTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_temp.isValid());
        m_realHome = qEnvironmentVariable("HOME", QDir::homePath());
        m_realConfig = qEnvironmentVariable("XDG_CONFIG_HOME", m_realHome + QStringLiteral("/.config"));
        m_realState = qEnvironmentVariable("XDG_STATE_HOME", m_realHome + QStringLiteral("/.local/state"));
        qputenv("HOME", m_temp.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", (m_temp.path() + QStringLiteral("/config")).toUtf8());
        qputenv("XDG_DATA_HOME", (m_temp.path() + QStringLiteral("/data")).toUtf8());
        qputenv("XDG_STATE_HOME", (m_temp.path() + QStringLiteral("/state")).toUtf8());
        qputenv("XDG_CACHE_HOME", (m_temp.path() + QStringLiteral("/cache")).toUtf8());
        qputenv("HYPER_THEMES_NO_RELOAD", "1");
    }

    void paletteHasMaterialFallbacks()
    {
        const QVariantMap palette = Palette::normalise(sampleTheme().value(QStringLiteral("palette")).toMap());
        QCOMPARE(palette.value(QStringLiteral("primary")).toString(), QStringLiteral("ff0000"));
        QCOMPARE(palette.value(QStringLiteral("background")).toString(), QStringLiteral("101014"));
        QVERIFY(palette.contains(QStringLiteral("surfaceContainerHigh")));
        QVERIFY(palette.contains(QStringLiteral("term15")));
    }

    void templatesRender()
    {
        const Theme theme = Theme::fromMap(sampleTheme());
        const QString waybar = TemplateRenderer::render(QStringLiteral("waybar.css"), theme);
        QVERIFY(waybar.contains(QStringLiteral("#ff0000")));
        QVERIFY(waybar.contains(QStringLiteral("#101014")));

        const QString kde = TemplateRenderer::render(QStringLiteral("kde.colors"), theme);
        QVERIFY(kde.contains(QStringLiteral("BackgroundNormal=16,16,20")));
        QVERIFY(kde.contains(QStringLiteral("DecorationFocus=255,0,0")));
    }

    void allTemplatesAreComplete()
    {
        const Theme theme = Theme::fromMap(sampleTheme());
        const QStringList templates = {
            QStringLiteral("kde.colors"), QStringLiteral("caelestia.json"), QStringLiteral("waybar.css"),
            QStringLiteral("gtk.css"), QStringLiteral("qt.conf"), QStringLiteral("konsole.colorscheme"),
            QStringLiteral("kitty.conf"), QStringLiteral("foot.ini"), QStringLiteral("spicetify.ini"),
            QStringLiteral("spicetify-user.css"), QStringLiteral("vesktop.css")
        };
        for (const QString &name : templates) {
            const QString rendered = TemplateRenderer::render(name, theme);
            QVERIFY2(!rendered.isEmpty(), qPrintable(name));
            QVERIFY2(!rendered.contains(QStringLiteral("{{")), qPrintable(name + QStringLiteral(" still has placeholders")));
        }
        QVERIFY(TemplateRenderer::render(QStringLiteral("kitty.conf"), theme).contains(QStringLiteral("color15")));
        QVERIFY(TemplateRenderer::render(QStringLiteral("kitty.conf"), theme).contains(QStringLiteral("color0 #")));
    }

    void themeMapKeepsPackageFields()
    {
        QVariantMap map = sampleTheme();
        map.insert(QStringLiteral("palette"), QVariantMap{});
        map.insert(QStringLiteral("packageId"), QStringLiteral("org.kde.test.lookandfeel"));
        map.insert(QStringLiteral("packageStructure"), QStringLiteral("Plasma/LookAndFeel"));
        map.insert(QStringLiteral("packageName"), QStringLiteral("Test Look"));

        const Theme theme = Theme::fromMap(map);
        QCOMPARE(theme.packageId, QStringLiteral("org.kde.test.lookandfeel"));
        QCOMPARE(theme.packageStructure, QStringLiteral("Plasma/LookAndFeel"));

        const QVariantMap roundTrip = theme.toMap();
        QCOMPARE(roundTrip.value(QStringLiteral("packageId")).toString(), theme.packageId);
        QCOMPARE(roundTrip.value(QStringLiteral("packageStructure")).toString(), theme.packageStructure);
        QCOMPARE(roundTrip.value(QStringLiteral("packageName")).toString(), theme.packageName);
    }

    void applyAndRollbackUserTargets()
    {
        const QString config = Paths::configHome();
        QDir(config).removeRecursively();
        QDir().mkpath(config + QStringLiteral("/gtk-3.0"));
        writeFile(config + QStringLiteral("/gtk-3.0/gtk.css"), QStringLiteral("/* user file */\n"));

        Applier applier;
        const QVariantMap result = applier.apply(sampleTheme(), {QStringLiteral("gtk"), QStringLiteral("qt")}, false);
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(),
                 qPrintable(result.value(QStringLiteral("error")).toString() + QStringLiteral(" / ")
                            + result.value(QStringLiteral("warnings")).toStringList().join(QStringLiteral("; "))));

        const QString gtkFile = config + QStringLiteral("/gtk-3.0/hyperthemes.css");
        const QString gtkImport = config + QStringLiteral("/gtk-3.0/gtk.css");
        const QString qtColors = config + QStringLiteral("/qt5ct/colors/HyperThemes.conf");
        const QString qtSettings = config + QStringLiteral("/qt5ct/qt5ct.conf");
        QVERIFY(QFileInfo::exists(gtkFile));
        QVERIFY(readFile(gtkFile).contains(QStringLiteral("#101014")));
        QVERIFY(readFile(gtkImport).startsWith(QStringLiteral("@import url(\"hyperthemes.css\");")));
        QVERIFY(QFileInfo::exists(qtColors));
        QVERIFY(readFile(qtColors).contains(QStringLiteral("[ColorScheme]")));
        QVERIFY(QFileInfo::exists(qtSettings));
        QVERIFY(readFile(qtSettings).contains(QStringLiteral("color_scheme=HyperThemes")));

        const QString backup = result.value(QStringLiteral("backup")).toString();
        QVERIFY(!backup.isEmpty());
        QVERIFY(applier.backups().contains(backup));
        QVERIFY(QFileInfo::exists(Paths::backupsDir() + QLatin1Char('/') + backup + QStringLiteral("/manifest.json")));

        const QVariantMap restored = applier.rollback(backup);
        QVERIFY(restored.value(QStringLiteral("ok")).toBool());
        QCOMPARE(readFile(gtkImport), QStringLiteral("/* user file */\n"));
        QVERIFY(!QFileInfo::exists(gtkFile));
        QVERIFY(!QFileInfo::exists(qtSettings));
        QVERIFY(!QFileInfo::exists(qtColors));
    }

    void catalogLoadsEveryThemeFile()
    {
        Catalog catalog;
        QElapsedTimer timer;
        timer.start();
        const QVariantList themes = catalog.load();
        const qint64 elapsed = timer.elapsed();
        qInfo() << "loaded" << themes.size() << "theme files in" << elapsed << "ms";

        QVERIFY2(themes.size() >= 1000, qPrintable(QStringLiteral("only %1 themes").arg(themes.size())));
        QVERIFY2(elapsed < 2000, qPrintable(QStringLiteral("catalog took %1 ms").arg(elapsed)));

        QSet<QString> categories;
        for (const QVariant &entry : themes) {
            const QVariantMap theme = entry.toMap();
            QVERIFY(!theme.value(QStringLiteral("id")).toString().isEmpty());
            QVERIFY(!theme.value(QStringLiteral("category")).toString().isEmpty());
            QVERIFY(!theme.value(QStringLiteral("source")).toString().isEmpty());
            QVERIFY(!theme.value(QStringLiteral("targets")).toStringList().isEmpty());
            QVERIFY(!theme.value(QStringLiteral("palette")).toMap().isEmpty());
            categories.insert(theme.value(QStringLiteral("category")).toString());
        }
        QVERIFY2(categories.size() >= 40, qPrintable(QStringLiteral("only %1 categories").arg(categories.size())));
    }

    void shellBuilderWritesQuickshellConfig()
    {
        ShellBuilder builder;
        QVERIFY(builder.widgets().size() >= 8);

        QVariantMap config = builder.defaults();
        config.insert(QStringLiteral("palette"), sampleTheme().value(QStringLiteral("palette")));
        config.insert(QStringLiteral("themeName"), QStringLiteral("Test Theme"));
        config.insert(QStringLiteral("name"), QStringLiteral("hyperthemes-test"));

        const QVariantMap result = builder.build(config);
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(),
                 qPrintable(result.value(QStringLiteral("error")).toString()));

        const QString dir = result.value(QStringLiteral("dir")).toString();
        QCOMPARE(dir, Paths::configHome() + QStringLiteral("/quickshell/hyperthemes-test"));

        const QStringList files = result.value(QStringLiteral("files")).toStringList();
        QVERIFY(files.contains(QStringLiteral("shell.qml")));
        QVERIFY(files.contains(QStringLiteral("Theme.qml")));
        QVERIFY(files.contains(QStringLiteral("bar/Bar.qml")));
        QVERIFY(files.contains(QStringLiteral("launcher/Launcher.qml")));
        QVERIFY(files.contains(QStringLiteral("dashboard/Dashboard.qml")));

        const QString theme = readFile(dir + QStringLiteral("/Theme.qml"));
        QVERIFY(theme.contains(QStringLiteral("#ff0000")));
        QVERIFY(theme.contains(QStringLiteral("barHeight: 34")));
        QVERIFY(theme.contains(QStringLiteral("Test Theme")));

        const QString dump = qEnvironmentVariable("HT_SHELL_DUMP");
        if (!dump.isEmpty()) {
            QDir(dump).removeRecursively();
            QDir().mkpath(dump);
            QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                const QString relative = QDir(dir).relativeFilePath(path);
                const QString target = dump + QLatin1Char('/') + relative;
                QDir().mkpath(QFileInfo(target).absolutePath());
                QFile::copy(path, target);
            }
        }

        // When Quickshell is installed, lint the generated tree so a template
        // change cannot ship syntactically broken QML.
        const QString imports = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
        const QString lint = QStandardPaths::findExecutable(QStringLiteral("qmllint"));
        if (!lint.isEmpty() && QFileInfo::exists(imports + QStringLiteral("/Quickshell"))) {
            const QStringList targets = {QStringLiteral("shell.qml"), QStringLiteral("Theme.qml"),
                                         QStringLiteral("bar/Bar.qml"),
                                         QStringLiteral("launcher/Launcher.qml"),
                                         QStringLiteral("dashboard/Dashboard.qml")};
            for (const QString &target : targets) {
                QProcess process;
                process.setWorkingDirectory(dir);
                process.start(lint, {target});
                QVERIFY(process.waitForFinished(20000));
                const QString output = QString::fromUtf8(process.readAllStandardOutput()
                                                         + process.readAllStandardError());
                QVERIFY2(process.exitCode() == 0,
                         qPrintable(target + QStringLiteral(": ") + output));
            }
        }

        const QString shell = readFile(dir + QStringLiteral("/shell.qml"));
        QVERIFY(shell.contains(QStringLiteral("ShellRoot")));
        QVERIFY(shell.contains(QStringLiteral("qs -c hyperthemes-test")));
        QVERIFY(shell.contains(QStringLiteral("onLauncherRequested: launcher.toggle()")));

        const QString bar = readFile(dir + QStringLiteral("/bar/Bar.qml"));
        QVERIFY(bar.contains(QStringLiteral("PanelWindow")));
        QVERIFY(bar.contains(QStringLiteral("top: true")));
        QVERIFY(bar.contains(QStringLiteral("SystemTray.items")));
        QVERIFY(bar.contains(QStringLiteral("theme.barHeight")));

        config.insert(QStringLiteral("position"), QStringLiteral("bottom"));
        config.insert(QStringLiteral("launcher"), false);
        config.insert(QStringLiteral("dashboard"), false);
        config.insert(QStringLiteral("zones"), QVariantMap{
            {QStringLiteral("left"), QStringList{QStringLiteral("logo")}},
            {QStringLiteral("center"), QStringList{QStringLiteral("media")}},
            {QStringLiteral("right"), QStringList{QStringLiteral("battery")}},
        });
        const QVariantMap second = builder.build(config);
        QVERIFY2(second.value(QStringLiteral("ok")).toBool(),
                 qPrintable(second.value(QStringLiteral("error")).toString()));
        QVERIFY(!second.value(QStringLiteral("backup")).toString().isEmpty());
        QVERIFY(builder.backups().size() >= 2);

        const QString bottomBar = readFile(dir + QStringLiteral("/bar/Bar.qml"));
        QVERIFY(bottomBar.contains(QStringLiteral("bottom: true")));
        QVERIFY(!bottomBar.contains(QStringLiteral("top: true")));
        QVERIFY(!bottomBar.contains(QStringLiteral("SystemTray.items")));
        QVERIFY(!QFileInfo::exists(dir + QStringLiteral("/launcher/Launcher.qml")));
        QVERIFY(!QFileInfo::exists(dir + QStringLiteral("/dashboard/Dashboard.qml")));
        QVERIFY(!readFile(dir + QStringLiteral("/shell.qml")).contains(QStringLiteral("Launcher {")));

        QVariantMap badName = config;
        badName.insert(QStringLiteral("name"), QStringLiteral("Bad Name!"));
        QVERIFY(!builder.build(badName).value(QStringLiteral("ok")).toBool());

        QVariantMap withoutPalette = builder.defaults();
        withoutPalette.insert(QStringLiteral("name"), QStringLiteral("no-palette"));
        const QVariantMap rejected = builder.build(withoutPalette);
        QVERIFY(!rejected.value(QStringLiteral("ok")).toBool());
        QVERIFY(rejected.value(QStringLiteral("error")).toString().contains(QStringLiteral("theme")));

        const QVariantMap restored = builder.restore(second.value(QStringLiteral("backup")).toString());
        QVERIFY2(restored.value(QStringLiteral("ok")).toBool(),
                 qPrintable(restored.value(QStringLiteral("error")).toString()));
        QVERIFY(QFileInfo::exists(dir + QStringLiteral("/launcher/Launcher.qml")));
    }

    void wallpaperOnlyApplyChecksTheFile()
    {
        Applier applier;
        const QVariantMap missing = applier.setWallpaper(m_temp.path() + QStringLiteral("/missing.png"));
        QCOMPARE(missing.value(QStringLiteral("ok")).toBool(), false);
        QVERIFY(missing.value(QStringLiteral("error")).toString().contains(QStringLiteral("Wallpaper")));
    }

    void caelestiaLayoutPresetAndRestore()
    {
        const QString shellPath = Paths::configHome() + QStringLiteral("/caelestia/shell.json");
        writeFile(shellPath, QStringLiteral("{\n  \"general\": {\"keep\": true},\n  \"bar\": {\"custom\": true, \"position\": \"top\"}\n}\n"));

        LayoutApplier applier;
        QCOMPARE(applier.presets().size(), 6);
        QVERIFY(applier.kdeScript(QStringLiteral("kde-top")).contains(QStringLiteral("new Panel(\"org.kde.panel\")")));
        QVERIFY(applier.kdeScript(QStringLiteral("kde-top")).contains(QStringLiteral("knownWidgetTypes")));
        QVERIFY(applier.kdeScript(QStringLiteral("kde-top")).contains(QStringLiteral("org.kde.plasma.kickoff")));
        QVERIFY(applier.kdeScript(QStringLiteral("kde-bottom")).contains(QStringLiteral("dodgewindows")));
        QVERIFY(applier.kdeScript(QStringLiteral("caelestia-top")).isEmpty());

        const QVariantMap result = applier.apply(QStringLiteral("caelestia-bottom"));
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(),
                 qPrintable(result.value(QStringLiteral("error")).toString()));
        QVERIFY(!result.value(QStringLiteral("backup")).toString().isEmpty());

        QFile file(shellPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QCOMPARE(root.value(QStringLiteral("bar")).toObject().value(QStringLiteral("position")).toString(),
                 QStringLiteral("bottom"));
        QCOMPARE(root.value(QStringLiteral("bar")).toObject().value(QStringLiteral("custom")).toBool(), true);
        QCOMPARE(root.value(QStringLiteral("general")).toObject().value(QStringLiteral("keep")).toBool(), true);
        QVERIFY(root.value(QStringLiteral("bar")).toObject().value(QStringLiteral("entries")).toArray().size() > 4);

        QVERIFY(applier.backups().contains(result.value(QStringLiteral("backup")).toString()));
        const QVariantMap restored = applier.restore(result.value(QStringLiteral("backup")).toString());
        QVERIFY2(restored.value(QStringLiteral("ok")).toBool(),
                 qPrintable(restored.value(QStringLiteral("error")).toString()));

        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject original = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QCOMPARE(original.value(QStringLiteral("bar")).toObject().value(QStringLiteral("position")).toString(),
                 QStringLiteral("top"));
    }

    void kdeLayoutLive()
    {
        if (!qEnvironmentVariableIsSet("HYPER_THEMES_LIVE_PLASMA"))
            QSKIP("Set HYPER_THEMES_LIVE_PLASMA=1 to script the running Plasma session");

        const QString qdbus = QStandardPaths::findExecutable(QStringLiteral("qdbus6"));
        if (qdbus.isEmpty())
            QSKIP("qdbus6 is not installed");

        const auto panelCount = [&qdbus]() -> int {
            QProcess process;
            process.start(qdbus, {QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                                  QStringLiteral("org.kde.PlasmaShell.evaluateScript"),
                                  QStringLiteral("print(\"count=\" + panels().length);")});
            if (!process.waitForFinished(8000) || process.exitCode() != 0)
                return -1;
            const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("count=(\\d+)"))
                                                      .match(QString::fromUtf8(process.readAllStandardOutput()));
            return match.hasMatch() ? match.captured(1).toInt() : -1;
        };

        const int before = panelCount();
        if (before < 0)
            QSKIP("Plasma is not running on this session");

        const QString configFile = m_realConfig + QStringLiteral("/plasma-org.kde.plasma.desktop-appletsrc");
        const QString original = readFile(configFile);
        const bool hadPanelPlugin = original.contains(QStringLiteral("plugin=org.kde.panel"));
        QVERIFY(QFileInfo::exists(configFile));

        const bool hadNoReload = qEnvironmentVariableIsSet("HYPER_THEMES_NO_RELOAD");
        qunsetenv("HYPER_THEMES_NO_RELOAD");
        qputenv("XDG_CONFIG_HOME", m_realConfig.toUtf8());
        qputenv("XDG_STATE_HOME", m_realState.toUtf8());
        const auto restoreEnvironment = qScopeGuard([&] {
            qputenv("XDG_CONFIG_HOME", (m_temp.path() + QStringLiteral("/config")).toUtf8());
            qputenv("XDG_STATE_HOME", (m_temp.path() + QStringLiteral("/state")).toUtf8());
            if (hadNoReload)
                qputenv("HYPER_THEMES_NO_RELOAD", "1");
        });
        Q_UNUSED(restoreEnvironment);

        LayoutApplier applier;
        const QVariantMap applied = applier.apply(QStringLiteral("kde-top"));
        QVERIFY2(applied.value(QStringLiteral("ok")).toBool(),
                 qPrintable(applied.value(QStringLiteral("error")).toString()));

        QTRY_COMPARE_WITH_TIMEOUT(panelCount(), 1, 15000);

        const QVariantMap restored = applier.restore(applied.value(QStringLiteral("backup")).toString());
        QVERIFY2(restored.value(QStringLiteral("ok")).toBool(),
                 qPrintable(restored.value(QStringLiteral("error")).toString()));

        QTRY_COMPARE_WITH_TIMEOUT(panelCount(), before, 25000);
        QTRY_COMPARE_WITH_TIMEOUT(readFile(configFile).contains(QStringLiteral("plugin=org.kde.panel")),
                                  hadPanelPlugin, 15000);
        QVERIFY2(readFile(configFile).contains(QStringLiteral("wallpaperplugin=org.kde.image")),
                 "the desktop containment was not restored");
    }

    void remoteInstallerExtractsArchive()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("bsdtar")).isEmpty())
            QSKIP("bsdtar is not installed");

        const QString sourceDir = m_temp.path() + QStringLiteral("/archive-source");
        QDir().mkpath(sourceDir);
        writeFile(sourceDir + QStringLiteral("/Nested.colors"),
                  QStringLiteral("[Colors:Window]\nBackgroundNormal=101014\nForegroundNormal=f2ecf5\n"));
        const QString archive = m_temp.path() + QStringLiteral("/theme.tar.gz");
        QProcess tar;
        tar.start(QStringLiteral("bsdtar"), {QStringLiteral("-czf"), archive, QStringLiteral("-C"), sourceDir, QStringLiteral("Nested.colors")});
        QVERIFY(tar.waitForFinished(10000));
        QCOMPARE(tar.exitCode(), 0);

        RemoteInstaller installer;
        QSignalSpy finished(&installer, &RemoteInstaller::finished);
        QSignalSpy failed(&installer, &RemoteInstaller::failed);
        installer.install({
            {QStringLiteral("id"), QStringLiteral("remote-archive-test")},
            {QStringLiteral("name"), QStringLiteral("Archive Test")},
            {QStringLiteral("source"), QStringLiteral("github")},
            {QStringLiteral("downloadUrl"), QUrl::fromLocalFile(archive).toString()},
            {QStringLiteral("downloadName"), QStringLiteral("theme.tar.gz")},
        });

        QVERIFY(finished.wait(5000) || failed.count() > 0);
        QVERIFY2(finished.count() > 0, qPrintable(failed.count() > 0 ? failed.first().first().toString() : QStringLiteral("no result")));
        const QStringList files = finished.first().first().toMap().value(QStringLiteral("files")).toStringList();
        QCOMPARE(files.size(), 1);
        QVERIFY(files.first().endsWith(QStringLiteral("/Nested.colors")));
        QVERIFY(QFileInfo::exists(files.first()));
    }

    void remoteInstallerRejectsSymlinks()
    {
        if (QStandardPaths::findExecutable(QStringLiteral("bsdtar")).isEmpty())
            QSKIP("bsdtar is not installed");

        const QString sourceDir = m_temp.path() + QStringLiteral("/symlink-source");
        QDir().mkpath(sourceDir);
        const QString escaped = sourceDir + QStringLiteral("/outside");
        QVERIFY(QFile::link(QStringLiteral("/etc/passwd"), escaped));
        const QString archive = m_temp.path() + QStringLiteral("/symlink.tar.gz");
        QProcess tar;
        tar.start(QStringLiteral("bsdtar"), {QStringLiteral("-czf"), archive, QStringLiteral("-C"), sourceDir, QStringLiteral("outside")});
        QVERIFY(tar.waitForFinished(10000));
        QCOMPARE(tar.exitCode(), 0);

        RemoteInstaller installer;
        QSignalSpy failed(&installer, &RemoteInstaller::failed);
        installer.install({
            {QStringLiteral("id"), QStringLiteral("remote-symlink-test")},
            {QStringLiteral("name"), QStringLiteral("Symlink Test")},
            {QStringLiteral("downloadUrl"), QUrl::fromLocalFile(archive).toString()},
            {QStringLiteral("downloadName"), QStringLiteral("theme.tar.gz")},
        });

        QVERIFY(failed.wait(5000));
        QVERIFY(failed.first().first().toString().contains(QStringLiteral("symbolic link")));
    }

    void remoteInstallerCopiesDirectPalette()
    {
        const QString sourcePath = m_temp.path() + QStringLiteral("/Direct.colors");
        writeFile(sourcePath, QStringLiteral("[Colors:Window]\nBackgroundNormal=101014\nForegroundNormal=f2ecf5\n"));

        RemoteInstaller installer;
        QSignalSpy finished(&installer, &RemoteInstaller::finished);
        QSignalSpy failed(&installer, &RemoteInstaller::failed);
        installer.install({
            {QStringLiteral("id"), QStringLiteral("remote-test")},
            {QStringLiteral("name"), QStringLiteral("Remote Test")},
            {QStringLiteral("source"), QStringLiteral("kde-store")},
            {QStringLiteral("downloadUrl"), QUrl::fromLocalFile(sourcePath).toString()},
            {QStringLiteral("downloadName"), QStringLiteral("Direct.colors")},
        });

        QVERIFY(finished.wait(5000) || failed.count() > 0);
        QVERIFY2(finished.count() > 0, qPrintable(failed.count() > 0 ? failed.first().first().toString() : QStringLiteral("no result")));

        const QVariantMap result = finished.first().first().toMap();
        const QStringList files = result.value(QStringLiteral("files")).toStringList();
        QCOMPARE(files.size(), 1);
        QVERIFY(QFileInfo::exists(files.first()));
        QVERIFY(readFile(files.first()).contains(QStringLiteral("BackgroundNormal=101014")));
        QVERIFY(files.first().startsWith(Paths::themesDir()));
    }

    void matugenPalette()
    {
        PaletteGenerator generator;
        if (!generator.available())
            QSKIP("matugen is not installed");

        const QString imagePath = m_temp.path() + QStringLiteral("/wallpaper.png");
        QImage image(96, 96, QImage::Format_RGB32);
        image.fill(QColor(QStringLiteral("#d03040")));
        QVERIFY(image.save(imagePath));

        const QVariantMap result = generator.fromWallpaper(imagePath, QStringLiteral("dark"),
                                                           QStringLiteral("scheme-tonal-spot"), 0.0);
        QVERIFY2(result.value(QStringLiteral("ok")).toBool(),
                 qPrintable(result.value(QStringLiteral("error")).toString()));
        const QVariantMap palette = result.value(QStringLiteral("palette")).toMap();
        QVERIFY(palette.contains(QStringLiteral("background")));
        QVERIFY(palette.contains(QStringLiteral("primary")));
        QVERIFY(palette.contains(QStringLiteral("term0")));
    }

private:
    QTemporaryDir m_temp;
    QString m_realHome;
    QString m_realConfig;
    QString m_realState;
};

QTEST_MAIN(CoreTest)
#include "test_core.moc"

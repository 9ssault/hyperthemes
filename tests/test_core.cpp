#include <QtTest>

#include "applier.h"
#include "layoutapplier.h"
#include "palette.h"
#include "palettegenerator.h"
#include "paths.h"
#include "remoteinstaller.h"
#include "templaterenderer.h"
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
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

    void caelestiaLayoutPresetAndRestore()
    {
        const QString shellPath = Paths::configHome() + QStringLiteral("/caelestia/shell.json");
        writeFile(shellPath, QStringLiteral("{\n  \"general\": {\"keep\": true},\n  \"bar\": {\"custom\": true, \"position\": \"top\"}\n}\n"));

        LayoutApplier applier;
        QCOMPARE(applier.presets().size(), 6);
        QVERIFY(applier.kdeScript(QStringLiteral("kde-top")).contains(QStringLiteral("new Panel")));
        QVERIFY(applier.kdeScript(QStringLiteral("kde-top")).contains(QStringLiteral("org.kde.plasma.kickoff")));
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
};

QTEST_MAIN(CoreTest)
#include "test_core.moc"

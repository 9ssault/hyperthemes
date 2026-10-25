#include <QtTest>

#include "applier.h"
#include "palette.h"
#include "palettegenerator.h"
#include "paths.h"
#include "templaterenderer.h"
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

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

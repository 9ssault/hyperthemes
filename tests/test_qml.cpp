#include <QtTest>

#include "applier.h"
#include "catalog.h"
#include "layoutapplier.h"
#include "localsource.h"
#include "palettegenerator.h"
#include "remoteinstaller.h"
#include "shellbuilder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>

class QmlTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_temp.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("HyperThemes"));
        QCoreApplication::setApplicationName(QStringLiteral("HyperThemesTest"));
        qputenv("HOME", m_temp.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", (m_temp.path() + QStringLiteral("/config")).toUtf8());
        qputenv("XDG_DATA_HOME", (m_temp.path() + QStringLiteral("/data")).toUtf8());
        qputenv("XDG_STATE_HOME", (m_temp.path() + QStringLiteral("/state")).toUtf8());
        qputenv("XDG_CACHE_HOME", (m_temp.path() + QStringLiteral("/cache")).toUtf8());
        qputenv("HYPER_THEMES_NO_RELOAD", "1");

        QQuickStyle::setStyle(QStringLiteral("Basic"));
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "Applier", &m_applier);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "Catalog", &m_catalog);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LocalSource", &m_localSource);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "PaletteGenerator", &m_paletteGenerator);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "RemoteInstaller", &m_remoteInstaller);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LayoutApplier", &m_layoutApplier);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "ShellBuilder", &m_shellBuilder);

        m_engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        QVERIFY2(!m_engine.rootObjects().isEmpty(), "Main.qml produced no root object");
        m_root = m_engine.rootObjects().first();
        QTRY_VERIFY_WITH_TIMEOUT(m_root->property("themes").toList().size() > 0, 5000);
    }

    void catalogAndInspector()
    {
        QCOMPARE(m_root->property("layoutPresets").toList().size(), 6);

        const QVariantList themes = m_root->property("themes").toList();
        QVERIFY2(themes.size() >= 1000, qPrintable(QStringLiteral("catalog has %1 themes").arg(themes.size())));
        QCOMPARE(m_root->property("filtered").toList().size(), themes.size());

        bool hasBase16 = false;
        bool hasCaelestiaFamily = false;
        bool hasCategories = true;
        for (const QVariant &entry : themes) {
            const QVariantMap theme = entry.toMap();
            if (theme.value(QStringLiteral("source")).toString() == QStringLiteral("base16"))
                hasBase16 = true;
            if (theme.value(QStringLiteral("id")).toString().startsWith(QStringLiteral("bundled-gruvbox")))
                hasCaelestiaFamily = true;
            if (theme.value(QStringLiteral("category")).toString().isEmpty())
                hasCategories = false;
        }
        QVERIFY(hasBase16);
        QVERIFY(hasCaelestiaFamily);
        QVERIFY(hasCategories);

        const QVariantList categories = m_root->property("categoryModel").toList();
        QVERIFY(!categories.isEmpty());
        int familyCount = 0;
        for (const QVariant &entry : categories) {
            if (entry.toMap().value(QStringLiteral("kind")).toString() == QStringLiteral("family"))
                ++familyCount;
        }
        QVERIFY2(familyCount > 10, qPrintable(QStringLiteral("only %1 families").arg(familyCount)));

        auto *grid = m_root->findChild<QObject *>(QStringLiteral("themeGrid"));
        QVERIFY(grid);
        const qreal gridWidth = grid->property("width").toReal();
        QVERIFY2(gridWidth > 600.0, qPrintable(QStringLiteral("grid is only %1 px wide").arg(gridWidth)));
        QVERIFY(grid->property("columns").toInt() >= 2);
        QVERIFY(grid->property("cellWidth").toReal() * grid->property("columns").toInt() <= gridWidth);

        const QVariant first = themes.first();
        QVERIFY(QMetaObject::invokeMethod(m_root, "selectTheme", Q_ARG(QVariant, first)));
        QTRY_COMPARE(m_root->property("selected").toMap().value(QStringLiteral("id")).toString(),
                     first.toMap().value(QStringLiteral("id")).toString());
        QVERIFY(!m_root->property("targetOptions").toList().isEmpty());

        m_root->setProperty("search", QStringLiteral("zenburn"));
        QVERIFY(QMetaObject::invokeMethod(m_root, "recompute"));
        QTRY_VERIFY(m_root->property("filtered").toList().size() >= 1);
        bool namedZenburn = false;
        for (const QVariant &entry : m_root->property("filtered").toList()) {
            if (entry.toMap().value(QStringLiteral("name")).toString()
                    .contains(QStringLiteral("Zenburn"), Qt::CaseInsensitive))
                namedZenburn = true;
        }
        QVERIFY(namedZenburn);

        m_root->setProperty("search", QString());
        m_root->setProperty("targetFilter", QStringLiteral("terminals"));
        QVERIFY(QMetaObject::invokeMethod(m_root, "recompute"));
        QVERIFY(m_root->property("filtered").toList().size() > 0);
        const QVariantList terminalThemes = m_root->property("filtered").toList();
        QVERIFY(terminalThemes.first().toMap().value(QStringLiteral("targets")).toStringList()
                    .contains(QStringLiteral("konsole")));
        QCOMPARE(m_root->property("filtered").toList().size(), terminalThemes.size());
        m_root->setProperty("targetFilter", QStringLiteral("All"));
        QVERIFY(QMetaObject::invokeMethod(m_root, "recompute"));

        m_root->setProperty("categoryFilter", QStringLiteral("Catppuccin"));
        QVERIFY(QMetaObject::invokeMethod(m_root, "recompute"));
        const QVariantList family = m_root->property("filtered").toList();
        QVERIFY(!family.isEmpty());
        QCOMPARE(family.first().toMap().value(QStringLiteral("category")).toString(),
                 QStringLiteral("Catppuccin"));
        QVERIFY(family.size() < themes.size());
        m_root->setProperty("categoryFilter", QStringLiteral("All"));
        QVERIFY(QMetaObject::invokeMethod(m_root, "recompute"));
    }

    void selectedPaletteThemesTheApp()
    {
        const QVariantList themes = m_root->property("themes").toList();
        QVariantMap dracula;
        for (const QVariant &entry : themes) {
            const QVariantMap theme = entry.toMap();
            if (theme.value(QStringLiteral("name")).toString().contains(QStringLiteral("Dracula"))) {
                dracula = theme;
                break;
            }
        }
        QVERIFY(!dracula.isEmpty());

        QVERIFY(QMetaObject::invokeMethod(m_root, "selectTheme", Q_ARG(QVariant, dracula)));
        QVariant primary;
        QVERIFY(QMetaObject::invokeMethod(m_root, "paletteColor", Q_RETURN_ARG(QVariant, primary),
                                          Q_ARG(QVariant, QStringLiteral("primary")),
                                          Q_ARG(QVariant, QStringLiteral("#000000"))));
        const QString expected = dracula.value(QStringLiteral("palette")).toMap()
                                     .value(QStringLiteral("primary")).toString();
        QCOMPARE(primary.toString().remove(QLatin1Char('#')), expected);

        QTRY_COMPARE(m_root->property("appAccent").value<QColor>().name().remove(QLatin1Char('#')), expected);
        QTRY_COMPARE(m_root->property("appBackground").value<QColor>().name().remove(QLatin1Char('#')),
                     dracula.value(QStringLiteral("palette")).toMap().value(QStringLiteral("background")).toString());

        m_root->setProperty("followPalette", false);
        QVERIFY(QMetaObject::invokeMethod(m_root, "paletteColor", Q_RETURN_ARG(QVariant, primary),
                                          Q_ARG(QVariant, QStringLiteral("primary")),
                                          Q_ARG(QVariant, QStringLiteral("#0d0a10"))));
        QCOMPARE(primary.toString(), QStringLiteral("#0d0a10"));
        m_root->setProperty("followPalette", true);
    }

    void quickshellBuilderPage()
    {
        const QVariantList themes = m_root->property("themes").toList();
        QVERIFY(!themes.isEmpty());
        QVERIFY(QMetaObject::invokeMethod(m_root, "selectTheme", Q_ARG(QVariant, themes.first())));

        m_root->setProperty("page", QStringLiteral("shell"));
        QVERIFY(m_root->property("shellWidgets").toList().size() >= 8);

        m_root->setProperty("shellName", QStringLiteral("test-shell"));
        m_root->setProperty("shellBarHeight", 40);
        m_root->setProperty("shellLauncher", false);
        QVERIFY(QMetaObject::invokeMethod(m_root, "shellBuild"));
        QTRY_VERIFY(!m_root->property("shellFiles").toStringList().isEmpty());
        QCOMPARE(m_root->property("shellError").toString(), QString());

        const QString dir = m_root->property("shellResult").toString();
        QVERIFY(dir.endsWith(QStringLiteral("/quickshell/test-shell")));
        QVERIFY(QFileInfo::exists(dir + QStringLiteral("/shell.qml")));
        QVERIFY(QFileInfo::exists(dir + QStringLiteral("/bar/Bar.qml")));
        QVERIFY(!QFileInfo::exists(dir + QStringLiteral("/launcher/Launcher.qml")));
        QVERIFY(QFileInfo::exists(dir + QStringLiteral("/dashboard/Dashboard.qml")));

        const QVariantMap zones = m_root->property("shellZones").toMap();
        QVERIFY(zones.value(QStringLiteral("center")).toStringList().contains(QStringLiteral("clock")));
        QVERIFY(QMetaObject::invokeMethod(m_root, "shellSetWidget", Q_ARG(QVariant, QStringLiteral("clock")),
                                          Q_ARG(QVariant, QStringLiteral("left")), Q_ARG(QVariant, true)));
        QVERIFY(m_root->property("shellZones").toMap().value(QStringLiteral("left")).toStringList()
                    .contains(QStringLiteral("clock")));
        QVERIFY(!m_root->property("shellZones").toMap().value(QStringLiteral("center")).toStringList()
                     .contains(QStringLiteral("clock")));

        m_root->setProperty("page", QStringLiteral("explore"));
    }

    void wallpaperSelectionIsRemembered()
    {
        const QString image = m_temp.path() + QStringLiteral("/my wallpaper.png");
        QFile file(image);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("png");
        file.close();

        m_root->setProperty("wallpaperPath", image);
        QTRY_COMPARE(m_root->property("wallpaperUrl").toString(),
                     QStringLiteral("file://") + m_temp.path() + QStringLiteral("/my%20wallpaper.png"));
        QCOMPARE(m_root->property("wallpaperPath").toString(), image);
    }

    void localImports()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());

        QFile caelestia(folder.path() + QStringLiteral("/dracula.txt"));
        QVERIFY(caelestia.open(QIODevice::WriteOnly | QIODevice::Text));
        caelestia.write("background 101014\nprimary ff0000\nsecondary 00ff00\ntertiary 0000ff\n");
        caelestia.close();

        QFile kde(folder.path() + QStringLiteral("/Test.colors"));
        QVERIFY(kde.open(QIODevice::WriteOnly | QIODevice::Text));
        kde.write("[Colors:Window]\nBackgroundNormal=101014\nForegroundNormal=f2ecf5\nDecorationFocus=ff0000\n"
                  "[Colors:View]\nBackgroundNormal=19151e\nForegroundNormal=f2ecf5\n"
                  "[Colors:Button]\nBackgroundNormal=221b2a\n"
                  "[Colors:Selection]\nBackgroundNormal=ff0000\nForegroundNormal=101014\n");
        kde.close();

        m_root->setProperty("search", QString());
        m_root->setProperty("page", QStringLiteral("explore"));
        QVERIFY(QMetaObject::invokeMethod(m_root, "recompute"));

        QVERIFY(QMetaObject::invokeMethod(m_root, "importPath", Q_ARG(QVariant, folder.path())));
        QTRY_COMPARE(m_root->property("localThemes").toList().size(), 2);

        bool foundCaelestia = false;
        bool foundKde = false;
        const QVariantList locals = m_root->property("localThemes").toList();
        for (const QVariant &entry : locals) {
            const QVariantMap theme = entry.toMap();
            if (theme.value(QStringLiteral("name")).toString() == QStringLiteral("dracula")) {
                foundCaelestia = true;
                QCOMPARE(theme.value(QStringLiteral("palette")).toMap().value(QStringLiteral("primary")).toString(),
                         QStringLiteral("ff0000"));
            }
            if (theme.value(QStringLiteral("name")).toString() == QStringLiteral("Test"))
                foundKde = true;
        }
        QVERIFY(foundCaelestia);
        QVERIFY(foundKde);
        QCOMPARE(m_root->property("page").toString(), QStringLiteral("mine"));
    }

    void layoutPageApplyAndRestore()
    {
        m_root->setProperty("page", QStringLiteral("layouts"));
        QVERIFY(QMetaObject::invokeMethod(m_root, "applyLayout", Q_ARG(QVariant, QStringLiteral("caelestia-top"))));
        QTRY_VERIFY(m_root->property("layoutBackups").toStringList().size() > 0);

        const QString shellPath = m_temp.path() + QStringLiteral("/config/caelestia/shell.json");
        QFile shell(shellPath);
        QVERIFY(shell.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(shell.readAll()).object();
        shell.close();
        QCOMPARE(root.value(QStringLiteral("bar")).toObject().value(QStringLiteral("position")).toString(),
                 QStringLiteral("top"));

        const QString backup = m_root->property("layoutBackups").toStringList().first();
        QVERIFY(QMetaObject::invokeMethod(m_root, "restoreLayout", Q_ARG(QVariant, backup)));
        QTRY_VERIFY(!QFileInfo::exists(shellPath));
    }

    void themedScreenshot()
    {
        const QString output = qEnvironmentVariable("HYPER_THEMES_SCREENSHOT");
        if (output.isEmpty())
            QSKIP("Set HYPER_THEMES_SCREENSHOT=/path/to.png to capture the window");

        const QString wanted = qEnvironmentVariable("HYPER_THEMES_SCREENSHOT_THEME", QStringLiteral("Dracula"));
        QVariantMap chosen;
        const QVariantList themes = m_root->property("themes").toList();
        for (const QVariant &entry : themes) {
            const QVariantMap theme = entry.toMap();
            if (theme.value(QStringLiteral("name")).toString().contains(wanted, Qt::CaseInsensitive)) {
                chosen = theme;
                break;
            }
        }
        QVERIFY(!chosen.isEmpty());
        m_root->setProperty("page", qEnvironmentVariable("HYPER_THEMES_SCREENSHOT_PAGE",
                                                       QStringLiteral("explore")));
        QVERIFY(QMetaObject::invokeMethod(m_root, "selectTheme", Q_ARG(QVariant, chosen)));

        auto *window = qobject_cast<QQuickWindow *>(m_root);
        QVERIFY(window);

        const QString size = qEnvironmentVariable("HYPER_THEMES_SCREENSHOT_SIZE");
        if (!size.isEmpty()) {
            const QStringList parts = size.split(QLatin1Char('x'));
            if (parts.size() == 2) {
                m_root->setProperty("width", parts.at(0).toInt());
                m_root->setProperty("height", parts.at(1).toInt());
            }
        }
        QTest::qWait(400);
        const QImage image = window->grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY2(image.save(output), qPrintable(output));
    }

    void remoteEntryBecomesInstallable()
    {
        const QVariantMap item = {
            {QStringLiteral("id"), 42},
            {QStringLiteral("name"), QStringLiteral("Remote Test")},
            {QStringLiteral("summary"), QStringLiteral("A test scheme")},
            {QStringLiteral("personid"), QStringLiteral("tester")},
            {QStringLiteral("detailpage"), QStringLiteral("https://example.invalid/p/42")},
            {QStringLiteral("downloadlink1"), QStringLiteral("https://example.invalid/test.colors")},
            {QStringLiteral("downloadname1"), QStringLiteral("test.colors")},
        };
        QVariant remote;
        QVERIFY(QMetaObject::invokeMethod(m_root, "remoteThemeFromKde", Q_RETURN_ARG(QVariant, remote),
                                          Q_ARG(QVariant, item), Q_ARG(QVariant, QStringLiteral("Plasma color scheme"))));
        const QVariantMap remoteMap = remote.toMap();
        QCOMPARE(remoteMap.value(QStringLiteral("source")).toString(), QStringLiteral("kde-store"));
        QCOMPARE(remoteMap.value(QStringLiteral("needsInstall")).toBool(), true);
        QCOMPARE(remoteMap.value(QStringLiteral("downloadUrl")).toString(), QStringLiteral("https://example.invalid/test.colors"));
        QCOMPARE(remoteMap.value(QStringLiteral("palette")).toMap().isEmpty(), true);

        const QString path = m_temp.path() + QStringLiteral("/Remote.colors");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("[Colors:Window]\nBackgroundNormal=101014\nForegroundNormal=f2ecf5\nDecorationFocus=ff0000\n"
                   "[Colors:View]\nBackgroundNormal=19151e\nForegroundNormal=f2ecf5\n"
                   "[Colors:Button]\nBackgroundNormal=221b2a\n"
                   "[Colors:Selection]\nBackgroundNormal=ff0000\nForegroundNormal=101014\n");
        file.close();

        m_root->setProperty("selected", remoteMap);
        QVERIFY(QMetaObject::invokeMethod(m_root, "handleRemoteInstalled", Q_ARG(QVariant, QVariant(QVariantMap{
            {QStringLiteral("ok"), true},
            {QStringLiteral("files"), QStringList{path}},
        }))));

        const QVariantMap selected = m_root->property("selected").toMap();
        QCOMPARE(selected.value(QStringLiteral("needsInstall")).toBool(), false);
        QCOMPARE(selected.value(QStringLiteral("installPath")).toString(), path);
        QCOMPARE(selected.value(QStringLiteral("source")).toString(), QStringLiteral("kde-store"));
        QCOMPARE(selected.value(QStringLiteral("palette")).toMap().value(QStringLiteral("primary")).toString(), QStringLiteral("ff0000"));
    }

    void remotePackageBecomesApplyable()
    {
        const QVariantMap target = {
            {QStringLiteral("id"), QStringLiteral("kde-777")},
            {QStringLiteral("name"), QStringLiteral("Breeze Test")},
            {QStringLiteral("source"), QStringLiteral("kde-store")},
            {QStringLiteral("needsInstall"), true},
        };
        m_root->setProperty("remoteInstallTarget", target);
        QVERIFY(QMetaObject::invokeMethod(m_root, "handleRemoteInstalled", Q_ARG(QVariant, QVariant(QVariantMap{
            {QStringLiteral("ok"), true},
            {QStringLiteral("package"), QVariantMap{
                 {QStringLiteral("id"), QStringLiteral("org.kde.test.lookandfeel")},
                 {QStringLiteral("structure"), QStringLiteral("Plasma/LookAndFeel")},
                 {QStringLiteral("name"), QStringLiteral("Test Look")},
             }},
        }))));

        const QVariantMap selected = m_root->property("selected").toMap();
        QCOMPARE(selected.value(QStringLiteral("packageId")).toString(), QStringLiteral("org.kde.test.lookandfeel"));
        QCOMPARE(selected.value(QStringLiteral("packageStructure")).toString(), QStringLiteral("Plasma/LookAndFeel"));
        QCOMPARE(selected.value(QStringLiteral("needsInstall")).toBool(), false);
        QCOMPARE(selected.value(QStringLiteral("targets")).toStringList(), QStringList{QStringLiteral("kde")});
        QCOMPARE(selected.value(QStringLiteral("name")).toString(), QStringLiteral("Breeze Test"));
    }

private:
    QTemporaryDir m_temp;
    Applier m_applier;
    Catalog m_catalog;
    LocalSource m_localSource;
    PaletteGenerator m_paletteGenerator;
    RemoteInstaller m_remoteInstaller;
    LayoutApplier m_layoutApplier;
    ShellBuilder m_shellBuilder;
    QQmlApplicationEngine m_engine;
    QObject *m_root = nullptr;
};

QTEST_MAIN(QmlTest)
#include "test_qml.moc"

#include <QtTest>

#include "applier.h"
#include "layoutapplier.h"
#include "localsource.h"
#include "palettegenerator.h"
#include "remoteinstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTemporaryDir>

class QmlTest : public QObject {
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

        QQuickStyle::setStyle(QStringLiteral("Basic"));
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "Applier", &m_applier);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LocalSource", &m_localSource);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "PaletteGenerator", &m_paletteGenerator);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "RemoteInstaller", &m_remoteInstaller);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LayoutApplier", &m_layoutApplier);

        m_engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        QVERIFY2(!m_engine.rootObjects().isEmpty(), "Main.qml produced no root object");
        m_root = m_engine.rootObjects().first();
        QTRY_VERIFY_WITH_TIMEOUT(m_root->property("themes").toList().size() > 0, 5000);
    }

    void catalogAndInspector()
    {
        QCOMPARE(m_root->property("layoutPresets").toList().size(), 6);

        const QVariantList themes = m_root->property("themes").toList();
        QCOMPARE(themes.size(), 15);
        QCOMPARE(m_root->property("filtered").toList().size(), 15);

        const QVariant first = themes.first();
        QVERIFY(QMetaObject::invokeMethod(m_root, "selectTheme", Q_ARG(QVariant, first)));
        QTRY_COMPARE(m_root->property("selected").toMap().value(QStringLiteral("id")).toString(),
                     first.toMap().value(QStringLiteral("id")).toString());
        QVERIFY(!m_root->property("targetOptions").toList().isEmpty());

        m_root->setProperty("search", QStringLiteral("dracula"));
        QVERIFY(QMetaObject::invokeMethod(m_root, "recompute"));
        QTRY_COMPARE(m_root->property("filtered").toList().size(), 1);
        QCOMPARE(m_root->property("filtered").toList().first().toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Dracula"));
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
    LocalSource m_localSource;
    PaletteGenerator m_paletteGenerator;
    RemoteInstaller m_remoteInstaller;
    LayoutApplier m_layoutApplier;
    QQmlApplicationEngine m_engine;
    QObject *m_root = nullptr;
};

QTEST_MAIN(QmlTest)
#include "test_qml.moc"

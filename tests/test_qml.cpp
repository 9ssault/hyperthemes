#include <QtTest>

#include "applier.h"
#include "localsource.h"
#include "palettegenerator.h"

#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTemporaryDir>

class QmlTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "Applier", &m_applier);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LocalSource", &m_localSource);
        qmlRegisterSingletonInstance("HyperThemes", 1, 0, "PaletteGenerator", &m_paletteGenerator);

        m_engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        QVERIFY2(!m_engine.rootObjects().isEmpty(), "Main.qml produced no root object");
        m_root = m_engine.rootObjects().first();
        QTRY_VERIFY_WITH_TIMEOUT(m_root->property("themes").toList().size() > 0, 5000);
    }

    void catalogAndInspector()
    {
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

private:
    Applier m_applier;
    LocalSource m_localSource;
    PaletteGenerator m_paletteGenerator;
    QQmlApplicationEngine m_engine;
    QObject *m_root = nullptr;
};

QTEST_MAIN(QmlTest)
#include "test_qml.moc"

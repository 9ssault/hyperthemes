#include "applier.h"
#include "layoutapplier.h"
#include "localsource.h"
#include "palettegenerator.h"
#include "remoteinstaller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("HyperThemes"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setOrganizationName(QStringLiteral("HyperThemes"));
    QGuiApplication::setDesktopFileName(QStringLiteral("hyperthemes"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/data/hyperthemes.svg")));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    Applier applier;
    LocalSource localSource;
    PaletteGenerator paletteGenerator;
    RemoteInstaller remoteInstaller;
    LayoutApplier layoutApplier;
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "Applier", &applier);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LocalSource", &localSource);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "PaletteGenerator", &paletteGenerator);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "RemoteInstaller", &remoteInstaller);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LayoutApplier", &layoutApplier);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

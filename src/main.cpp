#include "applier.h"
#include "catalog.h"
#include "cli.h"
#include "layoutapplier.h"
#include "localsource.h"
#include "palettegenerator.h"
#include "remoteinstaller.h"
#include "shellbuilder.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QStringList rawArguments;
    for (int i = 0; i < argc; ++i)
        rawArguments << QString::fromLocal8Bit(argv[i]);

    if (Cli::requested(rawArguments)) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("HyperThemes"));
        QCoreApplication::setApplicationVersion(QStringLiteral("0.2.0"));
        QCoreApplication::setOrganizationName(QStringLiteral("HyperThemes"));

        const Cli::Result result = Cli::run(QCoreApplication::arguments());
        if (!result.output.isEmpty()) {
            QTextStream(stdout) << result.output << Qt::endl;
        }
        if (!result.error.isEmpty()) {
            QTextStream(stderr) << result.error << Qt::endl;
        }
        return result.exitCode;
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("HyperThemes"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    QGuiApplication::setOrganizationName(QStringLiteral("HyperThemes"));
    QGuiApplication::setDesktopFileName(QStringLiteral("hyperthemes"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/data/hyperthemes.svg")));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    Applier applier;
    Catalog catalog;
    LocalSource localSource;
    PaletteGenerator paletteGenerator;
    RemoteInstaller remoteInstaller;
    LayoutApplier layoutApplier;
    ShellBuilder shellBuilder;
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "Applier", &applier);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "Catalog", &catalog);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LocalSource", &localSource);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "PaletteGenerator", &paletteGenerator);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "RemoteInstaller", &remoteInstaller);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "LayoutApplier", &layoutApplier);
    qmlRegisterSingletonInstance("HyperThemes", 1, 0, "ShellBuilder", &shellBuilder);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}

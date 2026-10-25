#include "palettegenerator.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace {
QString stripHash(QString value)
{
    value = value.trimmed();
    if (value.startsWith(QLatin1Char('#')))
        value.remove(0, 1);
    return value.toLower();
}
} // namespace

PaletteGenerator::PaletteGenerator(QObject *parent)
    : QObject(parent)
{
}

bool PaletteGenerator::available() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("matugen")).isEmpty();
}

QString PaletteGenerator::version() const
{
    QProcess process;
    process.start(QStringLiteral("matugen"), {QStringLiteral("--version")});
    if (!process.waitForFinished(5000))
        return {};
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QVariantMap PaletteGenerator::fromWallpaper(const QString &path, const QString &mode,
                                            const QString &scheme, double contrast) const
{
    if (!available())
        return {{QStringLiteral("error"), QStringLiteral("Matugen is not installed")}};
    if (path.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("No wallpaper was selected")}};

    QStringList arguments = {
        QStringLiteral("image"), path,
        QStringLiteral("--type"), scheme,
        QStringLiteral("--mode"), mode,
        QStringLiteral("--prefer"), QStringLiteral("saturation"),
        QStringLiteral("--json"), QStringLiteral("hex"),
        QStringLiteral("--old-json-output"),
        QStringLiteral("--dry-run"),
    };
    if (contrast != 0.0)
        arguments << QStringLiteral("--contrast") << QString::number(contrast);

    QProcess process;
    process.setProgram(QStringLiteral("matugen"));
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(5000))
        return {{QStringLiteral("error"), QStringLiteral("Could not start Matugen")}};
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(1000);
        return {{QStringLiteral("error"), QStringLiteral("Matugen timed out")}};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return {{QStringLiteral("error"), error.isEmpty() ? QStringLiteral("Matugen could not read that image") : error}};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (!document.isObject() || parseError.error != QJsonParseError::NoError)
        return {{QStringLiteral("error"), QStringLiteral("Matugen returned invalid JSON")}};

    const QJsonObject root = document.object();
    const QJsonObject colors = root.value(QStringLiteral("colors")).toObject();
    if (colors.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("Matugen returned no colors")}};

    QVariantMap palette;
    for (auto it = colors.begin(); it != colors.end(); ++it) {
        const QJsonObject value = it.value().toObject();
        QString hex = value.value(mode).toString();
        if (hex.isEmpty())
            hex = value.value(QStringLiteral("default")).toString();
        if (!hex.isEmpty())
            palette.insert(it.key(), stripHash(hex));
    }

    const QJsonObject base16 = root.value(QStringLiteral("base16")).toObject();
    for (auto it = base16.begin(); it != base16.end(); ++it) {
        const QString name = it.key();
        if (!name.startsWith(QLatin1String("base")))
            continue;
        const QJsonObject value = it.value().toObject();
        QString hex = value.value(mode).toString();
        if (hex.isEmpty())
            hex = value.value(QStringLiteral("default")).toString();
        if (hex.isEmpty())
            continue;
        bool ok = false;
        const int index = name.mid(4).toInt(&ok, 16);
        if (ok && index >= 0 && index < 16)
            palette.insert(QStringLiteral("term%1").arg(index), stripHash(hex));
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("palette"), palette},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("scheme"), scheme},
        {QStringLiteral("wallpaper"), path},
    };
}

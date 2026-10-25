#include "palette.h"

#include <QRegularExpression>

namespace Palette {

QString stripHash(const QString &value)
{
    QString result = value.trimmed();
    if (result.startsWith(QLatin1Char('#')))
        result.remove(0, 1);
    return result.toLower();
}

QString withHash(const QString &value)
{
    QString result = stripHash(value);
    if (result.size() == 3) {
        result = QStringLiteral("%1%1%2%2%3%3")
                     .arg(result.at(0), result.at(1), result.at(2));
    }
    if (result.size() != 6 || !QRegularExpression(QStringLiteral("^[0-9a-f]{6}$")).match(result).hasMatch())
        return QStringLiteral("#000000");
    return QLatin1Char('#') + result;
}

QString toHex(const QColor &colour)
{
    return colour.name(QColor::HexRgb).mid(1).toLower();
}

QColor colour(const QVariantMap &palette, const QString &key, const QColor &fallback)
{
    const QString value = stripHash(palette.value(key).toString());
    if (value.size() == 6) {
        const QColor parsed(QLatin1Char('#') + value);
        if (parsed.isValid())
            return parsed;
    }
    return fallback;
}

QString hex(const QVariantMap &palette, const QString &key, const QString &fallback)
{
    const QString value = stripHash(palette.value(key).toString());
    if (value.size() == 6)
        return value;
    return stripHash(fallback);
}

QString rgb(const QVariantMap &palette, const QString &key)
{
    const QColor c = colour(palette, key, Qt::black);
    return QStringLiteral("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
}

QString onColour(const QColor &background)
{
    // Rec. 709 luma is enough here and avoids a dependency on a contrast package.
    const double luma = 0.2126 * background.redF() + 0.7152 * background.greenF() + 0.0722 * background.blueF();
    return luma > 0.55 ? QStringLiteral("000000") : QStringLiteral("ffffff");
}

QVariantMap normalise(QVariantMap palette)
{
    for (auto it = palette.begin(); it != palette.end(); ++it) {
        if (it.value().canConvert<QString>())
            it.value() = stripHash(it.value().toString());
    }

    const QString background = hex(palette, QStringLiteral("background"), QStringLiteral("101014"));
    const QString surface = hex(palette, QStringLiteral("surface"), background);
    const QString onBackground = hex(palette, QStringLiteral("onBackground"),
                                     onColour(colour(palette, QStringLiteral("background"), QColor(background))));
    const QString onSurface = hex(palette, QStringLiteral("onSurface"), onBackground);
    const QString primary = hex(palette, QStringLiteral("primary"), QStringLiteral("8ab4f8"));
    const QString onPrimary = hex(palette, QStringLiteral("onPrimary"),
                                  onColour(colour(palette, QStringLiteral("primary"), QColor(QStringLiteral("#8ab4f8")))));
    const QString secondary = hex(palette, QStringLiteral("secondary"), primary);
    const QString tertiary = hex(palette, QStringLiteral("tertiary"), secondary);
    const QString error = hex(palette, QStringLiteral("error"), QStringLiteral("ff6b6b"));

    const QVector<QPair<QString, QString>> defaults = {
        {QStringLiteral("background"), background},
        {QStringLiteral("onBackground"), onBackground},
        {QStringLiteral("surface"), surface},
        {QStringLiteral("surfaceDim"), background},
        {QStringLiteral("surfaceBright"), hex(palette, QStringLiteral("surfaceContainerHigh"), surface)},
        {QStringLiteral("surfaceContainerLowest"), background},
        {QStringLiteral("surfaceContainerLow"), hex(palette, QStringLiteral("surfaceContainer"), surface)},
        {QStringLiteral("surfaceContainer"), surface},
        {QStringLiteral("surfaceContainerHigh"), surface},
        {QStringLiteral("surfaceContainerHighest"), surface},
        {QStringLiteral("onSurface"), onSurface},
        {QStringLiteral("surfaceVariant"), hex(palette, QStringLiteral("surfaceContainerHigh"), surface)},
        {QStringLiteral("onSurfaceVariant"), onSurface},
        {QStringLiteral("outline"), onSurface},
        {QStringLiteral("outlineVariant"), hex(palette, QStringLiteral("surfaceVariant"), surface)},
        {QStringLiteral("inverseSurface"), onSurface},
        {QStringLiteral("inverseOnSurface"), background},
        {QStringLiteral("shadow"), QStringLiteral("000000")},
        {QStringLiteral("scrim"), QStringLiteral("000000")},
        {QStringLiteral("surfaceTint"), primary},
        {QStringLiteral("primary"), primary},
        {QStringLiteral("onPrimary"), onPrimary},
        {QStringLiteral("primaryContainer"), hex(palette, QStringLiteral("surfaceContainerHigh"), surface)},
        {QStringLiteral("onPrimaryContainer"), onSurface},
        {QStringLiteral("inversePrimary"), onPrimary},
        {QStringLiteral("primaryFixed"), primary},
        {QStringLiteral("primaryFixedDim"), primary},
        {QStringLiteral("onPrimaryFixed"), onPrimary},
        {QStringLiteral("onPrimaryFixedVariant"), onPrimary},
        {QStringLiteral("secondary"), secondary},
        {QStringLiteral("onSecondary"), onPrimary},
        {QStringLiteral("secondaryContainer"), hex(palette, QStringLiteral("surfaceContainerHigh"), surface)},
        {QStringLiteral("onSecondaryContainer"), onSurface},
        {QStringLiteral("secondaryFixed"), secondary},
        {QStringLiteral("secondaryFixedDim"), secondary},
        {QStringLiteral("onSecondaryFixed"), onPrimary},
        {QStringLiteral("onSecondaryFixedVariant"), onPrimary},
        {QStringLiteral("tertiary"), tertiary},
        {QStringLiteral("onTertiary"), onPrimary},
        {QStringLiteral("tertiaryContainer"), hex(palette, QStringLiteral("surfaceContainerHigh"), surface)},
        {QStringLiteral("onTertiaryContainer"), onSurface},
        {QStringLiteral("tertiaryFixed"), tertiary},
        {QStringLiteral("tertiaryFixedDim"), tertiary},
        {QStringLiteral("onTertiaryFixed"), onPrimary},
        {QStringLiteral("onTertiaryFixedVariant"), onPrimary},
        {QStringLiteral("error"), error},
        {QStringLiteral("onError"), onPrimary},
        {QStringLiteral("errorContainer"), error},
        {QStringLiteral("onErrorContainer"), onPrimary},
    };
    for (const auto &[key, value] : defaults) {
        if (hex(palette, key, QString()).size() != 6)
            palette.insert(key, value);
    }

    for (int i = 0; i < 16; ++i) {
        const QString key = QStringLiteral("term%1").arg(i);
        if (hex(palette, key, QString()).size() != 6) {
            const QColor fallback = i == 0 ? QColor(QStringLiteral("#1a1a1a"))
                                           : (i == 7 ? QColor(QStringLiteral("#eeeeee"))
                                                     : colour(palette, i == 1 ? QStringLiteral("primary") : QStringLiteral("secondary")));
            palette.insert(key, toHex(fallback));
        }
    }

    return palette;
}

} // namespace Palette

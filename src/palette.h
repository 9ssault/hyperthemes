#pragma once

#include <QColor>
#include <QString>
#include <QVariantMap>

namespace Palette {

QVariantMap normalise(QVariantMap palette);
QString hex(const QVariantMap &palette, const QString &key, const QString &fallback = QStringLiteral("000000"));
QString rgb(const QVariantMap &palette, const QString &key);
QColor colour(const QVariantMap &palette, const QString &key, const QColor &fallback = Qt::black);
QString onColour(const QColor &background);
QString stripHash(const QString &value);
QString withHash(const QString &value);
QString toHex(const QColor &colour);

} // namespace Palette

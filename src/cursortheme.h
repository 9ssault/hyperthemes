#pragma once

#include "theme.h"

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QString>
#include <QVariantList>
#include <QVector>

namespace CursorTheme {

constexpr int minimumSize = 16;
constexpr int maximumSize = 128;
constexpr int defaultSize = 32;

int clampSize(int size);

/// Pixel sizes embedded in every cursor file: the requested size plus scaled
/// steps for HiDPI screens, never above maximumSize.
QVector<int> imageSizes(int size);

/// The XDG icon theme name and install folder every cursor file lands in.
QString themeName();
QString installDir();

/// `~/.icons/default/index.theme`, which makes "default" inherit our theme so
/// apps that never ask for a cursor theme by name pick it up.
QString defaultThemePath();
QByteArray defaultThemeFile();

/// One entry per drawable shape: id, label, hotspotX and hotspotY in the
/// 24-unit design box. Used by the inspector to list every generated pointer.
QVariantList shapes();

/// Renders a single shape for the inspector preview. `id` is one of the ids
/// from shapes(); unknown ids return a null image.
QImage render(const QVariantMap &palette, const QString &id, int size);

/// Cursor files keyed by their path relative to the theme folder. Everything is
/// generated in memory: the applier writes each file through the normal backup
/// pipeline, so a cursor apply can still be rolled back.
QHash<QString, QByteArray> build(const Theme &theme, int size = defaultSize);

} // namespace CursorTheme

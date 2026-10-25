#pragma once

#include "theme.h"

#include <QByteArray>
#include <QHash>
#include <QString>

namespace CursorTheme {

/// The XDG icon theme name and install folder every cursor file lands in.
QString themeName();
QString installDir();

/// `~/.icons/default/index.theme`, which makes "default" inherit our theme so
/// apps that never ask for a cursor theme by name pick it up.
QString defaultThemePath();
QByteArray defaultThemeFile();

/// Cursor files keyed by their path relative to the theme folder. Everything is
/// generated in memory: the applier writes each file through the normal backup
/// pipeline, so a cursor apply can still be rolled back.
QHash<QString, QByteArray> build(const Theme &theme);

} // namespace CursorTheme

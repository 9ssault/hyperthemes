#pragma once

#include <QString>

namespace Paths {

QString home();
QString configHome();
QString dataHome();
QString stateHome();
QString cacheHome();
QString stateDir();
QString backupsDir();
QString cacheDir();
QString themesDir();
QString templatePath(const QString &name);
QString expand(const QString &path);
QString userName();

} // namespace Paths

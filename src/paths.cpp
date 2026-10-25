#include "paths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace Paths {

QString home()
{
    return QDir::homePath();
}

QString configHome()
{
    return qEnvironmentVariableIsSet("XDG_CONFIG_HOME") ? qEnvironmentVariable("XDG_CONFIG_HOME")
                                                        : home() + QStringLiteral("/.config");
}

QString dataHome()
{
    return qEnvironmentVariableIsSet("XDG_DATA_HOME") ? qEnvironmentVariable("XDG_DATA_HOME")
                                                      : home() + QStringLiteral("/.local/share");
}

QString stateHome()
{
    return qEnvironmentVariableIsSet("XDG_STATE_HOME") ? qEnvironmentVariable("XDG_STATE_HOME")
                                                       : home() + QStringLiteral("/.local/state");
}

QString cacheHome()
{
    return qEnvironmentVariableIsSet("XDG_CACHE_HOME") ? qEnvironmentVariable("XDG_CACHE_HOME")
                                                       : home() + QStringLiteral("/.cache");
}

QString stateDir()
{
    return stateHome() + QStringLiteral("/hyperthemes");
}

QString backupsDir()
{
    return stateDir() + QStringLiteral("/backups");
}

QString cacheDir()
{
    return cacheHome() + QStringLiteral("/hyperthemes");
}

QString templatePath(const QString &name)
{
    return QStringLiteral(":/templates/") + name;
}

QString catalogPath()
{
    return QStringLiteral(":/catalog/themes.json");
}

QString expand(const QString &path)
{
    if (path == QLatin1String("~"))
        return home();
    if (path.startsWith(QLatin1String("~/")))
        return home() + path.mid(1);
    return path;
}

QString userName()
{
    return qEnvironmentVariable("USER", QStringLiteral("user"));
}

} // namespace Paths

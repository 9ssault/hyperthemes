#include "localsource.h"

#include "paths.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QUrl>

LocalSource::LocalSource(QObject *parent)
    : QObject(parent)
{
}

QStringList LocalSource::scan(const QString &folder) const
{
    QString root = folder;
    if (root.startsWith(QLatin1String("file:")))
        root = QUrl(root).toLocalFile();

    QStringList files;
    QDirIterator it(root, {QStringLiteral("*.json"), QStringLiteral("*.txt"), QStringLiteral("*.colors")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        files << it.next();
    files.sort();
    return files;
}

QString LocalSource::readText(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString LocalSource::defaultCaelestiaDir() const
{
    return Paths::home() + QStringLiteral("/.local/lib/caelestia/schemes");
}

QString LocalSource::defaultKdeColorDir() const
{
    return Paths::dataHome() + QStringLiteral("/color-schemes");
}

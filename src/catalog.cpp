#include "catalog.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace {

QString categoryOf(const QString &path)
{
    const QString folder = QFileInfo(path).dir().dirName();
    return folder.isEmpty() ? QStringLiteral("themes") : folder;
}

} // namespace

Catalog::Catalog(QObject *parent)
    : QObject(parent)
{
}

QVariantList Catalog::load() const
{
    QVariantList themes;
    QDirIterator it(QStringLiteral(":/catalog"), {QStringLiteral("*.json")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            continue;
        QVariantMap theme = document.object().toVariantMap();
        if (theme.value(QStringLiteral("id")).toString().isEmpty())
            continue;
        if (!theme.contains(QStringLiteral("category")))
            theme.insert(QStringLiteral("category"), categoryOf(path));
        themes << theme;
    }

    std::sort(themes.begin(), themes.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap left = a.toMap();
        const QVariantMap right = b.toMap();
        const int byCategory = left.value(QStringLiteral("category")).toString()
                                   .localeAwareCompare(right.value(QStringLiteral("category")).toString());
        if (byCategory != 0)
            return byCategory < 0;
        return left.value(QStringLiteral("name")).toString()
            .localeAwareCompare(right.value(QStringLiteral("name")).toString()) < 0;
    });
    return themes;
}

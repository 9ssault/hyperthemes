#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

struct Theme {
    QString id;
    QString name;
    QString description;
    QString author;
    QString license;
    QString source;
    QString sourceUrl;
    QString kind = QStringLiteral("static");
    QString mode = QStringLiteral("dark");
    QString flavour = QStringLiteral("default");
    QString variant = QStringLiteral("tonalspot");
    QString wallpaper;
    QString preview;
    QString installPath;
    QString packageId;
    QString packageStructure;
    QString packageName;
    QStringList targets;
    QVariantMap palette;
    bool remote = false;
    bool installable = true;
    int stars = 0;

    QVariantMap toMap() const;
    static Theme fromMap(const QVariantMap &map);
};

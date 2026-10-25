#pragma once

#include <QObject>
#include <QStringList>

class LocalSource : public QObject {
    Q_OBJECT

public:
    explicit LocalSource(QObject *parent = nullptr);

    Q_INVOKABLE QStringList scan(const QString &folder) const;
    Q_INVOKABLE QString readText(const QString &path) const;
    Q_INVOKABLE QString defaultCaelestiaDir() const;
    Q_INVOKABLE QString defaultKdeColorDir() const;
};

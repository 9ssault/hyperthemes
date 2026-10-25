#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

/// Writes a self-contained Quickshell configuration from the selected palette
/// and the bar/launcher/dashboard choices made in the Shell page.
class ShellBuilder : public QObject {
    Q_OBJECT

public:
    explicit ShellBuilder(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList widgets() const;
    Q_INVOKABLE QVariantMap defaults() const;
    Q_INVOKABLE QVariantMap build(const QVariantMap &config);
    Q_INVOKABLE QStringList backups() const;
    Q_INVOKABLE QVariantMap restore(const QString &backupId);
    Q_INVOKABLE bool quickshellAvailable() const;
    Q_INVOKABLE QString configDir(const QString &name) const;

private:
    QString backupTree(const QString &name, const QString &dir, bool existed);
};

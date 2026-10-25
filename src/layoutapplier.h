#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class LayoutApplier : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastBackup READ lastBackup NOTIFY lastBackupChanged)

public:
    explicit LayoutApplier(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList presets() const;
    Q_INVOKABLE QVariantMap apply(const QString &id);
    Q_INVOKABLE QVariantMap restore(const QString &backupId = {});
    Q_INVOKABLE QStringList backups() const;
    Q_INVOKABLE QString kdeScript(const QString &id) const;

    QString lastBackup() const { return m_lastBackup; }

signals:
    void lastBackupChanged();

private:
    QVariantMap applyCaelestia(const QString &id, const QString &name);
    QVariantMap applyKde(const QString &id, const QString &name);
    bool backupFile(const QString &id, const QString &name, const QString &target, const QString &path,
                    QString *backupId, QString *error);
    QString backupDir(const QString &id) const;

    QString m_lastBackup;
};

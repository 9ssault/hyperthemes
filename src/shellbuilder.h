#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class QProcess;

/// Writes a self-contained Quickshell configuration from the selected palette
/// and the bar/launcher/dashboard choices made in the Shell page.
class ShellBuilder : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString runLog READ runLog NOTIFY runLogChanged)

public:
    explicit ShellBuilder(QObject *parent = nullptr);
    ~ShellBuilder() override;

    Q_INVOKABLE QVariantList widgets() const;
    Q_INVOKABLE QVariantMap defaults() const;
    Q_INVOKABLE QVariantMap build(const QVariantMap &config);
    Q_INVOKABLE QVariantMap load(const QString &name);
    Q_INVOKABLE QVariantList configs() const;
    Q_INVOKABLE QVariantMap renameConfig(const QString &name, const QString &newName);
    Q_INVOKABLE QVariantMap removeConfig(const QString &name);
    Q_INVOKABLE QVariantMap lint(const QString &name);
    Q_INVOKABLE QVariantMap run(const QString &name);
    Q_INVOKABLE void stopRun();
    Q_INVOKABLE QVariantMap exportConfig(const QString &name, const QString &targetPath = {});
    Q_INVOKABLE QStringList backups() const;
    Q_INVOKABLE QVariantList backupEntries() const;
    Q_INVOKABLE QVariantMap restore(const QString &backupId);
    Q_INVOKABLE QVariantMap settings() const;
    Q_INVOKABLE void saveSettings(const QVariantMap &settings);
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE bool quickshellAvailable() const;
    Q_INVOKABLE QString configDir(const QString &name) const;

    bool isRunning() const;
    QString runLog() const { return m_runLog; }

signals:
    void runningChanged();
    void runLogChanged();

private:
    QString backupTree(const QString &name, const QString &dir, bool existed);
    void collectRunOutput();
    bool validName(const QString &name) const;

    QProcess *m_run = nullptr;
    QString m_runLog;
    QString m_runName;
};

#pragma once

#include "theme.h"

#include <QObject>
#include <QVariantMap>
#include <QVector>

class Applier : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lastBackup READ lastBackup NOTIFY lastBackupChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit Applier(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap apply(const QVariantMap &theme, const QStringList &targets, bool systemWide);
    Q_INVOKABLE QVariantMap rollback(const QString &backupId = {});
    Q_INVOKABLE QStringList backups() const;

    QString lastBackup() const { return m_lastBackup; }
    QString status() const { return m_status; }

signals:
    void lastBackupChanged();
    void statusChanged();

private:
    struct Entry {
        QString path;
        bool existed = false;
        QString backupRel;
    };
    struct Session {
        QString id;
        QString dir;
        QString themeId;
        bool system = false;
        QVector<Entry> entries;
        QStringList applied;
        QStringList warnings;
    };

    bool backupFile(Session &session, const QString &path, bool system);
    bool writeFile(Session &session, const QString &path, const QByteArray &data, bool system);
    bool ensureInclude(Session &session, const QString &path, const QString &line);
    bool setQtScheme(Session &session, const QString &path);
    void writeManifest(const Session &session) const;
    void setStatus(const QString &status);

    QString makeKde(const Theme &theme) const;
    QString makeCaelestia(const Theme &theme) const;
    QString makeSpicetify(const Theme &theme) const;
    QString makeVesktop(const Theme &theme) const;
    QString makeKonsole(const Theme &theme) const;
    QString makeKitty(const Theme &theme) const;
    QString makeFoot(const Theme &theme) const;

    void applyWallpaper(Session &session, const Theme &theme);
    void applyKde(Session &session, const Theme &theme, bool systemWide);
    void applyCaelestia(Session &session, const Theme &theme);
    void applyWaybar(Session &session, const Theme &theme);
    void applyGtk(Session &session, const Theme &theme);
    void applyQt(Session &session, const Theme &theme);
    void applyKonsole(Session &session, const Theme &theme);
    void applyKitty(Session &session, const Theme &theme);
    void applyFoot(Session &session, const Theme &theme);
    void applySpicetify(Session &session, const Theme &theme);
    void applyVesktop(Session &session, const Theme &theme);

    static bool run(const QString &program, const QStringList &args, int timeoutMs = 15000, QString *output = nullptr);
    static bool runPrivileged(const QStringList &args, int timeoutMs = 60000, QString *output = nullptr);
    static QString timestampId(const QString &themeId);
    static QString sessionDir(const QString &id);

    QString m_lastBackup;
    QString m_status;
};

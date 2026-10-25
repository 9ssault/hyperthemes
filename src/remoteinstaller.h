#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

class RemoteInstaller : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit RemoteInstaller(QObject *parent = nullptr);

    Q_INVOKABLE void install(const QVariantMap &theme);
    Q_INVOKABLE void cancel();

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }

signals:
    void busyChanged();
    void statusChanged();
    void finished(const QVariantMap &result);
    void failed(const QString &message);

private:
    void startDownload(const QUrl &url);
    void finishDownload();
    void processDownload();
    void collectFiles(const QString &root, const QString &stableRoot);
    void finishWithFiles(const QStringList &files);
    void fail(const QString &message);
    void setBusy(bool busy);
    void setStatus(const QString &status);

    QNetworkAccessManager *m_manager = nullptr;
    QNetworkReply *m_reply = nullptr;
    QFile *m_file = nullptr;
    QVariantMap m_theme;
    QString m_id;
    QString m_name;
    QString m_root;
    QString m_archive;
    bool m_busy = false;
    bool m_finished = false;
    QString m_status;
};

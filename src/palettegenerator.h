#pragma once

#include <QObject>
#include <QVariantMap>

class PaletteGenerator : public QObject {
    Q_OBJECT

public:
    explicit PaletteGenerator(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap fromWallpaper(const QString &path, const QString &mode = QStringLiteral("dark"),
                                          const QString &scheme = QStringLiteral("scheme-tonal-spot"),
                                          double contrast = 0.0) const;
    Q_INVOKABLE QString version() const;
    Q_INVOKABLE bool available() const;
};

#pragma once

#include "theme.h"

#include <QAbstractListModel>
#include <QVector>

class ThemeModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        AuthorRole,
        LicenseRole,
        SourceRole,
        SourceUrlRole,
        KindRole,
        ModeRole,
        PrimaryRole,
        BackgroundRole,
        SurfaceRole,
        OnSurfaceRole,
        SecondaryRole,
        TertiaryRole,
        ErrorRole,
        PreviewRole,
        WallpaperRole,
        TargetsRole,
        InstallableRole,
        RemoteRole,
        StarsRole,
        InstallPathRole,
        PaletteRole,
    };
    Q_ENUM(Roles)

    explicit ThemeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setThemes(const QVector<Theme> &themes);
    const Theme *themeAt(int row) const;

private:
    QVector<Theme> m_themes;
};

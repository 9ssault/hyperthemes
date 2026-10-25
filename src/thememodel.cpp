#include "thememodel.h"

ThemeModel::ThemeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ThemeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_themes.size();
}

QVariant ThemeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_themes.size())
        return {};

    const Theme &theme = m_themes.at(index.row());
    const auto colour = [&theme](const QString &key) {
        return theme.palette.value(key).toString();
    };

    switch (role) {
    case IdRole:
        return theme.id;
    case NameRole:
        return theme.name;
    case DescriptionRole:
        return theme.description;
    case AuthorRole:
        return theme.author;
    case LicenseRole:
        return theme.license;
    case SourceRole:
        return theme.source;
    case SourceUrlRole:
        return theme.sourceUrl;
    case KindRole:
        return theme.kind;
    case ModeRole:
        return theme.mode;
    case PrimaryRole:
        return colour(QStringLiteral("primary"));
    case BackgroundRole:
        return colour(QStringLiteral("background"));
    case SurfaceRole:
        return colour(QStringLiteral("surface"));
    case OnSurfaceRole:
        return colour(QStringLiteral("onSurface"));
    case SecondaryRole:
        return colour(QStringLiteral("secondary"));
    case TertiaryRole:
        return colour(QStringLiteral("tertiary"));
    case ErrorRole:
        return colour(QStringLiteral("error"));
    case PreviewRole:
        return theme.preview;
    case WallpaperRole:
        return theme.wallpaper;
    case TargetsRole:
        return theme.targets;
    case InstallableRole:
        return theme.installable;
    case RemoteRole:
        return theme.remote;
    case StarsRole:
        return theme.stars;
    case InstallPathRole:
        return theme.installPath;
    case PaletteRole:
        return theme.palette;
    default:
        return {};
    }
}

QHash<int, QByteArray> ThemeModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {AuthorRole, "author"},
        {LicenseRole, "license"},
        {SourceRole, "source"},
        {SourceUrlRole, "sourceUrl"},
        {KindRole, "kind"},
        {ModeRole, "mode"},
        {PrimaryRole, "primary"},
        {BackgroundRole, "background"},
        {SurfaceRole, "surface"},
        {OnSurfaceRole, "onSurface"},
        {SecondaryRole, "secondary"},
        {TertiaryRole, "tertiary"},
        {ErrorRole, "error"},
        {PreviewRole, "preview"},
        {WallpaperRole, "wallpaper"},
        {TargetsRole, "targets"},
        {InstallableRole, "installable"},
        {RemoteRole, "remote"},
        {StarsRole, "stars"},
        {InstallPathRole, "installPath"},
        {PaletteRole, "palette"},
    };
}

void ThemeModel::setThemes(const QVector<Theme> &themes)
{
    beginResetModel();
    m_themes = themes;
    endResetModel();
}

const Theme *ThemeModel::themeAt(int row) const
{
    if (row < 0 || row >= m_themes.size())
        return nullptr;
    return &m_themes.at(row);
}

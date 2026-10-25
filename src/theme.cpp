#include "theme.h"

QVariantMap Theme::toMap() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("description"), description},
        {QStringLiteral("author"), author},
        {QStringLiteral("license"), license},
        {QStringLiteral("source"), source},
        {QStringLiteral("sourceUrl"), sourceUrl},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("flavour"), flavour},
        {QStringLiteral("variant"), variant},
        {QStringLiteral("wallpaper"), wallpaper},
        {QStringLiteral("preview"), preview},
        {QStringLiteral("installPath"), installPath},
        {QStringLiteral("targets"), targets},
        {QStringLiteral("palette"), palette},
        {QStringLiteral("remote"), remote},
        {QStringLiteral("installable"), installable},
        {QStringLiteral("stars"), stars},
    };
}

Theme Theme::fromMap(const QVariantMap &map)
{
    Theme theme;
    theme.id = map.value(QStringLiteral("id")).toString();
    theme.name = map.value(QStringLiteral("name")).toString();
    theme.description = map.value(QStringLiteral("description")).toString();
    theme.author = map.value(QStringLiteral("author")).toString();
    theme.license = map.value(QStringLiteral("license")).toString();
    theme.source = map.value(QStringLiteral("source")).toString();
    theme.sourceUrl = map.value(QStringLiteral("sourceUrl")).toString();
    theme.kind = map.value(QStringLiteral("kind"), QStringLiteral("static")).toString();
    theme.mode = map.value(QStringLiteral("mode"), QStringLiteral("dark")).toString();
    theme.flavour = map.value(QStringLiteral("flavour"), QStringLiteral("default")).toString();
    theme.variant = map.value(QStringLiteral("variant"), QStringLiteral("tonalspot")).toString();
    theme.wallpaper = map.value(QStringLiteral("wallpaper")).toString();
    theme.preview = map.value(QStringLiteral("preview")).toString();
    theme.installPath = map.value(QStringLiteral("installPath")).toString();
    theme.targets = map.value(QStringLiteral("targets")).toStringList();
    theme.palette = map.value(QStringLiteral("palette")).toMap();
    theme.remote = map.value(QStringLiteral("remote")).toBool();
    theme.installable = map.value(QStringLiteral("installable"), true).toBool();
    theme.stars = map.value(QStringLiteral("stars")).toInt();
    return theme;
}

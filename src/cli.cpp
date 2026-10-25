#include "cli.h"

#include "applier.h"
#include "catalog.h"
#include "shellbuilder.h"

#include <QCoreApplication>
#include <QPair>
#include <QSet>
#include <QVector>

namespace {

const QVector<QPair<QString, QString>> &knownTargets()
{
    static const QVector<QPair<QString, QString>> targets = {
        {QStringLiteral("kde"), QStringLiteral("KDE Plasma color scheme")},
        {QStringLiteral("caelestia"), QStringLiteral("Caelestia / Quickshell")},
        {QStringLiteral("waybar"), QStringLiteral("Waybar")},
        {QStringLiteral("gtk"), QStringLiteral("GTK 3 and 4")},
        {QStringLiteral("qt"), QStringLiteral("Qt 5 and 6")},
        {QStringLiteral("konsole"), QStringLiteral("Konsole")},
        {QStringLiteral("kitty"), QStringLiteral("kitty")},
        {QStringLiteral("foot"), QStringLiteral("foot")},
        {QStringLiteral("btop"), QStringLiteral("btop")},
        {QStringLiteral("zellij"), QStringLiteral("Zellij")},
        {QStringLiteral("hyprland"), QStringLiteral("Hyprland")},
        {QStringLiteral("cursor"), QStringLiteral("Mouse pointer / Xcursor theme")},
        {QStringLiteral("spicetify"), QStringLiteral("Spotify / Spicetify")},
        {QStringLiteral("vesktop"), QStringLiteral("Discord / Vesktop")},
    };
    return targets;
}

QStringList targetNames()
{
    QStringList names;
    for (const auto &entry : knownTargets())
        names << entry.first;
    return names;
}

struct Options {
    QString apply;
    QStringList targets;
    QString filter;
    bool listThemes = false;
    bool listTargets = false;
    bool listConfigs = false;
    bool system = false;
    bool dryRun = false;
    bool help = false;
};

QString usageError(const QString &message)
{
    return message + QStringLiteral("\n\n") + Cli::helpText();
}

} // namespace

QString Cli::helpText()
{
    QString usage = QStringLiteral(
        "HyperThemes %1\n"
        "\n"
        "Usage:\n"
        "  hyperthemes                                      Open the window\n"
        "  hyperthemes --apply <theme> --target <target>    Apply one target\n"
        "  hyperthemes --apply <theme> --targets gtk,kitty  Apply several targets\n"
        "  hyperthemes --list-themes [filter]               List catalog themes\n"
        "  hyperthemes --list-targets                       List target names\n"
        "  hyperthemes --list-configs                       List generated shell configs\n"
        "\n"
        "Options:\n"
        "  --apply <theme>     Catalog id or exact theme name\n"
        "  --target <name>     Target to apply; repeat for several\n"
        "  --targets <a,b>     Comma-separated targets\n"
        "  --dry-run           Show the files an apply would change\n"
        "  --system            Write the KDE scheme system-wide (pkexec)\n"
        "  --list-themes [q]   Print matching ids, names and categories\n"
        "  --list-targets      Print supported targets\n"
        "  --list-configs      Print generated Quickshell configs\n"
        "  -h, --help          Show this text\n"
        "\n"
        "A CLI apply requires at least one target, so it never writes more files\n"
        "than the command names. Backups still land in the usual state folder.\n");
    return usage.arg(QCoreApplication::applicationVersion());
}

bool Cli::requested(const QStringList &arguments)
{
    static const QSet<QString> options = {
        QStringLiteral("--apply"), QStringLiteral("--target"), QStringLiteral("--targets"),
        QStringLiteral("--system"), QStringLiteral("--dry-run"), QStringLiteral("--list-themes"),
        QStringLiteral("--list-targets"), QStringLiteral("--list-configs"),
        QStringLiteral("--help"), QStringLiteral("-h"),
    };

    for (int i = 1; i < arguments.size(); ++i) {
        QString argument = arguments.at(i);
        if (argument.startsWith(QLatin1String("--")) && argument.contains(QLatin1Char('=')))
            argument = argument.left(argument.indexOf(QLatin1Char('=')));
        if (options.contains(argument))
            return true;
    }
    return false;
}

Cli::Result Cli::run(const QStringList &arguments)
{
    Result result;
    QStringList args = arguments;
    if (!args.isEmpty())
        args.removeFirst();
    if (args.isEmpty())
        return result;

    Options options;
    const auto fail = [&result](const QString &message) {
        result.handled = true;
        result.exitCode = 2;
        result.error = usageError(message);
        return result;
    };

    for (int i = 0; i < args.size(); ++i) {
        QString argument = args.at(i);
        QString inlineValue;
        const bool hasInline = argument.startsWith(QLatin1String("--"))
            && argument.contains(QLatin1Char('='));
        if (hasInline) {
            const qsizetype separator = argument.indexOf(QLatin1Char('='));
            inlineValue = argument.mid(separator + 1);
            argument = argument.left(separator);
        }

        const auto takeValue = [&args, &i, &inlineValue, hasInline](QString *value) {
            if (hasInline) {
                *value = inlineValue;
                return true;
            }
            if (i + 1 >= args.size())
                return false;
            *value = args.at(++i);
            return true;
        };

        if (argument == QLatin1String("--apply")) {
            QString value;
            if (!takeValue(&value) || value.isEmpty())
                return fail(QStringLiteral("--apply needs a theme id or name."));
            if (!options.apply.isEmpty())
                return fail(QStringLiteral("--apply was given more than once."));
            options.apply = value;
        } else if (argument == QLatin1String("--target")) {
            QString value;
            if (!takeValue(&value) || value.isEmpty())
                return fail(QStringLiteral("--target needs a target name."));
            options.targets << value.trimmed();
        } else if (argument == QLatin1String("--targets")) {
            QString value;
            if (!takeValue(&value) || value.isEmpty())
                return fail(QStringLiteral("--targets needs a comma-separated list."));
            const QStringList parts = value.split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (const QString &part : parts)
                options.targets << part.trimmed();
        } else if (argument == QLatin1String("--system")) {
            if (hasInline)
                return fail(QStringLiteral("--system does not take a value."));
            options.system = true;
        } else if (argument == QLatin1String("--list-configs")) {
            if (hasInline)
                return fail(QStringLiteral("--list-configs does not take a value."));
            options.listConfigs = true;
        } else if (argument == QLatin1String("--dry-run")) {
            if (hasInline)
                return fail(QStringLiteral("--dry-run does not take a value."));
            options.dryRun = true;
        } else if (argument == QLatin1String("--list-targets")) {
            if (hasInline)
                return fail(QStringLiteral("--list-targets does not take a value."));
            options.listTargets = true;
        } else if (argument == QLatin1String("--list-themes")) {
            options.listThemes = true;
            if (hasInline)
                options.filter = inlineValue;
            else if (i + 1 < args.size() && !args.at(i + 1).startsWith(QLatin1Char('-')))
                options.filter = args.at(++i);
        } else if (argument == QLatin1String("--help") || argument == QLatin1String("-h")) {
            options.help = true;
        } else {
            return fail(QStringLiteral("Unknown option: %1").arg(argument));
        }
    }

    result.handled = true;
    if (options.help) {
        result.output = helpText();
        return result;
    }

    int actions = 0;
    if (!options.apply.isEmpty())
        ++actions;
    if (options.listThemes)
        ++actions;
    if (options.listTargets)
        ++actions;
    if (options.listConfigs)
        ++actions;
    if (actions > 1)
        return fail(QStringLiteral("Choose one of --apply, --list-themes, --list-targets or --list-configs."));

    if (options.listTargets) {
        QStringList lines;
        for (const auto &entry : knownTargets())
            lines << entry.first + QLatin1Char('\t') + entry.second;
        result.output = lines.join(QLatin1Char('\n'));
        return result;
    }

    if (options.listConfigs) {
        ShellBuilder builder;
        QStringList lines;
        for (const QVariant &entry : builder.configs()) {
            const QVariantMap config = entry.toMap();
            lines << QStringLiteral("%1\t%2\t%3\t%4")
                         .arg(config.value(QStringLiteral("name")).toString(),
                              config.value(QStringLiteral("running")).toBool()
                                  ? QStringLiteral("running")
                                  : QStringLiteral("stopped"),
                              config.value(QStringLiteral("modified")).toString(),
                              config.value(QStringLiteral("theme")).toString());
        }
        result.output = lines.join(QLatin1Char('\n'));
        return result;
    }

    if (options.listThemes) {
        const QVariantList themes = Catalog().load();
        QStringList lines;
        for (const QVariant &entry : themes) {
            const QVariantMap theme = entry.toMap();
            const QString id = theme.value(QStringLiteral("id")).toString();
            const QString name = theme.value(QStringLiteral("name")).toString();
            const QString category = theme.value(QStringLiteral("category")).toString();
            if (!options.filter.isEmpty()
                && !id.contains(options.filter, Qt::CaseInsensitive)
                && !name.contains(options.filter, Qt::CaseInsensitive)
                && !category.contains(options.filter, Qt::CaseInsensitive))
                continue;
            lines << id + QLatin1Char('\t') + name + QLatin1Char('\t') + category;
        }
        lines.sort(Qt::CaseInsensitive);
        result.output = lines.join(QLatin1Char('\n'));
        return result;
    }

    if (options.apply.isEmpty()) {
        result.exitCode = 2;
        result.error = usageError(QStringLiteral("Choose --apply, --list-themes or --list-targets."));
        return result;
    }

    if (options.targets.isEmpty()) {
        result.exitCode = 2;
        result.error = usageError(QStringLiteral("Choose at least one target with --target or --targets."));
        return result;
    }

    QStringList targets;
    for (const QString &requested : options.targets) {
        const QString target = requested.trimmed().toLower();
        if (!targetNames().contains(target)) {
            result.exitCode = 2;
            result.error = usageError(QStringLiteral("Unknown target “%1”. Run --list-targets.").arg(requested));
            return result;
        }
        if (!targets.contains(target))
            targets << target;
    }

    const QVariantList themes = Catalog().load();
    QVariantMap chosenTheme;
    for (const QVariant &entry : themes) {
        const QVariantMap theme = entry.toMap();
        if (theme.value(QStringLiteral("id")).toString().compare(options.apply, Qt::CaseInsensitive) == 0
            || theme.value(QStringLiteral("name")).toString().compare(options.apply, Qt::CaseInsensitive) == 0) {
            chosenTheme = theme;
            break;
        }
    }

    if (chosenTheme.isEmpty()) {
        result.exitCode = 1;
        result.error = QStringLiteral("No theme matches “%1”. Run --list-themes to search the catalog.")
                           .arg(options.apply);
        return result;
    }

    Applier applier;
    const QVariantMap applied = options.dryRun
        ? applier.dryRun(chosenTheme, targets, options.system)
        : applier.apply(chosenTheme, targets, options.system);
    if (!applied.value(QStringLiteral("ok")).toBool()) {
        result.exitCode = 1;
        result.error = applied.value(QStringLiteral("error")).toString();
        return result;
    }

    QStringList lines;
    lines << QStringLiteral("%1 %2 to %3")
                 .arg(options.dryRun ? QStringLiteral("Would apply") : QStringLiteral("Applied"),
                      chosenTheme.value(QStringLiteral("name")).toString(),
                      targets.join(QStringLiteral(", ")));

    const QVariantList results = applied.value(QStringLiteral("results")).toList();
    for (const QVariant &entry : results) {
        const QVariantMap target = entry.toMap();
        lines << QStringLiteral("  %1: %2")
                     .arg(target.value(QStringLiteral("target")).toString(),
                          target.value(QStringLiteral("status")).toString());
    }

    const QStringList files = applied.value(QStringLiteral("files")).toStringList();
    if (options.dryRun) {
        if (files.isEmpty()) {
            lines << QStringLiteral("No files would change.");
        } else {
            lines << QStringLiteral("Would write:");
            for (const QString &file : files)
                lines << QStringLiteral("  %1").arg(file);
        }
    }

    const QString backup = applied.value(QStringLiteral("backup")).toString();
    if (!backup.isEmpty())
        lines << QStringLiteral("Backup: %1").arg(backup);
    for (const QString &warning : applied.value(QStringLiteral("warnings")).toStringList())
        lines << QStringLiteral("Warning: %1").arg(warning);
    result.output = lines.join(QLatin1Char('\n'));
    return result;
}

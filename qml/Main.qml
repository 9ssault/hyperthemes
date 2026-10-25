import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import HyperThemes 1.0
import "components"

ApplicationWindow {
    id: root

    width: 1380
    height: 880
    minimumWidth: 1080
    minimumHeight: 680
    visible: true
    title: "HyperThemes"
    color: colors.bg
    font.family: Qt.application.font.family

    readonly property var allTargets: ["kde", "caelestia", "waybar", "gtk", "qt", "konsole", "kitty", "foot", "spicetify", "vesktop"]

    property var themes: []
    property var localThemes: []
    property var remoteThemes: []
    property var filtered: []
    property var selected: null
    property var targetEnabled: ({})
    property var targetOptions: []
    property var sourceModel: [{ label: "All sources", value: "All" }]
    property var modeModel: [{ label: "Any mode", value: "All" }, { label: "Dark", value: "dark" }, { label: "Light", value: "light" }]
    property var sortModel: ["Name", "Stars", "Source"]
    property var targetFilterModel: [
        { label: "Everything", value: "All" },
        { label: "KDE Plasma", value: "kde" },
        { label: "Quickshell", value: "caelestia" },
        { label: "Waybar", value: "waybar" },
        { label: "GTK", value: "gtk" },
        { label: "Terminals", value: "terminals" },
        { label: "Apps", value: "apps" }
    ]
    property string sourceFilter: "All"
    property string modeFilter: "All"
    property string targetFilter: "All"
    property string sortMode: "Name"
    property bool followPalette: true
    readonly property color appAccent: colors.accent
    readonly property color appBackground: colors.bg
    readonly property color appText: colors.text
    readonly property var uiColors: colors
    property string search: ""
    property string page: "explore"
    property string statusText: "Ready"
    property string toast: ""
    property var applyWarnings: []
    property string applyError: ""
    property string remoteInstallError: ""
    property var remoteInstallTarget: null
    property bool busy: false
    property bool systemWide: false
    property bool remoteLoading: false
    property int remotePending: 0
    property string remoteError: ""
    property var backupList: []
    property var layoutPresets: []
    property var layoutBackups: []
    property var layoutWarnings: []
    property string layoutError: ""

    property string wallpaperPath: ""
    property string wallpaperUrl: ""
    property string dynamicMode: "dark"
    property string dynamicScheme: "scheme-tonal-spot"
    property real dynamicContrast: 0
    property string dynamicError: ""
    property var dynamicTheme: null

    function colorFrom(item, key, fallback) {
        if (!item || !item.palette)
            return fallback;
        var value = item.palette[key];
        if (typeof value !== "string" || value.length === 0)
            return fallback;
        return value.charAt(0) === "#" ? value : "#" + value;
    }

    function hasPalette(item) {
        return !!item && !!item.palette && Object.keys(item.palette).length > 0;
    }

    function paletteColor(key, fallback) {
        if (!followPalette || !hasPalette(selected))
            return fallback;
        return colorFrom(selected, key, fallback);
    }

    function channel(value) {
        var c = value / 255;
        return c <= 0.03928 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
    }

    function luminance(hex) {
        var value = String(hex).replace("#", "");
        if (value.length !== 6)
            return 0;
        return 0.2126 * channel(parseInt(value.substring(0, 2), 16))
             + 0.7152 * channel(parseInt(value.substring(2, 4), 16))
             + 0.0722 * channel(parseInt(value.substring(4, 6), 16));
    }

    function contrast(a, b) {
        var la = luminance(a);
        var lb = luminance(b);
        return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
    }

    function contrastLabel(ratio) {
        return ratio.toFixed(1) + ":1 " + (ratio >= 7 ? "AAA" : ratio >= 4.5 ? "AA" : ratio >= 3 ? "AA large" : "low");
    }

    function targetMatches(item, filter) {
        if (filter === "All")
            return true;
        var targets = item && item.targets ? item.targets : [];
        if (filter === "terminals")
            return targets.indexOf("konsole") >= 0 || targets.indexOf("kitty") >= 0 || targets.indexOf("foot") >= 0;
        if (filter === "apps")
            return targets.indexOf("spicetify") >= 0 || targets.indexOf("vesktop") >= 0;
        return targets.indexOf(filter) >= 0;
    }

    function sourceLabel(source) {
        if (source === "bundled")
            return "Bundled";
        if (source === "base16")
            return "Base16";
        if (source === "kde-store")
            return "KDE Store";
        if (source === "github")
            return "GitHub";
        if (source === "gitlab")
            return "GitLab";
        if (source === "local")
            return "Local";
        if (source === "wallpaper")
            return "Dynamic";
        return source || "Theme";
    }

    function targetLabel(key) {
        var labels = {
            kde: "KDE Plasma",
            caelestia: "Quickshell / Caelestia",
            waybar: "Waybar",
            gtk: "GTK 3 / 4",
            qt: "Qt 5 / 6",
            konsole: "Konsole",
            kitty: "kitty",
            foot: "foot",
            spicetify: "Spotify / Spicetify",
            vesktop: "Discord / Vesktop",
            wallpaper: "Wallpaper"
        };
        return labels[key] || key;
    }

    function notify(message) {
        toast = message;
        toastTimer.restart();
    }

    function loadCatalog() {
        var text = LocalSource.readText(":/catalog/themes.json");
        if (!text) {
            statusText = "Could not load the bundled catalog";
            recompute();
            return;
        }
        try {
            var document = JSON.parse(text);
            themes = document.themes || [];
            statusText = themes.length + " themes in the catalog";
        } catch (error) {
            statusText = "Could not parse the bundled catalog";
        }
        recompute();
    }

    function recompute() {
        var all = themes.concat(localThemes, remoteThemes);
        var list = [];
        var seen = {};
        var lower = search.toLowerCase();
        var sources = ["All"];

        for (var i = 0; i < all.length; ++i) {
            var item = all[i];
            if (!item || !item.id || seen[item.id])
                continue;
            seen[item.id] = true;
            if (item.source && sources.indexOf(item.source) < 0)
                sources.push(item.source);
            if (page === "mine" && item.source !== "local" && item.source !== "wallpaper")
                continue;
            if (sourceFilter !== "All" && item.source !== sourceFilter)
                continue;
            if (modeFilter !== "All" && item.mode !== modeFilter)
                continue;
            if (!targetMatches(item, targetFilter))
                continue;
            if (lower.length > 0) {
                var haystack = ((item.name || "") + " " + (item.description || "") + " " + (item.author || "") + " " + (item.source || "")).toLowerCase();
                if (haystack.indexOf(lower) < 0)
                    continue;
            }
            list.push(item);
        }

        list.sort(function(a, b) {
            if (sortMode === "Stars")
                return (b.stars || 0) - (a.stars || 0);
            if (sortMode === "Source")
                return String(a.source || "").localeCompare(String(b.source || ""));
            return String(a.name || "").localeCompare(String(b.name || ""));
        });

        filtered = list;
        var model = [{ label: "All sources", value: "All" }];
        for (var s = 0; s < sources.length; ++s) {
            if (sources[s] === "All")
                continue;
            model.push({ label: sourceLabel(sources[s]), value: sources[s] });
        }
        sourceModel = model;
    }

    function selectTheme(item) {
        if (!item)
            return;
        selected = item;
        applyWarnings = [];
        applyError = "";
        remoteInstallError = "";
        var options = [];
        var enabled = {};
        var targets = item.targets && item.targets.length > 0 ? item.targets : allTargets;
        for (var i = 0; i < targets.length; ++i) {
            var key = targets[i];
            if (allTargets.indexOf(key) < 0 && key !== "wallpaper")
                continue;
            options.push({ key: key, label: targetLabel(key) });
            enabled[key] = true;
        }
        targetOptions = options;
        targetEnabled = enabled;
    }

    function setTarget(key, value) {
        var next = {};
        for (var k in targetEnabled)
            next[k] = targetEnabled[k];
        next[key] = value;
        targetEnabled = next;
    }

    function targetList() {
        var list = [];
        for (var key in targetEnabled) {
            if (targetEnabled[key])
                list.push(key);
        }
        return list;
    }

    function applySelected() {
        if (!selected || selected.installable === false) {
            notify("This entry is browse-only. Open its source page to install it.");
            return;
        }
        if (selected.needsInstall) {
            notify("Download the theme first, then apply its palette.");
            return;
        }
        var targets = targetList();
        if (targets.length === 0) {
            notify("Choose at least one target first.");
            return;
        }
        busy = true;
        applyWarnings = [];
        applyError = "";
        var result = Applier.apply(selected, targets, systemWide);
        busy = false;
        applyWarnings = result.warnings || [];
        if (result.ok) {
            statusText = "Applied " + selected.name;
            notify("Applied " + selected.name + " · backup " + result.backup);
            refreshBackups();
        } else {
            applyError = result.error || "Apply failed";
            notify(applyError);
        }
    }

    function rollbackBackup(id) {
        busy = true;
        var result = Applier.rollback(id);
        busy = false;
        applyWarnings = result.warnings || [];
        if (result.ok) {
            statusText = "Rolled back";
            notify("Restored " + result.restored + " file(s).");
            refreshBackups();
        } else {
            notify(result.error || "Nothing could be restored.");
        }
    }

    function refreshBackups() {
        backupList = Applier.backups();
    }

    function refreshLayouts() {
        layoutPresets = LayoutApplier.presets();
        layoutBackups = LayoutApplier.backups();
    }

    function applyLayout(id) {
        layoutError = "";
        layoutWarnings = [];
        var result = LayoutApplier.apply(id);
        layoutWarnings = result.warnings || [];
        if (result.ok) {
            notify("Applied layout · backup " + result.backup);
            page = "layouts";
        } else {
            layoutError = result.error || "The layout could not be applied.";
            notify(layoutError);
        }
        refreshLayouts();
    }

    function restoreLayout(id) {
        layoutError = "";
        layoutWarnings = [];
        var result = LayoutApplier.restore(id);
        layoutWarnings = result.warnings || [];
        if (result.ok)
            notify("Restored layout · " + result.backup);
        else {
            layoutError = result.error || "The layout could not be restored.";
            notify(layoutError);
        }
        refreshLayouts();
    }

    function localPath(url) {
        var text = String(url);
        if (text.indexOf("file://") === 0)
            text = text.substring(7);
        try {
            return decodeURIComponent(text);
        } catch (error) {
            return text;
        }
    }

    function slug(value) {
        return String(value).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
    }

    function shortHash(value) {
        var hash = 0;
        for (var i = 0; i < value.length; ++i)
            hash = ((hash << 5) - hash + value.charCodeAt(i)) | 0;
        return Math.abs(hash).toString(36);
    }

    function addLocal(theme) {
        var next = [];
        for (var i = 0; i < localThemes.length; ++i) {
            if (localThemes[i].id !== theme.id)
                next.push(localThemes[i]);
        }
        next.push(theme);
        localThemes = next;
        recompute();
    }

    function importPath(folder) {
        var files = LocalSource.scan(folder);
        var added = 0;
        for (var i = 0; i < files.length; ++i) {
            var theme = parseLocalTheme(files[i]);
            if (theme) {
                addLocal(theme);
                ++added;
            }
        }
        if (added > 0) {
            notify("Imported " + added + " local theme(s).");
            page = "mine";
            selectTheme(localThemes[localThemes.length - 1]);
        } else {
            notify("No supported theme files found in that folder.");
        }
    }

    function parseLocalTheme(path) {
        var name = path.split("/").pop();
        var lower = name.toLowerCase();
        var text = LocalSource.readText(path);
        if (!text)
            return null;

        if (lower.endsWith(".colors"))
            return parseKdeTheme(text, name, path);

        if (lower.endsWith(".json")) {
            try {
                var object = JSON.parse(text);
                var palette = object.palette || object.colours || object.colors;
                if (!palette || typeof palette !== "object" || Array.isArray(palette))
                    return null;
                return buildLocalTheme(object.name || name.replace(/\.[^.]+$/, ""), palette, path, object);
            } catch (error) {
                return null;
            }
        }

        var scheme = parseSchemeText(text);
        if (Object.keys(scheme).length < 4)
            return null;
        return buildLocalTheme(name.replace(/\.[^.]+$/, ""), scheme, path, {});
    }

    function parseSchemeText(text) {
        var palette = {};
        var lines = text.split(/\r?\n/);
        for (var i = 0; i < lines.length; ++i) {
            var line = lines[i].trim();
            if (!line || line.charAt(0) === "#" || line.indexOf("//") === 0)
                continue;
            var parts = line.split(/\s+/);
            if (parts.length >= 2 && /^#?[0-9a-fA-F]{6}$/.test(parts[1])) {
                var value = parts[1].charAt(0) === "#" ? parts[1].substring(1) : parts[1];
                palette[parts[0]] = value.toLowerCase();
            }
        }
        return palette;
    }

    function parseKdeTheme(text, name, path) {
        var sections = {};
        var current = "";
        var lines = text.split(/\r?\n/);
        for (var i = 0; i < lines.length; ++i) {
            var line = lines[i].trim();
            if (!line || line.charAt(0) === "#")
                continue;
            if (line.charAt(0) === "[" && line.charAt(line.length - 1) === "]") {
                current = line.substring(1, line.length - 1);
                sections[current] = {};
                continue;
            }
            var equals = line.indexOf("=");
            if (equals > 0 && current !== "")
                sections[current][line.substring(0, equals)] = line.substring(equals + 1);
        }

        var win = sections["Colors:Window"] || {};
        var view = sections["Colors:View"] || {};
        var button = sections["Colors:Button"] || {};
        var selection = sections["Colors:Selection"] || {};
        if (!win.BackgroundNormal && !view.BackgroundNormal)
            return null;

        var palette = {
            background: win.BackgroundNormal || view.BackgroundNormal,
            onBackground: win.ForegroundNormal || view.ForegroundNormal,
            surface: view.BackgroundNormal || win.BackgroundNormal,
            surfaceContainer: button.BackgroundNormal || view.BackgroundNormal || win.BackgroundNormal,
            surfaceContainerHigh: button.BackgroundNormal || win.BackgroundNormal,
            onSurface: view.ForegroundNormal || win.ForegroundNormal,
            onSurfaceVariant: win.ForegroundInactive || view.ForegroundInactive || win.ForegroundNormal,
            primary: selection.BackgroundNormal || win.DecorationFocus || win.ForegroundActive,
            onPrimary: selection.ForegroundNormal || win.BackgroundNormal,
            secondary: win.ForegroundLink || win.ForegroundActive,
            tertiary: win.ForegroundNeutral || win.ForegroundActive,
            error: win.ForegroundNegative || win.ForegroundActive
        };
        return buildLocalTheme(name.replace(/\.colors$/i, ""), palette, path, { mode: "dark" });
    }

    function buildLocalTheme(name, palette, path, object) {
        var id = "local-" + slug(name) + "-" + shortHash(path);
        return {
            id: id,
            name: name,
            description: object.description || ("Imported from " + path),
            author: object.author || "Local import",
            license: object.license || "Unknown",
            source: "local",
            sourceUrl: "file://" + path,
            kind: object.kind || "static",
            mode: object.mode || "dark",
            flavour: object.flavour || "default",
            variant: object.variant || "tonalspot",
            wallpaper: object.wallpaper || "",
            preview: object.preview || "",
            installPath: object.installPath || path,
            targets: object.targets || allTargets,
            palette: palette,
            remote: false,
            installable: object.installable !== false,
            stars: object.stars || 0
        };
    }

    function remoteThemeFromKde(item, category) {
        var name = item.name || "KDE theme";
        return {
            id: "kde-" + item.id,
            name: name,
            description: item.summary || ("From the KDE Store · " + category),
            author: item.personid || "KDE Store contributor",
            license: item.license || "See source page",
            source: "kde-store",
            sourceUrl: item.detailpage || item.preview1 || "",
            kind: category,
            mode: name.toLowerCase().indexOf("light") >= 0 ? "light" : "dark",
            flavour: "default",
            variant: "tonalspot",
            wallpaper: "",
            preview: item.smallpreviewpic1 || item.previewpic1 || "",
            installPath: "",
            targets: ["kde"],
            palette: {},
            remote: true,
            installable: true,
            needsInstall: true,
            downloadUrl: item.downloadlink1 || "",
            downloadName: item.downloadname1 || "",
            stars: 0
        };
    }

    function remoteThemeFromGithub(item) {
        var name = item.full_name || item.name || "GitHub theme";
        var license = item.license ? (item.license.spdx_id || item.license.name || "See source") : "See source";
        return {
            id: "github-" + (item.id || shortHash(name)),
            name: name,
            description: item.description || "A community theme or desktop customization repository.",
            author: item.owner ? item.owner.login : "GitHub",
            license: license,
            source: "github",
            sourceUrl: item.html_url || "",
            kind: "repository",
            mode: "dark",
            flavour: "default",
            variant: "tonalspot",
            wallpaper: "",
            preview: "",
            installPath: "",
            targets: [],
            palette: {},
            remote: true,
            installable: true,
            needsInstall: true,
            downloadUrl: "",
            downloadName: "",
            repo: item.full_name || "",
            defaultBranch: item.default_branch || "main",
            stars: item.stargazers_count || 0
        };
    }

    function remoteThemeFromGitlab(item) {
        var path = item.path_with_namespace || item.name || "GitLab theme";
        var branch = item.default_branch || "main";
        var archive = "https://gitlab.com/" + path + "/-/archive/" + encodeURIComponent(branch)
            + "/" + encodeURIComponent(path.split("/").pop() + "-" + branch) + ".zip";
        return {
            id: "gitlab-" + (item.id || shortHash(path)),
            name: path,
            description: item.description || "A community theme or desktop customization repository.",
            author: item.namespace && item.namespace.path ? item.namespace.path : "GitLab",
            license: "See source",
            source: "gitlab",
            sourceUrl: item.web_url || "",
            kind: "repository",
            mode: "dark",
            flavour: "default",
            variant: "tonalspot",
            wallpaper: "",
            preview: "",
            installPath: "",
            targets: [],
            palette: {},
            remote: true,
            installable: true,
            needsInstall: true,
            downloadUrl: archive,
            downloadName: "theme.zip",
            repo: path,
            defaultBranch: branch,
            stars: item.star_count || 0
        };
    }

    function getJson(url, callback) {
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState !== XMLHttpRequest.DONE)
                return;
            var parsed = null;
            if (xhr.status === 200 || xhr.status === 0) {
                try {
                    parsed = JSON.parse(xhr.responseText);
                } catch (error) {
                    parsed = null;
                }
            }
            callback(parsed);
        };
        xhr.open("GET", url);
        xhr.send();
    }

    function fetchRemote() {
        remoteLoading = true;
        remotePending = 0;
        remoteError = "";
        remoteThemes = [];

        var categories = [
            { id: 112, label: "Plasma color scheme" },
            { id: 722, label: "Global theme" },
            { id: 104, label: "Plasma style" },
            { id: 462, label: "Konsole color scheme" },
            { id: 135, label: "GTK theme" }
        ];

        categories.forEach(function(category) {
            ++remotePending;
            getJson("https://api.kde-look.org/ocs/v1/content/data?format=json&pagesize=16&sort=downloads&categories=" + category.id, function(data) {
                if (!data || !data.data) {
                    finishRemote();
                    return;
                }
                var additions = [];
                for (var i = 0; i < data.data.length; ++i)
                    additions.push(remoteThemeFromKde(data.data[i], category.label));
                remoteThemes = remoteThemes.concat(additions);
                finishRemote();
            });
        });

        ++remotePending;
        getJson("https://gitlab.com/api/v4/projects?search=" + encodeURIComponent("quickshell theme")
                + "&per_page=8&order_by=star_count&sort=desc", function(data) {
            if (data && data.length) {
                var additions = [];
                for (var i = 0; i < data.length; ++i)
                    additions.push(remoteThemeFromGitlab(data[i]));
                remoteThemes = remoteThemes.concat(additions);
            }
            finishRemote();
        });

        ["quickshell theme", "waybar theme", "plasma theme", "base16 colorscheme", "catppuccin theme",
         "quickshell config", "kde plasma theme"].forEach(function(query) {
            ++remotePending;
            getJson("https://api.github.com/search/repositories?q=" + encodeURIComponent(query) + "&sort=stars&order=desc&per_page=12", function(data) {
                if (!data || !data.items) {
                    finishRemote();
                    return;
                }
                var additions = [];
                for (var i = 0; i < data.items.length; ++i)
                    additions.push(remoteThemeFromGithub(data.items[i]));
                remoteThemes = remoteThemes.concat(additions);
                finishRemote();
            });
        });
    }

    function finishRemote() {
        remotePending = Math.max(0, remotePending - 1);
        remoteLoading = remotePending > 0;
        if (!remoteLoading && remoteThemes.length === 0)
            remoteError = "The community sources did not return results. Check your connection.";
        recompute();
    }

    function installSelected() {
        if (!selected || !selected.needsInstall)
            return;
        remoteInstallError = "";
        remoteInstallTarget = selected;
        RemoteInstaller.install(selected);
    }

    function targetsForRemoteFile(path) {
        var lower = String(path).toLowerCase();
        if (lower.endsWith(".colors"))
            return ["kde"];
        if (lower.endsWith(".colorscheme"))
            return ["konsole"];
        return allTargets;
    }

    function handleRemoteInstalled(result) {
        if (!result || !result.ok) {
            remoteInstallError = "The download could not be installed.";
            notify(remoteInstallError);
            return;
        }

        var target = root.remoteInstallTarget || selected;
        var packageInfo = result.package || {};
        if (packageInfo.id) {
            var structure = packageInfo.structure || "";
            var packageTargets = structure === "Plasma/ColorScheme" ? ["kde", "konsole"] : ["kde"];
            var installedPackage = {
                id: target ? target.id : ("package-" + packageInfo.id),
                name: target ? target.name : (packageInfo.name || packageInfo.id),
                description: target ? target.description : "Installed KDE package (" + structure + ")",
                author: target ? target.author : "",
                license: target ? target.license : "",
                source: target ? target.source : "local",
                sourceUrl: target ? target.sourceUrl : "",
                kind: target ? target.kind : "remote",
                mode: "dark",
                flavour: "default",
                variant: "tonalspot",
                wallpaper: "",
                preview: target ? target.preview : "",
                installPath: "",
                packageId: packageInfo.id,
                packageStructure: structure,
                packageName: packageInfo.name || "",
                targets: packageTargets,
                palette: {},
                remote: true,
                installable: true,
                needsInstall: false,
                installed: true,
                stars: target ? target.stars : 0
            };
            root.remoteInstallTarget = null;
            selectTheme(installedPackage);
            notify("Installed " + installedPackage.name + ". Apply it to switch Plasma over.");
            return;
        }

        var files = result.files || [];
        for (var i = 0; i < files.length; ++i) {
            var parsed = parseLocalTheme(files[i]);
            if (!parsed || !parsed.palette)
                continue;

            var installed = {
                id: target ? target.id : ("remote-" + i),
                name: target ? target.name : parsed.name,
                description: target ? target.description : parsed.description,
                author: target ? target.author : parsed.author,
                license: target ? target.license : parsed.license,
                source: target ? target.source : "local",
                sourceUrl: target ? target.sourceUrl : parsed.sourceUrl,
                kind: target ? target.kind : "remote",
                mode: parsed.mode || "dark",
                flavour: parsed.flavour || "default",
                variant: parsed.variant || "tonalspot",
                wallpaper: "",
                preview: target ? target.preview : "",
                installPath: parsed.installPath || files[i],
                targets: targetsForRemoteFile(files[i]),
                palette: parsed.palette,
                remote: true,
                installable: true,
                needsInstall: false,
                installed: true,
                stars: target ? target.stars : 0
            };
            root.remoteInstallTarget = null;
            selectTheme(installed);
            notify("Installed " + installed.name + ". Review the palette and apply when ready.");
            return;
        }

        remoteInstallError = "Downloaded " + (target ? target.name : "the theme")
            + ", but no supported color scheme was found. This source may only contain layout files.";
        root.remoteInstallTarget = null;
        notify(remoteInstallError);
    }

    function chooseWallpaper() {
        wallpaperDialog.open();
    }

    function generateDynamic() {
        if (!wallpaperPath) {
            dynamicError = "Choose a wallpaper first.";
            return;
        }
        busy = true;
        dynamicError = "";
        var result = PaletteGenerator.fromWallpaper(wallpaperPath, dynamicMode, dynamicScheme, dynamicContrast);
        busy = false;
        if (!result.ok) {
            dynamicError = result.error || "Matugen could not generate a palette.";
            return;
        }

        var name = wallpaperPath.split("/").pop().replace(/\.[^.]+$/, "");
        dynamicTheme = {
            id: "dynamic-" + shortHash(wallpaperPath + dynamicMode + dynamicScheme),
            name: name,
            description: "Generated from " + wallpaperPath,
            author: "Matugen",
            license: "Your wallpaper",
            source: "wallpaper",
            sourceUrl: "",
            kind: "dynamic",
            mode: dynamicMode,
            flavour: "default",
            variant: dynamicScheme,
            wallpaper: wallpaperPath,
            preview: wallpaperUrl,
            installPath: "",
            targets: allTargets.concat(["wallpaper"]),
            palette: result.palette,
            remote: false,
            installable: true,
            stars: 0
        };
        selectTheme(dynamicTheme);
        notify("Generated a palette from " + name);
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: searchField.forceActiveFocus()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: searchField.focus = false
    }

    Timer {
        id: toastTimer
        interval: 5000
        onTriggered: root.toast = ""
    }

    Connections {
        target: Applier

        function onStatusChanged() {
            if (Applier.status.length > 0)
                root.statusText = Applier.status;
        }
    }

    Connections {
        target: RemoteInstaller

        function onFinished(result) {
            root.handleRemoteInstalled(result);
        }

        function onFailed(message) {
            root.remoteInstallTarget = null;
            root.remoteInstallError = message;
            root.notify(message);
        }

        function onStatusChanged() {
            if (RemoteInstaller.status.length > 0)
                root.statusText = RemoteInstaller.status;
        }
    }

    FolderDialog {
        id: importDialog
        title: "Import a theme folder"
        onAccepted: root.importPath(root.localPath(selectedFolder.toString()))
    }

    FileDialog {
        id: wallpaperDialog
        title: "Choose a wallpaper"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: {
            root.wallpaperUrl = selectedFile.toString();
            root.wallpaperPath = root.localPath(root.wallpaperUrl);
        }
    }

    QtObject {
        id: colors

        property color bg: root.paletteColor("background", "#0d0a10")
        property color panel: root.paletteColor("surfaceContainerLow", root.paletteColor("surfaceContainer", "#121016"))
        property color panel2: root.paletteColor("surfaceContainer", "#19151e")
        property color elevated: root.paletteColor("surfaceContainerHigh", "#211b28")
        property color border: root.paletteColor("outlineVariant", "#2e2735")
        property color text: root.paletteColor("onSurface", root.paletteColor("onBackground", "#f2ecf5"))
        property color muted: root.paletteColor("onSurfaceVariant", "#a79caf")
        property color faint: root.paletteColor("outline", "#746a7d")
        property color accent: root.paletteColor("primary", "#cfbcff")
        property color accent2: root.paletteColor("secondary", "#8ab4f8")
        property color danger: root.paletteColor("error", "#ffb4ab")
        property color success: root.paletteColor("term2", root.paletteColor("tertiary", "#8fd6a0"))
        property color onAccent: root.paletteColor("onPrimary", "#18121e")
    }

    Settings {
        id: prefs
        category: "appearance"
        property alias followPalette: root.followPalette
        property alias systemWide: root.systemWide
        property alias page: root.page
    }

    palette.window: colors.bg
    palette.windowText: colors.text
    palette.base: colors.panel2
    palette.alternateBase: colors.panel
    palette.text: colors.text
    palette.button: colors.elevated
    palette.buttonText: colors.text
    palette.brightText: colors.danger
    palette.highlight: colors.accent
    palette.highlightedText: colors.onAccent
    palette.toolTipBase: colors.elevated
    palette.toolTipText: colors.text
    palette.placeholderText: colors.faint

    Component.onCompleted: {
        loadCatalog();
        refreshBackups();
        refreshLayouts();
    }

    Rectangle {
        anchors.fill: parent
        color: colors.bg
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 232
            Layout.fillHeight: true
            color: colors.panel

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 18
                    spacing: 10

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        source: "qrc:/data/hyperthemes.svg"
                        sourceSize.width: 64
                        sourceSize.height: 64
                        smooth: true
                        fillMode: Image.PreserveAspectFit
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Text {
                            text: "HyperThemes"
                            color: colors.text
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }

                        Text {
                            text: "Desktop theme manager"
                            color: colors.faint
                            font.pixelSize: 10
                        }
                    }
                }

                NavButton {
                    Layout.fillWidth: true
                    iconSource: "qrc:/data/icons/explore.svg"
                    label: "Explore"
                    active: root.page === "explore"
                    onClicked: root.page = "explore"
                }

                NavButton {
                    Layout.fillWidth: true
                    iconSource: "qrc:/data/icons/heart.svg"
                    label: "My themes"
                    active: root.page === "mine"
                    onClicked: root.page = "mine"
                }

                NavButton {
                    Layout.fillWidth: true
                    iconSource: "qrc:/data/icons/palette.svg"
                    label: "Dynamic"
                    active: root.page === "dynamic"
                    onClicked: root.page = "dynamic"
                }

                NavButton {
                    Layout.fillWidth: true
                    iconSource: "qrc:/data/icons/layout.svg"
                    label: "Layouts"
                    active: root.page === "layouts"
                    onClicked: root.page = "layouts"
                }

                NavButton {
                    Layout.fillWidth: true
                    iconSource: "qrc:/data/icons/settings.svg"
                    label: "Settings"
                    active: root.page === "settings"
                    onClicked: root.page = "settings"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 14
                    height: 1
                    color: colors.border
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    text: "SOURCES"
                    color: colors.faint
                    font.pixelSize: 9
                    font.letterSpacing: 0.5
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: root.themes.length + " catalog · " + root.localThemes.length + " local"
                    color: colors.muted
                    font.pixelSize: 11
                }

                Text {
                    Layout.fillWidth: true
                    text: root.remoteThemes.length > 0 ? root.remoteThemes.length + " community" : "Community not loaded"
                    color: colors.muted
                    font.pixelSize: 11
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.hasPalette(root.selected)
                    text: "Previewing " + (root.selected ? root.selected.name : "")
                    color: colors.accent
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    radius: 12
                    color: colors.panel2
                    border.width: 1
                    border.color: colors.border
                    implicitHeight: matugenText.implicitHeight + 24

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 3

                        Text {
                            text: "MATUGEN"
                            color: colors.muted
                            font.pixelSize: 9
                    font.letterSpacing: 0.5
                    font.weight: Font.DemiBold
                        }

                        Text {
                            id: matugenText
                            Layout.fillWidth: true
                            text: PaletteGenerator.available() ? "Ready for wallpaper palettes" : "Not installed — dynamic palettes disabled"
                            color: PaletteGenerator.available() ? colors.success : colors.danger
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Local-first. Every write is backed up first."
                    color: colors.faint
                    font.pixelSize: 9
                    wrapMode: Text.WordWrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: colors.bg

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: root.page === "dynamic"
                                ? "Dynamic palette"
                                : root.page === "layouts"
                                    ? "Layout presets"
                                    : root.page === "settings"
                                        ? "Settings"
                                        : root.page === "mine"
                                            ? "My themes"
                                            : "Explore themes"
                            color: colors.text
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: root.page === "dynamic"
                                ? "Generate a complete palette from any wallpaper with Matugen, then send it everywhere."
                                : root.page === "layouts"
                                    ? "Rearrange Caelestia and KDE panels, with the previous layout backed up first."
                                    : root.page === "settings"
                                        ? "Sources, backups, and what HyperThemes can reach on this machine."
                                        : root.page === "mine"
                                            ? "Imported schemes and generated palettes. Add a folder to grow this shelf."
                                            : "One catalog for KDE, Quickshell, Waybar, terminals, and chat apps."
                            color: colors.muted
                            font.pixelSize: 12
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ActionButton {
                        text: root.remoteLoading ? "Loading…" : "Refresh community"
                        enabled: !root.remoteLoading
                        onClicked: root.fetchRemote()
                    }

                    ActionButton {
                        text: "Import folder"
                        primary: true
                        onClicked: importDialog.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: root.page === "explore" || root.page === "mine"

                    TextField {
                        id: searchField
                        Layout.preferredWidth: 270
                        Layout.preferredHeight: 32
                        leftPadding: 32
                        placeholderText: "Search themes, authors, sources…"
                        text: root.search
                        font.pixelSize: 12
                        color: colors.text
                        placeholderTextColor: colors.faint
                        selectByMouse: true
                        onTextChanged: {
                            root.search = text;
                            root.recompute();
                        }

                        AppIcon {
                            anchors.left: parent.left
                            anchors.leftMargin: 11
                            anchors.verticalCenter: parent.verticalCenter
                            iconSize: 15
                            width: 15
                            height: 15
                            source: "qrc:/data/icons/search.svg"
                            color: searchField.activeFocus ? colors.accent : colors.faint
                        }

                        background: Rectangle {
                            radius: 7
                            color: colors.panel2
                            border.width: 1
                            border.color: searchField.activeFocus ? colors.accent : colors.border
                        }
                    }

                    ComboBox {
                        Layout.preferredWidth: 150
                        model: root.sourceModel
                        Layout.preferredHeight: 32
                        textRole: "label"
                        font.pixelSize: 12
                        currentIndex: {
                            for (var i = 0; i < root.sourceModel.length; ++i) {
                                if (root.sourceModel[i].value === root.sourceFilter)
                                    return i;
                            }
                            return 0;
                        }
                        onActivated: function(index) {
                            root.sourceFilter = root.sourceModel[index].value;
                            root.recompute();
                        }
                    }

                    ComboBox {
                        Layout.preferredWidth: 128
                        model: root.modeModel
                        Layout.preferredHeight: 32
                        textRole: "label"
                        font.pixelSize: 12
                        currentIndex: root.modeFilter === "light" ? 2 : root.modeFilter === "dark" ? 1 : 0
                        onActivated: function(index) {
                            root.modeFilter = root.modeModel[index].value;
                            root.recompute();
                        }
                    }

                    ComboBox {
                        Layout.preferredWidth: 112
                        model: root.sortModel
                        Layout.preferredHeight: 32
                        font.pixelSize: 12
                        currentIndex: Math.max(0, root.sortModel.indexOf(root.sortMode))
                        onActivated: function(index) {
                            root.sortMode = root.sortModel[index];
                            root.recompute();
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: root.filtered.length + (root.filtered.length === 1 ? " theme" : " themes")
                        color: colors.faint
                        font.pixelSize: 11
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    visible: root.page === "explore" || root.page === "mine"

                    Repeater {
                        model: root.targetFilterModel

                        Rectangle {
                            required property var modelData

                            implicitWidth: chipText.implicitWidth + 22
                            implicitHeight: 27
                            radius: 14
                            color: root.targetFilter === modelData.value ? colors.accent : colors.panel
                            border.width: 1
                            border.color: root.targetFilter === modelData.value ? colors.accent : colors.border

                            Text {
                                id: chipText
                                anchors.centerIn: parent
                                text: modelData.label
                                color: root.targetFilter === modelData.value ? colors.onAccent : colors.muted
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                            }

                            TapHandler {
                                onTapped: {
                                    root.targetFilter = modelData.value;
                                    root.recompute();
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ActionButton {
                        visible: root.search !== "" || root.sourceFilter !== "All" || root.modeFilter !== "All" || root.targetFilter !== "All"
                        text: "Clear filters"
                        onClicked: {
                            root.search = "";
                            root.sourceFilter = "All";
                            root.modeFilter = "All";
                            root.targetFilter = "All";
                            searchField.text = "";
                            root.recompute();
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.page === "explore" || root.page === "mine" ? 0 : root.page === "dynamic" ? 1 : root.page === "layouts" ? 2 : 3

                    Item {
                        GridView {
                            id: grid
                            anchors.fill: parent
                            clip: true
                            cellWidth: 292
                            cellHeight: 264
                            leftMargin: 2
                            topMargin: 2
                            rightMargin: 2
                            bottomMargin: 2
                            model: root.filtered
                            boundsBehavior: Flickable.StopAtBounds
                            activeFocusOnTab: true
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            Keys.onPressed: function(event) {
                                if (event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter)
                                    return;
                                if (currentIndex < 0 || currentIndex >= root.filtered.length)
                                    return;
                                root.selectTheme(root.filtered[currentIndex]);
                                if (event.modifiers & Qt.ControlModifier)
                                    root.applySelected();
                                event.accepted = true;
                            }

                            delegate: ThemeCard {
                                required property var modelData

                                width: 280
                                height: 248
                                item: modelData
                                selected: root.selected && root.selected.id === modelData.id
                                onActivated: root.selectTheme(modelData)
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: 320
                            spacing: 8
                            visible: root.filtered.length === 0 && !root.remoteLoading

                            AppIcon {
                                Layout.alignment: Qt.AlignHCenter
                                iconSize: 32
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                source: "qrc:/data/icons/search.svg"
                                color: colors.faint
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Nothing here yet"
                                color: colors.text
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Try another search, import a local folder, or refresh the community sources."
                                color: colors.muted
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }

                    Item {
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 14

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 16

                                Rectangle {
                                    Layout.preferredWidth: 360
                                    Layout.preferredHeight: 220
                                    radius: 12
                                    color: colors.panel2
                                    border.width: 1
                                    border.color: colors.border
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        source: root.wallpaperUrl
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        visible: source !== "" && status !== Image.Error
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: root.wallpaperUrl === ""
                                        text: "Choose a wallpaper\nto extract a palette"
                                        color: colors.faint
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        lineHeight: 1.3
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Text {
                                        text: "WALLPAPER"
                                        color: colors.muted
                                        font.pixelSize: 9
                            font.letterSpacing: 0.5
                            font.weight: Font.DemiBold
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.wallpaperPath || "No image selected"
                                        color: colors.muted
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                    }

                                    RowLayout {
                                        spacing: 8

                                        ActionButton {
                                            text: "Choose image"
                                            onClicked: root.chooseWallpaper()
                                        }

                                        ActionButton {
                                            text: "Generate palette"
                                            primary: true
                                            enabled: !root.busy && root.wallpaperPath !== ""
                                            onClicked: root.generateDynamic()
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.dynamicError
                                        visible: root.dynamicError !== ""
                                        color: colors.danger
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "Matugen writes no files here: HyperThemes asks it for the palette, then applies the targets you choose."
                                        color: colors.faint
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 12
                                color: colors.panel
                                border.width: 1
                                border.color: colors.border

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 12

                                    Text {
                                        text: "PALETTE OPTIONS"
                                        color: colors.muted
                                        font.pixelSize: 9
                            font.letterSpacing: 0.5
                            font.weight: Font.DemiBold
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 16

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Text {
                                                text: "Mode"
                                                color: colors.muted
                                                font.pixelSize: 11
                                            }

                                            ComboBox {
                                                Layout.fillWidth: true
                                                model: ["Dark", "Light"]
                                                Layout.preferredHeight: 32
                                                onActivated: function(index) {
                                                    root.dynamicMode = index === 0 ? "dark" : "light";
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Text {
                                                text: "Scheme"
                                                color: colors.muted
                                                font.pixelSize: 11
                                            }

                                            ComboBox {
                                                Layout.fillWidth: true
                                                model: ["Tonal spot", "Vibrant", "Expressive", "Content", "Fidelity", "Monochrome", "Neutral", "Rainbow", "Fruit salad", "Smart"]
                                                Layout.preferredHeight: 32
                                                onActivated: function(index) {
                                                    var values = ["scheme-tonal-spot", "scheme-vibrant", "scheme-expressive", "scheme-content", "scheme-fidelity", "scheme-monochrome", "scheme-neutral", "scheme-rainbow", "scheme-fruit-salad", "scheme-smart"];
                                                    root.dynamicScheme = values[index];
                                                }
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12

                                        Text {
                                            text: "Contrast"
                                            color: colors.muted
                                            font.pixelSize: 11
                                        }

                                        Slider {
                                            Layout.fillWidth: true
                                            from: -1
                                            to: 1
                                            stepSize: 0.05
                                            value: root.dynamicContrast
                                            onMoved: root.dynamicContrast = value
                                        }

                                        Text {
                                            text: root.dynamicContrast.toFixed(2)
                                            color: colors.faint
                                            font.pixelSize: 10
                                        }
                                    }

                                    Item { Layout.fillHeight: true }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        visible: root.dynamicTheme !== null

                                        Repeater {
                                            model: ["primary", "secondary", "tertiary", "surface", "surfaceContainerHigh", "error"]

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 34
                                                radius: 7
                                                color: root.dynamicTheme ? root.colorFrom(root.dynamicTheme, modelData, "#333039") : "#333039"
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        visible: root.dynamicTheme !== null
                                        text: "The generated palette is selected on the right. Choose targets and apply it."
                                        color: colors.faint
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        ScrollView {
                            anchors.fill: parent
                            contentWidth: availableWidth
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded

                            ColumnLayout {
                                width: Math.max(320, parent.width - 8)
                                spacing: 12

                                Rectangle {
                                    Layout.fillWidth: true
                            radius: 10
                            color: colors.panel
                                    border.width: 1
                                    border.color: colors.border
                                    implicitHeight: layoutIntro.implicitHeight + 30

                                    ColumnLayout {
                                        id: layoutIntro
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 6

                                        Text {
                                            text: "PANEL LAYOUTS"
                                            color: colors.muted
                                            font.pixelSize: 9
                                        font.letterSpacing: 0.5
                                        font.weight: Font.DemiBold
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Caelestia presets merge into shell.json and keep your general, service and lock settings. KDE presets replace the current Plasma panels through Plasma's scripting API. Both paths take a backup first."
                                            color: colors.muted
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }

                                Repeater {
                                    model: root.layoutPresets

                                    Rectangle {
                                        required property var modelData

                                        Layout.fillWidth: true
                                    radius: 10
                                    color: colors.panel
                                        border.width: 1
                                        border.color: colors.border
                                        implicitHeight: layoutCard.implicitHeight + 30

                                        ColumnLayout {
                                            id: layoutCard
                                            anchors.fill: parent
                                            anchors.margins: 16
                                            spacing: 8

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8

                                                Pill {
                                                    text: modelData.targetLabel
                                                    background: colors.elevated
                                                    foreground: colors.accent
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.name
                                                    color: colors.text
                                                    font.pixelSize: 14
                                                    font.weight: Font.DemiBold
                                                    elide: Text.ElideRight
                                                }

                                                ActionButton {
                                                    text: "Apply layout"
                                                    primary: true
                                                    onClicked: root.applyLayout(modelData.id)
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.description
                                                color: colors.muted
                                                font.pixelSize: 11
                                                wrapMode: Text.WordWrap
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                height: 38
                                                radius: 8
                                                color: colors.bg

                                                Rectangle {
                                                    width: parent.width * 0.72
                                                    height: 9
                                                    radius: 4
                                                    color: colors.accent
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    y: String(modelData.id).indexOf("bottom") >= 0 ? parent.height - height - 6 : 6
                                                }
                                            }
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: root.layoutError !== ""
                                    text: root.layoutError
                                    color: colors.danger
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                }

                                Repeater {
                                    model: root.layoutWarnings

                                    Text {
                                        Layout.fillWidth: true
                                        text: "• " + modelData
                                        color: "#d8b26a"
                                        font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                            radius: 10
                            color: colors.panel
                                    border.width: 1
                                    border.color: colors.border
                                    implicitHeight: layoutBackupColumn.implicitHeight + 30

                                    ColumnLayout {
                                        id: layoutBackupColumn
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 8

                                        RowLayout {
                                            Layout.fillWidth: true

                                            Text {
                                                text: "LAYOUT BACKUPS"
                                                color: colors.muted
                                                font.pixelSize: 9
                                                font.letterSpacing: 0.5
                                                font.weight: Font.DemiBold
                                            }

                                            Item { Layout.fillWidth: true }

                                            ActionButton {
                                                text: "Restore last"
                                                enabled: root.layoutBackups.length > 0
                                                danger: true
                                                onClicked: root.restoreLayout(root.layoutBackups[0])
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            visible: root.layoutBackups.length === 0
                                            text: "No layout backups yet."
                                            color: colors.muted
                                            font.pixelSize: 11
                                        }

                                        Repeater {
                                            model: root.layoutBackups

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData
                                                    color: colors.muted
                                                    font.pixelSize: 11
                                                    elide: Text.ElideRight
                                                }

                                                ActionButton {
                                                    text: "Restore"
                                                    onClicked: root.restoreLayout(modelData)
                                                }
                                            }
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }
                    }

                    Item {
                        ScrollView {
                            anchors.fill: parent
                            contentWidth: availableWidth
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded

                            ColumnLayout {
                                width: Math.max(320, parent.width - 8)
                                spacing: 14

                                Rectangle {
                                    Layout.fillWidth: true
                            radius: 10
                            color: colors.panel
                                    border.width: 1
                                    border.color: colors.border
                                    implicitHeight: sourceColumn.implicitHeight + 30

                                    ColumnLayout {
                                        id: sourceColumn
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 8

                                        Text {
                                            text: "SOURCES"
                                            color: colors.muted
                                            font.pixelSize: 9
                                        font.letterSpacing: 0.5
                                        font.weight: Font.DemiBold
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "The catalog ships with bundled Caelestia schemes and Base16 palettes. Local imports read Caelestia scheme files, KDE .colors files, and HyperThemes JSON. KDE Store and GitHub entries download on demand; archives are inspected for a palette or a Plasma package before anything touches your configuration."
                                            color: colors.muted
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            ActionButton {
                                                text: "Import Caelestia schemes"
                                                enabled: LocalSource.defaultCaelestiaDir() !== ""
                                                onClicked: root.importPath(LocalSource.defaultCaelestiaDir())
                                            }

                                            ActionButton {
                                                text: "Import KDE colors"
                                                onClicked: root.importPath(LocalSource.defaultKdeColorDir())
                                            }

                                            ActionButton {
                                                text: "Import folder…"
                                                onClicked: importDialog.open()
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                            radius: 10
                            color: colors.panel
                                    border.width: 1
                                    border.color: colors.border
                                    implicitHeight: appearanceColumn.implicitHeight + 30

                                    ColumnLayout {
                                        id: appearanceColumn
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 8

                                        Text {
                                            text: "APPEARANCE"
                                            color: colors.muted
                                            font.pixelSize: 9
                                        font.letterSpacing: 0.5
                                        font.weight: Font.DemiBold
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 2

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: "Match the app to the selected palette"
                                                    color: colors.text
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                }

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: "Surfaces, text, accents, controls and this panel follow the theme you pick. Turn it off for the original dark shell."
                                                    color: colors.muted
                                                    font.pixelSize: 10
                                                    wrapMode: Text.WordWrap
                                                }
                                            }

                                            ThemeSwitch {
                                                text: ""
                                                checked: root.followPalette
                                                onToggled: root.followPalette = checked
                                            }
                                        }

                                        ActionButton {
                                            text: "Use the default app colours"
                                            enabled: root.followPalette
                                            onClicked: root.followPalette = false
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                            radius: 10
                            color: colors.panel
                                    border.width: 1
                                    border.color: colors.border
                                    implicitHeight: backupColumn.implicitHeight + 30

                                    ColumnLayout {
                                        id: backupColumn
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 8

                                        RowLayout {
                                            Layout.fillWidth: true

                                            Text {
                                                text: "BACKUPS"
                                                color: colors.muted
                                                font.pixelSize: 9
                                                font.letterSpacing: 0.5
                                                font.weight: Font.DemiBold
                                            }

                                            Item { Layout.fillWidth: true }

                                            ActionButton {
                                                text: "Refresh"
                                                onClicked: root.refreshBackups()
                                            }

                                            ActionButton {
                                                text: "Restore last"
                                                enabled: root.backupList.length > 0
                                                danger: true
                                                onClicked: root.rollbackBackup(root.backupList[0])
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            visible: root.backupList.length === 0
                                            text: "No backups yet. HyperThemes creates one before its first write."
                                            color: colors.muted
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }

                                        Repeater {
                                            model: root.backupList

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8

                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData
                                                    color: colors.muted
                                                    font.pixelSize: 11
                                                    elide: Text.ElideRight
                                                }

                                                ActionButton {
                                                    text: "Restore"
                                                    onClicked: root.rollbackBackup(modelData)
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                            radius: 10
                            color: colors.panel
                                    border.width: 1
                                    border.color: colors.border
                                    implicitHeight: runtimeColumn.implicitHeight + 30

                                    ColumnLayout {
                                        id: runtimeColumn
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 6

                                        Text {
                                            text: "RUNTIME"
                                            color: colors.muted
                                            font.pixelSize: 9
                                        font.letterSpacing: 0.5
                                        font.weight: Font.DemiBold
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Matugen: " + (PaletteGenerator.available() ? PaletteGenerator.version() : "not installed")
                                            color: PaletteGenerator.available() ? colors.success : colors.danger
                                            font.pixelSize: 11
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "System-wide writes use pkexec and are limited to the KDE color scheme. User targets stay in XDG config and data directories."
                                            color: colors.muted
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 32
                    radius: 7
                    color: colors.panel

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.toast.length > 0 ? root.toast : root.statusText
                        color: root.toast.length > 0 ? colors.accent : colors.faint
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        width: parent.width - 260
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: Applier.lastBackup.length > 0 ? "last backup · " + Applier.lastBackup : "no writes yet"
                        color: colors.faint
                        font.pixelSize: 10
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 352
            Layout.fillHeight: true
            color: colors.panel
            border.width: 0

            ScrollView {
                anchors.fill: parent
                anchors.margins: 18
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ColumnLayout {
                    width: 304
                    spacing: 12

                    Text {
                        text: "INSPECTOR"
                        color: colors.muted
                        font.pixelSize: 9
                        font.letterSpacing: 1.2
                        font.weight: Font.DemiBold
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        visible: root.selected !== null

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            radius: 14
                            clip: true
                            color: colors.panel2

                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: root.colorFrom(root.selected, "background", "#15111a") }
                                GradientStop { position: 0.60; color: root.colorFrom(root.selected, "surfaceContainer", "#241d2b") }
                                GradientStop { position: 1.0; color: root.colorFrom(root.selected, "primary", "#8a7bb8") }
                            }

                            Image {
                                anchors.fill: parent
                                source: root.selected ? (root.selected.preview || "") : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                visible: source !== "" && status !== Image.Error
                            }

                            Pill {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.margins: 10
                                text: root.sourceLabel(root.selected ? root.selected.source : "")
                                background: "#66120e18"
                                foreground: "#f3ecf7"
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.selected ? root.selected.name : ""
                            color: colors.text
                            font.pixelSize: 19
                            font.weight: Font.Bold
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.selected ? (root.selected.description || "") : ""
                            color: colors.muted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: root.selected ? (root.selected.author || "Unknown") : ""
                                color: colors.faint
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }

                            Text {
                                text: root.selected ? (root.selected.license || "") : ""
                                color: colors.faint
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: colors.border
                        }

                        Text {
                            text: "PALETTE"
                            color: colors.muted
                            font.pixelSize: 9
                    font.letterSpacing: 0.5
                    font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: !root.hasPalette(root.selected)
                            text: "No colour values yet. Download the theme to read its real palette; nothing is guessed from the name."
                            color: colors.muted
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            visible: root.hasPalette(root.selected)
                            columns: 6
                            rowSpacing: 6
                            columnSpacing: 6

                            Repeater {
                                model: ["background", "surface", "surfaceContainerHigh", "primary", "secondary", "tertiary", "onSurface", "onSurfaceVariant", "error", "term1", "term2", "term3"]

                                Rectangle {
                                    id: swatch
                                    Layout.preferredWidth: 44
                                    Layout.preferredHeight: 34
                                    radius: 6
                                    color: root.selected ? root.colorFrom(root.selected, modelData, "#333039") : "#333039"
                                    border.width: 1
                                    border.color: "#22000000"

                                    HoverHandler { id: swatchHover }

                                    ToolTip {
                                        visible: swatchHover.hovered
                                        text: modelData + "  #" + String(root.colorFrom(root.selected, modelData, "000000")).replace("#", "")
                                        delay: 150
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: root.hasPalette(root.selected)
                            spacing: 6

                            Rectangle {
                                radius: 6
                                color: colors.panel2
                                implicitWidth: textBadge.implicitWidth + 12
                                implicitHeight: 20

                                Text {
                                    id: textBadge
                                    anchors.centerIn: parent
                                    text: "Text " + root.contrastLabel(root.contrast(colors.text, colors.bg))
                                    color: colors.muted
                                    font.pixelSize: 9
                                }
                            }

                            Rectangle {
                                radius: 6
                                color: colors.panel2
                                implicitWidth: accentBadge.implicitWidth + 12
                                implicitHeight: 20

                                Text {
                                    id: accentBadge
                                    anchors.centerIn: parent
                                    text: "Accent " + root.contrastLabel(root.contrast(colors.accent, colors.bg))
                                    color: colors.muted
                                    font.pixelSize: 9
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            text: "TARGETS"
                            color: colors.muted
                            font.pixelSize: 9
                    font.letterSpacing: 0.5
                    font.weight: Font.DemiBold
                        }

                        Repeater {
                            model: root.targetOptions

                            ThemeCheckBox {
                                id: targetBox
                                Layout.fillWidth: true
                                property string themeId: root.selected ? root.selected.id : ""
                                text: modelData.label
                                checked: root.targetEnabled[modelData.key] !== false
                                onToggled: root.setTarget(modelData.key, checked)
                                onThemeIdChanged: checked = root.targetEnabled[modelData.key] !== false
                            }
                        }

                        ThemeSwitch {
                            Layout.fillWidth: true
                            text: "System-wide (KDE scheme only)"
                            checked: root.systemWide
                            onToggled: root.systemWide = checked
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            text: RemoteInstaller.busy ? "Downloading…" : "Download & inspect"
                            primary: true
                            visible: root.selected && root.selected.needsInstall === true
                            enabled: !RemoteInstaller.busy && root.selected && root.selected.needsInstall === true
                            onClicked: root.installSelected()
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            text: root.busy ? "Applying…" : "Apply to selected targets"
                            primary: true
                            visible: !root.selected || root.selected.needsInstall !== true
                            enabled: !root.busy && root.selected && root.selected.installable !== false
                                     && (root.hasPalette(root.selected) || (root.selected.packageId || "") !== "")
                            onClicked: root.applySelected()

                        Text {
                            Layout.fillWidth: true
                            visible: root.selected && !root.selected.needsInstall && !root.hasPalette(root.selected)
                                     && (root.selected.packageId || "") === ""
                            text: "This entry has no colour values or KDE package yet. Download it from its source first."
                            color: "#d8b26a"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: RemoteInstaller.busy
                            text: RemoteInstaller.status
                            color: colors.accent
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.remoteInstallError !== ""
                            text: root.remoteInstallError
                            color: colors.danger
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: "Open source page"
                            visible: root.selected && root.selected.sourceUrl !== ""
                            onClicked: Qt.openUrlExternally(root.selected.sourceUrl)
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.selected && root.selected.installable === false
                            text: "Browse-only. Install this theme from its source page, then import the downloaded files."
                            color: "#d8b26a"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.selected && root.selected.needsInstall === true
                            text: "This downloads the source archive, extracts it safely, and looks for a supported color scheme. Nothing is applied until you press Apply."
                            color: colors.faint
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: root.applyError !== ""
                            text: root.applyError
                            color: colors.danger
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        Repeater {
                            model: root.applyWarnings

                            Text {
                                Layout.fillWidth: true
                                text: "• " + modelData
                                color: "#d8b26a"
                                font.pixelSize: 10
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: root.selected === null
                        spacing: 10

                        Item { Layout.preferredHeight: 40 }

                        AppIcon {
                            Layout.alignment: Qt.AlignHCenter
                            iconSize: 36
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            source: "qrc:/data/icons/palette.svg"
                            color: colors.faint
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Pick a theme"
                            color: colors.text
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Select any card to inspect its palette, choose targets, and apply. Generated wallpaper palettes use the same pipeline."
                            color: colors.muted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.busy
        color: "#88000000"
        z: 100

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 10

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: root.busy
                palette.highlight: colors.accent
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Applying changes…"
                color: colors.text
                font.pixelSize: 12
            }
        }
    }
}

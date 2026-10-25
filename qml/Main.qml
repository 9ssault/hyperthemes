import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
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
    property string sourceFilter: "All"
    property string modeFilter: "All"
    property string sortMode: "Name"
    property string search: ""
    property string page: "explore"
    property string statusText: "Ready"
    property string toast: ""
    property var applyWarnings: []
    property string applyError: ""
    property bool busy: false
    property bool systemWide: false
    property bool remoteLoading: false
    property int remotePending: 0
    property string remoteError: ""
    property var backupList: []

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

    function sourceLabel(source) {
        if (source === "bundled")
            return "Bundled";
        if (source === "kde-store")
            return "KDE Store";
        if (source === "github")
            return "GitHub";
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
            statusText = themes.length + " bundled themes";
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

    function hslHex(hue, saturation, lightness) {
        var h = ((hue % 360) + 360) % 360 / 360;
        var s = Math.max(0, Math.min(1, saturation));
        var l = Math.max(0, Math.min(1, lightness));

        function channel(p, q, t) {
            if (t < 0)
                t += 1;
            if (t > 1)
                t -= 1;
            if (t < 1 / 6)
                return p + (q - p) * 6 * t;
            if (t < 1 / 2)
                return q;
            if (t < 2 / 3)
                return p + (q - p) * (2 / 3 - t) * 6;
            return p;
        }

        var r;
        var g;
        var b;
        if (s === 0) {
            r = g = b = l;
        } else {
            var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
            var p = 2 * l - q;
            r = channel(p, q, h + 1 / 3);
            g = channel(p, q, h);
            b = channel(p, q, h - 1 / 3);
        }

        function hex(value) {
            var text = Math.round(value * 255).toString(16);
            return text.length === 1 ? "0" + text : text;
        }
        return hex(r) + hex(g) + hex(b);
    }

    function paletteFromSeed(seed) {
        var hash = 0;
        for (var i = 0; i < seed.length; ++i)
            hash = ((hash << 5) - hash + seed.charCodeAt(i)) | 0;
        var hue = Math.abs(hash) % 360;
        return {
            background: hslHex(hue, 0.18, 0.07),
            onBackground: hslHex(hue, 0.10, 0.92),
            surface: hslHex(hue, 0.17, 0.10),
            surfaceContainer: hslHex(hue, 0.16, 0.14),
            surfaceContainerHigh: hslHex(hue, 0.15, 0.19),
            surfaceBright: hslHex(hue, 0.14, 0.24),
            onSurface: hslHex(hue, 0.08, 0.92),
            onSurfaceVariant: hslHex(hue, 0.10, 0.72),
            primary: hslHex(hue, 0.62, 0.70),
            onPrimary: hslHex(hue, 0.35, 0.12),
            secondary: hslHex(hue + 38, 0.45, 0.68),
            tertiary: hslHex(hue + 76, 0.48, 0.68),
            error: "ffb4ab",
            term0: hslHex(hue, 0.18, 0.09),
            term7: hslHex(hue, 0.08, 0.90),
            term1: hslHex(hue + 340, 0.65, 0.68),
            term2: hslHex(hue + 110, 0.50, 0.68),
            term3: hslHex(hue + 60, 0.55, 0.72),
            term4: hslHex(hue + 220, 0.55, 0.72),
            term5: hslHex(hue + 300, 0.52, 0.72),
            term6: hslHex(hue + 180, 0.48, 0.70)
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
            palette: paletteFromSeed(name),
            remote: true,
            installable: false,
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
            targets: ["kde"],
            palette: paletteFromSeed(name),
            remote: true,
            installable: false,
            stars: item.stargazers_count || 0
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
            { id: 462, label: "Konsole color scheme" },
            { id: 135, label: "GTK theme" }
        ];

        categories.forEach(function(category) {
            ++remotePending;
            getJson("https://api.kde-look.org/ocs/v1/content/data?format=json&pagesize=10&sort=downloads&categories=" + category.id, function(data) {
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

        ["quickshell theme", "waybar theme", "plasma theme"].forEach(function(query) {
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

        property color bg: "#0d0a10"
        property color panel: "#121016"
        property color panel2: "#19151e"
        property color elevated: "#211b28"
        property color border: "#2e2735"
        property color text: "#f2ecf5"
        property color muted: "#a79caf"
        property color faint: "#746a7d"
        property color accent: root.selected ? root.colorFrom(root.selected, "primary", "#cfbcff") : "#cfbcff"
        property color accent2: root.selected ? root.colorFrom(root.selected, "secondary", "#8ab4f8") : "#8ab4f8"
        property color danger: "#ffb4ab"
        property color success: "#8fd6a0"
    }

    Component.onCompleted: {
        loadCatalog();
        refreshBackups();
    }

    Rectangle {
        anchors.fill: parent
        color: colors.bg
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 228
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

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 11
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#cfbcff" }
                            GradientStop { position: 1.0; color: "#8ab4f8" }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "✦"
                            color: "#18121e"
                            font.pixelSize: 18
                            font.weight: Font.Bold
                        }
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
                            text: "themes, everywhere"
                            color: colors.faint
                            font.pixelSize: 10
                        }
                    }
                }

                NavButton {
                    Layout.fillWidth: true
                    icon: "◈"
                    label: "Explore"
                    active: root.page === "explore"
                    onClicked: root.page = "explore"
                }

                NavButton {
                    Layout.fillWidth: true
                    icon: "♡"
                    label: "My themes"
                    active: root.page === "mine"
                    onClicked: root.page = "mine"
                }

                NavButton {
                    Layout.fillWidth: true
                    icon: "✦"
                    label: "Dynamic"
                    active: root.page === "dynamic"
                    onClicked: root.page = "dynamic"
                }

                NavButton {
                    Layout.fillWidth: true
                    icon: "⚙"
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
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: root.themes.length + " bundled · " + root.localThemes.length + " local"
                    color: colors.muted
                    font.pixelSize: 11
                }

                Text {
                    Layout.fillWidth: true
                    text: root.remoteThemes.length + " community"
                    color: colors.muted
                    font.pixelSize: 11
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
                            color: colors.faint
                            font.pixelSize: 9
                            font.letterSpacing: 1.1
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
                                : root.page === "settings"
                                    ? "Settings"
                                    : root.page === "mine"
                                        ? "My themes"
                                        : "Explore themes"
                            color: colors.text
                            font.pixelSize: 26
                            font.weight: Font.Bold
                        }

                        Text {
                            text: root.page === "dynamic"
                                ? "Generate a complete palette from any wallpaper with Matugen, then send it everywhere."
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
                        Layout.preferredWidth: 250
                        placeholderText: "Search themes, authors, sources…"
                        text: root.search
                        font.pixelSize: 12
                        color: colors.text
                        selectByMouse: true
                        onTextChanged: {
                            root.search = text;
                            root.recompute();
                        }
                        background: Rectangle {
                            radius: 9
                            color: colors.panel2
                            border.width: 1
                            border.color: searchField.activeFocus ? colors.accent : colors.border
                        }
                    }

                    ComboBox {
                        Layout.preferredWidth: 150
                        model: root.sourceModel
                        textRole: "label"
                        font.pixelSize: 12
                        onActivated: function(index) {
                            root.sourceFilter = root.sourceModel[index].value;
                            root.recompute();
                        }
                    }

                    ComboBox {
                        Layout.preferredWidth: 128
                        model: root.modeModel
                        textRole: "label"
                        font.pixelSize: 12
                        onActivated: function(index) {
                            root.modeFilter = root.modeModel[index].value;
                            root.recompute();
                        }
                    }

                    ComboBox {
                        Layout.preferredWidth: 112
                        model: root.sortModel
                        font.pixelSize: 12
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

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: root.page === "explore" || root.page === "mine" ? 0 : root.page === "dynamic" ? 1 : 2

                    Item {
                        GridView {
                            id: grid
                            anchors.fill: parent
                            clip: true
                            cellWidth: 284
                            cellHeight: 252
                            leftMargin: 2
                            topMargin: 2
                            rightMargin: 2
                            bottomMargin: 2
                            model: root.filtered
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: ThemeCard {
                                required property var modelData

                                width: 272
                                height: 236
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

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "◌"
                                color: colors.faint
                                font.pixelSize: 36
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
                                    radius: 16
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
                                        color: colors.faint
                                        font.pixelSize: 9
                                        font.letterSpacing: 1.1
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
                                radius: 16
                                color: colors.panel
                                border.width: 1
                                border.color: colors.border

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 12

                                    Text {
                                        text: "PALETTE OPTIONS"
                                        color: colors.faint
                                        font.pixelSize: 9
                                        font.letterSpacing: 1.1
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
                                spacing: 14

                                Rectangle {
                                    Layout.fillWidth: true
                                    radius: 14
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
                                            color: colors.faint
                                            font.pixelSize: 9
                                            font.letterSpacing: 1.1
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Bundled palettes ship with the app. Local imports read Caelestia scheme files, KDE .colors files, and HyperThemes JSON. KDE Store and GitHub are browse-only: open the source page to install those themes with their own tooling."
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
                                    radius: 14
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
                                                color: colors.faint
                                                font.pixelSize: 9
                                                font.letterSpacing: 1.1
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
                                    radius: 14
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
                                            color: colors.faint
                                            font.pixelSize: 9
                                            font.letterSpacing: 1.1
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
                    radius: 9
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
                        color: colors.faint
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
                            color: colors.faint
                            font.pixelSize: 9
                            font.letterSpacing: 1.1
                            font.weight: Font.DemiBold
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 6
                            rowSpacing: 6
                            columnSpacing: 6

                            Repeater {
                                model: ["background", "surface", "surfaceContainerHigh", "primary", "secondary", "tertiary", "onSurface", "onSurfaceVariant", "error", "term1", "term2", "term3"]

                                Rectangle {
                                    Layout.preferredWidth: 42
                                    Layout.preferredHeight: 30
                                    radius: 7
                                    color: root.selected ? root.colorFrom(root.selected, modelData, "#333039") : "#333039"
                                    border.width: 1
                                    border.color: "#22000000"

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: "#ccffffff"
                                        font.pixelSize: 7
                                        elide: Text.ElideRight
                                        width: parent.width - 4
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                        }

                        Text {
                            text: "TARGETS"
                            color: colors.faint
                            font.pixelSize: 9
                            font.letterSpacing: 1.1
                            font.weight: Font.DemiBold
                        }

                        Repeater {
                            model: root.targetOptions

                            CheckBox {
                                id: targetBox
                                Layout.fillWidth: true
                                property string themeId: root.selected ? root.selected.id : ""
                                text: modelData.label
                                checked: root.targetEnabled[modelData.key] !== false
                                font.pixelSize: 11
                                onToggled: root.setTarget(modelData.key, checked)
                                onThemeIdChanged: checked = root.targetEnabled[modelData.key] !== false
                            }
                        }

                        Switch {
                            Layout.fillWidth: true
                            text: "System-wide (KDE scheme only)"
                            checked: root.systemWide
                            font.pixelSize: 11
                            onToggled: root.systemWide = checked
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            text: root.busy ? "Applying…" : "Apply to selected targets"
                            primary: true
                            enabled: !root.busy && root.selected && root.selected.installable !== false
                            onClicked: root.applySelected()
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

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "◇"
                            color: colors.faint
                            font.pixelSize: 42
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

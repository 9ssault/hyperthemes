import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import HyperThemes 1.0
import "."

Item {
    id: page

    required property var app

    readonly property var colors: page.app.uiColors

    property int previewPanel: 0

    function zoneList(zone) {
        return page.app.shellZoneList(zone);
    }

    function paletteColor(key, fallback) {
        return page.app.colorFrom(page.app.selected, key, fallback);
    }

    function chipLabel(id) {
        return page.app.shellChip(id);
    }

    function hasWidget(id) {
        return page.zoneList("left").indexOf(id) >= 0
            || page.zoneList("center").indexOf(id) >= 0
            || page.zoneList("right").indexOf(id) >= 0;
    }

    function previewBarColor() {
        var base = page.paletteColor("background", "#101014");
        return Qt.rgba(base.r, base.g, base.b, page.app.shellOpacity);
    }

    Component.onCompleted: {
        fontField.text = page.app.shellFontFamily;
        nameField.text = page.app.shellName;
    }

    Connections {
        target: page.app

        function onShellFontFamilyChanged() {
            if (!fontField.activeFocus)
                fontField.text = page.app.shellFontFamily;
        }

        function onShellNameChanged() {
            if (!nameField.activeFocus)
                nameField.text = page.app.shellName;
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 16

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 300
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: Math.max(280, parent.width - 8)
                spacing: 12

                ShellCard {
                    title: "QUICKSHELL"

                    Text {
                        Layout.fillWidth: true
                        text: page.app.shellRunning
                            ? "Running “" + page.app.shellName + "” from this window. Stop it here, or run `qs -c " + page.app.shellName + "` yourself."
                            : page.app.shellHasQuickshell
                                ? "Quickshell is installed. Configs are written to ~/.config/quickshell/" + page.app.shellName + " and run with qs -c " + page.app.shellName + "."
                                : "Quickshell is not on PATH. Files are still written and run anywhere Quickshell is installed."
                        color: page.app.shellRunning || page.app.shellHasQuickshell ? page.colors.success : page.colors.danger
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: page.app.shellLoadedTheme !== ""
                        text: "Loaded from the “" + page.app.shellLoadedTheme + "” palette."
                        color: page.colors.faint
                        font.pixelSize: 9
                        wrapMode: Text.WordWrap
                    }
                }

                ShellCard {
                    title: "BAR"

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Position"
                            color: page.colors.faint
                            font.pixelSize: 10
                            Layout.preferredWidth: 64
                        }

                        ActionButton {
                            text: "Top"
                            primary: page.app.shellPosition === "top"
                            onClicked: page.app.shellPosition = "top"
                        }

                        ActionButton {
                            text: "Bottom"
                            primary: page.app.shellPosition === "bottom"
                            onClicked: page.app.shellPosition = "bottom"
                        }

                        Item { Layout.fillWidth: true }
                    }

                    ShellSlider {
                        label: "Height"
                        from: 24
                        to: 64
                        value: page.app.shellBarHeight
                        onMoved: page.app.shellBarHeight = Math.round(value)
                    }

                    ShellSlider {
                        label: "Margin"
                        from: 0
                        to: 28
                        value: page.app.shellMargin
                        onMoved: page.app.shellMargin = Math.round(value)
                    }

                    ShellSlider {
                        label: "Gap"
                        from: 0
                        to: 28
                        value: page.app.shellGap
                        onMoved: page.app.shellGap = Math.round(value)
                    }

                    ShellSlider {
                        label: "Radius"
                        from: 0
                        to: 24
                        value: page.app.shellRadius
                        onMoved: page.app.shellRadius = Math.round(value)
                    }

                    ShellSlider {
                        label: "Opacity"
                        from: 0.5
                        to: 1.0
                        stepSize: 0.02
                        decimals: 2
                        suffix: ""
                        value: page.app.shellOpacity
                        onMoved: page.app.shellOpacity = value
                    }

                    ShellToggle {
                        text: "Border"
                        value: page.app.shellBorders
                        onChanged: page.app.shellBorders = value
                    }

                    ShellToggle {
                        text: "Drop shadow"
                        value: page.app.shellShadow
                        onChanged: page.app.shellShadow = value
                    }

                    ShellToggle {
                        text: "Bar on every monitor"
                        value: page.app.shellBarPerMonitor
                        onChanged: page.app.shellBarPerMonitor = value
                    }
                }

                ShellCard {
                    title: "ENTRIES"

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Repeater {
                            model: [
                                { label: "LEFT", key: "left" },
                                { label: "CENTER", key: "center" },
                                { label: "RIGHT", key: "right" }
                            ]

                            ColumnLayout {
                                id: zoneColumn

                                required property var modelData

                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: zoneColumn.modelData.label
                                    color: page.colors.muted
                                    font.pixelSize: 9
                                    font.letterSpacing: 0.5
                                    font.weight: Font.DemiBold
                                }

                                Repeater {
                                    model: page.app.shellWidgets

                                    Rectangle {
                                        id: widgetRow

                                        required property var modelData

                                        readonly property string zoneKey: zoneColumn.modelData.key
                                        readonly property bool active: page.app.shellHasWidget(widgetRow.modelData.id, widgetRow.zoneKey)

                                        Layout.fillWidth: true
                                        implicitHeight: 24
                                        radius: 5
                                        color: widgetRow.active ? page.colors.panel2 : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 6
                                            anchors.rightMargin: 4
                                            spacing: 6

                                            Rectangle {
                                                Layout.preferredWidth: 13
                                                Layout.preferredHeight: 13
                                                radius: 4
                                                color: widgetRow.active ? page.colors.accent : "transparent"
                                                border.width: 1
                                                border.color: widgetRow.active ? page.colors.accent : page.colors.border

                                                AppIcon {
                                                    anchors.centerIn: parent
                                                    visible: widgetRow.active
                                                    iconSize: 9
                                                    width: 9
                                                    height: 9
                                                    source: "qrc:/data/icons/check.svg"
                                                    color: page.colors.onAccent
                                                }
                                            }

                                            Text {
                                                Layout.fillWidth: true
                                                text: widgetRow.modelData.label
                                                color: page.colors.text
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }
                                        }

                                        TapHandler {
                                            onTapped: page.app.shellSetWidget(widgetRow.modelData.id,
                                                                              widgetRow.zoneKey,
                                                                              !widgetRow.active)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Each entry can live in one zone. Click an entry to move it here."
                        color: page.colors.faint
                        font.pixelSize: 9
                        wrapMode: Text.WordWrap
                    }
                }

                ShellCard {
                    title: "MODULES & TYPE"

                    ShellToggle {
                        objectName: "shellLauncherToggle"
                        text: "Application launcher"
                        value: page.app.shellLauncher
                        onChanged: page.app.shellLauncher = value
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !page.app.shellLauncher && page.hasWidget("launcher")
                        text: "The Launcher entry is in the bar, but the launcher module is off, so it will not open anything."
                        color: page.colors.danger
                        font.pixelSize: 9
                        wrapMode: Text.WordWrap
                    }

                    ShellToggle {
                        text: "Dashboard with media and session actions"
                        value: page.app.shellDashboard
                        onChanged: page.app.shellDashboard = value
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !page.app.shellDashboard
                            && (page.hasWidget("clock") || page.hasWidget("date") || page.hasWidget("power"))
                        text: "Clock, date and power entries open the dashboard, which is currently off."
                        color: page.colors.danger
                        font.pixelSize: 9
                        wrapMode: Text.WordWrap
                    }

                    ShellToggle {
                        text: "24-hour clock"
                        value: page.app.shellClock24h
                        onChanged: page.app.shellClock24h = value
                    }

                    ShellToggle {
                        text: "Show seconds"
                        value: page.app.shellShowSeconds
                        onChanged: page.app.shellShowSeconds = value
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            text: "Font"
                            color: page.colors.faint
                            font.pixelSize: 10
                            Layout.preferredWidth: 64
                        }

                        TextField {
                            id: fontField

                            Layout.fillWidth: true
                            placeholderText: "Application default"
                            font.pixelSize: 10
                            color: page.colors.text
                            placeholderTextColor: page.colors.faint
                            selectByMouse: true
                            onTextEdited: page.app.shellFontFamily = text.trim()
                            background: Rectangle {
                                radius: 6
                                color: page.colors.panel2
                                border.width: 1
                                border.color: fontField.activeFocus ? page.colors.accent : page.colors.border
                            }
                        }
                    }

                    ShellSlider {
                        label: "Font size"
                        from: 8
                        to: 18
                        value: page.app.shellFontSize
                        onMoved: page.app.shellFontSize = Math.round(value)
                    }

                    ShellSlider {
                        label: "Icon size"
                        from: 10
                        to: 24
                        value: page.app.shellIconSize
                        onMoved: page.app.shellIconSize = Math.round(value)
                    }

                    ShellSlider {
                        label: "Workspaces"
                        from: 1
                        to: 12
                        value: page.app.shellWorkspaceCount
                        onMoved: page.app.shellWorkspaceCount = Math.round(value)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Workspaces"
                            color: page.colors.faint
                            font.pixelSize: 10
                            Layout.preferredWidth: 64
                        }

                        ActionButton {
                            text: "Hyprland"
                            primary: page.app.shellCompositor === "hyprland"
                            onClicked: page.app.shellCompositor = "hyprland"
                        }

                        ActionButton {
                            text: "Sway / i3"
                            primary: page.app.shellCompositor === "sway"
                            onClicked: page.app.shellCompositor = "sway"
                        }

                        ActionButton {
                            text: "Placeholder"
                            primary: page.app.shellCompositor === "none"
                            onClicked: page.app.shellCompositor = "none"
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: page.app.shellCompositor === "hyprland"
                            ? "Workspace entries follow Quickshell.Hyprland and switch workspaces when clicked."
                            : page.app.shellCompositor === "sway"
                                ? "Workspace entries follow Quickshell.I3 (Sway/i3) and activate workspaces when clicked."
                                : "Workspaces are decorative dots; pick Hyprland or Sway/i3 to wire them up."
                        color: page.colors.faint
                        font.pixelSize: 9
                        wrapMode: Text.WordWrap
                    }
                }

                Item { Layout.preferredHeight: 4 }
            }
        }

        ScrollView {
            visible: page.width >= 680
            Layout.preferredWidth: Math.min(360, Math.max(270, page.width * 0.42))
            Layout.fillHeight: true
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: Math.max(250, parent.width - 10)
                spacing: 12

                ShellCard {
                    title: "PREVIEW"

                    Rectangle {
                        id: preview

                        Layout.fillWidth: true
                        Layout.preferredHeight: 300
                        radius: 10
                        clip: true
                        color: page.paletteColor("surfaceContainerLow", page.colors.panel2)

                        Rectangle {
                            anchors.fill: parent
                            opacity: 0.72
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: page.paletteColor("background", "#101014") }
                                GradientStop { position: 0.6; color: page.paletteColor("surfaceContainer", "#19151e") }
                                GradientStop { position: 1.0; color: page.paletteColor("primary", "#8a7bb8") }
                            }
                        }

                        Rectangle {
                            visible: page.app.shellShadow
                            x: barPreview.x + 2
                            y: barPreview.y + 3
                            width: barPreview.width
                            height: barPreview.height
                            radius: barPreview.radius
                            color: "#000000"
                            opacity: 0.28
                        }

                        Rectangle {
                            id: barPreview

                            readonly property int inset: 8 + Math.round(page.app.shellMargin * 0.5)

                            x: inset
                            width: preview.width - inset * 2
                            height: Math.max(22, page.app.shellBarHeight * 0.72)
                            y: page.app.shellPosition === "top" ? inset : preview.height - height - inset
                            radius: Math.max(0, page.app.shellRadius * 0.7)
                            clip: true
                            color: page.previewBarColor()
                            border.width: page.app.shellBorders ? 1 : 0
                            border.color: page.paletteColor("outlineVariant", "#2e2735")

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 4

                                Repeater {
                                    model: page.zoneList("left")

                                    Rectangle {
                                        required property var modelData
                                        implicitWidth: leftChip.implicitWidth + 10
                                        implicitHeight: 16
                                        radius: 4
                                        color: page.paletteColor("surfaceContainerHigh", "#2a2630")

                                        Text {
                                            id: leftChip
                                            anchors.centerIn: parent
                                            text: page.chipLabel(modelData)
                                            color: page.paletteColor("onSurface", "#f2ecf5")
                                            font.pixelSize: 8
                                        }
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Repeater {
                                    model: page.zoneList("center")

                                    Rectangle {
                                        required property var modelData
                                        implicitWidth: centerChip.implicitWidth + 10
                                        implicitHeight: 16
                                        radius: 4
                                        color: page.paletteColor("surfaceContainerHigh", "#2a2630")

                                        Text {
                                            id: centerChip
                                            anchors.centerIn: parent
                                            text: page.chipLabel(modelData)
                                            color: page.paletteColor("onSurface", "#f2ecf5")
                                            font.pixelSize: 8
                                        }
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Repeater {
                                    model: page.zoneList("right")

                                    Rectangle {
                                        required property var modelData
                                        implicitWidth: rightChip.implicitWidth + 10
                                        implicitHeight: 16
                                        radius: 4
                                        color: page.paletteColor("surfaceContainerHigh", "#2a2630")

                                        Text {
                                            id: rightChip
                                            anchors.centerIn: parent
                                            text: page.chipLabel(modelData)
                                            color: page.paletteColor("onSurface", "#f2ecf5")
                                            font.pixelSize: 8
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: page.previewPanel === 1 && page.app.shellLauncher
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 48, 300)
                            height: Math.min(parent.height - 64, 210)
                            radius: 10
                            color: Qt.rgba(page.paletteColor("surface", "#19151e").r,
                                           page.paletteColor("surface", "#19151e").g,
                                           page.paletteColor("surface", "#19151e").b, 0.97)
                            border.width: 1
                            border.color: page.paletteColor("surfaceContainerHigh", "#2a2630")

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 7

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 26
                                    radius: 6
                                    color: page.paletteColor("surfaceContainerHigh", "#2a2630")

                                    Text {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        verticalAlignment: Text.AlignVCenter
                                        text: "Search applications…"
                                        color: page.paletteColor("onSurfaceVariant", "#a79caf")
                                        font.pixelSize: 9
                                    }
                                }

                                Repeater {
                                    model: ["Firefox", "Files", "Terminal"]

                                    Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 24
                                        radius: 5
                                        color: "transparent"

                                        Text {
                                            anchors.fill: parent
                                            anchors.leftMargin: 4
                                            verticalAlignment: Text.AlignVCenter
                                            text: modelData
                                            color: page.paletteColor("onSurface", "#f2ecf5")
                                            font.pixelSize: 9
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: page.previewPanel === 2 && page.app.shellDashboard
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 48, 300)
                            height: Math.min(parent.height - 64, 210)
                            radius: 10
                            color: Qt.rgba(page.paletteColor("surface", "#19151e").r,
                                           page.paletteColor("surface", "#19151e").g,
                                           page.paletteColor("surface", "#19151e").b, 0.97)
                            border.width: 1
                            border.color: page.paletteColor("surfaceContainerHigh", "#2a2630")

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: "Sunday, 25 October · 12:30"
                                    color: page.paletteColor("onSurface", "#f2ecf5")
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: "Nothing playing"
                                    color: page.paletteColor("onSurfaceVariant", "#a79caf")
                                    font.pixelSize: 9
                                }

                                Item { Layout.fillHeight: true }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Repeater {
                                        model: ["Lock", "Sleep", "Reboot", "Off"]

                                        Rectangle {
                                            required property var modelData
                                            implicitWidth: 48
                                            implicitHeight: 20
                                            radius: 5
                                            color: page.paletteColor("surfaceContainerHigh", "#2a2630")

                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData
                                                color: page.paletteColor("onSurface", "#f2ecf5")
                                                font.pixelSize: 8
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.margins: 10
                            visible: !page.app.selected
                            text: "Select a theme for a real preview"
                            color: "#f2ecf5"
                            font.pixelSize: 9
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        ActionButton {
                            text: "Launcher"
                            primary: page.previewPanel === 1
                            enabled: page.app.shellLauncher
                            onClicked: page.previewPanel = page.previewPanel === 1 ? 0 : 1
                        }

                        ActionButton {
                            text: "Dashboard"
                            primary: page.previewPanel === 2
                            enabled: page.app.shellDashboard
                            onClicked: page.previewPanel = page.previewPanel === 2 ? 0 : 2
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: page.app.shellBarPerMonitor ? "all monitors" : "first monitor"
                            color: page.colors.faint
                            font.pixelSize: 9
                        }
                    }
                }

                ShellCard {
                    title: "OUTPUT"

                    TextField {
                        id: nameField

                        Layout.fillWidth: true
                        placeholderText: "config name"
                        font.pixelSize: 11
                        color: page.colors.text
                        placeholderTextColor: page.colors.faint
                        selectByMouse: true
                        onTextEdited: page.app.shellName = text.toLowerCase().trim()
                        background: Rectangle {
                            radius: 6
                            color: page.colors.panel2
                            border.width: 1
                            border.color: nameField.activeFocus ? page.colors.accent : page.colors.border
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: ShellBuilder.configDir(page.app.shellName)
                        color: page.colors.faint
                        font.pixelSize: 9
                        elide: Text.ElideMiddle
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        ActionButton {
                            text: "Write config"
                            primary: true
                            onClicked: page.app.shellBuild()
                        }

                        ActionButton {
                            text: "Validate"
                            enabled: page.app.shellResult !== ""
                            onClicked: page.app.shellValidate()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        ActionButton {
                            text: page.app.shellRunning ? "Stop" : "Run"
                            primary: !page.app.shellRunning
                            danger: page.app.shellRunning
                            enabled: page.app.shellResult !== "" && page.app.shellHasQuickshell
                            onClicked: page.app.shellRunning ? page.app.shellStop() : page.app.shellRun()
                        }

                        ActionButton {
                            text: "Open folder"
                            enabled: page.app.shellResult !== ""
                            onClicked: Qt.openUrlExternally(page.app.fileUrl(page.app.shellResult))
                        }

                        ActionButton {
                            text: "Export"
                            enabled: page.app.shellResult !== ""
                            onClicked: page.app.openShellExport()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        ActionButton {
                            text: "Reset defaults"
                            onClicked: page.app.shellResetSettings()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: page.app.shellError !== ""
                        text: page.app.shellError
                        color: page.colors.danger
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: page.app.shellValidation !== ""
                        text: page.app.shellValidation
                        color: page.app.shellValidationOk ? page.colors.success : page.colors.danger
                        font.pixelSize: 9
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: page.app.shellFiles.length > 0
                        text: page.app.shellFiles.join("  ·  ")
                        color: page.colors.success
                        font.pixelSize: 9
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        visible: page.app.shellRunLog !== ""
                        radius: 6
                        color: page.colors.panel2
                        border.width: 1
                        border.color: page.colors.border

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 8
                            clip: true

                            TextArea {
                                readOnly: true
                                text: page.app.shellRunLog
                                color: page.colors.faint
                                font.family: "monospace"
                                font.pixelSize: 9
                                wrapMode: TextArea.Wrap
                                selectByMouse: true
                                background: null
                            }
                        }
                    }
                }

                ShellCard {
                    title: "GENERATED CONFIGS"
                    visible: page.app.shellConfigs.length > 0

                    Repeater {
                        model: page.app.shellConfigs

                        RowLayout {
                            required property var modelData

                            Layout.fillWidth: true
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: page.colors.text
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.managed
                                        ? (modelData.theme ? modelData.theme : "custom palette")
                                        : "created outside HyperThemes"
                                    color: page.colors.faint
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }

                            ActionButton {
                                text: "Load"
                                enabled: modelData.managed
                                onClicked: page.app.shellLoad(modelData.name)
                            }

                            ActionButton {
                                text: "Rename"
                                onClicked: {
                                    renameDialog.sourceName = modelData.name;
                                    renameDialog.open();
                                }
                            }

                            ActionButton {
                                text: "Delete"
                                danger: true
                                onClicked: {
                                    deleteDialog.targetName = modelData.name;
                                    deleteDialog.open();
                                }
                            }
                        }
                    }
                }

                ShellCard {
                    title: "BACKUPS"
                    visible: page.app.shellBackupEntries.length > 0

                    Repeater {
                        model: page.app.shellBackupEntries

                        RowLayout {
                            required property var modelData
                            required property int index

                            Layout.fillWidth: true
                            spacing: 8
                            visible: index < 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: page.colors.text
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.created
                                        ? String(modelData.created).replace("T", " ").substring(0, 16)
                                        : modelData.id
                                    color: page.colors.faint
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }

                            ActionButton {
                                text: "Restore"
                                danger: true
                                onClicked: page.app.shellRestore(modelData.id)
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: page.app.shellBackupEntries.length > 8
                        text: "Showing the 8 most recent backups."
                        color: page.colors.faint
                        font.pixelSize: 9
                    }
                }

                Item { Layout.preferredHeight: 4 }
            }
        }
    }

    Dialog {
        id: renameDialog
        objectName: "renameDialog"

        property string sourceName: ""
        property string proposedName: ""

        function refreshOk() {
            var button = renameDialog.standardButton(Dialog.Ok);
            if (button)
                button.enabled = proposedName !== sourceName && proposedName.length > 0;
        }

        title: sourceName === "" ? "Rename config" : "Rename “" + sourceName + "”"
        modal: true
        anchors.centerIn: parent
        width: 380
        standardButtons: Dialog.Ok | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape
        onProposedNameChanged: refreshOk()
        onOpened: {
            proposedName = sourceName;
            renameField.text = sourceName;
            renameField.forceActiveFocus();
            renameField.selectAll();
            refreshOk();
        }
        onAccepted: page.app.shellRename(sourceName, proposedName)

        contentItem: ColumnLayout {
            implicitWidth: 340
            spacing: 8

            TextField {
                id: renameField

                Layout.fillWidth: true
                font.pixelSize: 11
                color: page.colors.text
                placeholderTextColor: page.colors.faint
                selectByMouse: true
                onTextEdited: renameDialog.proposedName = text.toLowerCase().trim()
                background: Rectangle {
                    radius: 6
                    color: page.colors.panel2
                    border.width: 1
                    border.color: renameField.activeFocus ? page.colors.accent : page.colors.border
                }
            }

            Text {
                Layout.fillWidth: true
                text: "Lowercase letters, digits, dots, dashes and underscores only."
                color: page.colors.faint
                font.pixelSize: 9
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: deleteDialog
        objectName: "deleteDialog"

        property string targetName: ""

        title: targetName === "" ? "Delete config" : "Delete “" + targetName + "”?"
        modal: true
        anchors.centerIn: parent
        width: 380
        standardButtons: Dialog.Cancel | Dialog.Ok
        closePolicy: Popup.CloseOnEscape
        onAccepted: page.app.shellRemove(targetName)

        contentItem: ColumnLayout {
            implicitWidth: 340
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: "The folder is copied to Backups first, so it can be restored later."
                color: page.colors.text
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: page.app.shellRunning
                    ? "Stop the generated shell before deleting it."
                    : "This removes shell.qml and every generated module from the folder."
                color: page.colors.faint
                font.pixelSize: 9
                wrapMode: Text.WordWrap
            }
        }
    }
}

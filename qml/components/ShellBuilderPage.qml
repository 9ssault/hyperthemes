import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import HyperThemes 1.0
import "."

Item {
    id: page

    required property var app

    readonly property var colors: page.app.uiColors

    RowLayout {
        anchors.fill: parent
        spacing: 16

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 280
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: Math.max(260, parent.width - 8)
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: statusText.implicitHeight + 20
                    radius: 10
                    color: page.colors.panel
                    border.width: 1
                    border.color: page.colors.border

                    Text {
                        id: statusText
                        anchors.fill: parent
                        anchors.margins: 10
                        text: page.app.shellHasQuickshell
                            ? "Quickshell is installed. The config lands in ~/.config/quickshell/" + page.app.shellName + " and runs with qs -c " + page.app.shellName + "."
                            : "Quickshell is not on PATH. The files are still written and run anywhere Quickshell is installed."
                        color: page.app.shellHasQuickshell ? page.colors.success : page.colors.danger
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: barCard.implicitHeight + 28
                    radius: 12
                    color: page.colors.panel
                    border.width: 1
                    border.color: page.colors.border

                    ColumnLayout {
                        id: barCard
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Text {
                            text: "BAR"
                            color: page.colors.muted
                            font.pixelSize: 9
                            font.letterSpacing: 0.5
                            font.weight: Font.DemiBold
                        }

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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                text: "Height"
                                color: page.colors.faint
                                font.pixelSize: 10
                                Layout.preferredWidth: 64
                            }

                            Slider {
                                Layout.fillWidth: true
                                from: 26
                                to: 56
                                stepSize: 1
                                value: page.app.shellBarHeight
                                onMoved: page.app.shellBarHeight = Math.round(value)
                            }

                            Text {
                                text: page.app.shellBarHeight + " px"
                                color: page.colors.faint
                                font.pixelSize: 10
                                Layout.preferredWidth: 42
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                text: "Margin"
                                color: page.colors.faint
                                font.pixelSize: 10
                                Layout.preferredWidth: 64
                            }

                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 24
                                stepSize: 1
                                value: page.app.shellMargin
                                onMoved: page.app.shellMargin = Math.round(value)
                            }

                            Text {
                                text: page.app.shellMargin + " px"
                                color: page.colors.faint
                                font.pixelSize: 10
                                Layout.preferredWidth: 42
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: entriesCard.implicitHeight + 28
                    radius: 12
                    color: page.colors.panel
                    border.width: 1
                    border.color: page.colors.border

                    ColumnLayout {
                        id: entriesCard
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text {
                            text: "ENTRIES"
                            color: page.colors.muted
                            font.pixelSize: 9
                            font.letterSpacing: 0.5
                            font.weight: Font.DemiBold
                        }

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
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: modulesCard.implicitHeight + 28
                    radius: 12
                    color: page.colors.panel
                    border.width: 1
                    border.color: page.colors.border

                    ColumnLayout {
                        id: modulesCard
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text {
                            text: "MODULES"
                            color: page.colors.muted
                            font.pixelSize: 9
                            font.letterSpacing: 0.5
                            font.weight: Font.DemiBold
                        }

                        ThemeSwitch {
                            text: "Application launcher"
                            checked: page.app.shellLauncher
                            onToggled: page.app.shellLauncher = checked
                        }

                        ThemeSwitch {
                            text: "Dashboard with power actions"
                            checked: page.app.shellDashboard
                            onToggled: page.app.shellDashboard = checked
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                text: "Radius"
                                color: page.colors.faint
                                font.pixelSize: 10
                                Layout.preferredWidth: 64
                            }

                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 24
                                stepSize: 1
                                value: page.app.shellRadius
                                onMoved: page.app.shellRadius = Math.round(value)
                            }

                            Text {
                                text: page.app.shellRadius + " px"
                                color: page.colors.faint
                                font.pixelSize: 10
                                Layout.preferredWidth: 42
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                text: "Opacity"
                                color: page.colors.faint
                                font.pixelSize: 10
                                Layout.preferredWidth: 64
                            }

                            Slider {
                                Layout.fillWidth: true
                                from: 0.5
                                to: 1.0
                                stepSize: 0.02
                                value: page.app.shellOpacity
                                onMoved: page.app.shellOpacity = value
                            }

                            Text {
                                text: page.app.shellOpacity.toFixed(2)
                                color: page.colors.faint
                                font.pixelSize: 10
                                Layout.preferredWidth: 42
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 4 }
            }
        }

        ScrollView {
            visible: page.width >= 640
            Layout.preferredWidth: Math.min(320, Math.max(210, page.width * 0.4))
            Layout.fillHeight: true
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: Math.max(190, parent.width - 10)
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    radius: 12
                    clip: true
                    color: page.app.colorFrom(page.app.selected, "surfaceContainerLow", page.colors.panel2)

                    Rectangle {
                        anchors.fill: parent
                        opacity: 0.72
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: page.app.colorFrom(page.app.selected, "background", "#101014") }
                            GradientStop { position: 0.6; color: page.app.colorFrom(page.app.selected, "surfaceContainer", "#19151e") }
                            GradientStop { position: 1.0; color: page.app.colorFrom(page.app.selected, "primary", "#8a7bb8") }
                        }
                    }

                    Rectangle {
                        id: barPreview

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        y: page.app.shellPosition === "top" ? 10 : parent.height - height - 10
                        height: Math.max(24, page.app.shellBarHeight * 0.8)
                        radius: 6
                        clip: true
                        color: Qt.rgba(page.app.colorFrom(page.app.selected, "background", "#101014").r,
                                       page.app.colorFrom(page.app.selected, "background", "#101014").g,
                                       page.app.colorFrom(page.app.selected, "background", "#101014").b,
                                       page.app.shellOpacity)
                        border.width: 1
                        border.color: page.app.colorFrom(page.app.selected, "outlineVariant", "#2e2735")

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Repeater {
                                model: page.app.shellZoneList("left")

                                Rectangle {
                                    required property var modelData
                                    implicitWidth: leftChip.implicitWidth + 10
                                    implicitHeight: 16
                                    radius: 4
                                    color: page.app.colorFrom(page.app.selected, "surfaceContainerHigh", "#2a2630")

                                    Text {
                                        id: leftChip
                                        anchors.centerIn: parent
                                        text: page.app.shellChip(modelData)
                                        color: page.app.colorFrom(page.app.selected, "onSurface", "#f2ecf5")
                                        font.pixelSize: 8
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Repeater {
                                model: page.app.shellZoneList("center")

                                Rectangle {
                                    required property var modelData
                                    implicitWidth: centerChip.implicitWidth + 10
                                    implicitHeight: 16
                                    radius: 4
                                    color: page.app.colorFrom(page.app.selected, "surfaceContainerHigh", "#2a2630")

                                    Text {
                                        id: centerChip
                                        anchors.centerIn: parent
                                        text: page.app.shellChip(modelData)
                                        color: page.app.colorFrom(page.app.selected, "onSurface", "#f2ecf5")
                                        font.pixelSize: 8
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Repeater {
                                model: page.app.shellZoneList("right")

                                Rectangle {
                                    required property var modelData
                                    implicitWidth: rightChip.implicitWidth + 10
                                    implicitHeight: 16
                                    radius: 4
                                    color: page.app.colorFrom(page.app.selected, "surfaceContainerHigh", "#2a2630")

                                    Text {
                                        id: rightChip
                                        anchors.centerIn: parent
                                        text: page.app.shellChip(modelData)
                                        color: page.app.colorFrom(page.app.selected, "onSurface", "#f2ecf5")
                                        font.pixelSize: 8
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

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: outputCard.implicitHeight + 28
                    radius: 12
                    color: page.colors.panel
                    border.width: 1
                    border.color: page.colors.border

                    ColumnLayout {
                        id: outputCard
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text {
                            text: "OUTPUT"
                            color: page.colors.muted
                            font.pixelSize: 9
                            font.letterSpacing: 0.5
                            font.weight: Font.DemiBold
                        }

                        TextField {
                            id: nameField

                            Layout.fillWidth: true
                            text: page.app.shellName
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
                                text: "Open folder"
                                enabled: page.app.shellResult !== ""
                                onClicked: Qt.openUrlExternally(page.app.fileUrl(page.app.shellResult))
                            }
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
                            visible: page.app.shellFiles.length > 0
                            text: page.app.shellFiles.join("  ·  ")
                            color: page.colors.success
                            font.pixelSize: 9
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: page.app.shellBackups.length > 0
                                    ? page.app.shellBackups.length + " saved config(s)"
                                    : "No backups yet"
                                color: page.colors.faint
                                font.pixelSize: 9
                            }

                            ActionButton {
                                text: "Restore last"
                                danger: true
                                enabled: page.app.shellBackups.length > 0
                                onClicked: page.app.shellRestore(page.app.shellBackups[0])
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 4 }
            }
        }
    }
}

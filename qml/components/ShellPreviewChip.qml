import QtQuick

import HyperThemes 1.0

/// One widget in the bar preview. It mirrors the generated widget's role
/// instead of printing the same generic chip for everything.
Item {
    id: root

    required property string widgetId
    required property var app

    readonly property bool textChip: widgetId !== "logo" && widgetId !== "workspaces"
                                     && widgetId !== "separator" && widgetId !== "spacer"

    implicitHeight: 16
    implicitWidth: widgetId === "spacer" ? 12
                 : widgetId === "separator" ? 1
                 : widgetId === "logo" ? 18
                 : widgetId === "workspaces" ? 30
                 : label.implicitWidth + 10

    Rectangle {
        anchors.fill: parent
        visible: root.textChip
        radius: 4
        color: root.app.colorFrom(root.app.selected, "surfaceContainerHigh", "#2a2630")
    }

    Text {
        id: label
        anchors.centerIn: parent
        visible: root.textChip
        text: root.textChip ? root.app.shellChip(root.widgetId) : ""
        color: root.app.colorFrom(root.app.selected, "onSurface", "#f2ecf5")
        font.pixelSize: 8
        elide: Text.ElideRight
    }

    Rectangle {
        anchors.centerIn: parent
        visible: root.widgetId === "separator"
        width: 1
        height: 12
        color: root.app.colorFrom(root.app.selected, "outlineVariant", "#2e2735")
    }

    Rectangle {
        anchors.fill: parent
        visible: root.widgetId === "logo"
        radius: 4
        color: root.app.colorFrom(root.app.selected, "primary", "#8a7bb8")

        Text {
            anchors.centerIn: parent
            text: "◆"
            color: root.app.colorFrom(root.app.selected, "onPrimary", "#101014")
            font.pixelSize: 8
        }
    }

    Row {
        anchors.centerIn: parent
        visible: root.widgetId === "workspaces"
        spacing: 3

        Repeater {
            model: 3

            Rectangle {
                width: index === 0 ? 10 : 5
                height: 5
                radius: 2.5
                y: 5
                color: index === 0
                    ? root.app.colorFrom(root.app.selected, "primary", "#8a7bb8")
                    : root.app.colorFrom(root.app.selected, "surfaceHigh", "#2e2735")
            }
        }
    }
}

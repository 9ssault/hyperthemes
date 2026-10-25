import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Window

Item {
    id: root

    property url iconSource
    property string label: ""
    property bool active: false
    signal clicked()

    implicitHeight: 40

    readonly property var appTheme: Window.window && Window.window.uiColors ? Window.window.uiColors : null

    function themeColor(key, fallback) {
        return root.appTheme && root.appTheme[key] !== undefined ? root.appTheme[key] : fallback;
    }

    Rectangle {
        anchors.fill: parent
        radius: 7
        color: root.active
            ? root.themeColor("elevated", "#211b28")
            : hover.hovered ? root.themeColor("panel2", "#19151e") : "transparent"

        Rectangle {
            visible: root.active
            width: 3
            height: 16
            radius: 1.5
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: root.themeColor("accent", "#cfbcff")
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 10

            AppIcon {
                iconSize: 18
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                source: root.iconSource
                color: root.active ? root.themeColor("accent", "#cfbcff")
                                   : root.themeColor("muted", "#a79caf")
            }

            Text {
                Layout.fillWidth: true
                text: root.label
                color: root.active ? root.themeColor("text", "#f2ecf5")
                                   : root.themeColor("muted", "#a79caf")
                font.pixelSize: 13
                font.weight: root.active ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }
        }
    }

    HoverHandler {
        id: hover
    }

    TapHandler {
        onTapped: root.clicked()
    }
}

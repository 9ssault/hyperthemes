import QtQuick
import QtQuick.Controls
import QtQuick.Window

CheckBox {
    id: control

    implicitHeight: 28
    spacing: 8
    font.pixelSize: 11

    readonly property var appTheme: Window.window && Window.window.uiColors ? Window.window.uiColors : null

    function themeColor(key, fallback) {
        return control.appTheme && control.appTheme[key] !== undefined ? control.appTheme[key] : fallback;
    }

    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: 4
        color: control.checked ? control.themeColor("accent", "#cfbcff") : "transparent"
        border.width: 1
        border.color: control.checked ? control.themeColor("accent", "#cfbcff")
                                      : control.themeColor("border", "#2e2735")

        AppIcon {
            anchors.centerIn: parent
            visible: control.checked
            iconSize: 10
            width: 10
            height: 10
            source: "qrc:/data/icons/check.svg"
            color: control.themeColor("onAccent", "#18121e")
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.enabled ? control.themeColor("text", "#f2ecf5")
                               : control.themeColor("faint", "#746a7d")
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}

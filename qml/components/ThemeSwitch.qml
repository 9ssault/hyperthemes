import QtQuick
import QtQuick.Controls
import QtQuick.Window

Switch {
    id: control

    implicitHeight: 30
    spacing: 8
    font.pixelSize: 11

    HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }

    readonly property var appTheme: Window.window && Window.window.uiColors ? Window.window.uiColors : null

    function themeColor(key, fallback) {
        return control.appTheme && control.appTheme[key] !== undefined ? control.appTheme[key] : fallback;
    }

    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 38
        implicitHeight: 20
        radius: 10
        color: control.checked ? control.themeColor("accent", "#cfbcff")
                               : control.themeColor("panel2", "#19151e")
        border.width: 1
        border.color: control.checked ? control.themeColor("accent", "#cfbcff")
                                      : control.themeColor("border", "#2e2735")

        Rectangle {
            width: 14
            height: 14
            radius: 7
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: control.checked ? control.themeColor("onAccent", "#18121e")
                                   : control.themeColor("muted", "#a79caf")

            Behavior on x {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }
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

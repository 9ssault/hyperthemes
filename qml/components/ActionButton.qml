import QtQuick
import QtQuick.Controls
import QtQuick.Window

Button {
    id: control

    property bool primary: false
    property bool danger: false

    implicitHeight: 32
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: 12
    font.weight: Font.Medium
    hoverEnabled: true

    readonly property var appTheme: Window.window && Window.window.uiColors ? Window.window.uiColors : null

    function themeColor(key, fallback) {
        return control.appTheme && control.appTheme[key] !== undefined ? control.appTheme[key] : fallback;
    }

    background: Rectangle {
        radius: 7
        color: !control.enabled
            ? Qt.rgba(control.themeColor("panel2", "#19151e").r,
                      control.themeColor("panel2", "#19151e").g,
                      control.themeColor("panel2", "#19151e").b, 0.5)
            : control.primary
                ? (control.hovered ? Qt.lighter(control.themeColor("accent", "#cfbcff"), 1.08)
                                   : control.themeColor("accent", "#cfbcff"))
                : control.danger
                    ? (control.hovered
                        ? Qt.rgba(control.themeColor("danger", "#ffb4ab").r,
                                  control.themeColor("danger", "#ffb4ab").g,
                                  control.themeColor("danger", "#ffb4ab").b, 0.22)
                        : Qt.rgba(control.themeColor("danger", "#ffb4ab").r,
                                  control.themeColor("danger", "#ffb4ab").g,
                                  control.themeColor("danger", "#ffb4ab").b, 0.12))
                    : (control.hovered ? Qt.lighter(control.themeColor("elevated", "#211b28"), 1.15)
                                       : control.themeColor("panel2", "#19151e"))
        border.width: control.primary ? 0 : 1
        border.color: control.danger
            ? Qt.rgba(control.themeColor("danger", "#ffb4ab").r,
                      control.themeColor("danger", "#ffb4ab").g,
                      control.themeColor("danger", "#ffb4ab").b, 0.4)
            : control.themeColor("border", "#2e2735")
        opacity: control.enabled ? 1 : 0.6
    }

    contentItem: Text {
        text: control.text
        color: !control.enabled
            ? control.themeColor("faint", "#746a7d")
            : control.primary
                ? control.themeColor("onAccent", "#18121e")
                : control.danger
                    ? control.themeColor("danger", "#ffb4ab")
                    : control.themeColor("text", "#f2ecf5")
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}

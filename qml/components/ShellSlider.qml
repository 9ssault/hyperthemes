import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

RowLayout {
    id: control

    property string label: ""
    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real value: 0
    property string suffix: " px"
    property int decimals: 0

    signal moved(real value)

    readonly property var appTheme: Window.window && Window.window.uiColors ? Window.window.uiColors : null

    function themeColor(key, fallback) {
        return control.appTheme && control.appTheme[key] !== undefined ? control.appTheme[key] : fallback;
    }

    Layout.fillWidth: true
    spacing: 10

    Text {
        Layout.preferredWidth: 64
        text: control.label
        color: control.themeColor("faint", "#746a7d")
        font.pixelSize: 10
        elide: Text.ElideRight
    }

    Slider {
        id: slider

        Layout.fillWidth: true
        from: control.from
        to: control.to
        stepSize: control.stepSize
        onMoved: control.moved(value)

        Binding {
            target: slider
            property: "value"
            value: control.value
            restoreMode: Binding.RestoreNone
        }
    }

    Text {
        Layout.preferredWidth: 48
        text: control.decimals > 0
            ? control.value.toFixed(control.decimals)
            : Math.round(control.value) + control.suffix
        color: control.themeColor("faint", "#746a7d")
        font.pixelSize: 10
    }
}

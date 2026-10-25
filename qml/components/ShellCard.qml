import QtQuick
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id: card

    property string title: ""
    default property alias content: body.data

    readonly property var appTheme: Window.window && Window.window.uiColors ? Window.window.uiColors : null

    function themeColor(key, fallback) {
        return card.appTheme && card.appTheme[key] !== undefined ? card.appTheme[key] : fallback;
    }

    Layout.fillWidth: true
    implicitHeight: body.implicitHeight + 28
    radius: 12
    color: card.themeColor("panel", "#19151e")
    border.width: 1
    border.color: card.themeColor("border", "#2e2735")

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        Text {
            Layout.fillWidth: true
            visible: card.title !== ""
            text: card.title
            color: card.themeColor("muted", "#a79caf")
            font.pixelSize: 9
            font.letterSpacing: 0.5
            font.weight: Font.DemiBold
        }
    }
}

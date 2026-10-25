import QtQuick

Rectangle {
    id: root

    property string text: ""
    property color background: "#2a2430"
    property color foreground: "#d8d0df"

    implicitWidth: label.implicitWidth + 16
    implicitHeight: 22
    radius: 11
    color: root.background

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: root.foreground
        font.pixelSize: 10
        font.weight: Font.DemiBold
        font.letterSpacing: 0.6
        font.capitalization: Font.AllUppercase
    }
}

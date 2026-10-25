import QtQuick

Rectangle {
    id: root

    property string text: ""
    property color background: "#2a2430"
    property color foreground: "#d8d0df"

    implicitWidth: label.implicitWidth + 16
    implicitHeight: 21
    radius: 10
    color: root.background

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: root.foreground
        font.pixelSize: 10
        font.weight: Font.Medium
    }
}

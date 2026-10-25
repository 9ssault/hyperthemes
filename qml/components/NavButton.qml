import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property string icon: "•"
    property string label: ""
    property bool active: false
    signal clicked()

    implicitHeight: 44

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: root.active ? "#292331" : (hover.hovered ? "#1d1922" : "transparent")

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 12

            Text {
                text: root.icon
                color: root.active ? "#d8c7ff" : "#8e8498"
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                Layout.preferredWidth: 20
            }

            Text {
                text: root.label
                color: root.active ? "#f2ecf5" : "#b9afc2"
                font.pixelSize: 13
                font.weight: root.active ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Rectangle {
            visible: root.active
            width: 3
            height: 18
            radius: 2
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: "#cfbcff"
        }
    }

    HoverHandler {
        id: hover
    }

    TapHandler {
        onTapped: root.clicked()
    }
}

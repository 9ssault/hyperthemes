import QtQuick
import QtQuick.Controls

Button {
    id: root

    property bool primary: false
    property bool danger: false

    implicitHeight: 36
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 12
    font.weight: Font.DemiBold

    background: Rectangle {
        radius: 10
        color: !root.enabled
            ? "#211c27"
            : root.primary
                ? (hover.hovered ? "#ded1ff" : "#cfbcff")
                : root.danger
                    ? (hover.hovered ? "#3a2023" : "#2c1a1d")
                    : (hover.hovered ? "#30283a" : "#241f2b")
        border.width: 1
        border.color: root.primary ? "transparent" : (root.danger ? "#5d2f35" : "#37303f")
    }

    contentItem: Text {
        text: root.text
        color: !root.enabled ? "#6f6579" : root.primary ? "#21182b" : root.danger ? "#ffb4ab" : "#e8e1ee"
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    HoverHandler {
        id: hover
    }
}

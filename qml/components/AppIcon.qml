import QtQuick
import QtQuick.Effects

Item {
    id: root

    property url source
    property color color: "#ffffff"
    property int iconSize: 18

    implicitWidth: iconSize
    implicitHeight: iconSize

    Image {
        id: image

        anchors.fill: parent
        source: root.source
        sourceSize.width: root.iconSize * 2
        sourceSize.height: root.iconSize * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: image
        colorization: 1.0
        colorizationColor: root.color
        saturation: 0
        brightness: 0
    }
}

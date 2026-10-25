import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property var modelData
    required property var paletteColors

    signal activated()

    width: 216
    height: 128
    radius: 12
    color: hover.hovered ? root.paletteColors.panel2 : root.paletteColors.panel
    border.width: 1
    border.color: hover.hovered ? root.paletteColors.accent : root.paletteColors.border

    HoverHandler {
        id: hover
    }

    TapHandler {
        onTapped: root.activated()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: root.modelData ? root.modelData.label : ""
                color: root.paletteColors.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                text: root.modelData ? root.modelData.count : 0
                color: root.paletteColors.faint
                font.pixelSize: 10
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: root.modelData ? root.modelData.colours : []

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 22
                    radius: 5
                    color: modelData
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.modelData && root.modelData.samples.length > 0 ? root.modelData.samples.join(" · ") : ""
            color: root.paletteColors.muted
            font.pixelSize: 10
            elide: Text.ElideRight
        }

        Item { Layout.fillHeight: true }

        Text {
            text: root.modelData && root.modelData.kind === "family" ? "Family" : "Collection"
            color: root.paletteColors.faint
            font.pixelSize: 9
        }
    }
}

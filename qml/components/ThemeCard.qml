import QtQuick
import QtQuick.Layouts

Item {
    id: root

    required property var item
    required property bool selected
    signal activated()

    implicitWidth: 272
    implicitHeight: 236

    function hex(key, fallback) {
        if (!root.item || !root.item.palette)
            return fallback;
        var value = root.item.palette[key];
        if (typeof value !== "string" || value.length === 0)
            return fallback;
        return value.charAt(0) === "#" ? value : "#" + value;
    }

    function sourceLabel(source) {
        if (source === "bundled")
            return "Bundled";
        if (source === "kde-store")
            return "KDE Store";
        if (source === "github")
            return "GitHub";
        if (source === "local")
            return "Local";
        if (source === "wallpaper")
            return "Dynamic";
        return source || "Theme";
    }

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: root.item && root.item.palette ? root.hex("surfaceContainer", "#1d1722") : "#1d1722"
        border.width: 1
        border.color: root.selected ? "#cfbcff" : (hover.hovered ? "#4a4054" : "#2b2632")
        clip: true

        Rectangle {
            id: preview

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 104
            radius: 18

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: root.hex("background", "#161219") }
                GradientStop { position: 0.58; color: root.hex("surfaceContainer", "#241d2b") }
                GradientStop { position: 1.0; color: root.hex("primary", "#8a7bb8") }
            }

            Image {
                anchors.fill: parent
                source: root.item && root.item.preview ? root.item.preview : ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                visible: source !== "" && status !== Image.Error
            }

            Rectangle {
                anchors.fill: parent
                visible: root.item && root.item.preview !== ""
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#55000000" }
                    GradientStop { position: 1.0; color: "#aa000000" }
                }
            }

            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 6

                Pill {
                    text: root.sourceLabel(root.item ? root.item.source : "")
                    background: "#66120e18"
                    foreground: "#eee7f2"
                }

                Rectangle {
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    radius: 3
                    color: root.item && root.item.installable === false ? "#d8b26a" : "#8fd6a0"
                }

                Item { Layout.fillWidth: true }

                Text {
                    visible: root.item && root.item.stars > 0
                    text: "★ " + (root.item ? root.item.stars : 0)
                    color: "#f3e7c8"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                text: root.item ? root.item.name : ""
                color: "#ffffff"
                font.pixelSize: 17
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            anchors.top: preview.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 5

            Text {
                Layout.fillWidth: true
                text: root.item ? (root.item.description || "") : ""
                color: root.hex("onSurfaceVariant", "#aaa0b3")
                font.pixelSize: 11
                lineHeight: 1.2
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Repeater {
                    model: ["primary", "secondary", "tertiary", "surface", "surfaceContainerHigh", "error"]

                    Rectangle {
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        radius: 4
                        color: root.hex(modelData, "#313039")
                        border.width: 1
                        border.color: "#33000000"
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: root.item ? (root.item.mode || "") : ""
                    color: root.hex("onSurfaceVariant", "#9b91a6")
                    font.pixelSize: 10
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 0.5
                }

                Text {
                    text: root.item && root.item.needsInstall ? "Install" : (root.item && root.item.installable === false ? "Browse" : "Apply")
                    color: root.item && root.item.installable === false ? "#d8b26a" : root.hex("primary", "#cfbcff")
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
            }
        }
    }

    HoverHandler {
        id: hover
    }

    TapHandler {
        onTapped: root.activated()
    }
}

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root

    required property var item
    required property bool selected
    signal activated()

    implicitWidth: 280
    implicitHeight: 248

    function hex(key, fallback) {
        if (!root.item || !root.item.palette)
            return fallback;
        var value = root.item.palette[key];
        if (typeof value !== "string" || value.length === 0)
            return fallback;
        return value.charAt(0) === "#" ? value : "#" + value;
    }

    readonly property bool hasColors: !!item && !!item.palette && Object.keys(item.palette).length > 0

    function sourceLabel(source) {
        if (source === "bundled")
            return "Bundled";
        if (source === "base16")
            return "Base16";
        if (source === "kde-store")
            return "KDE Store";
        if (source === "github")
            return "GitHub";
        if (source === "gitlab")
            return "GitLab";
        if (source === "local")
            return "Local";
        if (source === "wallpaper")
            return "Dynamic";
        return source || "Theme";
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: root.hex("surfaceContainerLow", root.hex("surfaceContainer", "#16131a"))
        border.width: 1
        border.color: root.selected
            ? root.hex("primary", "#cfbcff")
            : (hover.hovered ? root.hex("outline", "#4a4054") : root.hex("outlineVariant", "#2b2632"))
        clip: true

        Rectangle {
            id: preview

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 108
            radius: 12

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: root.hex("background", "#161219") }
                GradientStop { position: 0.62; color: root.hex("surfaceContainer", "#241d2b") }
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
                          + (root.item && root.item.mode ? "  ·  " + root.item.mode.charAt(0).toUpperCase() + root.item.mode.slice(1) : "")
                    background: "#8c0e0b13"
                    foreground: "#e8e2ee"
                }

                Rectangle {
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    radius: 3
                    color: root.item && root.item.installable === false
                        ? root.hex("tertiary", "#d8b26a") : root.hex("term2", "#8fd6a0")
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 3
                    visible: root.item && root.item.stars > 0

                    AppIcon {
                        iconSize: 10
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        source: "qrc:/data/icons/star.svg"
                        color: "#f3e7c8"
                    }

                    Text {
                        text: root.item && root.item.stars !== undefined ? root.item.stars : 0
                        color: "#f3e7c8"
                        font.pixelSize: 10
                        font.weight: Font.Medium
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 54
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: "#00000000" }
                    GradientStop { position: 1.0; color: "#99000000" }
                }
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                text: root.item ? root.item.name : ""
                color: "#ffffff"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            anchors.top: preview.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            spacing: 7

            Text {
                Layout.fillWidth: true
                text: root.item ? (root.item.description || "") : ""
                color: root.hex("onSurfaceVariant", "#aaa0b3")
                font.pixelSize: 11
                lineHeight: 1.25
                maximumLineCount: 2
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Repeater {
                    model: ["primary", "secondary", "tertiary", "surfaceContainerHigh", "error"]

                    Rectangle {
                        visible: root.hasColors
                        Layout.preferredWidth: 13
                        Layout.preferredHeight: 13
                        radius: 6
                        color: root.hex(modelData, "#313039")
                        border.width: 1
                        border.color: "#33000000"
                    }
                }

                Text {
                    visible: !root.hasColors
                    text: "No palette yet"
                    color: root.hex("outline", "#8b7f96")
                    font.pixelSize: 9
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: root.item && root.item.needsInstall ? "Download" : (root.item && root.item.installable === false ? "View" : "Apply")
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

#include "shellbuilder.h"

#include "palette.h"
#include "paths.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QPair>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QVector>
#include <utility>

namespace {

struct Config {
    QString name = QStringLiteral("hyperthemes");
    QString position = QStringLiteral("top");
    int barHeight = 34;
    int margin = 6;
    int radius = 10;
    qreal opacity = 0.94;
    int gap = 10;
    bool barPerMonitor = false;
    bool launcher = true;
    bool dashboard = true;
    bool clock24h = true;
    bool showSeconds = false;
    bool borders = true;
    bool shadow = true;
    QString style = QStringLiteral("floating");
    int animationDuration = 120;
    QStringList monitors;
    QStringList bottomMonitors;
    QString fontFamily;
    int fontSize = 12;
    int iconSize = 16;
    int workspaceCount = 5;
    QString compositor = QStringLiteral("hyprland");
    QStringList left;
    QStringList center;
    QStringList right;
    QVariantMap palette;
    QString themeName = QStringLiteral("HyperThemes");
    QString themeId;
    QString themeMode;
};

QString qmlString(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString qmlStringList(const QStringList &values)
{
    QStringList quoted;
    for (const QString &value : values)
        quoted << qmlString(value);
    return QLatin1Char('[') + quoted.join(QStringLiteral(", ")) + QLatin1Char(']');
}

QString pad(const QString &text, int spaces)
{
    const QString prefix(spaces, QLatin1Char(' '));
    QString body = text;
    while (body.endsWith(QLatin1Char('\n')))
        body.chop(1);

    QString out;
    const QStringList lines = body.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (!line.isEmpty())
            out += prefix;
        out += line;
        out += QLatin1Char('\n');
    }
    return out;
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString colour(const QVariantMap &palette, const QString &role)
{
    return Palette::withHash(Palette::hex(palette, role));
}

QString clockFormat(const Config &config)
{
    if (config.clock24h)
        return config.showSeconds ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm");
    return config.showSeconds ? QStringLiteral("h:mm:ss AP") : QStringLiteral("h:mm AP");
}

bool usesWidget(const Config &config, const QString &id)
{
    return config.left.contains(id) || config.center.contains(id) || config.right.contains(id);
}

QString widgetQml(const QString &id, const Config &config)
{
    if (id == QLatin1String("logo")) {
        return QStringLiteral(R"QML(
Rectangle {
    implicitWidth: theme.iconSize + 12
    implicitHeight: theme.iconSize + 12
    radius: theme.radius
    color: logoHover.hovered ? theme.primaryContainer : theme.primary

    Text {
        anchors.centerIn: parent
        text: "◆"
        color: theme.primaryText
        font.family: theme.fontFamily
        font.pixelSize: Math.round(theme.iconSize * 0.7)
    }

    HoverHandler { id: logoHover }
    TapHandler { onTapped: bar.launcherRequested() }
    ToolTip.visible: logoHover.hovered
    ToolTip.text: "Open the launcher"
}
)QML");
    }
    if (id == QLatin1String("workspaces")) {
        if (config.compositor == QLatin1String("hyprland")) {
            return QStringLiteral(R"QML(
RowLayout {
    spacing: 6

    Repeater {
        model: Hyprland.workspaces.values.filter(function(workspace) { return workspace.id > 0; })

        Rectangle {
            required property var modelData
            readonly property bool active: Hyprland.focusedWorkspace !== null
                && modelData.id === Hyprland.focusedWorkspace.id

            implicitWidth: active ? 22 : 9
            implicitHeight: 9
            radius: 4
            color: active ? theme.primary : (modelData.urgent ? theme.error : theme.surfaceHigh)
            Behavior on implicitWidth { NumberAnimation { duration: %1 } }
            TapHandler { onTapped: Hyprland.dispatch("workspace " + modelData.id) }
        }
    }
}
)QML").arg(config.animationDuration);
        }
        if (config.compositor == QLatin1String("sway")) {
            return QStringLiteral(R"QML(
RowLayout {
    spacing: 6

    Repeater {
        model: I3.workspaces.values

        Rectangle {
            required property var modelData
            implicitWidth: modelData.focused ? 22 : 9
            implicitHeight: 9
            radius: 4
            color: modelData.focused ? theme.primary : (modelData.urgent ? theme.error : theme.surfaceHigh)
            Behavior on implicitWidth { NumberAnimation { duration: %1 } }
            TapHandler { onTapped: modelData.activate() }
        }
    }
}
)QML").arg(config.animationDuration);
        }
        return QStringLiteral(R"QML(
RowLayout {
    spacing: 6

    Repeater {
        model: theme.workspaceCount

        Rectangle {
            implicitWidth: index === 0 ? 22 : 9
            implicitHeight: 9
            radius: 4
            color: index === 0 ? theme.primary : theme.surfaceHigh
        }
    }
}
)QML");
    }
    if (id == QLatin1String("separator")) {
        return QStringLiteral(R"QML(
Rectangle {
    implicitWidth: 1
    implicitHeight: theme.iconSize
    color: theme.outline
}
)QML");
    }
    if (id == QLatin1String("spacer")) {
        return QStringLiteral(R"QML(
Item {
    implicitWidth: 14
    implicitHeight: 1
}
)QML");
    }
    if (id == QLatin1String("systemLoad")) {
        return QStringLiteral(R"QML(
RowLayout {
    spacing: 4

    Text {
        id: loadLabel
        text: "load …"
        color: theme.muted
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 1
    }

    Process {
        id: loadProcess
        command: ["sh", "-c", "cut -d' ' -f1 /proc/loadavg"]
        running: true
        stdout: SplitParser {
            onRead: function(data) { loadLabel.text = "load " + String(data).trim(); }
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: true
        onTriggered: loadProcess.running = true
    }
}
)QML");
    }
    if (id == QLatin1String("uptime")) {
        return QStringLiteral(R"QML(
RowLayout {
    spacing: 4

    Text {
        id: uptimeLabel
        text: "up …"
        color: theme.muted
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 1
    }

    Process {
        id: uptimeProcess
        command: ["sh", "-c", "cut -d. -f1 /proc/uptime"]
        running: true
        stdout: SplitParser {
            onRead: function(data) {
                var seconds = parseInt(String(data).trim(), 10);
                if (isNaN(seconds))
                    return;
                var hours = Math.floor(seconds / 3600);
                var minutes = Math.floor((seconds % 3600) / 60);
                uptimeLabel.text = "up " + hours + "h " + minutes + "m";
            }
        }
    }

    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: uptimeProcess.running = true
    }
}
)QML");
    }
    if (id == QLatin1String("notifications")) {
        return QStringLiteral(R"QML(
RowLayout {
    spacing: 4
    visible: bar.notificationCount > 0

    Text {
        text: "🔔"
        color: theme.muted
        font.pixelSize: theme.fontSize - 1
    }

    Text {
        text: bar.notificationCount
        color: theme.text
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 1
    }

    HoverHandler {
        id: notificationHover
        cursorShape: Qt.PointingHandCursor
    }
    ToolTip.visible: notificationHover.hovered
    ToolTip.text: bar.notificationSummary()

    TapHandler { onTapped: bar.notificationsRequested() }
}
)QML");
    }
    if (id == QLatin1String("memory")) {
        return QStringLiteral(R"QML(
RowLayout {
    spacing: 4

    Text {
        id: memoryLabel
        text: "mem …"
        color: theme.muted
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 1
    }

    Process {
        id: memoryProcess
        command: ["sh", "-c",
                  "awk '/MemTotal/{t=$2} /MemAvailable/{a=$2} END{print int((t-a)/1024)}" +
                  " /proc/meminfo"]
        running: true
        stdout: SplitParser {
            onRead: function(data) {
                var used = parseInt(String(data).trim(), 10);
                if (!isNaN(used))
                    memoryLabel.text = "mem " + used + " MB";
            }
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: true
        onTriggered: memoryProcess.running = true
    }
}
)QML");
    }
    if (id == QLatin1String("launcher")) {
        return QStringLiteral(R"QML(
Rectangle {
    implicitWidth: theme.iconSize + 52
    implicitHeight: theme.iconSize + 12
    radius: theme.radius
    color: launcherHover.hovered ? theme.surfaceHigh : theme.surface

    Text {
        anchors.centerIn: parent
        text: "Search"
        color: theme.muted
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 1
    }

    HoverHandler { id: launcherHover }
    TapHandler { onTapped: bar.launcherRequested() }
    ToolTip.visible: launcherHover.hovered
    ToolTip.text: "Open the application launcher"
}
)QML");
    }
    if (id == QLatin1String("clock")) {
        return QStringLiteral(R"QML(
Text {
    id: clockLabel
    property var now: new Date()

    text: Qt.formatDateTime(now, theme.clockFormat)
    color: theme.text
    font.family: theme.fontFamily
    font.pixelSize: theme.fontSize
    font.weight: Font.DemiBold
    verticalAlignment: Text.AlignVCenter

    HoverHandler { id: clockHover }
    TapHandler { onTapped: bar.dashboardRequested() }
    ToolTip.visible: clockHover.hovered
    ToolTip.text: Qt.formatDateTime(now, "dddd, d MMMM yyyy")

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: clockLabel.now = new Date()
    }
}
)QML");
    }
    if (id == QLatin1String("date")) {
        return QStringLiteral(R"QML(
Text {
    id: dateLabel
    property var now: new Date()

    text: Qt.formatDateTime(now, theme.dateFormat)
    color: theme.muted
    font.family: theme.fontFamily
    font.pixelSize: theme.fontSize - 1
    verticalAlignment: Text.AlignVCenter

    HoverHandler { id: dateHover }
    TapHandler { onTapped: bar.dashboardRequested() }
    ToolTip.visible: dateHover.hovered
    ToolTip.text: "Open the dashboard"

    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: dateLabel.now = new Date()
    }
}
)QML");
    }
    if (id == QLatin1String("media")) {
        return QStringLiteral(R"QML(
RowLayout {
    id: mediaWidget

    readonly property var player: Mpris.players.values.length > 0 ? Mpris.players.values[0] : null

    spacing: 6
    visible: mediaWidget.player !== null

    Text {
        Layout.maximumWidth: 160
        text: mediaWidget.player && mediaWidget.player.trackTitle ? mediaWidget.player.trackTitle : ""
        color: theme.muted
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 1
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        text: mediaWidget.player && mediaWidget.player.isPlaying ? "⏸" : "▶"
        color: mediaHover.hovered ? theme.text : theme.muted
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 1
        verticalAlignment: Text.AlignVCenter
        TapHandler {
            onTapped: {
                if (mediaWidget.player)
                    mediaWidget.player.togglePlaying();
            }
        }
    }

    HoverHandler { id: mediaHover }
    ToolTip.visible: mediaHover.hovered
    ToolTip.text: mediaWidget.player
        ? (mediaWidget.player.trackArtist ? mediaWidget.player.trackArtist + " · " : "") + (mediaWidget.player.trackTitle || "")
        : ""
}
)QML");
    }
    if (id == QLatin1String("volume")) {
        return QStringLiteral(R"QML(
RowLayout {
    id: volumeWidget

    readonly property var sink: Pipewire.defaultAudioSink
    readonly property real level: volumeWidget.sink && volumeWidget.sink.audio
        ? Math.max(0, Math.min(1, volumeWidget.sink.audio.volume)) : 0
    readonly property bool muted: volumeWidget.sink && volumeWidget.sink.audio
        ? volumeWidget.sink.audio.muted : true

    spacing: 6

    Text {
        text: volumeWidget.muted ? "🔇" : "🔊"
        color: volumeHover.hovered ? theme.text : theme.muted
        font.pixelSize: theme.fontSize - 1
        verticalAlignment: Text.AlignVCenter
    }

    Rectangle {
        Layout.preferredWidth: 42
        Layout.preferredHeight: 4
        radius: 2
        color: theme.surfaceHigh

        Rectangle {
            width: parent.width * volumeWidget.level
            height: parent.height
            radius: 2
            color: volumeWidget.muted ? theme.faint : theme.primary
        }
    }

    Text {
        text: Math.round(volumeWidget.level * 100) + "%"
        color: theme.faint
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 2
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler { id: volumeHover }
    TapHandler {
        onTapped: {
            if (volumeWidget.sink && volumeWidget.sink.audio)
                volumeWidget.sink.audio.muted = !volumeWidget.sink.audio.muted;
        }
    }
    ToolTip.visible: volumeHover.hovered
    ToolTip.text: "Volume · click to mute"
}
)QML");
    }
    if (id == QLatin1String("network")) {
        return QStringLiteral(R"QML(
RowLayout {
    id: networkWidget

    readonly property var device: Networking.devices.values.find(function(candidate) {
        return candidate.connected;
    }) || null

    spacing: 6

    Text {
        text: networkWidget.device === null
            ? "⊘"
            : (networkWidget.device.type === DeviceType.Wifi ? "⌁" : "⇄")
        color: networkWidget.device === null
            ? theme.error
            : (networkHover.hovered ? theme.text : theme.muted)
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler { id: networkHover }
    ToolTip.visible: networkHover.hovered
    ToolTip.text: networkWidget.device === null ? "No network connection" : networkWidget.device.name
}
)QML");
    }
    if (id == QLatin1String("bluetooth")) {
        return QStringLiteral(R"QML(
RowLayout {
    id: bluetoothWidget

    readonly property var adapter: Bluetooth.defaultAdapter
    readonly property int connectedDevices: bluetoothWidget.adapter
        ? bluetoothWidget.adapter.devices.values.filter(function(device) { return device.connected; }).length
        : 0

    spacing: 6
    visible: bluetoothWidget.adapter !== null

    Text {
        text: "ᛒ"
        color: bluetoothWidget.adapter && bluetoothWidget.adapter.enabled ? theme.primary : theme.faint
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize
        opacity: bluetoothWidget.adapter && bluetoothWidget.adapter.enabled ? 1.0 : 0.6
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        visible: bluetoothWidget.connectedDevices > 0
        text: bluetoothWidget.connectedDevices
        color: theme.muted
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 2
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler { id: bluetoothHover }
    ToolTip.visible: bluetoothHover.hovered
    ToolTip.text: !bluetoothWidget.adapter ? "No Bluetooth adapter"
        : (bluetoothWidget.adapter.enabled
            ? bluetoothWidget.connectedDevices + " connected device(s)"
            : "Bluetooth is off")
}
)QML");
    }
    if (id == QLatin1String("tray")) {
        return QStringLiteral(R"QML(
RowLayout {
    spacing: 6
    visible: SystemTray.items.values.length > 0

    Repeater {
        model: SystemTray.items.values

        Image {
            required property var modelData

            source: Quickshell.iconPath(modelData.icon)
            sourceSize.width: theme.iconSize
            sourceSize.height: theme.iconSize
            Layout.preferredWidth: theme.iconSize
            Layout.preferredHeight: theme.iconSize
            opacity: trayHover.hovered ? 1.0 : 0.85
            fillMode: Image.PreserveAspectFit

            HoverHandler { id: trayHover }
            MouseArea {
                anchors.fill: parent
                onClicked: modelData.activate()
            }
            ToolTip.visible: trayHover.hovered
            ToolTip.text: modelData.title || ""
        }
    }
}
)QML");
    }
    if (id == QLatin1String("battery")) {
        return QStringLiteral(R"QML(
RowLayout {
    id: batteryWidget

    readonly property var device: UPower.displayDevice
    readonly property real level: batteryWidget.device
        ? Math.max(0, Math.min(1, batteryWidget.device.percentage)) : 0
    readonly property bool low: batteryWidget.level <= 0.15
        && batteryWidget.device && !batteryWidget.device.charging

    spacing: 6
    visible: batteryWidget.device !== null && batteryWidget.device.isLaptopBattery

    Text {
        text: Math.round(batteryWidget.level * 100) + "%"
        color: batteryWidget.low ? theme.error : theme.muted
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize - 1
        verticalAlignment: Text.AlignVCenter
    }

    Rectangle {
        Layout.preferredWidth: 26
        Layout.preferredHeight: 8
        radius: 4
        color: theme.surfaceHigh

        Rectangle {
            width: parent.width * batteryWidget.level
            height: parent.height
            radius: 4
            color: batteryWidget.low ? theme.error : theme.primary
        }
    }

    HoverHandler { id: batteryHover }
    ToolTip.visible: batteryHover.hovered
    ToolTip.text: batteryWidget.device
        ? (batteryWidget.device.charging ? "Charging · " : "") + Math.round(batteryWidget.level * 100) + "%"
        : ""
}
)QML");
    }
    if (id == QLatin1String("statusIcons")) {
        return QStringLiteral(R"QML(
RowLayout {
    spacing: 8

    Text {
        text: Networking.connectivity === NetworkConnectivity.Full ? "◍" : "◌"
        color: Networking.connectivity === NetworkConnectivity.Full ? theme.muted : theme.error
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize
    }

    Text {
        text: Bluetooth.defaultAdapter && Bluetooth.defaultAdapter.enabled ? "ᛒ" : "ᛒ"
        color: Bluetooth.defaultAdapter && Bluetooth.defaultAdapter.enabled ? theme.muted : theme.faint
        opacity: Bluetooth.defaultAdapter && Bluetooth.defaultAdapter.enabled ? 1.0 : 0.45
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize
    }

    HoverHandler { id: statusHover }
    ToolTip.visible: statusHover.hovered
    ToolTip.text: "Network and Bluetooth status"
}
)QML");
    }
    if (id == QLatin1String("power")) {
        return QStringLiteral(R"QML(
Rectangle {
    implicitWidth: theme.iconSize + 12
    implicitHeight: theme.iconSize + 12
    radius: theme.radius
    color: powerHover.hovered ? theme.surfaceHigh : "transparent"

    Text {
        anchors.centerIn: parent
        text: "⏻"
        color: theme.text
        font.family: theme.fontFamily
        font.pixelSize: theme.fontSize
    }

    HoverHandler { id: powerHover }
    TapHandler { onTapped: bar.dashboardRequested() }
    ToolTip.visible: powerHover.hovered
    ToolTip.text: "Session actions"
}
)QML");
    }
    return {};
}

QString zoneQml(const QStringList &ids, const Config &config)
{
    QString out;
    for (const QString &id : ids) {
        const QString snippet = widgetQml(id, config);
        if (snippet.isEmpty())
            continue;
        if (!out.isEmpty())
            out += QLatin1Char('\n');
        out += snippet;
    }
    return out;
}

QString themeFile(const Config &config)
{
    const QVariantMap palette = Palette::normalise(config.palette);
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("// Palette source: ") + config.themeName + QLatin1Char('\n');
    text += QStringLiteral("// This is the only file you need to edit to retune the generated shell.\n");
    text += QStringLiteral("import QtQuick\n\n");
    text += QStringLiteral("QtObject {\n");
    text += QStringLiteral("    readonly property string sourceName: ") + qmlString(config.themeName) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property string sourceMode: ") + qmlString(config.themeMode) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property string position: ") + qmlString(config.position) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property bool barPerMonitor: ") + boolText(config.barPerMonitor) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property string style: ") + qmlString(config.style) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property int animationDuration: %1\n").arg(config.animationDuration);
    text += QStringLiteral("    readonly property var monitors: ") + qmlStringList(config.monitors) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property var bottomMonitors: ") + qmlStringList(config.bottomMonitors) + QLatin1Char('\n');
    text += QStringLiteral("    function hasBar(name) { return monitors.length === 0 || monitors.indexOf(name) >= 0; }\n");
    text += QStringLiteral("    function barPosition(name) { return bottomMonitors.indexOf(name) >= 0 ? \"bottom\" : position; }\n");
    text += QStringLiteral("\n");
    text += QStringLiteral("    // Geometry\n");
    text += QStringLiteral("    readonly property int barHeight: %1\n").arg(config.barHeight);
    text += QStringLiteral("    readonly property int margin: %1\n").arg(config.margin);
    text += QStringLiteral("    readonly property int radius: %1\n").arg(config.radius);
    text += QStringLiteral("    readonly property int gap: %1\n").arg(config.gap);
    text += QStringLiteral("    readonly property real opacity: %1\n").arg(config.opacity, 0, 'f', 2);
    text += QStringLiteral("    readonly property int borderWidth: %1\n").arg(config.borders ? 1 : 0);
    text += QStringLiteral("    readonly property bool shadow: ") + boolText(config.shadow) + QLatin1Char('\n');
    text += QStringLiteral("\n");
    text += QStringLiteral("    // Type\n");
    if (config.fontFamily.isEmpty())
        text += QStringLiteral("    readonly property string fontFamily: Qt.application.font.family\n");
    else
        text += QStringLiteral("    readonly property string fontFamily: ") + qmlString(config.fontFamily) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property int fontSize: %1\n").arg(config.fontSize);
    text += QStringLiteral("    readonly property int iconSize: %1\n").arg(config.iconSize);
    text += QStringLiteral("    readonly property string clockFormat: ") + qmlString(clockFormat(config)) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property string dateFormat: \"ddd, d MMM\"\n");
    text += QStringLiteral("    readonly property string dashboardFormat: ") + qmlString(QStringLiteral("dddd, d MMMM · ") + clockFormat(config)) + QLatin1Char('\n');
    text += QStringLiteral("\n");
    text += QStringLiteral("    // Workspaces\n");
    text += QStringLiteral("    readonly property string workspaceCompositor: ") + qmlString(config.compositor) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property int workspaceCount: %1\n").arg(config.workspaceCount);
    text += QStringLiteral("\n");
    text += QStringLiteral("    // Colours\n");
    const QVector<QPair<QString, QString>> roles = {
        {QStringLiteral("background"), QStringLiteral("background")},
        {QStringLiteral("surface"), QStringLiteral("surfaceContainer")},
        {QStringLiteral("surfaceHigh"), QStringLiteral("surfaceContainerHigh")},
        {QStringLiteral("surfaceHighest"), QStringLiteral("surfaceContainerHighest")},
        {QStringLiteral("text"), QStringLiteral("onSurface")},
        {QStringLiteral("muted"), QStringLiteral("onSurfaceVariant")},
        {QStringLiteral("faint"), QStringLiteral("outline")},
        {QStringLiteral("outline"), QStringLiteral("outlineVariant")},
        {QStringLiteral("primary"), QStringLiteral("primary")},
        {QStringLiteral("primaryText"), QStringLiteral("onPrimary")},
        {QStringLiteral("primaryContainer"), QStringLiteral("primaryContainer")},
        {QStringLiteral("primaryContainerText"), QStringLiteral("onPrimaryContainer")},
        {QStringLiteral("secondary"), QStringLiteral("secondary")},
        {QStringLiteral("tertiary"), QStringLiteral("tertiary")},
        {QStringLiteral("error"), QStringLiteral("error")},
        {QStringLiteral("success"), QStringLiteral("term2")},
        {QStringLiteral("warning"), QStringLiteral("term3")},
    };
    for (const auto &role : roles) {
        text += QStringLiteral("    readonly property color %1: %2\n")
                    .arg(role.first, qmlString(colour(palette, role.second)));
    }
    text += QStringLiteral("    readonly property color shadowColor: \"#000000\"\n");
    text += QStringLiteral("}\n");
    return text;
}

QString barFile(const Config &config)
{
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("// One bar per monitor when Theme.barPerMonitor is true.\n");
    text += QStringLiteral("import QtQuick\n");
    text += QStringLiteral("import QtQuick.Controls\n");
    text += QStringLiteral("import QtQuick.Layouts\n");
    text += QStringLiteral("import Quickshell\n");
    text += QStringLiteral("import Quickshell.Wayland\n");
    if (usesWidget(config, QStringLiteral("tray")))
        text += QStringLiteral("import Quickshell.Services.SystemTray\n");
    if (usesWidget(config, QStringLiteral("media")))
        text += QStringLiteral("import Quickshell.Services.Mpris\n");
    if (usesWidget(config, QStringLiteral("battery")))
        text += QStringLiteral("import Quickshell.Services.UPower\n");
    if (usesWidget(config, QStringLiteral("volume")))
        text += QStringLiteral("import Quickshell.Services.Pipewire\n");
    if (usesWidget(config, QStringLiteral("network")) || usesWidget(config, QStringLiteral("statusIcons")))
        text += QStringLiteral("import Quickshell.Networking\n");
    if (usesWidget(config, QStringLiteral("bluetooth")) || usesWidget(config, QStringLiteral("statusIcons")))
        text += QStringLiteral("import Quickshell.Bluetooth\n");
    if (usesWidget(config, QStringLiteral("systemLoad")))
        text += QStringLiteral("import Quickshell.Io\n");
    if (usesWidget(config, QStringLiteral("notifications")))
        text += QStringLiteral("import Quickshell.Services.Notifications\n");
    if (usesWidget(config, QStringLiteral("workspaces")) && config.compositor == QLatin1String("hyprland"))
        text += QStringLiteral("import Quickshell.Hyprland\n");
    if (usesWidget(config, QStringLiteral("workspaces")) && config.compositor == QLatin1String("sway"))
        text += QStringLiteral("import Quickshell.I3\n");
    text += QStringLiteral("\n");
    text += QStringLiteral("Variants {\n");
    text += QStringLiteral("    id: bar\n\n");
    text += QStringLiteral("    required property var theme\n");
    text += QStringLiteral("    signal launcherRequested()\n");
    text += QStringLiteral("    signal dashboardRequested()\n");
    if (usesWidget(config, QStringLiteral("notifications")))
        text += QStringLiteral("    signal notificationsRequested()\n");
    if (usesWidget(config, QStringLiteral("notifications"))) {
        text += QStringLiteral("\n");
        text += QStringLiteral("    readonly property var notificationList: notificationServer.trackedNotifications\n");
        text += QStringLiteral("    readonly property int notificationCount: notificationList ? notificationList.values.length : 0\n");
        text += QStringLiteral("    function notificationSummary() {\n");
        text += QStringLiteral("        var list = notificationList ? notificationList.values : [];\n");
        text += QStringLiteral("        var lines = [];\n");
        text += QStringLiteral("        for (var i = 0; i < list.length && i < 4; ++i)\n");
        text += QStringLiteral("            lines.push(String(list[i].summary || list[i].body || list[i].appName || \"Notification\"));\n");
        text += QStringLiteral("        if (list.length > 4)\n");
        text += QStringLiteral("            lines.push(\"+\" + (list.length - 4) + \" more\");\n");
        text += QStringLiteral("        return lines.join(\"\\n\");\n");
        text += QStringLiteral("    }\n\n");
        text += QStringLiteral("    NotificationServer {\n");
        text += QStringLiteral("        id: notificationServer\n");
        text += QStringLiteral("        keepOnReload: true\n");
        text += QStringLiteral("        bodySupported: true\n");
        text += QStringLiteral("        imageSupported: true\n");
        text += QStringLiteral("        actionsSupported: true\n");
        text += QStringLiteral("    }\n");
    }
    text += QStringLiteral("\n");
    text += QStringLiteral("    model: theme.monitors.length > 0\n");
    text += QStringLiteral("        ? Quickshell.screens.filter(function(screen) { return theme.hasBar(screen.name); })\n");
    text += QStringLiteral("        : (theme.barPerMonitor\n");
    text += QStringLiteral("            ? Quickshell.screens\n");
    text += QStringLiteral("            : (Quickshell.screens.length > 0 ? [Quickshell.screens[0]] : []))\n\n");
    text += QStringLiteral("    delegate: PanelWindow {\n");
    text += QStringLiteral("        required property var modelData\n\n");
    text += QStringLiteral("        screen: modelData\n");
    text += QStringLiteral("        anchors {\n");
    text += QStringLiteral("            top: theme.barPosition(modelData.name) !== \"bottom\"\n");
    text += QStringLiteral("            bottom: theme.barPosition(modelData.name) === \"bottom\"\n");
    text += QStringLiteral("            left: true\n");
    text += QStringLiteral("            right: true\n");
    text += QStringLiteral("        }\n");
    text += QStringLiteral("        margins {\n");
    text += QStringLiteral("            top: theme.barPosition(modelData.name) === \"bottom\" ? 0 : theme.margin\n");
    text += QStringLiteral("            bottom: theme.barPosition(modelData.name) === \"bottom\" ? theme.margin : 0\n");
    text += QStringLiteral("            left: theme.margin\n");
    text += QStringLiteral("            right: theme.margin\n");
    text += QStringLiteral("        }\n");
    text += QStringLiteral("        implicitHeight: theme.barHeight\n");
    text += QStringLiteral("        exclusiveZone: theme.barHeight + theme.margin\n");
    text += QStringLiteral("        color: \"transparent\"\n");
    text += QStringLiteral("        WlrLayershell.namespace: \"hyperthemes-bar\"\n");
    text += QStringLiteral("        WlrLayershell.layer: WlrLayer.Top\n\n");
    text += QStringLiteral("        Rectangle {\n");
    text += QStringLiteral("            visible: theme.shadow\n");
    text += QStringLiteral("            anchors.fill: surface\n");
    text += QStringLiteral("            anchors.topMargin: 4\n");
    text += QStringLiteral("            anchors.bottomMargin: -4\n");
    text += QStringLiteral("            radius: theme.radius\n");
    text += QStringLiteral("            color: theme.shadowColor\n");
    text += QStringLiteral("            opacity: 0.28\n");
    text += QStringLiteral("        }\n\n");
    text += QStringLiteral("        Rectangle {\n");
    text += QStringLiteral("            id: surface\n");
    text += QStringLiteral("            anchors.fill: parent\n");
    text += QStringLiteral("            radius: theme.radius\n");
    text += QStringLiteral("            color: Qt.rgba(theme.background.r, theme.background.g, theme.background.b, theme.opacity)\n");
    text += QStringLiteral("            border.width: theme.borderWidth\n");
    text += QStringLiteral("            border.color: theme.outline\n\n");
    if (!config.left.isEmpty()) {
        text += QStringLiteral("            RowLayout {\n");
        text += QStringLiteral("                anchors.left: parent.left\n");
        text += QStringLiteral("                anchors.leftMargin: 12\n");
        text += QStringLiteral("                anchors.verticalCenter: parent.verticalCenter\n");
        text += QStringLiteral("                spacing: theme.gap\n\n");
        text += pad(zoneQml(config.left, config), 16);
        text += QStringLiteral("            }\n\n");
    }
    if (!config.center.isEmpty()) {
        text += QStringLiteral("            RowLayout {\n");
        text += QStringLiteral("                anchors.horizontalCenter: parent.horizontalCenter\n");
        text += QStringLiteral("                anchors.verticalCenter: parent.verticalCenter\n");
        text += QStringLiteral("                spacing: theme.gap\n\n");
        text += pad(zoneQml(config.center, config), 16);
        text += QStringLiteral("            }\n\n");
    }
    if (!config.right.isEmpty()) {
        text += QStringLiteral("            RowLayout {\n");
        text += QStringLiteral("                anchors.right: parent.right\n");
        text += QStringLiteral("                anchors.rightMargin: 12\n");
        text += QStringLiteral("                anchors.verticalCenter: parent.verticalCenter\n");
        text += QStringLiteral("                spacing: theme.gap\n\n");
        text += pad(zoneQml(config.right, config), 16);
        text += QStringLiteral("            }\n");
    }
    text += QStringLiteral("        }\n");
    text += QStringLiteral("    }\n");
    text += QStringLiteral("}\n");
    return text;
}

QString launcherFile(const Config &config)
{
    Q_UNUSED(config);
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("// Toggle it with: qs ipc call launcher toggle\n");
    text += QStringLiteral("import QtQuick\n");
    text += QStringLiteral("import QtQuick.Controls\n");
    text += QStringLiteral("import QtQuick.Layouts\n");
    text += QStringLiteral("import Quickshell\n");
    text += QStringLiteral("import Quickshell.Wayland\n");
    text += QStringLiteral("import Quickshell.Io\n\n");
    text += QStringLiteral("PanelWindow {\n");
    text += QStringLiteral("    id: launcher\n\n");
    text += QStringLiteral("    required property var theme\n");
    text += QStringLiteral("    visible: false\n");
    text += QStringLiteral("    color: \"transparent\"\n");
    text += QStringLiteral("    exclusionMode: ExclusionMode.Ignore\n");
    text += QStringLiteral("    WlrLayershell.namespace: \"hyperthemes-launcher\"\n");
    text += QStringLiteral("    WlrLayershell.layer: WlrLayer.Overlay\n");
    text += QStringLiteral("    WlrLayershell.keyboardFocus: WlrKeyboardFocus.OnDemand\n");
    text += QStringLiteral("    focusable: true\n");
    text += QStringLiteral("    anchors {\n");
    text += QStringLiteral("        top: true\n        bottom: true\n        left: true\n        right: true\n    }\n\n");
    text += QStringLiteral("    function toggle() {\n");
    text += QStringLiteral("        visible = !visible;\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    function launch(entry) {\n");
    text += QStringLiteral("        if (!entry)\n");
    text += QStringLiteral("            return;\n");
    text += QStringLiteral("        entry.execute();\n");
    text += QStringLiteral("        visible = false;\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    function launchCurrent() {\n");
    text += QStringLiteral("        var entries = results.model;\n");
    text += QStringLiteral("        if (entries && results.currentIndex >= 0 && results.currentIndex < entries.length)\n");
    text += QStringLiteral("            launch(entries[results.currentIndex]);\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    onVisibleChanged: {\n");
    text += QStringLiteral("        if (visible) {\n");
    text += QStringLiteral("            search.text = \"\";\n");
    text += QStringLiteral("            results.currentIndex = 0;\n");
    text += QStringLiteral("            search.forceActiveFocus();\n");
    text += QStringLiteral("        }\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    IpcHandler {\n");
    text += QStringLiteral("        target: \"launcher\"\n");
    text += QStringLiteral("        function toggle() { launcher.visible = !launcher.visible; }\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    Rectangle {\n");
    text += QStringLiteral("        anchors.fill: parent\n");
    text += QStringLiteral("        color: theme.background\n");
    text += QStringLiteral("        opacity: 0.4\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    MouseArea {\n");
    text += QStringLiteral("        anchors.fill: parent\n");
    text += QStringLiteral("        onClicked: launcher.visible = false\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    Rectangle {\n");
    text += QStringLiteral("        id: panel\n");
    text += QStringLiteral("        anchors.horizontalCenter: parent.horizontalCenter\n");
    text += QStringLiteral("        y: Math.round(parent.height * 0.16)\n");
    text += QStringLiteral("        width: Math.min(parent.width - 64, 580)\n");
    text += QStringLiteral("        height: Math.min(parent.height - 160, 460)\n");
    text += QStringLiteral("        radius: theme.radius * 2\n");
    text += QStringLiteral("        color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, Math.max(0.94, theme.opacity))\n");
    text += QStringLiteral("        border.width: 1\n");
    text += QStringLiteral("        border.color: theme.surfaceHigh\n\n");
    text += QStringLiteral("        MouseArea { anchors.fill: parent }\n\n");
    text += QStringLiteral("        ColumnLayout {\n");
    text += QStringLiteral("            anchors.fill: parent\n");
    text += QStringLiteral("            anchors.margins: 16\n");
    text += QStringLiteral("            spacing: 10\n\n");
    text += QStringLiteral("            TextField {\n");
    text += QStringLiteral("                id: search\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                placeholderText: \"Search applications…\"\n");
    text += QStringLiteral("                color: theme.text\n");
    text += QStringLiteral("                placeholderTextColor: theme.faint\n");
    text += QStringLiteral("                font.family: theme.fontFamily\n");
    text += QStringLiteral("                font.pixelSize: theme.fontSize\n");
    text += QStringLiteral("                selectByMouse: true\n");
    text += QStringLiteral("                background: Rectangle {\n");
    text += QStringLiteral("                    color: theme.surfaceHigh\n");
    text += QStringLiteral("                    radius: theme.radius\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("                onTextChanged: results.currentIndex = 0\n");
    text += QStringLiteral("                Keys.onDownPressed: results.incrementCurrentIndex()\n");
    text += QStringLiteral("                Keys.onUpPressed: results.decrementCurrentIndex()\n");
    text += QStringLiteral("                Keys.onReturnPressed: launcher.launchCurrent()\n");
    text += QStringLiteral("                Keys.onEnterPressed: launcher.launchCurrent()\n");
    text += QStringLiteral("                Keys.onEscapePressed: launcher.visible = false\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            ListView {\n");
    text += QStringLiteral("                id: results\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                Layout.fillHeight: true\n");
    text += QStringLiteral("                clip: true\n");
    text += QStringLiteral("                spacing: 2\n");
    text += QStringLiteral("                onCountChanged: {\n");
    text += QStringLiteral("                    if (currentIndex >= count)\n");
    text += QStringLiteral("                        currentIndex = Math.max(0, count - 1);\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("                model: DesktopEntries.applications.values.filter(function(app) {\n");
    text += QStringLiteral("                    if (app.noDisplay)\n");
    text += QStringLiteral("                        return false;\n");
    text += QStringLiteral("                    var query = search.text.toLowerCase();\n");
    text += QStringLiteral("                    return app.name.toLowerCase().includes(query)\n");
    text += QStringLiteral("                        || (app.genericName && app.genericName.toLowerCase().includes(query));\n");
    text += QStringLiteral("                })\n");
    text += QStringLiteral("                delegate: Rectangle {\n");
    text += QStringLiteral("                    required property var modelData\n");
    text += QStringLiteral("                    required property int index\n");
    text += QStringLiteral("                    width: results.width\n");
    text += QStringLiteral("                    height: 36\n");
    text += QStringLiteral("                    radius: theme.radius\n");
    text += QStringLiteral("                    color: results.currentIndex === index || itemHover.hovered\n");
    text += QStringLiteral("                        ? theme.surfaceHigh : \"transparent\"\n");
    text += QStringLiteral("                    RowLayout {\n");
    text += QStringLiteral("                        anchors.fill: parent\n");
    text += QStringLiteral("                        anchors.margins: 6\n");
    text += QStringLiteral("                        spacing: 8\n");
    text += QStringLiteral("                        Image {\n");
    text += QStringLiteral("                            source: Quickshell.iconPath(modelData.icon)\n");
    text += QStringLiteral("                            sourceSize.width: 18\n");
    text += QStringLiteral("                            sourceSize.height: 18\n");
    text += QStringLiteral("                            Layout.preferredWidth: 18\n");
    text += QStringLiteral("                            Layout.preferredHeight: 18\n");
    text += QStringLiteral("                            fillMode: Image.PreserveAspectFit\n");
    text += QStringLiteral("                        }\n");
    text += QStringLiteral("                        Text {\n");
    text += QStringLiteral("                            Layout.fillWidth: true\n");
    text += QStringLiteral("                            text: modelData.name\n");
    text += QStringLiteral("                            color: theme.text\n");
    text += QStringLiteral("                            font.family: theme.fontFamily\n");
    text += QStringLiteral("                            font.pixelSize: theme.fontSize\n");
    text += QStringLiteral("                            elide: Text.ElideRight\n");
    text += QStringLiteral("                            verticalAlignment: Text.AlignVCenter\n");
    text += QStringLiteral("                        }\n");
    text += QStringLiteral("                        Text {\n");
    text += QStringLiteral("                            text: modelData.genericName || \"\"\n");
    text += QStringLiteral("                            color: theme.faint\n");
    text += QStringLiteral("                            font.family: theme.fontFamily\n");
    text += QStringLiteral("                            font.pixelSize: theme.fontSize - 2\n");
    text += QStringLiteral("                            elide: Text.ElideRight\n");
    text += QStringLiteral("                            verticalAlignment: Text.AlignVCenter\n");
    text += QStringLiteral("                        }\n");
    text += QStringLiteral("                    }\n");
    text += QStringLiteral("                    HoverHandler { id: itemHover }\n");
    text += QStringLiteral("                    MouseArea {\n");
    text += QStringLiteral("                        anchors.fill: parent\n");
    text += QStringLiteral("                        onClicked: launcher.launch(modelData)\n");
    text += QStringLiteral("                    }\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            Text {\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                visible: results.count === 0\n");
    text += QStringLiteral("                text: \"No applications match “\" + search.text + \"”.\"\n");
    text += QStringLiteral("                color: theme.muted\n");
    text += QStringLiteral("                font.family: theme.fontFamily\n");
    text += QStringLiteral("                font.pixelSize: theme.fontSize - 1\n");
    text += QStringLiteral("                horizontalAlignment: Text.AlignHCenter\n");
    text += QStringLiteral("            }\n");
    text += QStringLiteral("        }\n");
    text += QStringLiteral("    }\n");
    text += QStringLiteral("}\n");
    return text;
}

QString powerButton(const QString &label, const QString &command, const QString &key)
{
    return QStringLiteral(
               "Rectangle {\n"
               "    implicitWidth: %1Label.implicitWidth + 26\n"
               "    implicitHeight: 30\n"
               "    radius: theme.radius\n"
               "    color: %1Hover.hovered ? theme.surfaceHigh : theme.surface\n"
               "    Text {\n"
               "        id: %1Label\n"
               "        anchors.centerIn: parent\n"
               "        text: %2\n"
               "        color: theme.text\n"
               "        font.family: theme.fontFamily\n"
               "        font.pixelSize: theme.fontSize - 1\n"
               "    }\n"
               "    HoverHandler { id: %1Hover }\n"
               "    TapHandler {\n"
               "        onTapped: Quickshell.execDetached(%3)\n"
               "    }\n"
               "    ToolTip.visible: %1Hover.hovered\n"
               "    ToolTip.text: %2\n"
               "}\n")
        .arg(key, qmlString(label), command);
}

QString dashboardFile(const Config &config)
{
    Q_UNUSED(config);
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("// Toggle it with: qs ipc call dashboard toggle\n");
    text += QStringLiteral("import QtQuick\n");
    text += QStringLiteral("import QtQuick.Controls\n");
    text += QStringLiteral("import QtQuick.Layouts\n");
    text += QStringLiteral("import Quickshell\n");
    text += QStringLiteral("import Quickshell.Wayland\n");
    text += QStringLiteral("import Quickshell.Io\n");
    text += QStringLiteral("import Quickshell.Services.Mpris\n");
    text += QStringLiteral("import Quickshell.Services.UPower\n\n");
    text += QStringLiteral("PanelWindow {\n");
    text += QStringLiteral("    id: dashboard\n\n");
    text += QStringLiteral("    required property var theme\n");
    text += QStringLiteral("    readonly property var player: Mpris.players.values.length > 0 ? Mpris.players.values[0] : null\n\n");
    text += QStringLiteral("    visible: false\n");
    text += QStringLiteral("    color: \"transparent\"\n");
    text += QStringLiteral("    exclusionMode: ExclusionMode.Ignore\n");
    text += QStringLiteral("    WlrLayershell.namespace: \"hyperthemes-dashboard\"\n");
    text += QStringLiteral("    WlrLayershell.layer: WlrLayer.Overlay\n");
    text += QStringLiteral("    WlrLayershell.keyboardFocus: WlrKeyboardFocus.OnDemand\n");
    text += QStringLiteral("    focusable: true\n");
    text += QStringLiteral("    anchors {\n");
    text += QStringLiteral("        top: true\n        bottom: true\n        left: true\n        right: true\n    }\n\n");
    text += QStringLiteral("    function toggle() {\n");
    text += QStringLiteral("        visible = !visible;\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    IpcHandler {\n");
    text += QStringLiteral("        target: \"dashboard\"\n");
    text += QStringLiteral("        function toggle() { dashboard.visible = !dashboard.visible; }\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    Shortcut {\n");
    text += QStringLiteral("        sequence: \"Escape\"\n");
    text += QStringLiteral("        onActivated: dashboard.visible = false\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    Rectangle {\n");
    text += QStringLiteral("        anchors.fill: parent\n");
    text += QStringLiteral("        color: theme.background\n");
    text += QStringLiteral("        opacity: 0.42\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    MouseArea {\n");
    text += QStringLiteral("        anchors.fill: parent\n");
    text += QStringLiteral("        onClicked: dashboard.visible = false\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    Rectangle {\n");
    text += QStringLiteral("        anchors.centerIn: parent\n");
    text += QStringLiteral("        width: Math.min(parent.width - 64, 580)\n");
    text += QStringLiteral("        height: Math.min(parent.height - 140, 380)\n");
    text += QStringLiteral("        radius: theme.radius * 2\n");
    text += QStringLiteral("        color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, Math.max(0.94, theme.opacity))\n");
    text += QStringLiteral("        border.width: 1\n");
    text += QStringLiteral("        border.color: theme.surfaceHigh\n\n");
    text += QStringLiteral("        MouseArea { anchors.fill: parent }\n\n");
    text += QStringLiteral("        ColumnLayout {\n");
    text += QStringLiteral("            anchors.fill: parent\n");
    text += QStringLiteral("            anchors.margins: 22\n");
    text += QStringLiteral("            spacing: 12\n\n");
    text += QStringLiteral("            Text {\n");
    text += QStringLiteral("                id: dashboardClock\n");
    text += QStringLiteral("                property var now: new Date()\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                text: Qt.formatDateTime(now, theme.dashboardFormat)\n");
    text += QStringLiteral("                color: theme.text\n");
    text += QStringLiteral("                font.family: theme.fontFamily\n");
    text += QStringLiteral("                font.pixelSize: 24\n");
    text += QStringLiteral("                font.weight: Font.DemiBold\n");
    text += QStringLiteral("                elide: Text.ElideRight\n");
    text += QStringLiteral("                Timer {\n");
    text += QStringLiteral("                    interval: 1000\n");
    text += QStringLiteral("                    running: true\n");
    text += QStringLiteral("                    repeat: true\n");
    text += QStringLiteral("                    onTriggered: dashboardClock.now = new Date()\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            RowLayout {\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                spacing: 10\n\n");
    text += QStringLiteral("                Text {\n");
    text += QStringLiteral("                    Layout.fillWidth: true\n");
    text += QStringLiteral("                    text: dashboard.player && dashboard.player.trackTitle\n");
    text += QStringLiteral("                        ? (dashboard.player.trackArtist ? dashboard.player.trackArtist + \" · \" : \"\") + dashboard.player.trackTitle\n");
    text += QStringLiteral("                        : \"Nothing playing\"\n");
    text += QStringLiteral("                    color: theme.muted\n");
    text += QStringLiteral("                    font.family: theme.fontFamily\n");
    text += QStringLiteral("                    font.pixelSize: theme.fontSize\n");
    text += QStringLiteral("                    elide: Text.ElideRight\n");
    text += QStringLiteral("                    verticalAlignment: Text.AlignVCenter\n");
    text += QStringLiteral("                }\n\n");
    text += QStringLiteral("                Text {\n");
    text += QStringLiteral("                    visible: dashboard.player !== null\n");
    text += QStringLiteral("                    text: \"⏮\"\n");
    text += QStringLiteral("                    color: theme.muted\n");
    text += QStringLiteral("                    font.pixelSize: theme.fontSize + 2\n");
    text += QStringLiteral("                    TapHandler { onTapped: if (dashboard.player) dashboard.player.previous() }\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("                Text {\n");
    text += QStringLiteral("                    visible: dashboard.player !== null\n");
    text += QStringLiteral("                    text: dashboard.player && dashboard.player.isPlaying ? \"⏸\" : \"▶\"\n");
    text += QStringLiteral("                    color: theme.text\n");
    text += QStringLiteral("                    font.pixelSize: theme.fontSize + 2\n");
    text += QStringLiteral("                    TapHandler { onTapped: if (dashboard.player) dashboard.player.togglePlaying() }\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("                Text {\n");
    text += QStringLiteral("                    visible: dashboard.player !== null\n");
    text += QStringLiteral("                    text: \"⏭\"\n");
    text += QStringLiteral("                    color: theme.muted\n");
    text += QStringLiteral("                    font.pixelSize: theme.fontSize + 2\n");
    text += QStringLiteral("                    TapHandler { onTapped: if (dashboard.player) dashboard.player.next() }\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            RowLayout {\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                spacing: 10\n\n");
    text += QStringLiteral("                Text {\n");
    text += QStringLiteral("                    Layout.fillWidth: true\n");
    text += QStringLiteral("                    text: UPower.displayDevice && UPower.displayDevice.isLaptopBattery\n");
    text += QStringLiteral("                        ? \"Battery \" + Math.round(UPower.displayDevice.percentage * 100) + \"%\"\n");
    text += QStringLiteral("                        : \"Connected to power\"\n");
    text += QStringLiteral("                    color: theme.muted\n");
    text += QStringLiteral("                    font.family: theme.fontFamily\n");
    text += QStringLiteral("                    font.pixelSize: theme.fontSize\n");
    text += QStringLiteral("                }\n\n");
    text += QStringLiteral("                Rectangle {\n");
    text += QStringLiteral("                    visible: UPower.displayDevice && UPower.displayDevice.isLaptopBattery\n");
    text += QStringLiteral("                    Layout.preferredWidth: 120\n");
    text += QStringLiteral("                    Layout.preferredHeight: 8\n");
    text += QStringLiteral("                    radius: 4\n");
    text += QStringLiteral("                    color: theme.surfaceHigh\n\n");
    text += QStringLiteral("                    Rectangle {\n");
    text += QStringLiteral("                        width: parent.width * Math.max(0, Math.min(1, UPower.displayDevice.percentage))\n");
    text += QStringLiteral("                        height: parent.height\n");
    text += QStringLiteral("                        radius: 4\n");
    text += QStringLiteral("                        color: theme.primary\n");
    text += QStringLiteral("                    }\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            Item { Layout.fillHeight: true }\n\n");
    text += QStringLiteral("            RowLayout {\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                spacing: 8\n\n");
    text += pad(powerButton(QStringLiteral("Lock"), QStringLiteral("[\"loginctl\", \"lock-session\"]"), QStringLiteral("lock")), 16);
    text += pad(powerButton(QStringLiteral("Log out"),
                           QStringLiteral("[\"loginctl\", \"terminate-user\", Quickshell.env(\"USER\")]"),
                           QStringLiteral("logout")),
                16);
    text += pad(powerButton(QStringLiteral("Sleep"), QStringLiteral("[\"systemctl\", \"suspend\"]"), QStringLiteral("sleep")), 16);
    text += pad(powerButton(QStringLiteral("Reboot"), QStringLiteral("[\"systemctl\", \"reboot\"]"), QStringLiteral("reboot")), 16);
    text += pad(powerButton(QStringLiteral("Power off"), QStringLiteral("[\"systemctl\", \"poweroff\"]"), QStringLiteral("power")), 16);
    text += QStringLiteral("                Item { Layout.fillWidth: true }\n");
    text += QStringLiteral("            }\n");
    text += QStringLiteral("        }\n");
    text += QStringLiteral("    }\n");
    text += QStringLiteral("}\n");
    return text;
}

QString notificationsFile(const Config &config)
{
    const bool bottom = config.position == QLatin1String("bottom");
    const QString edges = bottom ? QStringLiteral("        bottom: true\n        right: true\n")
                                 : QStringLiteral("        top: true\n        right: true\n");
    const QString insets = bottom
        ? QStringLiteral("        bottom: theme.barHeight + theme.margin * 2\n        right: theme.margin\n")
        : QStringLiteral("        top: theme.barHeight + theme.margin * 2\n        right: theme.margin\n");

    QString text = QStringLiteral(R"QML(// Generated by HyperThemes for Quickshell.
// Toggle it with: qs ipc call notifications toggle
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland

PanelWindow {
    id: notifications

    required property var theme
    property var tracked

    readonly property var list: tracked ? tracked.values : []

    visible: false
    color: "transparent"
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.namespace: "hyperthemes-notifications"
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.OnDemand
    focusable: true
    screen: Quickshell.screens.length > 0 ? Quickshell.screens[0] : null
    implicitWidth: 380
    implicitHeight: Math.min(420, screen ? screen.height - theme.barHeight - theme.margin * 3 : 420)
    anchors {
%1    }
    margins {
%2    }

    function toggle() {
        visible = !visible;
    }

    IpcHandler {
        target: "notifications"
        function toggle() { notifications.visible = !notifications.visible; }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: notifications.visible = false
    }

    Rectangle {
        anchors.fill: parent
        radius: theme.radius * 2
        color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, Math.max(0.94, theme.opacity))
        border.width: 1
        border.color: theme.surfaceHigh

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: notifications.list.length > 0
                        ? "Notifications · " + notifications.list.length
                        : "Notifications"
                    color: theme.text
                    font.family: theme.fontFamily
                    font.pixelSize: theme.fontSize + 1
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    text: "✕"
                    color: theme.muted
                    font.pixelSize: theme.fontSize + 1
                    TapHandler { onTapped: notifications.visible = false }
                }
            }

            Text {
                visible: notifications.list.length === 0
                Layout.fillWidth: true
                text: "Nothing to show"
                color: theme.muted
                font.family: theme.fontFamily
                font.pixelSize: theme.fontSize
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: notifications.list.length > 0
                contentHeight: entryColumn.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {}

                ColumnLayout {
                    id: entryColumn
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: notifications.list

                        delegate: Rectangle {
                            id: entry

                            required property var modelData
                            required property int index

                            readonly property string origin: String(modelData.appName || "")
                            readonly property string heading: String(modelData.summary || modelData.body || "Notification")
                            readonly property string detail: String(modelData.body || "")

                            Layout.fillWidth: true
                            implicitHeight: entryLayout.implicitHeight + 20
                            radius: theme.radius
                            color: theme.surfaceHigh

                            ColumnLayout {
                                id: entryLayout
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 3

                                Text {
                                    Layout.fillWidth: true
                                    text: entry.origin.length > 0 ? entry.origin : "Notification"
                                    color: theme.muted
                                    font.family: theme.fontFamily
                                    font.pixelSize: theme.fontSize - 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: entry.heading
                                    color: theme.text
                                    font.family: theme.fontFamily
                                    font.pixelSize: theme.fontSize
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: entry.detail.length > 0 && entry.detail !== entry.heading
                                    text: entry.detail
                                    color: theme.muted
                                    font.family: theme.fontFamily
                                    font.pixelSize: theme.fontSize - 1
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 3
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
)QML");
    text.replace(QStringLiteral("%1"), edges);
    text.replace(QStringLiteral("%2"), insets);
    return text;
}

QString shellFile(const Config &config)
{
    const bool notifications = usesWidget(config, QStringLiteral("notifications"));
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("// Run it with: qs -c %1\n").arg(config.name);
    text += QStringLiteral("import QtQuick\n");
    text += QStringLiteral("import Quickshell\n");
    text += QStringLiteral("import \"bar\"\n");
    if (config.launcher)
        text += QStringLiteral("import \"launcher\"\n");
    if (config.dashboard)
        text += QStringLiteral("import \"dashboard\"\n");
    if (notifications)
        text += QStringLiteral("import \"notifications\"\n");
    text += QStringLiteral("\n");
    text += QStringLiteral("ShellRoot {\n");
    text += QStringLiteral("    Theme { id: theme }\n\n");
    if (config.launcher)
        text += QStringLiteral("    Launcher { id: launcher; theme: theme }\n");
    if (config.dashboard)
        text += QStringLiteral("    Dashboard { id: dashboard; theme: theme }\n");
    text += QStringLiteral("    Bar {\n");
    text += QStringLiteral("        id: bar\n");
    text += QStringLiteral("        theme: theme\n");
    if (config.launcher)
        text += QStringLiteral("        onLauncherRequested: launcher.toggle()\n");
    if (config.dashboard)
        text += QStringLiteral("        onDashboardRequested: dashboard.toggle()\n");
    if (notifications)
        text += QStringLiteral("        onNotificationsRequested: notifications.toggle()\n");
    text += QStringLiteral("    }\n");
    if (notifications) {
        text += QStringLiteral("    Notifications {\n");
        text += QStringLiteral("        id: notifications\n");
        text += QStringLiteral("        theme: theme\n");
        text += QStringLiteral("        tracked: bar.notificationList\n");
        text += QStringLiteral("    }\n");
    }
    text += QStringLiteral("}\n");
    return text;
}

QString readmeFile(const Config &config)
{
    QString text;
    text += QStringLiteral("# %1 — Quickshell config\n\n").arg(config.name);
    text += QStringLiteral("Generated by HyperThemes from the **%1** palette.\n\n").arg(config.themeName);
    text += QStringLiteral("```bash\n");
    text += QStringLiteral("qs -c %1\n").arg(config.name);
    text += QStringLiteral("```\n\n");
    text += QStringLiteral("## What is here\n\n");
    text += QStringLiteral("- `shell.qml` — entry point (ShellRoot) that loads the modules below.\n");
    text += QStringLiteral("- `Theme.qml` — colours, geometry, typography and workspace settings taken from the palette.\n");
    text += QStringLiteral("- `bar/Bar.qml` — the %1 panel, one per monitor when `barPerMonitor` is on.\n").arg(config.position);
    text += QStringLiteral("- `hyperthemes.json` — the builder inputs, so HyperThemes can load this config again.\n");
    if (config.launcher) {
        text += QStringLiteral("- `launcher/Launcher.qml` — searchable application launcher.\n");
        text += QStringLiteral("  Toggle it with `qs ipc call launcher toggle`.\n");
    }
    if (config.dashboard) {
        text += QStringLiteral("- `dashboard/Dashboard.qml` — clock, media, battery and session actions.\n");
        text += QStringLiteral("  Toggle it with `qs ipc call dashboard toggle`.\n");
    }
    if (usesWidget(config, QStringLiteral("notifications"))) {
        text += QStringLiteral("- `notifications/Notifications.qml` — the tray the notification widget opens.\n");
        text += QStringLiteral("  Toggle it with `qs ipc call notifications toggle`; it follows the bar's edge.\n");
    }
    text += QStringLiteral("\n## Notes\n\n");
    text += QStringLiteral("- `Theme.qml` is the supported place to retune the generated shell; a rebuild from HyperThemes overwrites it.\n");
    text += QStringLiteral("- The bar uses `Variants` over `Quickshell.screens`; turn on **Bar on every monitor** in HyperThemes to show one per display.\n");
    if (config.compositor == QLatin1String("hyprland"))
        text += QStringLiteral("- Workspaces use `Quickshell.Hyprland`; change `workspaceCompositor` in `Theme.qml` or rebuild for Sway/i3 or a placeholder.\n");
    else if (config.compositor == QLatin1String("sway"))
        text += QStringLiteral("- Workspaces use `Quickshell.I3` (Sway/i3); change `workspaceCompositor` in `Theme.qml` or rebuild for Hyprland or a placeholder.\n");
    else
        text += QStringLiteral("- Workspaces are placeholders; rebuild with a Hyprland or Sway/i3 compositor to wire them to real workspaces.\n");
    text += QStringLiteral("- Every rewrite made by HyperThemes backs up the previous folder under `~/.local/state/hyperthemes/shell-backups/`.\n");
    QString lint = QStringLiteral("shell.qml Theme.qml bar/Bar.qml");
    if (usesWidget(config, QStringLiteral("notifications")))
        lint += QStringLiteral(" notifications/Notifications.qml");
    text += QStringLiteral("- Validate after editing with `qmllint %1`.\n").arg(lint);
    return text;
}

QStringList knownWidgets()
{
    return {
        QStringLiteral("logo"), QStringLiteral("workspaces"), QStringLiteral("launcher"),
        QStringLiteral("clock"), QStringLiteral("date"), QStringLiteral("media"),
        QStringLiteral("volume"), QStringLiteral("network"), QStringLiteral("bluetooth"),
        QStringLiteral("tray"), QStringLiteral("battery"), QStringLiteral("statusIcons"),
        QStringLiteral("power"), QStringLiteral("separator"), QStringLiteral("spacer"),
        QStringLiteral("systemLoad"), QStringLiteral("uptime"), QStringLiteral("memory"),
        QStringLiteral("notifications"),
    };
}

QStringList cleanZone(const QVariant &value)
{
    QStringList out;
    const QStringList ids = value.toStringList();
    for (const QString &id : ids) {
        if (knownWidgets().contains(id) && !out.contains(id))
            out << id;
    }
    return out;
}

Config parseConfig(const QVariantMap &map)
{
    Config config;
    config.name = map.value(QStringLiteral("name"), QStringLiteral("hyperthemes")).toString().trimmed().toLower();
    config.position = map.value(QStringLiteral("position"), QStringLiteral("top")).toString();
    config.barHeight = qBound(24, map.value(QStringLiteral("barHeight"), 34).toInt(), 64);
    config.margin = qBound(0, map.value(QStringLiteral("margin"), 6).toInt(), 32);
    config.radius = qBound(0, map.value(QStringLiteral("radius"), 10).toInt(), 28);
    config.opacity = qBound(0.4, map.value(QStringLiteral("opacity"), 0.94).toDouble(), 1.0);
    config.gap = qBound(0, map.value(QStringLiteral("gap"), 10).toInt(), 32);
    config.barPerMonitor = map.value(QStringLiteral("barPerMonitor"), false).toBool();
    config.launcher = map.value(QStringLiteral("launcher"), true).toBool();
    config.dashboard = map.value(QStringLiteral("dashboard"), true).toBool();
    config.clock24h = map.value(QStringLiteral("clock24h"), true).toBool();
    config.showSeconds = map.value(QStringLiteral("showSeconds"), false).toBool();
    config.borders = map.value(QStringLiteral("borders"), true).toBool();
    config.shadow = map.value(QStringLiteral("shadow"), true).toBool();
    config.style = map.value(QStringLiteral("style"), QStringLiteral("floating")).toString();
    if (config.style != QLatin1String("floating") && config.style != QLatin1String("edge")
        && config.style != QLatin1String("pill") && config.style != QLatin1String("minimal"))
        config.style = QStringLiteral("floating");
    config.animationDuration = qBound(0, map.value(QStringLiteral("animationDuration"), 120).toInt(), 500);
    const auto appendMonitors = [](QStringList &target, const QVariant &value) {
        QStringList names;
        const QVariantList list = value.toList();
        if (!list.isEmpty()) {
            for (const QVariant &entry : list)
                names << entry.toString();
        } else {
            for (const QString &part : value.toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
                names << part;
        }
        for (const QString &monitor : std::as_const(names)) {
            const QString name = monitor.trimmed();
            if (!name.isEmpty() && !target.contains(name))
                target << name;
        }
    };
    appendMonitors(config.monitors, map.value(QStringLiteral("monitors")));
    appendMonitors(config.bottomMonitors, map.value(QStringLiteral("bottomMonitors")));
    config.fontFamily = map.value(QStringLiteral("fontFamily")).toString().trimmed();
    config.fontSize = qBound(8, map.value(QStringLiteral("fontSize"), 12).toInt(), config.barHeight - 8);
    const int iconMax = qMax(10, config.barHeight - 14);
    config.iconSize = qBound(10, map.value(QStringLiteral("iconSize"), 16).toInt(), iconMax);
    config.workspaceCount = qBound(1, map.value(QStringLiteral("workspaceCount"), 5).toInt(), 12);
    config.compositor = map.value(QStringLiteral("compositor"), QStringLiteral("hyprland")).toString();
    if (config.compositor != QLatin1String("hyprland") && config.compositor != QLatin1String("sway")
        && config.compositor != QLatin1String("none"))
        config.compositor = QStringLiteral("hyprland");
    config.palette = map.value(QStringLiteral("palette")).toMap();
    config.themeName = map.value(QStringLiteral("themeName"), QStringLiteral("HyperThemes")).toString();
    config.themeId = map.value(QStringLiteral("themeId")).toString();
    config.themeMode = map.value(QStringLiteral("themeMode")).toString();

    const QVariantMap zones = map.value(QStringLiteral("zones")).toMap();
    config.left = cleanZone(zones.value(QStringLiteral("left")));
    config.center = cleanZone(zones.value(QStringLiteral("center")));
    config.right = cleanZone(zones.value(QStringLiteral("right")));

    // A widget may only live in one zone.
    for (const QString &id : config.center)
        config.left.removeAll(id);
    for (const QString &id : config.right) {
        config.left.removeAll(id);
        config.center.removeAll(id);
    }
    if (config.position != QLatin1String("bottom"))
        config.position = QStringLiteral("top");
    return config;
}

QVariantMap configMap(const Config &config, bool withIdentity)
{
    QVariantMap map;
    map.insert(QStringLiteral("name"), config.name);
    map.insert(QStringLiteral("position"), config.position);
    map.insert(QStringLiteral("barHeight"), config.barHeight);
    map.insert(QStringLiteral("margin"), config.margin);
    map.insert(QStringLiteral("radius"), config.radius);
    map.insert(QStringLiteral("opacity"), config.opacity);
    map.insert(QStringLiteral("gap"), config.gap);
    map.insert(QStringLiteral("barPerMonitor"), config.barPerMonitor);
    map.insert(QStringLiteral("launcher"), config.launcher);
    map.insert(QStringLiteral("dashboard"), config.dashboard);
    map.insert(QStringLiteral("clock24h"), config.clock24h);
    map.insert(QStringLiteral("showSeconds"), config.showSeconds);
    map.insert(QStringLiteral("borders"), config.borders);
    map.insert(QStringLiteral("shadow"), config.shadow);
    map.insert(QStringLiteral("style"), config.style);
    map.insert(QStringLiteral("animationDuration"), config.animationDuration);
    map.insert(QStringLiteral("monitors"), config.monitors);
    map.insert(QStringLiteral("bottomMonitors"), config.bottomMonitors);
    map.insert(QStringLiteral("fontFamily"), config.fontFamily);
    map.insert(QStringLiteral("fontSize"), config.fontSize);
    map.insert(QStringLiteral("iconSize"), config.iconSize);
    map.insert(QStringLiteral("workspaceCount"), config.workspaceCount);
    map.insert(QStringLiteral("compositor"), config.compositor);
    map.insert(QStringLiteral("zones"), QVariantMap{
        {QStringLiteral("left"), config.left},
        {QStringLiteral("center"), config.center},
        {QStringLiteral("right"), config.right},
    });
    if (withIdentity) {
        map.insert(QStringLiteral("palette"), config.palette);
        map.insert(QStringLiteral("themeName"), config.themeName);
        map.insert(QStringLiteral("themeId"), config.themeId);
        map.insert(QStringLiteral("themeMode"), config.themeMode);
    }
    return map;
}

QStringList storedKeys()
{
    return {
        QStringLiteral("name"), QStringLiteral("position"), QStringLiteral("barHeight"),
        QStringLiteral("margin"), QStringLiteral("radius"), QStringLiteral("opacity"),
        QStringLiteral("gap"), QStringLiteral("barPerMonitor"), QStringLiteral("launcher"),
        QStringLiteral("dashboard"), QStringLiteral("clock24h"), QStringLiteral("showSeconds"),
        QStringLiteral("borders"), QStringLiteral("shadow"), QStringLiteral("style"),
        QStringLiteral("animationDuration"), QStringLiteral("monitors"), QStringLiteral("bottomMonitors"),
        QStringLiteral("fontFamily"), QStringLiteral("fontSize"), QStringLiteral("iconSize"),
        QStringLiteral("workspaceCount"), QStringLiteral("compositor"), QStringLiteral("zones"),
    };
}

bool writeFile(const QString &path, const QString &contents, QString *error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Could not write %1").arg(path);
        return false;
    }
    file.write(contents.toUtf8());
    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("Could not commit %1").arg(path);
        return false;
    }
    return true;
}

bool copyTree(const QString &source, const QString &target, QString *error)
{
    QDirIterator it(source, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString relative = QDir(source).relativeFilePath(path);
        const QString destination = target + QLatin1Char('/') + relative;
        QDir().mkpath(QFileInfo(destination).absolutePath());
        if (!QFile::copy(path, destination)) {
            if (error)
                *error = QStringLiteral("Could not back up %1").arg(relative);
            return false;
        }
    }
    return true;
}

QString manifestPath(const QString &dir)
{
    return dir + QStringLiteral("/hyperthemes.json");
}

} // namespace

ShellBuilder::ShellBuilder(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(Paths::stateDir() + QStringLiteral("/shell-backups"));
    m_run = new QProcess(this);
    m_run->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_run, &QProcess::readyReadStandardOutput, this, &ShellBuilder::collectRunOutput);
    connect(m_run, &QProcess::readyReadStandardError, this, &ShellBuilder::collectRunOutput);
    connect(m_run, &QProcess::finished, this, [this] {
        collectRunOutput();
        m_runName.clear();
        emit runningChanged();
    });
    connect(m_run, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_runName.clear();
        m_runLog += QStringLiteral("Quickshell could not be started.\n");
        emit runLogChanged();
        emit runningChanged();
    });
}

ShellBuilder::~ShellBuilder()
{
    if (m_run && m_run->state() != QProcess::NotRunning) {
        m_run->terminate();
        if (!m_run->waitForFinished(1500)) {
            m_run->kill();
            m_run->waitForFinished(1000);
        }
    }
}

void ShellBuilder::collectRunOutput()
{
    const QString chunk = QString::fromUtf8(m_run->readAllStandardOutput()
                                            + m_run->readAllStandardError());
    if (chunk.isEmpty())
        return;
    m_runLog += chunk;
    const qsizetype limit = 24000;
    if (m_runLog.size() > limit)
        m_runLog = m_runLog.right(limit);
    emit runLogChanged();
}

bool ShellBuilder::validName(const QString &name) const
{
    return QRegularExpression(QStringLiteral("^[a-z0-9][a-z0-9._-]{0,31}$")).match(name).hasMatch();
}

QVariantList ShellBuilder::widgets() const
{
    const QVector<QPair<QString, QString>> list = {
        {QStringLiteral("logo"), QStringLiteral("Logo")},
        {QStringLiteral("workspaces"), QStringLiteral("Workspaces")},
        {QStringLiteral("launcher"), QStringLiteral("Launcher")},
        {QStringLiteral("clock"), QStringLiteral("Clock")},
        {QStringLiteral("date"), QStringLiteral("Date")},
        {QStringLiteral("media"), QStringLiteral("Media")},
        {QStringLiteral("volume"), QStringLiteral("Volume")},
        {QStringLiteral("network"), QStringLiteral("Network")},
        {QStringLiteral("bluetooth"), QStringLiteral("Bluetooth")},
        {QStringLiteral("tray"), QStringLiteral("System tray")},
        {QStringLiteral("battery"), QStringLiteral("Battery")},
        {QStringLiteral("statusIcons"), QStringLiteral("Status icons")},
        {QStringLiteral("power"), QStringLiteral("Power")},
        {QStringLiteral("separator"), QStringLiteral("Separator")},
        {QStringLiteral("spacer"), QStringLiteral("Spacer")},
        {QStringLiteral("systemLoad"), QStringLiteral("System load")},
        {QStringLiteral("uptime"), QStringLiteral("Uptime")},
        {QStringLiteral("memory"), QStringLiteral("Memory")},
        {QStringLiteral("notifications"), QStringLiteral("Notifications")},
    };

    QVariantList result;
    for (const auto &entry : list) {
        result.append(QVariantMap{
            {QStringLiteral("id"), entry.first},
            {QStringLiteral("label"), entry.second},
        });
    }
    return result;
}

QVariantMap ShellBuilder::defaults() const
{
    Config config;
    config.left = {QStringLiteral("logo"), QStringLiteral("workspaces")};
    config.center = {QStringLiteral("clock")};
    config.right = {QStringLiteral("tray"), QStringLiteral("media"), QStringLiteral("volume"),
                   QStringLiteral("battery"), QStringLiteral("power")};
    return configMap(config, false);
}

QString ShellBuilder::configDir(const QString &name) const
{
    return Paths::configHome() + QStringLiteral("/quickshell/") + name;
}

QString ShellBuilder::backupsPath() const
{
    return Paths::stateDir() + QStringLiteral("/shell-backups");
}

bool ShellBuilder::quickshellAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("qs")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("quickshell")).isEmpty();
}

bool ShellBuilder::isRunning() const
{
    return m_run && m_run->state() != QProcess::NotRunning;
}

QString ShellBuilder::backupTree(const QString &name, const QString &dir, bool existed)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString id = stamp + QLatin1Char('-') + name;
    int counter = 2;
    while (QFileInfo::exists(Paths::stateDir() + QStringLiteral("/shell-backups/") + id))
        id = stamp + QStringLiteral("-") + name + QStringLiteral("-%1").arg(counter++);

    const QString backupDir = Paths::stateDir() + QStringLiteral("/shell-backups/") + id;
    QDir().mkpath(backupDir + QStringLiteral("/files"));

    if (existed) {
        QString error;
        if (!copyTree(dir, backupDir + QStringLiteral("/files"), &error))
            return {};
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("id"), id);
    manifest.insert(QStringLiteral("name"), name);
    manifest.insert(QStringLiteral("dir"), dir);
    manifest.insert(QStringLiteral("existed"), existed);
    manifest.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));

    QSaveFile file(backupDir + QStringLiteral("/manifest.json"));
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return {};
    return id;
}

QVariantMap ShellBuilder::build(const QVariantMap &config)
{
    const Config parsed = parseConfig(config);

    if (!validName(parsed.name)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"),
                 QStringLiteral("Use 1–32 lowercase letters, digits, dots, dashes or underscores for the config name.")}};
    }
    if (parsed.palette.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Select a theme first so the shell gets a real palette.")}};
    }

    const QString dir = configDir(parsed.name);
    const bool existed = QDir(dir).exists();
    const QString backup = backupTree(parsed.name, dir, existed);
    if (backup.isEmpty())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Could not back up the existing config.")}};

    QVector<QPair<QString, QString>> files = {
        {QStringLiteral("shell.qml"), shellFile(parsed)},
        {QStringLiteral("Theme.qml"), themeFile(parsed)},
        {QStringLiteral("bar/Bar.qml"), barFile(parsed)},
        {QStringLiteral("README.md"), readmeFile(parsed)},
    };
    if (parsed.launcher)
        files.append({QStringLiteral("launcher/Launcher.qml"), launcherFile(parsed)});
    if (parsed.dashboard)
        files.append({QStringLiteral("dashboard/Dashboard.qml"), dashboardFile(parsed)});
    if (usesWidget(parsed, QStringLiteral("notifications")))
        files.append({QStringLiteral("notifications/Notifications.qml"), notificationsFile(parsed)});

    // Drop optional modules that are switched off, so the folder matches the
    // configuration instead of keeping stale files from an earlier build.
    if (!parsed.launcher) {
        QFile::remove(dir + QStringLiteral("/launcher/Launcher.qml"));
        QDir().rmdir(dir + QStringLiteral("/launcher"));
    }
    if (!parsed.dashboard) {
        QFile::remove(dir + QStringLiteral("/dashboard/Dashboard.qml"));
        QDir().rmdir(dir + QStringLiteral("/dashboard"));
    }
    if (!usesWidget(parsed, QStringLiteral("notifications"))) {
        QFile::remove(dir + QStringLiteral("/notifications/Notifications.qml"));
        QDir().rmdir(dir + QStringLiteral("/notifications"));
    }

    QStringList written;
    for (const auto &file : files) {
        QString error;
        if (!writeFile(dir + QLatin1Char('/') + file.first, file.second, &error))
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), error},
                    {QStringLiteral("backup"), backup}};
        written << file.first;
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("generator"), QStringLiteral("HyperThemes"));
    manifest.insert(QStringLiteral("version"), 2);
    manifest.insert(QStringLiteral("name"), parsed.name);
    manifest.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    manifest.insert(QStringLiteral("theme"), QJsonObject{
        {QStringLiteral("id"), parsed.themeId},
        {QStringLiteral("name"), parsed.themeName},
        {QStringLiteral("mode"), parsed.themeMode},
    });
    manifest.insert(QStringLiteral("config"), QJsonObject::fromVariantMap(configMap(parsed, false)));
    manifest.insert(QStringLiteral("palette"), QJsonObject::fromVariantMap(parsed.palette));
    manifest.insert(QStringLiteral("files"), QJsonArray::fromStringList(written));

    if (!writeFile(manifestPath(dir), QString::fromUtf8(QJsonDocument(manifest).toJson(QJsonDocument::Indented)), nullptr)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Could not write the HyperThemes manifest.")},
                {QStringLiteral("backup"), backup}};
    }
    written << QStringLiteral("hyperthemes.json");

    saveSettings(configMap(parsed, false));

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("dir"), dir},
            {QStringLiteral("files"), written},
            {QStringLiteral("backup"), backup},
            {QStringLiteral("theme"), parsed.themeName},
            {QStringLiteral("config"), configMap(parsed, false)},
            {QStringLiteral("quickshell"), quickshellAvailable()}};
}

QVariantMap ShellBuilder::load(const QString &name)
{
    if (!validName(name))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That config name is not valid.")}};

    const QString dir = configDir(name);
    QFile manifestFile(manifestPath(dir));
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"),
                 QStringLiteral("No HyperThemes manifest in %1. The config can still be run, but not edited here.").arg(dir)}};
    }

    const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll());
    if (!document.isObject())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The HyperThemes manifest is not valid JSON.")}};

    const QJsonObject root = document.object();
    QVariantMap result = defaults();
    const QVariantMap stored = root.value(QStringLiteral("config")).toObject().toVariantMap();
    for (auto it = stored.cbegin(); it != stored.cend(); ++it)
        result.insert(it.key(), it.value());
    result.insert(QStringLiteral("name"), name);
    result.insert(QStringLiteral("palette"), root.value(QStringLiteral("palette")).toObject().toVariantMap());
    const QJsonObject theme = root.value(QStringLiteral("theme")).toObject();
    result.insert(QStringLiteral("themeName"), theme.value(QStringLiteral("name")).toString());
    result.insert(QStringLiteral("themeId"), theme.value(QStringLiteral("id")).toString());
    result.insert(QStringLiteral("themeMode"), theme.value(QStringLiteral("mode")).toString());

    if (result.value(QStringLiteral("palette")).toMap().isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The manifest has no palette; rebuild the config from a theme.")}};
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("config"), result},
            {QStringLiteral("created"), root.value(QStringLiteral("created")).toString()},
            {QStringLiteral("theme"), theme.value(QStringLiteral("name")).toString()},
            {QStringLiteral("files"), root.value(QStringLiteral("files")).toVariant().toStringList()},
            {QStringLiteral("dir"), dir}};
}

QVariantList ShellBuilder::configs() const
{
    QVariantList result;
    const QDir root(configDir(QString()));
    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &entry : entries) {
        const QString dir = root.filePath(entry);
        if (!QFileInfo::exists(dir + QStringLiteral("/shell.qml")))
            continue;

        QVariantMap item;
        item.insert(QStringLiteral("name"), entry);
        item.insert(QStringLiteral("dir"), dir);
        item.insert(QStringLiteral("managed"), QFileInfo::exists(manifestPath(dir)));
        item.insert(QStringLiteral("running"), isRunning() && m_runName == entry);
        item.insert(QStringLiteral("modified"),
                    QFileInfo(dir + QStringLiteral("/shell.qml")).lastModified().toString(Qt::ISODate));

        QFile manifest(manifestPath(dir));
        if (manifest.open(QIODevice::ReadOnly)) {
            const QJsonObject rootObject = QJsonDocument::fromJson(manifest.readAll()).object();
            item.insert(QStringLiteral("created"), rootObject.value(QStringLiteral("created")).toString());
            item.insert(QStringLiteral("theme"),
                        rootObject.value(QStringLiteral("theme")).toObject().value(QStringLiteral("name")).toString());
        }
        result.append(item);
    }
    return result;
}

QVariantMap ShellBuilder::duplicateConfig(const QString &name, const QString &newName)
{
    if (!validName(name) || !validName(newName)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"),
                 QStringLiteral("Use 1–32 lowercase letters, digits, dots, dashes or underscores for the config name.")}};
    }
    if (name == newName) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Choose a different name for the copy.")}};
    }

    const QString from = configDir(name);
    const QString to = configDir(newName);
    if (!QFileInfo::exists(from + QStringLiteral("/shell.qml"))) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That config no longer exists.")}};
    }
    if (QFileInfo::exists(to)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"),
                 QStringLiteral("A config named %1 already exists.").arg(newName)}};
    }

    QString error;
    if (!copyTree(from, to, &error)) {
        QDir(to).removeRecursively();
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }

    // A copy is a new config: refresh its identity in the manifest so Load
    // reports the copy rather than the folder it came from.
    QJsonObject manifest;
    QFile manifestFile(manifestPath(to));
    if (manifestFile.open(QIODevice::ReadOnly))
        manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    manifestFile.close();

    if (!manifest.isEmpty()) {
        manifest.insert(QStringLiteral("name"), newName);
        manifest.insert(QStringLiteral("created"),
                        QDateTime::currentDateTime().toString(Qt::ISODate));
        QJsonObject stored = manifest.value(QStringLiteral("config")).toObject();
        if (!stored.isEmpty()) {
            stored.insert(QStringLiteral("name"), newName);
            manifest.insert(QStringLiteral("config"), stored);
        }

        QSaveFile file(manifestPath(to));
        bool written = file.open(QIODevice::WriteOnly);
        if (written) {
            file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
            written = file.commit();
        }
        if (!written) {
            QDir(to).removeRecursively();
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("The copy could not be written.")}};
        }
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("name"), newName},
            {QStringLiteral("dir"), to},
            {QStringLiteral("source"), name}};
}

QVariantMap ShellBuilder::renameConfig(const QString &name, const QString &newName)
{
    if (!validName(name) || !validName(newName)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"),
                 QStringLiteral("Use 1–32 lowercase letters, digits, dots, dashes or underscores for the config name.")}};
    }
    if (name == newName) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Choose a different name for the config.")}};
    }

    const QString from = configDir(name);
    const QString to = configDir(newName);
    if (!QFileInfo::exists(from + QStringLiteral("/shell.qml"))) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That config no longer exists.")}};
    }
    if (QFileInfo::exists(to)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"),
                 QStringLiteral("A config named %1 already exists.").arg(newName)}};
    }
    if (isRunning() && m_runName == name) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Stop the generated shell before renaming it.")}};
    }

    if (!QDir().rename(from, to)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The config folder could not be renamed.")}};
    }

    // Keep the manifest in step with the folder. A config without a manifest
    // is still renamed; only HyperThemes-managed metadata is rewritten.
    QJsonObject manifest;
    QFile manifestFile(manifestPath(to));
    if (manifestFile.open(QIODevice::ReadOnly))
        manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    manifestFile.close();

    if (!manifest.isEmpty()) {
        manifest.insert(QStringLiteral("name"), newName);
        QJsonObject stored = manifest.value(QStringLiteral("config")).toObject();
        if (!stored.isEmpty()) {
            stored.insert(QStringLiteral("name"), newName);
            manifest.insert(QStringLiteral("config"), stored);
        }

        QSaveFile file(manifestPath(to));
        bool written = file.open(QIODevice::WriteOnly);
        if (written) {
            file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
            written = file.commit();
        }
        if (!written) {
            const bool rolledBack = QDir().rename(to, from);
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"),
                     rolledBack
                         ? QStringLiteral("The manifest could not be updated; the config was left unchanged.")
                         : QStringLiteral("The manifest could not be updated, and the folder is now named %1.").arg(newName)}};
        }
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("name"), newName},
            {QStringLiteral("dir"), to}};
}

QVariantMap ShellBuilder::removeConfig(const QString &name)
{
    if (!validName(name)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That config name is not valid.")}};
    }

    const QString dir = configDir(name);
    if (!QFileInfo::exists(dir + QStringLiteral("/shell.qml"))) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That config no longer exists.")}};
    }
    if (isRunning() && m_runName == name) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Stop the generated shell before deleting it.")}};
    }

    const QString backup = backupTree(name, dir, true);
    if (backup.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The config could not be backed up, so it was not deleted.")}};
    }

    if (!QDir(dir).removeRecursively()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The config folder could not be deleted.")},
                {QStringLiteral("backup"), backup}};
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("name"), name},
            {QStringLiteral("backup"), backup},
            {QStringLiteral("warnings"), QStringList{QStringLiteral("The config was deleted; restore it from Backups if needed.")}}};
}

QVariantMap ShellBuilder::lint(const QString &name)
{
    if (!validName(name))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That config name is not valid.")}};

    const QString dir = configDir(name);
    if (!QFileInfo::exists(dir + QStringLiteral("/shell.qml"))) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Write the config before validating it.")}};
    }

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("qmllint"));
    if (executable.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("skipped"), true},
                {QStringLiteral("error"), QStringLiteral("qmllint is not installed, so the config was not validated.")}};
    }

    const QString imports = QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
    if (!QFileInfo::exists(imports + QStringLiteral("/Quickshell"))) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("skipped"), true},
                {QStringLiteral("error"),
                 QStringLiteral("Quickshell's QML module is not installed, so the config was not validated.")}};
    }

    QStringList files = {QStringLiteral("shell.qml"), QStringLiteral("Theme.qml"), QStringLiteral("bar/Bar.qml")};
    if (QFileInfo::exists(dir + QStringLiteral("/launcher/Launcher.qml")))
        files << QStringLiteral("launcher/Launcher.qml");
    if (QFileInfo::exists(dir + QStringLiteral("/dashboard/Dashboard.qml")))
        files << QStringLiteral("dashboard/Dashboard.qml");
    if (QFileInfo::exists(dir + QStringLiteral("/notifications/Notifications.qml")))
        files << QStringLiteral("notifications/Notifications.qml");

    QStringList problems;
    QStringList checked;
    for (const QString &file : files) {
        QProcess process;
        process.setWorkingDirectory(dir);
        process.setProgram(executable);
        process.setArguments({file});
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start();
        if (!process.waitForStarted(5000)) {
            problems << QStringLiteral("qmllint could not start");
            break;
        }
        if (!process.waitForFinished(30000)) {
            process.kill();
            process.waitForFinished(1000);
            problems << file + QStringLiteral(": qmllint timed out");
            continue;
        }
        const QString output = QString::fromUtf8(process.readAll()).trimmed();
        if (process.exitCode() != 0) {
            problems << file + QStringLiteral(": ")
                + (output.isEmpty() ? QStringLiteral("qmllint reported an error") : output);
        } else {
            checked << file;
        }
    }

    return {{QStringLiteral("ok"), problems.isEmpty()},
            {QStringLiteral("files"), checked},
            {QStringLiteral("errors"), problems},
            {QStringLiteral("output"), problems.join(QLatin1Char('\n'))},
            {QStringLiteral("skipped"), false}};
}

QVariantMap ShellBuilder::run(const QString &name)
{
    if (!validName(name))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That config name is not valid.")}};

    if (isRunning()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("A generated shell is already running from this window.")}};
    }

    const QString dir = configDir(name);
    if (!QFileInfo::exists(dir + QStringLiteral("/shell.qml"))) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Write the config before running it.")}};
    }

    QString executable = QStandardPaths::findExecutable(QStringLiteral("qs"));
    if (executable.isEmpty())
        executable = QStandardPaths::findExecutable(QStringLiteral("quickshell"));
    if (executable.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Quickshell is not on PATH.")}};
    }

    m_runLog.clear();
    emit runLogChanged();
    m_run->setWorkingDirectory(dir);
    m_run->setProgram(executable);
    m_run->setArguments({QStringLiteral("-c"), name});
    m_run->start();
    if (!m_run->waitForStarted(5000)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Quickshell did not start: %1").arg(m_run->errorString())}};
    }

    m_runName = name;
    emit runningChanged();
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("pid"), static_cast<qlonglong>(m_run->processId())},
            {QStringLiteral("command"), executable + QStringLiteral(" -c ") + name},
            {QStringLiteral("dir"), dir}};
}

void ShellBuilder::stopRun()
{
    if (!isRunning())
        return;
    m_run->terminate();
    if (!m_run->waitForFinished(2000)) {
        m_run->kill();
        m_run->waitForFinished(1000);
    }
    collectRunOutput();
    m_runName.clear();
    emit runningChanged();
}

QVariantMap ShellBuilder::exportConfig(const QString &name, const QString &targetPath)
{
    if (!validName(name))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That config name is not valid.")}};

    const QString dir = configDir(name);
    if (!QFileInfo::exists(dir + QStringLiteral("/shell.qml"))) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Write the config before exporting it.")}};
    }

    QString target = targetPath.trimmed();
    if (target.isEmpty()) {
        QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (base.isEmpty())
            base = Paths::home();
        target = base + QLatin1Char('/') + name + QStringLiteral(".tar.gz");
    } else if (QFileInfo(target).isDir()) {
        target = target + QLatin1Char('/') + name + QStringLiteral(".tar.gz");
    } else if (!target.endsWith(QLatin1String(".tar.gz")) && !target.endsWith(QLatin1String(".tgz"))) {
        target += QStringLiteral(".tar.gz");
    }
    target = QFileInfo(target).absoluteFilePath();

    if (QDir::cleanPath(target).startsWith(QDir::cleanPath(dir) + QLatin1Char('/'))) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Choose a target outside the config folder.")}};
    }

    QString executable = QStandardPaths::findExecutable(QStringLiteral("bsdtar"));
    if (executable.isEmpty())
        executable = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (executable.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Install bsdtar or tar to export a config.")}};
    }

    QDir().mkpath(QFileInfo(target).absolutePath());
    QFile::remove(target);

    QProcess process;
    process.setProgram(executable);
    process.setArguments({QStringLiteral("-czf"), target, QStringLiteral("-C"),
                          QFileInfo(dir).absolutePath(), name});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(5000) || !process.waitForFinished(120000) || process.exitCode() != 0) {
        const QString output = QString::fromUtf8(process.readAll()).trimmed();
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The archive could not be written: %1").arg(output)}};
    }
    if (!QFileInfo::exists(target)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The archive tool reported success but wrote no file.")}};
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("path"), target},
            {QStringLiteral("bytes"), QFileInfo(target).size()}};
}

QStringList ShellBuilder::backups() const
{
    QStringList result;
    const QDir root(Paths::stateDir() + QStringLiteral("/shell-backups"));
    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QString &entry : entries) {
        if (QFileInfo::exists(root.filePath(entry) + QStringLiteral("/manifest.json")))
            result << entry;
    }
    return result;
}

QVariantList ShellBuilder::backupEntries() const
{
    QVariantList result;
    const QDir root(Paths::stateDir() + QStringLiteral("/shell-backups"));
    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QString &entry : entries) {
        QFile manifest(root.filePath(entry) + QStringLiteral("/manifest.json"));
        if (!manifest.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject object = QJsonDocument::fromJson(manifest.readAll()).object();
        result.append(QVariantMap{
            {QStringLiteral("id"), entry},
            {QStringLiteral("name"), object.value(QStringLiteral("name")).toString()},
            {QStringLiteral("dir"), object.value(QStringLiteral("dir")).toString()},
            {QStringLiteral("created"), object.value(QStringLiteral("created")).toString()},
            {QStringLiteral("existed"), object.value(QStringLiteral("existed")).toBool()},
        });
    }
    return result;
}

QVariantMap ShellBuilder::restore(const QString &backupId)
{
    if (backupId.isEmpty())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("No backup chosen.")}};

    const QString backupDir = Paths::stateDir() + QStringLiteral("/shell-backups/") + backupId;
    QFile manifestFile(backupDir + QStringLiteral("/manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("That backup no longer exists.")}};

    const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const QString dir = QDir::cleanPath(manifest.value(QStringLiteral("dir")).toString());
    const QString expectedName = manifest.value(QStringLiteral("name")).toString();
    const bool existed = manifest.value(QStringLiteral("existed")).toBool();
    if (dir.isEmpty())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("The backup manifest is incomplete.")}};

    // Only ever restore into the config folder named by this backup.
    const QString root = QDir::cleanPath(configDir(QString()));
    if (QFileInfo(dir).absolutePath() != root
        || (!expectedName.isEmpty() && QFileInfo(dir).fileName() != expectedName)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The backup points outside its Quickshell config folder.")}};
    }

    QStringList warnings;
    if (existed) {
        if (!QFileInfo::exists(backupDir + QStringLiteral("/files"))) {
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("The backup is incomplete; its files folder is missing.")}};
        }

        const QString staging = dir + QStringLiteral(".hyperthemes-restore-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        QDir(staging).removeRecursively();

        QString error;
        if (!copyTree(backupDir + QStringLiteral("/files"), staging, &error)) {
            QDir(staging).removeRecursively();
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
        }

        QString removeError;
        if (QDir(dir).exists() && !QDir(dir).removeRecursively()) {
            QDir(staging).removeRecursively();
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("Could not replace %1").arg(dir)}};
        }
        if (!QDir().rename(staging, dir)) {
            QDir(staging).removeRecursively();
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), QStringLiteral("Could not move the restored config into place.")}};
        }
        warnings << QStringLiteral("The previous Quickshell config was restored.");
    } else {
        QDir(dir).removeRecursively();
        warnings << QStringLiteral("The generated config was removed.");
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("dir"), dir},
            {QStringLiteral("warnings"), warnings}};
}

QVariantMap ShellBuilder::settings() const
{
    QSettings store(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("HyperThemes"), QStringLiteral("HyperThemes"));
    const QVariantMap stored = store.value(QStringLiteral("shellBuilder/config")).toMap();

    QVariantMap merged = defaults();
    for (const QString &key : storedKeys()) {
        if (stored.contains(key))
            merged.insert(key, stored.value(key));
    }
    return configMap(parseConfig(merged), false);
}

void ShellBuilder::saveSettings(const QVariantMap &settingsMap)
{
    const Config parsed = parseConfig(settingsMap);
    const QVariantMap clean = configMap(parsed, false);

    QVariantMap stored;
    for (const QString &key : storedKeys()) {
        if (clean.contains(key))
            stored.insert(key, clean.value(key));
    }

    QSettings store(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("HyperThemes"), QStringLiteral("HyperThemes"));
    store.setValue(QStringLiteral("shellBuilder/config"), stored);
    store.sync();
}

void ShellBuilder::resetSettings()
{
    QSettings store(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("HyperThemes"), QStringLiteral("HyperThemes"));
    store.remove(QStringLiteral("shellBuilder/config"));
    store.sync();
}

QVariantMap ShellBuilder::savePreset(const QString &name, const QVariantMap &config)
{
    if (!validName(name)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"),
                 QStringLiteral("Use 1–32 lowercase letters, digits, dots, dashes or underscores for the preset name.")}};
    }

    const Config parsed = parseConfig(config);
    if (parsed.palette.isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("Select a theme before saving a builder preset.")}};
    }

    const QString dir = Paths::stateDir() + QStringLiteral("/shell-presets");
    QDir().mkpath(dir);

    QJsonObject root;
    root.insert(QStringLiteral("generator"), QStringLiteral("HyperThemes"));
    root.insert(QStringLiteral("name"), name);
    root.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("config"), QJsonObject::fromVariantMap(configMap(parsed, true)));

    QSaveFile file(dir + QLatin1Char('/') + name + QStringLiteral(".json"));
    if (!file.open(QIODevice::WriteOnly)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The preset could not be written.")}};
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The preset could not be committed.")}};
    }

    return {{QStringLiteral("ok"), true}, {QStringLiteral("name"), name}};
}

QVariantList ShellBuilder::presets() const
{
    QVariantList result;
    const QDir dir(Paths::stateDir() + QStringLiteral("/shell-presets"));
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        QFile handle(dir.filePath(file));
        if (!handle.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject root = QJsonDocument::fromJson(handle.readAll()).object();
        const QVariantMap stored = root.value(QStringLiteral("config")).toObject().toVariantMap();
        result.append(QVariantMap{
            {QStringLiteral("name"), file.chopped(5)},
            {QStringLiteral("created"), root.value(QStringLiteral("created")).toString()},
            {QStringLiteral("style"), stored.value(QStringLiteral("style")).toString()},
            {QStringLiteral("theme"), stored.value(QStringLiteral("themeName")).toString()},
            {QStringLiteral("monitors"), stored.value(QStringLiteral("monitors")).toStringList()},
        });
    }
    return result;
}

QVariantMap ShellBuilder::loadPreset(const QString &name)
{
    if (!validName(name)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That preset name is not valid.")}};
    }

    QFile file(Paths::stateDir() + QStringLiteral("/shell-presets/") + name + QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That preset no longer exists.")}};
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QVariantMap stored = root.value(QStringLiteral("config")).toObject().toVariantMap();
    if (stored.value(QStringLiteral("palette")).toMap().isEmpty()) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The preset has no palette; save it again from a theme.")}};
    }

    QVariantMap config = defaults();
    for (auto it = stored.cbegin(); it != stored.cend(); ++it)
        config.insert(it.key(), it.value());
    config.insert(QStringLiteral("name"), name);

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("config"), config},
            {QStringLiteral("created"), root.value(QStringLiteral("created")).toString()},
            {QStringLiteral("name"), name}};
}

QVariantMap ShellBuilder::deletePreset(const QString &name)
{
    if (!validName(name)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That preset name is not valid.")}};
    }

    const QString path = Paths::stateDir() + QStringLiteral("/shell-presets/") + name + QStringLiteral(".json");
    if (!QFileInfo::exists(path)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("That preset no longer exists.")}};
    }
    if (!QFile::remove(path)) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("The preset could not be deleted.")}};
    }
    return {{QStringLiteral("ok"), true}, {QStringLiteral("name"), name}};
}

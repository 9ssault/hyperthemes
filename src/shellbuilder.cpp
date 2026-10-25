#include "shellbuilder.h"

#include "palette.h"
#include "paths.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVector>

namespace {

struct Config {
    QString name = QStringLiteral("hyperthemes");
    QString position = QStringLiteral("top");
    int barHeight = 34;
    int margin = 6;
    int radius = 10;
    qreal opacity = 0.94;
    bool launcher = true;
    bool dashboard = true;
    QStringList left;
    QStringList center;
    QStringList right;
    QVariantMap palette;
    QString themeName = QStringLiteral("HyperThemes");
};

QString qmlString(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString pad(const QString &text, int spaces)
{
    const QString prefix(spaces, QLatin1Char(' '));
    QString out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (!line.isEmpty())
            out += prefix;
        out += line;
        out += QLatin1Char('\n');
    }
    return out;
}

QString widgetQml(const QString &id)
{
    if (id == QLatin1String("logo")) {
        return QStringLiteral(
            "Rectangle {\n"
            "    implicitWidth: 24\n"
            "    implicitHeight: 24\n"
            "    radius: theme.radius\n"
            "    color: theme.primary\n"
            "    Text {\n"
            "        anchors.centerIn: parent\n"
            "        text: \"◆\"\n"
            "        color: theme.onPrimary\n"
            "        font.family: theme.fontFamily\n"
            "        font.pixelSize: 11\n"
            "    }\n"
            "}\n");
    }
    if (id == QLatin1String("workspaces")) {
        return QStringLiteral(
            "Row {\n"
            "    spacing: 6\n"
            "    // Wire this to your compositor's workspace service.\n"
            "    Repeater {\n"
            "        model: 5\n"
            "        Rectangle {\n"
            "            width: index === 0 ? 20 : 8\n"
            "            height: 8\n"
            "            radius: 4\n"
            "            color: index === 0 ? theme.primary : theme.surfaceHigh\n"
            "        }\n"
            "    }\n"
            "}\n");
    }
    if (id == QLatin1String("launcher")) {
        return QStringLiteral(
            "Rectangle {\n"
            "    implicitWidth: 74\n"
            "    implicitHeight: 24\n"
            "    radius: theme.radius\n"
            "    color: theme.surfaceHigh\n"
            "    Text {\n"
            "        anchors.centerIn: parent\n"
            "        text: \"Search\"\n"
            "        color: theme.muted\n"
            "        font.family: theme.fontFamily\n"
            "        font.pixelSize: 11\n"
            "    }\n"
            "    TapHandler {\n"
            "        onTapped: bar.launcherRequested()\n"
            "    }\n"
            "}\n");
    }
    if (id == QLatin1String("clock")) {
        return QStringLiteral(
            "Text {\n"
            "    id: clockLabel\n"
            "    property var now: new Date()\n"
            "    text: Qt.formatDateTime(now, \"HH:mm\")\n"
            "    color: theme.text\n"
            "    font.family: theme.fontFamily\n"
            "    font.pixelSize: 12\n"
            "    Timer {\n"
            "        interval: 1000\n"
            "        running: true\n"
            "        repeat: true\n"
            "        onTriggered: clockLabel.now = new Date()\n"
            "    }\n"
            "    TapHandler {\n"
            "        onTapped: bar.dashboardRequested()\n"
            "    }\n"
            "}\n");
    }
    if (id == QLatin1String("media")) {
        return QStringLiteral(
            "RowLayout {\n"
            "    readonly property var players: Mpris.players.values\n"
            "    spacing: 6\n"
            "    visible: players.length > 0\n"
            "    Text {\n"
            "        text: players.length > 0 && players[0].trackTitle ? players[0].trackTitle : \"\"\n"
            "        color: theme.muted\n"
            "        font.family: theme.fontFamily\n"
            "        font.pixelSize: 11\n"
            "        elide: Text.ElideRight\n"
            "        Layout.maximumWidth: 180\n"
            "    }\n"
            "    TapHandler {\n"
            "        onTapped: players.length > 0 && players[0].togglePlaying()\n"
            "    }\n"
            "}\n");
    }
    if (id == QLatin1String("tray")) {
        return QStringLiteral(
            "RowLayout {\n"
            "    spacing: 6\n"
            "    Repeater {\n"
            "        model: SystemTray.items.values\n"
            "        Image {\n"
            "            required property var modelData\n"
            "            source: Quickshell.iconPath(modelData.icon)\n"
            "            sourceSize.width: 16\n"
            "            sourceSize.height: 16\n"
            "            Layout.preferredWidth: 16\n"
            "            Layout.preferredHeight: 16\n"
            "        }\n"
            "    }\n"
            "}\n");
    }
    if (id == QLatin1String("battery")) {
        return QStringLiteral(
            "Text {\n"
            "    visible: UPower.displayDevice.isLaptopBattery\n"
            "    text: Math.round(UPower.displayDevice.percentage * 100) + \"%\"\n"
            "    color: theme.muted\n"
            "    font.family: theme.fontFamily\n"
            "    font.pixelSize: 11\n"
            "}\n");
    }
    if (id == QLatin1String("statusIcons")) {
        return QStringLiteral(
            "Row {\n"
            "    spacing: 8\n"
            "    // Replace these glyphs with your network, audio and notification services.\n"
            "    Text {\n"
            "        text: \"◍\"\n"
            "        color: theme.muted\n"
            "        font.family: theme.fontFamily\n"
            "        font.pixelSize: 12\n"
            "    }\n"
            "    Text {\n"
            "        text: \"◔\"\n"
            "        color: theme.muted\n"
            "        font.family: theme.fontFamily\n"
            "        font.pixelSize: 12\n"
            "    }\n"
            "}\n");
    }
    if (id == QLatin1String("power")) {
        return QStringLiteral(
            "Rectangle {\n"
            "    implicitWidth: 26\n"
            "    implicitHeight: 24\n"
            "    radius: theme.radius\n"
            "    color: theme.surfaceHigh\n"
            "    Text {\n"
            "        anchors.centerIn: parent\n"
            "        text: \"⏻\"\n"
            "        color: theme.text\n"
            "        font.family: theme.fontFamily\n"
            "        font.pixelSize: 12\n"
            "    }\n"
            "    TapHandler {\n"
            "        onTapped: bar.dashboardRequested()\n"
            "    }\n"
            "}\n");
    }
    return {};
}

QString zoneQml(const QStringList &ids)
{
    QString out;
    for (const QString &id : ids) {
        const QString snippet = widgetQml(id);
        if (!snippet.isEmpty())
            out += snippet + QLatin1Char('\n');
    }
    return out;
}

QString colour(const QVariantMap &palette, const QString &role)
{
    return Palette::withHash(Palette::hex(palette, role));
}

QString themeFile(const Config &config)
{
    const QVariantMap palette = Palette::normalise(config.palette);
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("// Palette source: ") + config.themeName + QLatin1Char('\n');
    text += QStringLiteral("import QtQuick\n\n");
    text += QStringLiteral("QtObject {\n");
    text += QStringLiteral("    readonly property string sourceName: ") + qmlString(config.themeName) + QLatin1Char('\n');
    text += QStringLiteral("\n");
    text += QStringLiteral("    readonly property color background: ") + qmlString(colour(palette, QStringLiteral("background"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color surface: ") + qmlString(colour(palette, QStringLiteral("surfaceContainer"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color surfaceHigh: ") + qmlString(colour(palette, QStringLiteral("surfaceContainerHigh"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color text: ") + qmlString(colour(palette, QStringLiteral("onSurface"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color muted: ") + qmlString(colour(palette, QStringLiteral("onSurfaceVariant"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color faint: ") + qmlString(colour(palette, QStringLiteral("outline"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color primary: ") + qmlString(colour(palette, QStringLiteral("primary"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color onPrimary: ") + qmlString(colour(palette, QStringLiteral("onPrimary"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color secondary: ") + qmlString(colour(palette, QStringLiteral("secondary"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color error: ") + qmlString(colour(palette, QStringLiteral("error"))) + QLatin1Char('\n');
    text += QStringLiteral("    readonly property color outline: ") + qmlString(colour(palette, QStringLiteral("outlineVariant"))) + QLatin1Char('\n');
    text += QStringLiteral("\n");
    text += QStringLiteral("    readonly property int barHeight: %1\n").arg(config.barHeight);
    text += QStringLiteral("    readonly property int margin: %1\n").arg(config.margin);
    text += QStringLiteral("    readonly property int radius: %1\n").arg(config.radius);
    text += QStringLiteral("    readonly property int gap: 10\n");
    text += QStringLiteral("    readonly property real opacity: %1\n").arg(config.opacity, 0, 'f', 2);
    text += QStringLiteral("    readonly property string fontFamily: Qt.application.font.family\n");
    text += QStringLiteral("}\n");
    return text;
}

QString barFile(const Config &config)
{
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("import QtQuick\n");
    text += QStringLiteral("import QtQuick.Layouts\n");
    text += QStringLiteral("import Quickshell\n");
    text += QStringLiteral("import Quickshell.Wayland\n");
    text += QStringLiteral("import Quickshell.Services.SystemTray\n");
    text += QStringLiteral("import Quickshell.Services.Mpris\n");
    text += QStringLiteral("import Quickshell.Services.UPower\n\n");
    text += QStringLiteral("PanelWindow {\n");
    text += QStringLiteral("    id: bar\n\n");
    text += QStringLiteral("    required property var theme\n");
    text += QStringLiteral("    signal launcherRequested()\n");
    text += QStringLiteral("    signal dashboardRequested()\n\n");
    text += QStringLiteral("    // Use Variants over Quickshell.screens for one bar per monitor.\n");
    text += QStringLiteral("    screen: Quickshell.screens.length > 0 ? Quickshell.screens[0] : null\n");
    text += QStringLiteral("    anchors {\n");
    text += QStringLiteral("        %1: true\n").arg(config.position == QLatin1String("bottom")
                                                        ? QStringLiteral("bottom")
                                                        : QStringLiteral("top"));
    text += QStringLiteral("        left: true\n");
    text += QStringLiteral("        right: true\n");
    text += QStringLiteral("    }\n");
    text += QStringLiteral("    implicitHeight: theme.barHeight\n");
    text += QStringLiteral("    exclusiveZone: theme.barHeight\n");
    text += QStringLiteral("    color: Qt.rgba(theme.background.r, theme.background.g, theme.background.b, theme.opacity)\n");
    text += QStringLiteral("    WlrLayershell.namespace: \"hyperthemes-bar\"\n\n");
    text += QStringLiteral("    RowLayout {\n");
    text += QStringLiteral("        anchors.fill: parent\n");
    text += QStringLiteral("        anchors.leftMargin: theme.margin + 4\n");
    text += QStringLiteral("        anchors.rightMargin: theme.margin + 4\n");
    text += QStringLiteral("        spacing: theme.gap\n\n");
    text += pad(zoneQml(config.left), 8);
    text += QStringLiteral("        Item { Layout.fillWidth: true }\n\n");
    text += pad(zoneQml(config.center), 8);
    text += QStringLiteral("        Item { Layout.fillWidth: true }\n\n");
    text += pad(zoneQml(config.right), 8);
    text += QStringLiteral("    }\n");
    text += QStringLiteral("}\n");
    return text;
}

QString launcherFile(const Config &config)
{
    Q_UNUSED(config);
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("import QtQuick\n");
    text += QStringLiteral("import QtQuick.Controls\n");
    text += QStringLiteral("import QtQuick.Layouts\n");
    text += QStringLiteral("import Quickshell\n");
    text += QStringLiteral("import Quickshell.Wayland\n\n");
    text += QStringLiteral("PanelWindow {\n");
    text += QStringLiteral("    id: launcher\n\n");
    text += QStringLiteral("    required property var theme\n");
    text += QStringLiteral("    visible: false\n");
    text += QStringLiteral("    color: \"transparent\"\n");
    text += QStringLiteral("    exclusionMode: ExclusionMode.Ignore\n");
    text += QStringLiteral("    WlrLayershell.namespace: \"hyperthemes-launcher\"\n");
    text += QStringLiteral("    WlrLayershell.layer: WlrLayer.Overlay\n");
    text += QStringLiteral("    WlrLayershell.keyboardFocus: WlrKeyboardFocus.OnDemand\n");
    text += QStringLiteral("    anchors {\n");
    text += QStringLiteral("        top: true\n        bottom: true\n        left: true\n        right: true\n    }\n\n");
    text += QStringLiteral("    function toggle() {\n");
    text += QStringLiteral("        visible = !visible;\n");
    text += QStringLiteral("        if (visible) {\n");
    text += QStringLiteral("            search.text = \"\";\n");
    text += QStringLiteral("            search.forceActiveFocus();\n");
    text += QStringLiteral("        }\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    MouseArea {\n");
    text += QStringLiteral("        anchors.fill: parent\n");
    text += QStringLiteral("        onClicked: launcher.visible = false\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    Rectangle {\n");
    text += QStringLiteral("        id: panel\n");
    text += QStringLiteral("        anchors.horizontalCenter: parent.horizontalCenter\n");
    text += QStringLiteral("        y: Math.round(parent.height * 0.18)\n");
    text += QStringLiteral("        width: Math.min(parent.width - 64, 560)\n");
    text += QStringLiteral("        height: 420\n");
    text += QStringLiteral("        radius: theme.radius * 2\n");
    text += QStringLiteral("        color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, Math.max(0.94, theme.opacity))\n");
    text += QStringLiteral("        border.width: 1\n");
    text += QStringLiteral("        border.color: theme.surfaceHigh\n\n");
    text += QStringLiteral("        ColumnLayout {\n");
    text += QStringLiteral("            anchors.fill: parent\n");
    text += QStringLiteral("            anchors.margins: 16\n");
    text += QStringLiteral("            spacing: 10\n\n");
    text += QStringLiteral("            TextField {\n");
    text += QStringLiteral("                id: search\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                placeholderText: \"Search applications…\"\n");
    text += QStringLiteral("                color: theme.text\n");
    text += QStringLiteral("                font.family: theme.fontFamily\n");
    text += QStringLiteral("                background: Rectangle {\n");
    text += QStringLiteral("                    color: theme.surfaceHigh\n");
    text += QStringLiteral("                    radius: theme.radius\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("                Keys.onEscapePressed: launcher.visible = false\n");
    text += QStringLiteral("                Keys.onReturnPressed: {\n");
    text += QStringLiteral("                    if (results.count > 0) {\n");
    text += QStringLiteral("                        results.itemAt(0).modelData.execute();\n");
    text += QStringLiteral("                        launcher.visible = false;\n");
    text += QStringLiteral("                    }\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            ListView {\n");
    text += QStringLiteral("                id: results\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                Layout.fillHeight: true\n");
    text += QStringLiteral("                clip: true\n");
    text += QStringLiteral("                model: DesktopEntries.applications.values.filter(app => !app.noDisplay\n");
    text += QStringLiteral("                    && app.name.toLowerCase().includes(search.text.toLowerCase()))\n");
    text += QStringLiteral("                delegate: Rectangle {\n");
    text += QStringLiteral("                    required property var modelData\n");
    text += QStringLiteral("                    width: results.width\n");
    text += QStringLiteral("                    height: 34\n");
    text += QStringLiteral("                    radius: theme.radius\n");
    text += QStringLiteral("                    color: hover.hovered ? theme.surfaceHigh : \"transparent\"\n");
    text += QStringLiteral("                    RowLayout {\n");
    text += QStringLiteral("                        anchors.fill: parent\n");
    text += QStringLiteral("                        anchors.margins: 6\n");
    text += QStringLiteral("                        Image {\n");
    text += QStringLiteral("                            source: Quickshell.iconPath(modelData.icon)\n");
    text += QStringLiteral("                            Layout.preferredWidth: 18\n");
    text += QStringLiteral("                            Layout.preferredHeight: 18\n");
    text += QStringLiteral("                        }\n");
    text += QStringLiteral("                        Text {\n");
    text += QStringLiteral("                            Layout.fillWidth: true\n");
    text += QStringLiteral("                            text: modelData.name\n");
    text += QStringLiteral("                            color: theme.text\n");
    text += QStringLiteral("                            font.family: theme.fontFamily\n");
    text += QStringLiteral("                            font.pixelSize: 12\n");
    text += QStringLiteral("                            elide: Text.ElideRight\n");
    text += QStringLiteral("                        }\n");
    text += QStringLiteral("                    }\n");
    text += QStringLiteral("                    HoverHandler { id: hover }\n");
    text += QStringLiteral("                    TapHandler {\n");
    text += QStringLiteral("                        onTapped: {\n");
    text += QStringLiteral("                            modelData.execute();\n");
    text += QStringLiteral("                            launcher.visible = false;\n");
    text += QStringLiteral("                        }\n");
    text += QStringLiteral("                    }\n");
    text += QStringLiteral("                }\n");
    text += QStringLiteral("            }\n");
    text += QStringLiteral("        }\n");
    text += QStringLiteral("    }\n");
    text += QStringLiteral("}\n");
    return text;
}

QString powerButton(const QString &label, const QString &command)
{
    return QStringLiteral(
               "Rectangle {\n"
               "    implicitWidth: label.implicitWidth + 26\n"
               "    implicitHeight: 30\n"
               "    radius: theme.radius\n"
               "    color: theme.surfaceHigh\n"
               "    Text {\n"
               "        id: label\n"
               "        anchors.centerIn: parent\n"
               "        text: %1\n"
               "        color: theme.text\n"
               "        font.family: theme.fontFamily\n"
               "        font.pixelSize: 11\n"
               "    }\n"
               "    TapHandler {\n"
               "        onTapped: Quickshell.execDetached(%2)\n"
               "    }\n"
               "}\n")
        .arg(qmlString(label), command);
}

QString dashboardFile(const Config &config)
{
    Q_UNUSED(config);
    QString text;
    text += QStringLiteral("// Generated by HyperThemes for Quickshell.\n");
    text += QStringLiteral("import QtQuick\n");
    text += QStringLiteral("import QtQuick.Layouts\n");
    text += QStringLiteral("import Quickshell\n");
    text += QStringLiteral("import Quickshell.Wayland\n");
    text += QStringLiteral("import Quickshell.Services.Mpris\n");
    text += QStringLiteral("import Quickshell.Services.UPower\n\n");
    text += QStringLiteral("PanelWindow {\n");
    text += QStringLiteral("    id: dashboard\n\n");
    text += QStringLiteral("    required property var theme\n");
    text += QStringLiteral("    visible: false\n");
    text += QStringLiteral("    color: \"transparent\"\n");
    text += QStringLiteral("    exclusionMode: ExclusionMode.Ignore\n");
    text += QStringLiteral("    WlrLayershell.namespace: \"hyperthemes-dashboard\"\n");
    text += QStringLiteral("    WlrLayershell.layer: WlrLayer.Overlay\n");
    text += QStringLiteral("    WlrLayershell.keyboardFocus: WlrKeyboardFocus.OnDemand\n");
    text += QStringLiteral("    anchors {\n");
    text += QStringLiteral("        top: true\n        bottom: true\n        left: true\n        right: true\n    }\n\n");
    text += QStringLiteral("    readonly property var players: Mpris.players.values\n\n");
    text += QStringLiteral("    function toggle() {\n");
    text += QStringLiteral("        visible = !visible;\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    MouseArea {\n");
    text += QStringLiteral("        anchors.fill: parent\n");
    text += QStringLiteral("        onClicked: dashboard.visible = false\n");
    text += QStringLiteral("    }\n\n");
    text += QStringLiteral("    Rectangle {\n");
    text += QStringLiteral("        anchors.centerIn: parent\n");
    text += QStringLiteral("        width: Math.min(parent.width - 64, 560)\n");
    text += QStringLiteral("        height: 300\n");
    text += QStringLiteral("        radius: theme.radius * 2\n");
    text += QStringLiteral("        color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, Math.max(0.94, theme.opacity))\n");
    text += QStringLiteral("        border.width: 1\n");
    text += QStringLiteral("        border.color: theme.surfaceHigh\n\n");
    text += QStringLiteral("        ColumnLayout {\n");
    text += QStringLiteral("            anchors.fill: parent\n");
    text += QStringLiteral("            anchors.margins: 22\n");
    text += QStringLiteral("            spacing: 14\n\n");
    text += QStringLiteral("            Text {\n");
    text += QStringLiteral("                text: Qt.formatDateTime(new Date(), \"dddd, d MMMM · HH:mm\")\n");
    text += QStringLiteral("                color: theme.text\n");
    text += QStringLiteral("                font.family: theme.fontFamily\n");
    text += QStringLiteral("                font.pixelSize: 24\n");
    text += QStringLiteral("                font.weight: Font.DemiBold\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            Text {\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                text: players.length > 0 && players[0].trackTitle\n");
    text += QStringLiteral("                    ? players[0].trackArtist + \" · \" + players[0].trackTitle\n");
    text += QStringLiteral("                    : \"Nothing playing\"\n");
    text += QStringLiteral("                color: theme.muted\n");
    text += QStringLiteral("                font.family: theme.fontFamily\n");
    text += QStringLiteral("                font.pixelSize: 12\n");
    text += QStringLiteral("                elide: Text.ElideRight\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            Text {\n");
    text += QStringLiteral("                text: UPower.displayDevice.isLaptopBattery\n");
    text += QStringLiteral("                    ? \"Battery \" + Math.round(UPower.displayDevice.percentage * 100) + \"%\"\n");
    text += QStringLiteral("                    : \"Connected to power\"\n");
    text += QStringLiteral("                color: theme.muted\n");
    text += QStringLiteral("                font.family: theme.fontFamily\n");
    text += QStringLiteral("                font.pixelSize: 12\n");
    text += QStringLiteral("            }\n\n");
    text += QStringLiteral("            Item { Layout.fillHeight: true }\n\n");
    text += QStringLiteral("            RowLayout {\n");
    text += QStringLiteral("                Layout.fillWidth: true\n");
    text += QStringLiteral("                spacing: 8\n\n");
    text += pad(powerButton(QStringLiteral("Lock"),
                           QStringLiteral("[\"loginctl\", \"lock-session\"]")),
                12);
    text += pad(powerButton(QStringLiteral("Log out"),
                           QStringLiteral("[\"loginctl\", \"terminate-user\", Quickshell.env(\"USER\")]")),
                12);
    text += pad(powerButton(QStringLiteral("Reboot"), QStringLiteral("[\"systemctl\", \"reboot\"]")), 12);
    text += pad(powerButton(QStringLiteral("Power off"), QStringLiteral("[\"systemctl\", \"poweroff\"]")), 12);
    text += QStringLiteral("                Item { Layout.fillWidth: true }\n");
    text += QStringLiteral("            }\n");
    text += QStringLiteral("        }\n");
    text += QStringLiteral("    }\n");
    text += QStringLiteral("}\n");
    return text;
}

QString shellFile(const Config &config)
{
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
    text += QStringLiteral("\n");
    text += QStringLiteral("ShellRoot {\n");
    text += QStringLiteral("    Theme { id: theme }\n\n");
    if (config.launcher)
        text += QStringLiteral("    Launcher { id: launcher; theme: theme }\n");
    if (config.dashboard)
        text += QStringLiteral("    Dashboard { id: dashboard; theme: theme }\n");
    text += QStringLiteral("    Bar {\n");
    text += QStringLiteral("        theme: theme\n");
    if (config.launcher)
        text += QStringLiteral("        onLauncherRequested: launcher.toggle()\n");
    if (config.dashboard)
        text += QStringLiteral("        onDashboardRequested: dashboard.toggle()\n");
    text += QStringLiteral("    }\n");
    text += QStringLiteral("}\n");
    return text;
}

QString readmeFile(const Config &config)
{
    QString text;
    text += QStringLiteral("# %1 — Quickshell config\n\n").arg(config.name);
    text += QStringLiteral("Generated by HyperThemes from the **%1** palette.\n\n").arg(config.themeName);
    text += QStringLiteral("```bash\nqs -c %1\n```\n\n").arg(config.name);
    text += QStringLiteral("## What is here\n\n");
    text += QStringLiteral("- `shell.qml` — entry point (ShellRoot) that loads the modules below.\n");
    text += QStringLiteral("- `Theme.qml` — colours, sizes and opacity taken from the palette. Edit freely.\n");
    text += QStringLiteral("- `bar/Bar.qml` — the %1 panel.\n").arg(config.position);
    if (config.launcher)
        text += QStringLiteral("- `launcher/Launcher.qml` — application launcher opened from the bar.\n");
    if (config.dashboard)
        text += QStringLiteral("- `dashboard/Dashboard.qml` — clock, media, battery and power actions.\n");
    text += QStringLiteral("\n## Notes\n\n");
    text += QStringLiteral("- The bar targets the first screen. Wrap `Bar` in `Variants { model: Quickshell.screens }` for one bar per monitor.\n");
    text += QStringLiteral("- Workspaces and status icons are placeholders; connect them to your compositor's services.\n");
    text += QStringLiteral("- Every rewrite made by HyperThemes backs up the previous folder under `~/.local/state/hyperthemes/shell-backups/`.\n");
    return text;
}

QStringList knownWidgets()
{
    return {
        QStringLiteral("logo"), QStringLiteral("workspaces"), QStringLiteral("launcher"),
        QStringLiteral("clock"), QStringLiteral("media"), QStringLiteral("tray"),
        QStringLiteral("battery"), QStringLiteral("statusIcons"), QStringLiteral("power"),
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
    config.launcher = map.value(QStringLiteral("launcher"), true).toBool();
    config.dashboard = map.value(QStringLiteral("dashboard"), true).toBool();
    config.palette = map.value(QStringLiteral("palette")).toMap();
    config.themeName = map.value(QStringLiteral("themeName"), QStringLiteral("HyperThemes")).toString();

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

} // namespace

ShellBuilder::ShellBuilder(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(Paths::stateDir() + QStringLiteral("/shell-backups"));
}

QVariantList ShellBuilder::widgets() const
{
    const QVector<QPair<QString, QString>> list = {
        {QStringLiteral("logo"), QStringLiteral("Logo")},
        {QStringLiteral("workspaces"), QStringLiteral("Workspaces")},
        {QStringLiteral("launcher"), QStringLiteral("Launcher")},
        {QStringLiteral("clock"), QStringLiteral("Clock")},
        {QStringLiteral("media"), QStringLiteral("Media")},
        {QStringLiteral("tray"), QStringLiteral("System tray")},
        {QStringLiteral("battery"), QStringLiteral("Battery")},
        {QStringLiteral("statusIcons"), QStringLiteral("Status icons")},
        {QStringLiteral("power"), QStringLiteral("Power")},
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
    return {
        {QStringLiteral("name"), QStringLiteral("hyperthemes")},
        {QStringLiteral("position"), QStringLiteral("top")},
        {QStringLiteral("barHeight"), 34},
        {QStringLiteral("margin"), 6},
        {QStringLiteral("radius"), 10},
        {QStringLiteral("opacity"), 0.94},
        {QStringLiteral("launcher"), true},
        {QStringLiteral("dashboard"), true},
        {QStringLiteral("zones"), QVariantMap{
             {QStringLiteral("left"), QStringList{QStringLiteral("logo"), QStringLiteral("workspaces")}},
             {QStringLiteral("center"), QStringList{QStringLiteral("clock")}},
             {QStringLiteral("right"), QStringList{
                  QStringLiteral("tray"), QStringLiteral("media"),
                  QStringLiteral("battery"), QStringLiteral("power")}},
         }},
    };
}

QString ShellBuilder::configDir(const QString &name) const
{
    return Paths::configHome() + QStringLiteral("/quickshell/") + name;
}

bool ShellBuilder::quickshellAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("qs")).isEmpty()
        || !QStandardPaths::findExecutable(QStringLiteral("quickshell")).isEmpty();
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

    if (!QRegularExpression(QStringLiteral("^[a-z0-9][a-z0-9._-]{0,31}$")).match(parsed.name).hasMatch()) {
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

    QStringList written;
    for (const auto &file : files) {
        QString error;
        if (!writeFile(dir + QLatin1Char('/') + file.first, file.second, &error))
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"), error},
                    {QStringLiteral("backup"), backup}};
        written << file.first;
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("dir"), dir},
            {QStringLiteral("files"), written},
            {QStringLiteral("backup"), backup},
            {QStringLiteral("quickshell"), quickshellAvailable()}};
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

QVariantMap ShellBuilder::restore(const QString &backupId)
{
    if (backupId.isEmpty())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("No backup chosen.")}};

    const QString backupDir = Paths::stateDir() + QStringLiteral("/shell-backups/") + backupId;
    QFile manifestFile(backupDir + QStringLiteral("/manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly))
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("That backup no longer exists.")}};

    const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const QString dir = manifest.value(QStringLiteral("dir")).toString();
    const bool existed = manifest.value(QStringLiteral("existed")).toBool();
    if (dir.isEmpty())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("The backup manifest is incomplete.")}};

    QDir(dir).removeRecursively();
    QStringList warnings;
    if (existed) {
        QString error;
        if (!copyTree(backupDir + QStringLiteral("/files"), dir, &error))
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
        warnings << QStringLiteral("The previous Quickshell config was restored.");
    } else {
        warnings << QStringLiteral("The generated config was removed.");
    }

    return {{QStringLiteral("ok"), true},
            {QStringLiteral("dir"), dir},
            {QStringLiteral("warnings"), warnings}};
}

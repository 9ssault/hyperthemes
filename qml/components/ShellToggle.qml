import QtQuick

ThemeSwitch {
    id: control

    property bool value: false

    signal changed(bool value)

    onToggled: control.changed(checked)

    Binding {
        target: control
        property: "checked"
        value: control.value
        restoreMode: Binding.RestoreNone
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    anchors.fill: parent
    clip: true

    function commit() {
        appSettings.mobileConnectListenIp = listenIpField.text.trim()
        appSettings.mobileConnectPort = portBox.value
        appSettings.mobileConnectEnabled = enabledSwitch.checked
        reload()
        return true
    }

    function reload() {
        enabledSwitch.checked = appSettings.mobileConnectEnabled
        listenIpField.text = appSettings.mobileConnectListenIp
        portBox.value = appSettings.mobileConnectPort
    }

    GridLayout {
        width: root.availableWidth
        columns: width >= 460 ? 2 : 1
        rowSpacing: 10
        columnSpacing: 16

        Label {
            text: qsTr("Enable Server")
        }

        Switch {
            id: enabledSwitch
            checked: appSettings.mobileConnectEnabled
            text: checked ? qsTr("On") : qsTr("Off")
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Accept transfers from the Android companion app over your local network. The server starts on app launch only when this is enabled.")
            color: palette.mid
        }

        Label {
            text: qsTr("Listen IP")
        }

        TextField {
            id: listenIpField
            Layout.fillWidth: true
            placeholderText: "0.0.0.0"
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Use 0.0.0.0 to listen on all local interfaces, or enter a specific local IPv4 address.")
            color: palette.mid
        }

        Label {
            text: qsTr("Port")
        }

        SpinBox {
            id: portBox
            from: 1024
            to: 65535
            value: appSettings.mobileConnectPort
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Default port is 45437. Changes take effect when you press OK.")
            color: palette.mid
        }

        Item {
            Layout.fillHeight: true
        }
    }

    Component.onCompleted: reload()
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    anchors.centerIn: parent
    modal: true
    focus: true
    title: qsTr("Settings")
    width: Math.min(parent ? parent.width - 80 : 760, 760)
    height: Math.min(parent ? parent.height - 80 : 620, 620)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        TabBar {
            id: tabBar
            Layout.fillWidth: true

            TabButton { text: qsTr("System") }
            TabButton { text: qsTr("Mobile Connect") }
            TabButton { text: qsTr("Log") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            Item {
                SystemSettings {
                    id: systemSettings
                    anchors.fill: parent
                }
            }

            Item {
                MobileConnectSettings {
                    id: mobileConnectSettings
                    anchors.fill: parent
                }
            }

            Item {
                LogSettings {
                    id: logSettings
                    anchors.fill: parent
                }
            }
        }

        DialogButtonBox {
            Layout.fillWidth: true
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel

            onAccepted: {
                if (!systemSettings.commit()) {
                    return
                }
                mobileConnectSettings.commit()
                logSettings.commit()
                root.close()
            }

            onRejected: {
                systemSettings.reload()
                mobileConnectSettings.reload()
                logSettings.reload()
                root.close()
            }
        }
    }

    onOpened: {
        systemSettings.reload()
        mobileConnectSettings.reload()
        logSettings.reload()
    }
}

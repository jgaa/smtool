import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    anchors.centerIn: parent
    modal: true
    focus: true
    title: qsTr("About %1").arg(appInfo.applicationName)
    standardButtons: Dialog.Ok
    width: Math.min(parent ? parent.width - 80 : 560, 560)
    height: Math.min(parent ? parent.height - 80 : implicitHeight, 560)
    padding: 16

    contentItem: ScrollView {
        id: scrollView
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        contentWidth: availableWidth

        Column {
            width: scrollView.availableWidth
            spacing: 16

            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                text: appInfo.description
            }

            GridLayout {
                width: parent.width
                columns: 2
                columnSpacing: 16
                rowSpacing: 8

                Label { text: qsTr("App") }
                Label { text: appInfo.applicationName }

                Label { text: qsTr("Version") }
                Label { text: appInfo.applicationVersion }

                Label { text: qsTr("Qt") }
                Label { text: appInfo.qtVersion }

                Label { text: qsTr("Components") }
                Label {
                    text: appInfo.components.join(", ")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                color: palette.mid
                text: qsTr("SmTool helps collect raw ideas, organize content work, generate derivative bursts, and track items through a simple publishing workflow.")
            }
        }
    }
}

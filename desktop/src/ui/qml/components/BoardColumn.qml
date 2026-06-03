import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property string title
    required property var model

    property var statusOptions: [
        "inbox",
        "clarifying",
        "shaping",
        "drafting",
        "ready",
        "scheduled",
        "published",
        "reviewing",
        "archived"
    ]

    width: 260

    background: Rectangle {
        radius: 8
        color: "#fafafa"
        border.color: "#d7d7d7"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: root.title
            font.bold: true
            font.pixelSize: 16
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            spacing: 8

            delegate: Rectangle {
                width: ListView.view.width
                radius: 6
                color: "white"
                border.color: "#d5d5d5"
                implicitHeight: cardLayout.implicitHeight + 16

                ColumnLayout {
                    id: cardLayout
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Label {
                        text: title
                        wrapMode: Text.Wrap
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: [pillar, kind, series].filter(function(value) { return value.length > 0 }).join(" | ")
                        wrapMode: Text.Wrap
                        color: "#505050"
                        visible: text.length > 0
                        Layout.fillWidth: true
                    }

                    Label {
                        text: "Priority " + priority + (scheduledAt ? " | " + Qt.formatDateTime(scheduledAt, "yyyy-MM-dd hh:mm") : "")
                        color: "#505050"
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        ComboBox {
                            id: targetStatusBox
                            Layout.fillWidth: true
                            model: root.statusOptions
                            Component.onCompleted: currentIndex = find(status)
                        }

                        Button {
                            text: "Move"
                            onClicked: appController.moveContentToStatus(itemId, targetStatusBox.currentText)
                        }
                    }
                }
            }
        }
    }
}

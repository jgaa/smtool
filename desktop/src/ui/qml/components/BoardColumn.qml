import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    required property string columnTitle
    required property string statusKey
    required property string infoText
    required property var model
    required property var editDialog
    required property var dragLayer

    implicitWidth: 260
    implicitHeight: 720

    background: Rectangle {
        radius: 8
        color: dropArea.containsDrag ? "#eef7ee" : "#fafafa"
        border.color: dropArea.containsDrag ? "#2e7d32" : "#d7d7d7"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: root.columnTitle
                font.bold: true
                font.pixelSize: 16
                Layout.fillWidth: true
            }

            ToolButton {
                id: infoButton
                text: "i"
                font.bold: true
                onClicked: statusInfoPopup.open()
            }
        }

        Popup {
            id: statusInfoPopup
            parent: Overlay.overlay
            anchors.centerIn: Overlay.overlay
            width: Math.min(parent ? parent.width - 80 : 560, 560)
            height: Math.min(parent ? parent.height - 80 : 640, 640)
            modal: true
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            background: Rectangle {
                radius: 10
                color: "white"
                border.color: "#cfcfcf"
            }

            contentItem: ColumnLayout {
                spacing: 12

                Label {
                    text: root.columnTitle
                    font.bold: true
                    font.pixelSize: 18
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextEdit {
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        textFormat: TextEdit.MarkdownText
                        text: root.infoText
                        color: "#202020"
                        width: parent.width
                    }
                }
            }
        }

        DropArea {
            id: dropArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            onDropped: function(drop) {
                if (!drop.source || !drop.source.itemId || drop.source.currentStatus === root.statusKey) {
                    return
                }
                appController.moveContentToStatus(drop.source.itemId, root.statusKey)
            }

            ListView {
                anchors.fill: parent
                clip: true
                model: root.model
                spacing: 8

                delegate: Rectangle {
                    id: cardRoot
                    required property string itemId
                    required property string title
                    required property string descriptionPreview
                    required property string displayTags
                    required property string pillar
                    required property string kind
                    required property string series
                    required property int priority
                    required property date scheduledAt
                    required property string status
                    width: ListView.view.width
                    radius: 6
                    color: "white"
                    border.color: "#d5d5d5"
                    implicitHeight: cardLayout.implicitHeight + 16
                    property string currentStatus: status
                    property Item homeParent: null
                    property real homeX: 0
                    property real homeY: 0
                    Drag.active: dragHandler.active
                    Drag.source: cardRoot
                    Drag.hotSpot.x: width / 2
                    Drag.hotSpot.y: 20
                    z: dragHandler.active ? 10 : 0

                    DragHandler {
                        id: dragHandler
                        target: cardRoot

                        onActiveChanged: {
                            if (active) {
                                cardRoot.homeParent = cardRoot.parent
                                cardRoot.homeX = cardRoot.x
                                cardRoot.homeY = cardRoot.y
                                const point = cardRoot.mapToItem(root.dragLayer, 0, 0)
                                cardRoot.parent = root.dragLayer
                                cardRoot.x = point.x
                                cardRoot.y = point.y
                            } else if (cardRoot.homeParent) {
                                cardRoot.Drag.drop()
                                const point = cardRoot.mapToItem(cardRoot.homeParent, 0, 0)
                                cardRoot.parent = cardRoot.homeParent
                                cardRoot.x = point.x
                                cardRoot.y = point.y
                            }
                        }
                    }

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
                            text: descriptionPreview
                            visible: text.length > 0
                            wrapMode: Text.Wrap
                            color: "#505050"
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
                            text: displayTags
                            visible: text.length > 0
                            wrapMode: Text.Wrap
                            color: "#2f6f44"
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "Priority " + priority + (scheduledAt ? " | " + Qt.formatDate(scheduledAt, "yyyy-MM-dd") : "")
                            color: "#505050"
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            ToolButton {
                                icon.name: "document-edit"
                                text: "Edit"
                                display: AbstractButton.IconOnly
                                ToolTip.visible: hovered
                                ToolTip.text: text
                                onClicked: editDialog.openForEdit(itemId)
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            ToolButton {
                                icon.name: "archive-insert"
                                text: "Archive"
                                display: AbstractButton.IconOnly
                                ToolTip.visible: hovered
                                ToolTip.text: text
                                enabled: status !== "archived"
                                onClicked: appController.moveContentToStatus(itemId, "archived")
                            }
                        }
                    }
                }
            }
        }
    }
}

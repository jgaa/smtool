import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    title: qsTr("Edit Channels")
    width: Math.min(parent ? parent.width - 80 : 640, 640)
    height: Math.min(parent ? parent.height - 80 : 720, 720)

    property string errorText: ""

    function openManager() {
        reloadChannels(true)
        open()
    }

    function reloadChannels(clearError) {
        if (clearError === undefined || clearError) {
            errorText = ""
        }
        channelListModel.clear()
        const channels = appController.channelManagementItems()
        for (let index = 0; index < channels.length; ++index) {
            channelListModel.append(channels[index])
        }
    }

    function indexOfChannel(channelId) {
        for (let index = 0; index < channelListModel.count; ++index) {
            if (channelListModel.get(index).channelId === channelId) {
                return index
            }
        }
        return -1
    }

    function orderedChannelIds() {
        const ids = []
        for (let index = 0; index < channelListModel.count; ++index) {
            ids.push(channelListModel.get(index).channelId)
        }
        return ids
    }

    function reorderChannel(channelId, centerPoint) {
        const fromIndex = indexOfChannel(channelId)
        if (fromIndex < 0) {
            return
        }

        let toIndex = channelListView.indexAt(centerPoint.x, centerPoint.y)
        if (toIndex < 0) {
            toIndex = centerPoint.y <= 0 ? 0 : channelListModel.count - 1
        }
        if (toIndex < 0 || toIndex === fromIndex) {
            return
        }

        channelListModel.move(fromIndex, toIndex, 1)
    }

    function commitOrder() {
        errorText = ""
        if (appController.saveChannelOrder(orderedChannelIds())) {
            reloadChannels(true)
            return
        }

        errorText = appController.statusMessage
        reloadChannels(false)
    }

    function removeChannel(channelId) {
        errorText = ""
        if (appController.deleteChannel(channelId)) {
            reloadChannels(true)
            return
        }

        errorText = appController.statusMessage
    }

    ListModel {
        id: channelListModel
    }

    ChannelEditorDialog {
        id: channelEditorDialog
        onChannelSaved: root.reloadChannels(true)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Drag a row by its Move handle to reorder channels. Long-click a row to edit its key, display name, or active state.")
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Add New")
                onClicked: channelEditorDialog.openForCreate()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: channelListView
                width: parent.width
                height: parent.height
                model: channelListModel
                spacing: 8
                boundsBehavior: Flickable.StopAtBounds

                displaced: Transition {
                    NumberAnimation {
                        properties: "x,y"
                        duration: 120
                    }
                }

                delegate: Rectangle {
                    id: channelCard
                    required property string channelId
                    required property string key
                    required property string displayName
                    required property bool isActive
                    width: channelListView.width
                    radius: 8
                    color: moveArea.drag.active ? "#eef6ff" : "#ffffff"
                    border.width: 1
                    border.color: moveArea.drag.active ? "#4a79d9" : "#d7d7d7"
                    z: moveArea.drag.active ? 10 : 0
                    implicitHeight: channelLayout.implicitHeight + 18

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.WithinBounds
                        onLongPressed: channelEditorDialog.openForChannel(channelId)
                    }

                    RowLayout {
                        id: channelLayout
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 12

                        Rectangle {
                            id: moveHandle
                            Layout.preferredWidth: 44
                            Layout.fillHeight: true
                            radius: 6
                            color: moveArea.pressed ? "#d7e5ff" : "#edf3ff"
                            border.width: 1
                            border.color: "#9bb6ea"

                            Grid {
                                anchors.centerIn: parent
                                rows: 3
                                columns: 2
                                rowSpacing: 4
                                columnSpacing: 4

                                Repeater {
                                    model: 6

                                    delegate: Rectangle {
                                        width: 4
                                        height: 4
                                        radius: 2
                                        color: "#355892"
                                    }
                                }
                            }

                            MouseArea {
                                id: moveArea
                                anchors.fill: parent
                                cursorShape: Qt.OpenHandCursor
                                drag.target: channelCard
                                drag.axis: Drag.YAxis

                                onPressed: cursorShape = Qt.ClosedHandCursor
                                onReleased: {
                                    cursorShape = Qt.OpenHandCursor
                                    channelCard.x = 0
                                    channelCard.y = 0
                                    root.commitOrder()
                                }
                                onCanceled: {
                                    cursorShape = Qt.OpenHandCursor
                                    channelCard.x = 0
                                    channelCard.y = 0
                                    root.reloadChannels(true)
                                }
                                onPositionChanged: {
                                    const centerPoint = channelCard.mapToItem(channelListView.contentItem,
                                                                              channelCard.width / 2,
                                                                              channelCard.height / 2)
                                    root.reorderChannel(channelId, centerPoint)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                text: displayName
                                font.bold: true
                                wrapMode: Text.Wrap
                            }

                            Label {
                                Layout.fillWidth: true
                                color: "#5d5d5d"
                                text: key
                                wrapMode: Text.Wrap
                            }
                        }

                        Rectangle {
                            radius: 10
                            color: isActive ? "#e6f6ea" : "#f3f3f3"
                            border.width: 1
                            border.color: isActive ? "#9dd0aa" : "#d0d0d0"
                            implicitWidth: activeLabel.implicitWidth + 18
                            implicitHeight: activeLabel.implicitHeight + 8

                            Label {
                                id: activeLabel
                                anchors.centerIn: parent
                                text: isActive ? qsTr("Active") : qsTr("Inactive")
                                color: isActive ? "#1f6b35" : "#5d5d5d"
                            }
                        }

                        ToolButton {
                            icon.name: "edit-delete"
                            text: qsTr("Delete")
                            display: AbstractButton.IconOnly
                            ToolTip.visible: hovered
                            ToolTip.text: text
                            onClicked: root.removeChannel(channelId)
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            wrapMode: Text.Wrap
            color: "#b42318"
            text: root.errorText
        }

        DialogButtonBox {
            Layout.fillWidth: true
            standardButtons: DialogButtonBox.Close
            onRejected: root.close()
        }
    }
}

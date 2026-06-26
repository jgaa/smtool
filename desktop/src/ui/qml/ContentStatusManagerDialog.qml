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
    title: qsTr("Edit Statuses")
    width: Math.min(parent ? parent.width - 80 : 760, 760)
    height: Math.min(parent ? parent.height - 80 : 780, 780)

    property string errorText: ""

    function openManager() {
        reloadStatuses(true)
        open()
    }

    function reloadStatuses(clearError) {
        if (clearError === undefined || clearError) {
            errorText = ""
        }
        contentStatusListModel.clear()
        const statuses = appController.contentStatusManagementItems()
        for (let index = 0; index < statuses.length; ++index) {
            contentStatusListModel.append(statuses[index])
        }
    }

    function indexOfStatus(contentStatusId) {
        for (let index = 0; index < contentStatusListModel.count; ++index) {
            if (contentStatusListModel.get(index).contentStatusId === contentStatusId) {
                return index
            }
        }
        return -1
    }

    function orderedStatusIds() {
        const ids = []
        for (let index = 0; index < contentStatusListModel.count; ++index) {
            ids.push(contentStatusListModel.get(index).contentStatusId)
        }
        return ids
    }

    function reorderStatus(contentStatusId, centerPoint) {
        const fromIndex = indexOfStatus(contentStatusId)
        if (fromIndex < 0) {
            return
        }

        let toIndex = 0
        for (let index = 0; index < contentStatusListModel.count; ++index) {
            if (index === fromIndex) {
                continue
            }

            const item = contentStatusListView.itemAtIndex(index)
            if (item && centerPoint.y > item.y + item.height / 2) {
                toIndex += 1
            }
        }
        if (toIndex < 0 || toIndex === fromIndex) {
            return
        }

        contentStatusListModel.move(fromIndex, toIndex, 1)
    }

    function commitOrder() {
        errorText = ""
        if (appController.saveContentStatusOrder(orderedStatusIds())) {
            reloadStatuses(true)
            return
        }

        errorText = appController.statusMessage
        reloadStatuses(false)
    }

    function removeStatus(contentStatusId) {
        errorText = ""
        if (appController.deleteContentStatus(contentStatusId)) {
            reloadStatuses(true)
            return
        }

        errorText = appController.statusMessage
    }

    ListModel {
        id: contentStatusListModel
    }

    ContentStatusEditorDialog {
        id: contentStatusEditorDialog
        onContentStatusSaved: root.reloadStatuses(true)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Drag a row by its handle to reorder statuses. Long-click a row to edit its key or info text.")
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Add New")
                onClicked: contentStatusEditorDialog.openForCreate()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: contentStatusListView
                width: parent.width
                height: parent.height
                model: contentStatusListModel
                spacing: 8
                boundsBehavior: Flickable.StopAtBounds

                displaced: Transition {
                    NumberAnimation {
                        properties: "x,y"
                        duration: 120
                    }
                }

                delegate: Rectangle {
                    id: contentStatusCard
                    required property string contentStatusId
                    required property string key
                    required property string displayName
                    required property string info
                    required property bool isSystem
                    width: contentStatusListView.width
                    radius: 8
                    color: moveArea.drag.active ? "#eef6ff" : "#ffffff"
                    border.width: 1
                    border.color: moveArea.drag.active ? "#4a79d9" : "#d7d7d7"
                    z: moveArea.drag.active ? 10 : 0
                    implicitHeight: contentStatusLayout.implicitHeight + 18

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.WithinBounds
                        onLongPressed: contentStatusEditorDialog.openForContentStatus(contentStatusId)
                    }

                    RowLayout {
                        id: contentStatusLayout
                        anchors.fill: parent
                        anchors.margins: 9
                        spacing: 12

                        Rectangle {
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
                                drag.target: contentStatusCard
                                drag.axis: Drag.YAxis

                                onPressed: cursorShape = Qt.ClosedHandCursor
                                onReleased: {
                                    cursorShape = Qt.OpenHandCursor
                                    contentStatusCard.x = 0
                                    contentStatusCard.y = 0
                                    root.commitOrder()
                                }
                                onCanceled: {
                                    cursorShape = Qt.OpenHandCursor
                                    contentStatusCard.x = 0
                                    contentStatusCard.y = 0
                                    root.reloadStatuses(true)
                                }
                                onPositionChanged: {
                                    const centerPoint = contentStatusCard.mapToItem(contentStatusListView.contentItem,
                                                                                    contentStatusCard.width / 2,
                                                                                    contentStatusCard.height / 2)
                                    root.reorderStatus(contentStatusId, centerPoint)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    font.bold: true
                                    text: displayName
                                }

                                Rectangle {
                                    visible: isSystem
                                    radius: 999
                                    color: "#f4f1d1"
                                    border.width: 1
                                    border.color: "#d0c57d"
                                    implicitWidth: systemBadgeLabel.implicitWidth + 18
                                    implicitHeight: systemBadgeLabel.implicitHeight + 8

                                    Label {
                                        id: systemBadgeLabel
                                        anchors.centerIn: parent
                                        color: "#7a6500"
                                        text: qsTr("System")
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                color: "#666666"
                                text: key
                            }

                            Label {
                                Layout.fillWidth: true
                                color: "#444444"
                                wrapMode: Text.Wrap
                                text: info.length > 0 ? info : qsTr("No info text.")
                                maximumLineCount: 6
                                elide: Text.ElideRight
                            }
                        }

                        ToolButton {
                            Layout.alignment: Qt.AlignTop
                            text: qsTr("Delete")
                            display: AbstractButton.IconOnly
                            icon.name: "edit-delete"
                            onClicked: root.removeStatus(contentStatusId)
                            Accessible.name: qsTr("Delete Status")
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: errorText.length > 0
            color: "#b42318"
            wrapMode: Text.Wrap
            text: errorText
        }

        DialogButtonBox {
            Layout.fillWidth: true
            standardButtons: DialogButtonBox.Close
            onRejected: root.close()
        }
    }
}

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
    title: qsTr("Edit Content-Kind")
    width: Math.min(parent ? parent.width - 80 : 640, 640)
    height: Math.min(parent ? parent.height - 80 : 720, 720)

    property string errorText: ""

    function openManager() {
        reloadContentKinds(true)
        open()
    }

    function reloadContentKinds(clearError) {
        if (clearError === undefined || clearError) {
            errorText = ""
        }
        contentKindListModel.clear()
        const contentKinds = appController.contentKindManagementItems()
        for (let index = 0; index < contentKinds.length; ++index) {
            contentKindListModel.append(contentKinds[index])
        }
    }

    function indexOfContentKind(contentKindId) {
        for (let index = 0; index < contentKindListModel.count; ++index) {
            if (contentKindListModel.get(index).contentKindId === contentKindId) {
                return index
            }
        }
        return -1
    }

    function orderedContentKindIds() {
        const ids = []
        for (let index = 0; index < contentKindListModel.count; ++index) {
            ids.push(contentKindListModel.get(index).contentKindId)
        }
        return ids
    }

    function reorderContentKind(contentKindId, centerPoint) {
        const fromIndex = indexOfContentKind(contentKindId)
        if (fromIndex < 0) {
            return
        }

        let toIndex = contentKindListView.indexAt(centerPoint.x, centerPoint.y)
        if (toIndex < 0) {
            toIndex = centerPoint.y <= 0 ? 0 : contentKindListModel.count - 1
        }
        if (toIndex < 0 || toIndex === fromIndex) {
            return
        }

        contentKindListModel.move(fromIndex, toIndex, 1)
    }

    function commitOrder() {
        errorText = ""
        if (appController.saveContentKindOrder(orderedContentKindIds())) {
            reloadContentKinds(true)
            return
        }

        errorText = appController.statusMessage
        reloadContentKinds(false)
    }

    function removeContentKind(contentKindId) {
        errorText = ""
        if (appController.deleteContentKind(contentKindId)) {
            reloadContentKinds(true)
            return
        }

        errorText = appController.statusMessage
    }

    ListModel {
        id: contentKindListModel
    }

    ContentKindEditorDialog {
        id: contentKindEditorDialog
        onContentKindSaved: root.reloadContentKinds(true)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Drag a row by its handle to reorder content kinds. Long-click a row to edit its key, display name, or active state.")
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Add New")
                onClicked: contentKindEditorDialog.openForCreate()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: contentKindListView
                width: parent.width
                height: parent.height
                model: contentKindListModel
                spacing: 8
                boundsBehavior: Flickable.StopAtBounds

                displaced: Transition {
                    NumberAnimation {
                        properties: "x,y"
                        duration: 120
                    }
                }

                delegate: Rectangle {
                    id: contentKindCard
                    required property string contentKindId
                    required property string key
                    required property string displayName
                    required property bool isActive
                    width: contentKindListView.width
                    radius: 8
                    color: moveArea.drag.active ? "#eef6ff" : "#ffffff"
                    border.width: 1
                    border.color: moveArea.drag.active ? "#4a79d9" : "#d7d7d7"
                    z: moveArea.drag.active ? 10 : 0
                    implicitHeight: contentKindLayout.implicitHeight + 18

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.WithinBounds
                        onLongPressed: contentKindEditorDialog.openForContentKind(contentKindId)
                    }

                    RowLayout {
                        id: contentKindLayout
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
                                drag.target: contentKindCard
                                drag.axis: Drag.YAxis

                                onPressed: cursorShape = Qt.ClosedHandCursor
                                onReleased: {
                                    cursorShape = Qt.OpenHandCursor
                                    contentKindCard.x = 0
                                    contentKindCard.y = 0
                                    root.commitOrder()
                                }
                                onCanceled: {
                                    cursorShape = Qt.OpenHandCursor
                                    contentKindCard.x = 0
                                    contentKindCard.y = 0
                                    root.reloadContentKinds(true)
                                }
                                onPositionChanged: {
                                    const centerPoint = contentKindCard.mapToItem(contentKindListView.contentItem,
                                                                                  contentKindCard.width / 2,
                                                                                  contentKindCard.height / 2)
                                    root.reorderContentKind(contentKindId, centerPoint)
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                Layout.fillWidth: true
                                font.bold: true
                                text: displayName
                            }

                            Label {
                                Layout.fillWidth: true
                                color: "#666666"
                                text: key
                            }
                        }

                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter
                            radius: 999
                            color: isActive ? "#e7f6ec" : "#f2f2f2"
                            border.width: 1
                            border.color: isActive ? "#8cc79a" : "#cccccc"
                            implicitWidth: badgeLabel.implicitWidth + 18
                            implicitHeight: badgeLabel.implicitHeight + 8

                            Label {
                                id: badgeLabel
                                anchors.centerIn: parent
                                color: isActive ? "#24663a" : "#666666"
                                text: isActive ? qsTr("Active") : qsTr("Inactive")
                            }
                        }

                        ToolButton {
                            text: qsTr("Delete")
                            display: AbstractButton.IconOnly
                            icon.name: "edit-delete"
                            onClicked: root.removeContentKind(contentKindId)
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.errorText.length > 0
            color: "#b42318"
            wrapMode: Text.Wrap
            text: root.errorText
        }

        DialogButtonBox {
            Layout.fillWidth: true
            standardButtons: DialogButtonBox.Close
            onRejected: root.close()
        }
    }
}

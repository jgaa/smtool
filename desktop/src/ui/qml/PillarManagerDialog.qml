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
    title: qsTr("Edit Pillars")
    width: Math.min(parent ? parent.width - 80 : 640, 640)
    height: Math.min(parent ? parent.height - 80 : 720, 720)

    property string errorText: ""

    function openManager() {
        reloadPillars(true)
        open()
    }

    function reloadPillars(clearError) {
        if (clearError === undefined || clearError) {
            errorText = ""
        }
        pillarListModel.clear()
        const pillars = appController.pillarManagementItems()
        for (let index = 0; index < pillars.length; ++index) {
            pillarListModel.append(pillars[index])
        }
    }

    function indexOfPillar(pillarId) {
        for (let index = 0; index < pillarListModel.count; ++index) {
            if (pillarListModel.get(index).pillarId === pillarId) {
                return index
            }
        }
        return -1
    }

    function orderedPillarIds() {
        const ids = []
        for (let index = 0; index < pillarListModel.count; ++index) {
            ids.push(pillarListModel.get(index).pillarId)
        }
        return ids
    }

    function reorderPillar(pillarId, centerPoint) {
        const fromIndex = indexOfPillar(pillarId)
        if (fromIndex < 0) {
            return
        }

        let toIndex = 0
        for (let index = 0; index < pillarListModel.count; ++index) {
            if (index === fromIndex) {
                continue
            }

            const item = pillarListView.itemAtIndex(index)
            if (item && centerPoint.y > item.y + item.height / 2) {
                toIndex += 1
            }
        }
        if (toIndex < 0 || toIndex === fromIndex) {
            return
        }

        pillarListModel.move(fromIndex, toIndex, 1)
    }

    function commitOrder() {
        errorText = ""
        if (appController.savePillarOrder(orderedPillarIds())) {
            reloadPillars(true)
            return
        }

        errorText = appController.statusMessage
        reloadPillars(false)
    }

    function removePillar(pillarId) {
        errorText = ""
        if (appController.deletePillar(pillarId)) {
            reloadPillars(true)
            return
        }

        errorText = appController.statusMessage
    }

    ListModel {
        id: pillarListModel
    }

    PillarEditorDialog {
        id: pillarEditorDialog
        onPillarSaved: root.reloadPillars(true)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Drag a row by its handle to reorder pillars. Long-click a row to edit its key, display name, or active state.")
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Add New")
                onClicked: pillarEditorDialog.openForCreate()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: pillarListView
                width: parent.width
                height: parent.height
                model: pillarListModel
                spacing: 8
                boundsBehavior: Flickable.StopAtBounds

                displaced: Transition {
                    NumberAnimation {
                        properties: "x,y"
                        duration: 120
                    }
                }

                delegate: Rectangle {
                    id: pillarCard
                    required property string pillarId
                    required property string key
                    required property string displayName
                    required property bool isActive
                    width: pillarListView.width
                    radius: 8
                    color: moveArea.drag.active ? "#eef6ff" : "#ffffff"
                    border.width: 1
                    border.color: moveArea.drag.active ? "#4a79d9" : "#d7d7d7"
                    z: moveArea.drag.active ? 10 : 0
                    implicitHeight: pillarLayout.implicitHeight + 18

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.WithinBounds
                        onLongPressed: pillarEditorDialog.openForPillar(pillarId)
                    }

                    RowLayout {
                        id: pillarLayout
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
                                drag.target: pillarCard
                                drag.axis: Drag.YAxis

                                onPressed: cursorShape = Qt.ClosedHandCursor
                                onReleased: {
                                    cursorShape = Qt.OpenHandCursor
                                    pillarCard.x = 0
                                    pillarCard.y = 0
                                    root.commitOrder()
                                }
                                onCanceled: {
                                    cursorShape = Qt.OpenHandCursor
                                    pillarCard.x = 0
                                    pillarCard.y = 0
                                    root.reloadPillars(true)
                                }
                                onPositionChanged: {
                                    const centerPoint = pillarCard.mapToItem(pillarListView.contentItem,
                                                                             pillarCard.width / 2,
                                                                             pillarCard.height / 2)
                                    root.reorderPillar(pillarId, centerPoint)
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
                            onClicked: root.removePillar(pillarId)
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

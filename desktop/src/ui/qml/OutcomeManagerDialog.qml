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
    title: qsTr("Edit Outcomes")
    width: Math.min(parent ? parent.width - 80 : 640, 640)
    height: Math.min(parent ? parent.height - 80 : 720, 720)

    property string errorText: ""

    function openManager() {
        reloadOutcomes(true)
        open()
    }

    function reloadOutcomes(clearError) {
        if (clearError === undefined || clearError) {
            errorText = ""
        }
        outcomeListModel.clear()
        const outcomes = appController.outcomeManagementItems()
        for (let index = 0; index < outcomes.length; ++index) {
            outcomeListModel.append(outcomes[index])
        }
    }

    function indexOfOutcome(outcomeId) {
        for (let index = 0; index < outcomeListModel.count; ++index) {
            if (outcomeListModel.get(index).outcomeId === outcomeId) {
                return index
            }
        }
        return -1
    }

    function orderedOutcomeIds() {
        const ids = []
        for (let index = 0; index < outcomeListModel.count; ++index) {
            ids.push(outcomeListModel.get(index).outcomeId)
        }
        return ids
    }

    function reorderOutcome(outcomeId, centerPoint) {
        const fromIndex = indexOfOutcome(outcomeId)
        if (fromIndex < 0) {
            return
        }

        let toIndex = outcomeListView.indexAt(centerPoint.x, centerPoint.y)
        if (toIndex < 0) {
            toIndex = centerPoint.y <= 0 ? 0 : outcomeListModel.count - 1
        }
        if (toIndex < 0 || toIndex === fromIndex) {
            return
        }

        outcomeListModel.move(fromIndex, toIndex, 1)
    }

    function commitOrder() {
        errorText = ""
        if (appController.saveOutcomeOrder(orderedOutcomeIds())) {
            reloadOutcomes(true)
            return
        }

        errorText = appController.statusMessage
        reloadOutcomes(false)
    }

    function removeOutcome(outcomeId) {
        errorText = ""
        if (appController.deleteOutcome(outcomeId)) {
            reloadOutcomes(true)
            return
        }

        errorText = appController.statusMessage
    }

    ListModel {
        id: outcomeListModel
    }

    OutcomeEditorDialog {
        id: outcomeEditorDialog
        onOutcomeSaved: root.reloadOutcomes(true)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Drag a row by its handle to reorder outcomes. Long-click a row to edit its key, display name, or active state.")
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Add New")
                onClicked: outcomeEditorDialog.openForCreate()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: outcomeListView
                width: parent.width
                height: parent.height
                model: outcomeListModel
                spacing: 8
                boundsBehavior: Flickable.StopAtBounds

                displaced: Transition {
                    NumberAnimation {
                        properties: "x,y"
                        duration: 120
                    }
                }

                delegate: Rectangle {
                    id: outcomeCard
                    required property string outcomeId
                    required property string key
                    required property string displayName
                    required property bool isActive
                    width: outcomeListView.width
                    radius: 8
                    color: moveArea.drag.active ? "#eef6ff" : "#ffffff"
                    border.width: 1
                    border.color: moveArea.drag.active ? "#4a79d9" : "#d7d7d7"
                    z: moveArea.drag.active ? 10 : 0
                    implicitHeight: outcomeLayout.implicitHeight + 18

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        gesturePolicy: TapHandler.WithinBounds
                        onLongPressed: outcomeEditorDialog.openForOutcome(outcomeId)
                    }

                    RowLayout {
                        id: outcomeLayout
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
                                drag.target: outcomeCard
                                drag.axis: Drag.YAxis

                                onPressed: cursorShape = Qt.ClosedHandCursor
                                onReleased: {
                                    cursorShape = Qt.OpenHandCursor
                                    outcomeCard.x = 0
                                    outcomeCard.y = 0
                                    root.commitOrder()
                                }
                                onCanceled: {
                                    cursorShape = Qt.OpenHandCursor
                                    outcomeCard.x = 0
                                    outcomeCard.y = 0
                                    root.reloadOutcomes(true)
                                }
                                onPositionChanged: {
                                    const centerPoint = outcomeCard.mapToItem(outcomeListView.contentItem,
                                                                              outcomeCard.width / 2,
                                                                              outcomeCard.height / 2)
                                    root.reorderOutcome(outcomeId, centerPoint)
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
                            onClicked: root.removeOutcome(outcomeId)
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

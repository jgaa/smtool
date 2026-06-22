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
    title: creatingNew ? qsTr("Add Status") : qsTr("Edit Status")
    width: Math.min(parent ? parent.width - 80 : 760, 760)
    height: Math.min(parent ? parent.height - 80 : 760, 760)

    property string contentStatusId: ""
    property string originalKey: ""
    property string keyError: ""
    property string saveError: ""
    property bool creatingNew: false
    property bool systemStatus: false

    signal contentStatusSaved()

    function openForContentStatus(nextContentStatusId) {
        const contentStatus = appController.contentStatusDetails(nextContentStatusId)
        if (!contentStatus.contentStatusId) {
            saveError = qsTr("Status not found.")
            return
        }

        creatingNew = false
        contentStatusId = contentStatus.contentStatusId
        originalKey = contentStatus.key
        systemStatus = contentStatus.isSystem
        keyField.text = contentStatus.key
        infoField.text = contentStatus.info
        keyError = ""
        saveError = ""
        open()
    }

    function openForCreate() {
        creatingNew = true
        contentStatusId = ""
        originalKey = ""
        systemStatus = false
        keyField.text = ""
        infoField.text = ""
        keyError = qsTr("Key is required.")
        saveError = ""
        open()
    }

    function refreshKeyState() {
        const trimmedKey = keyField.text.trim()
        if (!creatingNew && trimmedKey === originalKey) {
            keyError = ""
            return
        }

        const validation = appController.validateContentStatusKey(contentStatusId, trimmedKey)
        keyError = validation.valid ? "" : validation.message
    }

    function formValid() {
        return keyError.length === 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        Label {
            Layout.fillWidth: true
            visible: root.systemStatus
            wrapMode: Text.Wrap
            color: "#666666"
            text: qsTr("This is a system status. Its key is fixed, but you can still update the info text.")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            Label { text: qsTr("Key") }
            TextField {
                id: keyField
                Layout.fillWidth: true
                readOnly: root.systemStatus
                color: root.keyError.length > 0 ? "#b42318" : palette.text
                placeholderText: qsTr("status_key")
                background: Rectangle {
                    radius: 6
                    border.width: 1
                    border.color: root.keyError.length > 0 ? "#b42318" : "#c8c8c8"
                    color: palette.base
                }
                onTextEdited: root.refreshKeyState()
            }

            Label {
                Layout.alignment: Qt.AlignTop
                text: qsTr("Info")
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 420
                clip: true

                TextArea {
                    id: infoField
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    placeholderText: qsTr("Describe how this status should be used.")
                    onTextChanged: root.saveError = ""
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.keyError.length > 0
            color: "#b42318"
            wrapMode: Text.Wrap
            text: root.keyError
        }

        Label {
            Layout.fillWidth: true
            visible: root.saveError.length > 0
            color: "#b42318"
            wrapMode: Text.Wrap
            text: root.saveError
        }

        DialogButtonBox {
            Layout.fillWidth: true
            standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

            Component.onCompleted: standardButton(DialogButtonBox.Save).enabled = Qt.binding(function() {
                return root.formValid()
            })

            onAccepted: {
                root.refreshKeyState()
                if (!root.formValid()) {
                    return
                }

                root.saveError = ""
                if (appController.saveContentStatus(root.contentStatusId,
                                                    keyField.text,
                                                    infoField.text)) {
                    root.close()
                    root.contentStatusSaved()
                    return
                }

                root.saveError = appController.statusMessage
            }

            onRejected: root.close()
        }
    }
}

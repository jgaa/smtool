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
    title: creatingNew ? qsTr("Add Content Kind") : qsTr("Edit Content Kind")
    width: Math.min(parent ? parent.width - 80 : 480, 480)

    property string contentKindId: ""
    property string originalKey: ""
    property string keyError: ""
    property string saveError: ""
    property bool creatingNew: false

    signal contentKindSaved()

    function openForContentKind(nextContentKindId) {
        const contentKind = appController.contentKindDetails(nextContentKindId)
        if (!contentKind.contentKindId) {
            saveError = qsTr("Content kind not found.")
            return
        }

        creatingNew = false
        contentKindId = contentKind.contentKindId
        originalKey = contentKind.key
        keyField.text = contentKind.key
        displayNameField.text = contentKind.displayName
        activeBox.checked = contentKind.isActive
        keyError = ""
        saveError = ""
        open()
    }

    function openForCreate() {
        creatingNew = true
        contentKindId = ""
        originalKey = ""
        keyField.text = ""
        displayNameField.text = ""
        activeBox.checked = true
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

        const validation = appController.validateContentKindKey(contentKindId, trimmedKey)
        keyError = validation.valid ? "" : validation.message
    }

    function formValid() {
        return keyError.length === 0 && displayNameField.text.trim().length > 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            Label { text: qsTr("Key") }
            TextField {
                id: keyField
                Layout.fillWidth: true
                color: root.keyError.length > 0 ? "#b42318" : palette.text
                placeholderText: qsTr("content_kind_key")
                background: Rectangle {
                    radius: 6
                    border.width: 1
                    border.color: root.keyError.length > 0 ? "#b42318" : "#c8c8c8"
                    color: palette.base
                }
                onTextEdited: root.refreshKeyState()
            }

            Label { text: qsTr("Display Name") }
            TextField {
                id: displayNameField
                Layout.fillWidth: true
                onTextEdited: root.saveError = ""
            }

            Label { text: qsTr("Active") }
            CheckBox {
                id: activeBox
                text: qsTr("Selectable in the app")
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
                if (appController.saveContentKind(root.contentKindId,
                                                  keyField.text,
                                                  displayNameField.text,
                                                  activeBox.checked)) {
                    root.close()
                    root.contentKindSaved()
                    return
                }

                root.saveError = appController.statusMessage
            }

            onRejected: root.close()
        }
    }
}

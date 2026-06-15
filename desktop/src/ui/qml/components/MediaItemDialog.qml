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
    width: Math.min(parent ? parent.width - 80 : 720, 720)
    height: Math.min(parent ? parent.height - 80 : 360, dialogLayout.implicitHeight + 40)

    property int editIndex: -1
    property string itemId: ""
    property string sourceType: "file"
    property string mediaDataDir: ""

    signal savedItem(var item, int editIndex)

    title: editIndex >= 0 ? qsTr("Edit Media") : qsTr("Add Media")

    function baseName(path) {
        const clean = path.replace(/\\/g, "/")
        const parts = clean.split("/")
        return parts.length > 0 ? parts[parts.length - 1] : clean
    }

    function isRemoteUrl(value) {
        return /^https?:\/\//i.test(value.trim())
    }

    function openForCreate(prefilledLocation) {
        appController.logDebug("MediaItemDialog.openForCreate prefilledLocation='" + (prefilledLocation ? prefilledLocation : "") + "'")
        editIndex = -1
        itemId = ""
        sourceType = "file"
        nameField.clear()
        locationField.text = prefilledLocation ? prefilledLocation : ""
        copyFileSwitch.checked = false
        if (locationField.text.length > 0 && !isRemoteUrl(locationField.text)) {
            nameField.text = baseName(locationField.text)
        }
        open()
    }

    function openForEdit(index, item) {
        appController.logDebug("MediaItemDialog.openForEdit index=" + index
                               + " id='" + (item.id ? item.id : "")
                               + "' name='" + (item.name ? item.name : "")
                               + "' sourceType='" + (item.sourceType ? item.sourceType : "")
                               + "' location='" + (item.location ? item.location : "") + "'")
        editIndex = index
        itemId = item.id ? item.id : ""
        sourceType = item.sourceType ? item.sourceType : "file"
        nameField.text = item.name ? item.name : ""
        locationField.text = item.location ? item.location : ""
        copyFileSwitch.checked = false
        open()
    }

    ColumnLayout {
        id: dialogLayout
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 12
            Layout.fillWidth: true

            Label { text: "Name" }
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "Unnamed"
            }

            Label { text: "Location" }
            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: locationField
                    Layout.fillWidth: true
                    placeholderText: "https://... or /path/to/file"

                    onTextChanged: {
                        if (root.isRemoteUrl(text)) {
                            root.sourceType = "url"
                            copyFileSwitch.checked = false
                            return
                        }

                        if (text.trim().length === 0) {
                            return
                        }

                        if (nameField.text.trim().length === 0 || nameField.text === root.baseName(text)) {
                            nameField.text = root.baseName(text)
                        }
                        root.sourceType = text.startsWith("/") ? "file" : root.sourceType
                    }
                }

                Button {
                    text: "Browse"
                    onClicked: {
                        const path = appController.chooseMediaFile()
                        if (path.length === 0) {
                            return
                        }
                        locationField.text = path
                        root.sourceType = "file"
                        if (nameField.text.trim().length === 0) {
                            nameField.text = root.baseName(path)
                        }
                    }
                }
            }
        }

        Switch {
            id: copyFileSwitch
            text: "Copy file"
            visible: !root.isRemoteUrl(locationField.text) && locationField.text.trim().length > 0
        }

        Item {
            Layout.fillHeight: true
        }

        DialogButtonBox {
            Layout.fillWidth: true
            standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

            onAccepted: {
                let sourceType = root.isRemoteUrl(locationField.text) ? "url" : root.sourceType
                let location = locationField.text.trim()
                appController.logDebug("MediaItemDialog.accepted before-copy editIndex=" + root.editIndex
                                       + " id='" + root.itemId
                                       + "' name='" + nameField.text.trim()
                                       + "' sourceType='" + sourceType
                                       + "' location='" + location
                                       + "' copyFile=" + copyFileSwitch.checked)

                if (copyFileSwitch.checked && sourceType !== "url") {
                    const copiedPath = appController.copyMediaFileToDataDir(location, root.mediaDataDir)
                    if (copiedPath.length === 0) {
                        appController.logDebug("MediaItemDialog.accepted copy failed for location='" + location + "'")
                        return
                    }
                    sourceType = "managed_file"
                    location = copiedPath
                    appController.logDebug("MediaItemDialog.accepted copied to '" + location + "'")
                }

                appController.logDebug("MediaItemDialog.accepted emit savedItem name='" + nameField.text.trim()
                                       + "' sourceType='" + sourceType
                                       + "' location='" + location + "'")
                root.savedItem({
                    id: root.itemId,
                    name: nameField.text.trim(),
                    sourceType: sourceType,
                    location: location,
                    copyFile: false
                }, root.editIndex)
                root.close()
            }

            onRejected: root.close()
        }
    }
}

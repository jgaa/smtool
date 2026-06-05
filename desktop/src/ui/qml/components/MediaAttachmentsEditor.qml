import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property var items: []
    property string mediaDataDir: ""

    function droppedLocalPath(drop) {
        if (drop.urls && drop.urls.length > 0) {
            for (let index = 0; index < drop.urls.length; ++index) {
                const localPath = appController.localPathFromUrl(drop.urls[index].toString())
                if (localPath.length > 0) {
                    return localPath
                }
            }
        }

        const droppedText = drop.text ? drop.text.toString().trim() : ""
        if (droppedText.length > 0) {
            const firstLine = droppedText.split(/\r?\n/, 1)[0]
            return appController.localPathFromUrl(firstLine)
        }

        return ""
    }

    function cloneItems() {
        return items ? items.slice() : []
    }

    function replaceItems(nextItems) {
        items = nextItems ? nextItems.slice() : []
    }

    function addOrUpdateItem(item, index) {
        appController.logDebug("MediaAttachmentsEditor.addOrUpdateItem index=" + index
                               + " id='" + (item.id ? item.id : "")
                               + "' name='" + (item.name ? item.name : "")
                               + "' sourceType='" + (item.sourceType ? item.sourceType : "")
                               + "' location='" + (item.location ? item.location : "") + "'")
        const nextItems = cloneItems()
        if (index >= 0) {
            nextItems[index] = item
        } else {
            nextItems.push(item)
        }
        items = nextItems
    }

    function removeItem(index) {
        const nextItems = cloneItems()
        nextItems.splice(index, 1)
        items = nextItems
    }

    function openForDroppedLocation(location) {
        mediaItemDialog.openForCreate(location)
    }

    background: Rectangle {
        radius: 8
        color: mediaDropArea.containsDrag ? "#eef6ff" : "#ffffff"
        border.color: mediaDropArea.containsDrag ? "#4a76b8" : "#d6d6d6"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Media / Files"
                font.bold: true
            }

            DropArea {
                id: mediaDropArea
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                onDropped: function(drop) {
                    const localPath = root.droppedLocalPath(drop)
                    if (localPath.length > 0) {
                        root.openForDroppedLocation(localPath)
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: "transparent"
                    border.width: 1
                    border.color: mediaDropArea.containsDrag ? "#4a76b8" : "#d6d6d6"
                    implicitHeight: 34

                    Label {
                        anchors.centerIn: parent
                        text: "Drop local file here"
                        color: "#666666"
                    }
                }
            }

            Button {
                text: "Add"
                onClicked: mediaItemDialog.openForCreate("")
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(120, Math.min(contentHeight, 240))
            clip: true
            model: root.items
            spacing: 8

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                width: 18

                contentItem: Rectangle {
                    implicitWidth: 18
                    radius: 9
                    color: parent.pressed ? "#8f8f8f" : "#a7a7a7"
                }

                background: Rectangle {
                    radius: 9
                    color: "#ececec"
                }
            }

            delegate: Rectangle {
                required property var modelData
                required property int index

                width: ListView.view.width
                radius: 6
                color: "#fafafa"
                border.color: "#d6d6d6"
                implicitHeight: mediaLayout.implicitHeight + 12

                RowLayout {
                    id: mediaLayout
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: modelData.name && modelData.name.length > 0 ? modelData.name : "Unnamed"
                            font.bold: true
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }

                        Label {
                            text: (modelData.sourceType === "url" ? "URL" : (modelData.sourceType === "managed_file" ? "App file" : "File"))
                                + " | " + modelData.location
                            color: "#555555"
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                        }
                    }

                    ToolButton {
                        text: "Open"
                        onClicked: appController.openMedia(modelData, root.mediaDataDir)
                    }

                    ToolButton {
                        text: "Edit"
                        onClicked: mediaItemDialog.openForEdit(index, modelData)
                    }

                    ToolButton {
                        text: "Delete"
                        onClicked: root.removeItem(index)
                    }
                }
            }
        }
    }

    MediaItemDialog {
        id: mediaItemDialog
        mediaDataDir: root.mediaDataDir
        onSavedItem: function(item, editIndex) {
            root.addOrUpdateItem(item, editIndex)
        }
    }
}

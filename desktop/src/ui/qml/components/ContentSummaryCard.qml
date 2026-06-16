import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string titleText: ""
    property string bodyText: ""
    property string metaText: ""
    property int bodyWordCap: 100
    property bool markdownEnabled: true
    property bool selected: false
    property color borderColorOverride: "transparent"
    property int borderWidthOverride: 0
    property bool showEditAction: true
    property bool showDeleteAction: true
    default property alias extraContent: extraContainer.data

    signal clicked()
    signal editRequested()
    signal deleteRequested()

    function clippedBodyText() {
        const source = bodyText.trim()
        if (source.length === 0 || bodyWordCap === 0) {
            return source
        }

        const words = source.split(/\s+/).filter(function(word) { return word.length > 0 })
        if (words.length <= bodyWordCap) {
            return source
        }
        return words.slice(0, bodyWordCap).join(" ") + "..."
    }

    radius: 6
    color: selected ? "#e8f2ff" : "white"
    border.width: borderWidthOverride > 0 ? borderWidthOverride : 1
    border.color: borderWidthOverride > 0 ? borderColorOverride : selected ? "#8fb7e6" : "#d5d5d5"
    implicitHeight: cardLayout.implicitHeight + 16

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }

    ColumnLayout {
        id: cardLayout
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.titleText
                font.bold: true
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            ToolButton {
                visible: root.showEditAction
                icon.name: "document-edit"
                text: "Edit"
                display: AbstractButton.IconOnly
                ToolTip.visible: hovered
                ToolTip.text: text
                onClicked: root.editRequested()
            }

            ToolButton {
                visible: root.showDeleteAction
                icon.name: "edit-delete"
                text: "Delete"
                display: AbstractButton.IconOnly
                ToolTip.visible: hovered
                ToolTip.text: text
                onClicked: root.deleteRequested()
            }
        }

        Label {
            text: root.clippedBodyText()
            visible: text.length > 0
            textFormat: root.markdownEnabled ? Text.MarkdownText : Text.PlainText
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Label {
            text: root.metaText
            visible: text.length > 0
            color: "#555555"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        ColumnLayout {
            id: extraContainer
            Layout.fillWidth: true
            spacing: 6
        }
    }
}

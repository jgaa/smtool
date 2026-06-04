import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string titleText: ""
    property string bodyText: ""
    property string metaText: ""
    property bool selected: false
    property bool showEditAction: true
    property bool showDeleteAction: true
    default property alias extraContent: extraContainer.data

    signal clicked()
    signal editRequested()
    signal deleteRequested()

    radius: 6
    color: selected ? "#e8f2ff" : "white"
    border.color: selected ? "#8fb7e6" : "#d5d5d5"
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
            text: root.bodyText
            visible: text.length > 0
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

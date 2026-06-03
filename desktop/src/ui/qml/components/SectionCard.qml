import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root

    property alias title: titleLabel.text
    default property alias sectionChildren: contentColumn.data

    Layout.fillWidth: true

    background: Rectangle {
        radius: 8
        color: "#f8f8f8"
        border.color: "#d8d8d8"
    }

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        spacing: 8

        Label {
            id: titleLabel
            font.pixelSize: 18
            font.bold: true
        }
    }
}

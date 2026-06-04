import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    anchors.fill: parent
    clip: true

    readonly property var logLevelLabels: [
        qsTr("Disabled"),
        qsTr("Error"),
        qsTr("Warning"),
        qsTr("Notice"),
        qsTr("Info"),
        qsTr("Debug"),
        qsTr("Trace")
    ]

    function commit() {
        appSettings.appLogLevel = appLevelBox.currentIndex
        appSettings.fileLogLevel = fileLevelBox.currentIndex
        appSettings.logFilePath = logPathField.text.trim()
        appSettings.pruneLogFile = pruneCheck.checked
    }

    function reload() {
        appLevelBox.currentIndex = appSettings.appLogLevel
        fileLevelBox.currentIndex = appSettings.fileLogLevel
        logPathField.text = appSettings.logFilePath
        pruneCheck.checked = appSettings.pruneLogFile
    }

    GridLayout {
        id: formLayout
        width: root.availableWidth
        columns: width >= 460 ? 2 : 1
        rowSpacing: 10
        columnSpacing: 16

        Label {
            text: qsTr("Application Log Level")
            visible: Qt.platform.os === "linux"
        }

        ComboBox {
            id: appLevelBox
            visible: Qt.platform.os === "linux"
            Layout.fillWidth: true
            model: root.logLevelLabels
        }

        Label {
            text: qsTr("File Log Level")
        }

        ComboBox {
            id: fileLevelBox
            Layout.fillWidth: true
            model: root.logLevelLabels
        }

        Label {
            text: qsTr("Log File Path")
        }

        TextField {
            id: logPathField
            Layout.fillWidth: true
            placeholderText: qsTr("/tmp/smtool.log")
        }

        Item { }

        CheckBox {
            id: pruneCheck
            text: qsTr("Prune log file on startup")
        }

        Label {
            Layout.columnSpan: formLayout.columns
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Log settings apply on the next application start.")
            color: palette.mid
        }

        Item {
            Layout.fillHeight: true
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    anchors.fill: parent
    clip: true

    function commit() {
        const nextPath = databasePathField.text.trim()
        if (!appController.applyDatabasePath(nextPath)) {
            return false
        }
        appSettings.configuredDatabasePath = nextPath
        appSettings.configuredMediaDataDir = mediaDataDirField.text.trim()
        appSettings.defaultContentPriority = defaultContentPriorityBox.value
        appSettings.batchMarkdownImports = batchMarkdownImportsSwitch.checked
        appSettings.boardDescriptionPreviewWordCap = previewWordCapBox.value
        appSettings.confirmContentDeletion = confirmDeleteSwitch.checked
        appSettings.fetchAddedUrlTitles = fetchUrlTitlesSwitch.checked
        reload()
        return true
    }

    function reload() {
        databasePathField.text = appSettings.configuredDatabasePath.length > 0
            ? appSettings.configuredDatabasePath
            : appSettings.defaultDatabasePath
        mediaDataDirField.text = appSettings.configuredMediaDataDir.length > 0
            ? appSettings.configuredMediaDataDir
            : appSettings.defaultMediaDataDir
        defaultContentPriorityBox.value = appSettings.defaultContentPriority
        batchMarkdownImportsSwitch.checked = appSettings.batchMarkdownImports
        previewWordCapBox.value = appSettings.boardDescriptionPreviewWordCap
        confirmDeleteSwitch.checked = appSettings.confirmContentDeletion
        fetchUrlTitlesSwitch.checked = appSettings.fetchAddedUrlTitles
    }

    GridLayout {
        id: formLayout
        width: root.availableWidth
        columns: width >= 460 ? 2 : 1
        rowSpacing: 10
        columnSpacing: 16

        Label {
            text: qsTr("Database Path")
        }

        TextField {
            id: databasePathField
            Layout.fillWidth: true
            placeholderText: appSettings.defaultDatabasePath
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Set an absolute SQLite path. If the file already exists, SmTool will reopen it there. If the current database exists, SmTool will move it when possible.")
            color: palette.mid
        }

        Label {
            text: qsTr("Default Path")
        }

        Label {
            Layout.fillWidth: true
            text: appSettings.defaultDatabasePath
            wrapMode: Text.WrapAnywhere
            color: palette.mid
        }

        Label {
            text: qsTr("Media Data Dir")
        }

        TextField {
            id: mediaDataDirField
            Layout.fillWidth: true
            placeholderText: appSettings.defaultMediaDataDir
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Managed media files are stored here. Relative managed media paths are resolved from this directory.")
            color: palette.mid
        }

        Label {
            text: qsTr("Default Media Dir")
        }

        Label {
            Layout.fillWidth: true
            text: appSettings.defaultMediaDataDir
            wrapMode: Text.WrapAnywhere
            color: palette.mid
        }

        Label {
            text: qsTr("Default Priority")
        }

        SpinBox {
            id: defaultContentPriorityBox
            from: 0
            to: 100
            value: appSettings.defaultContentPriority
        }

        Label {
            text: qsTr("Batch Markdown Import")
        }

        Switch {
            id: batchMarkdownImportsSwitch
            checked: appSettings.batchMarkdownImports
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("When enabled, dropping or importing a Markdown file can create multiple ideas split by markdown sections. Text files always create one idea.")
            color: palette.mid
        }

        Label {
            text: qsTr("Board Preview Words")
        }

        SpinBox {
            id: previewWordCapBox
            from: 3
            to: 30
            value: appSettings.boardDescriptionPreviewWordCap
        }

        Label {
            text: qsTr("Confirm Delete")
        }

        Switch {
            id: confirmDeleteSwitch
            checked: appSettings.confirmContentDeletion
        }

        Label {
            text: qsTr("Fetch URL Titles")
        }

        Switch {
            id: fetchUrlTitlesSwitch
            checked: appSettings.fetchAddedUrlTitles
        }

        Item {
            Layout.fillHeight: true
        }
    }

    Component.onCompleted: reload()
}

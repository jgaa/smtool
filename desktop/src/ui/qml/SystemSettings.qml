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
        appSettings.cardDescriptionWordCap = cardDescriptionWordCapBox.value
        appSettings.cardDescriptionMarkdownEnabled = cardDescriptionMarkdownSwitch.checked
        appSettings.importedIdeaTitleWordCap = importedIdeaTitleWordCapBox.value
        appSettings.confirmContentDeletion = confirmDeleteSwitch.checked
        appSettings.fetchAddedUrlTitles = fetchUrlTitlesSwitch.checked
        appSettings.calendarFirstDayOfWeek = calendarFirstDayBox.currentValue
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
        cardDescriptionWordCapBox.value = appSettings.cardDescriptionWordCap
        cardDescriptionMarkdownSwitch.checked = appSettings.cardDescriptionMarkdownEnabled
        importedIdeaTitleWordCapBox.value = appSettings.importedIdeaTitleWordCap
        confirmDeleteSwitch.checked = appSettings.confirmContentDeletion
        fetchUrlTitlesSwitch.checked = appSettings.fetchAddedUrlTitles
        calendarFirstDayBox.currentIndex = root.calendarFirstDayIndex(appSettings.calendarFirstDayOfWeek)
    }

    readonly property var calendarFirstDayOptions: [
        { label: qsTr("Locale default"), value: 0 },
        { label: qsTr("Monday"), value: 1 },
        { label: qsTr("Tuesday"), value: 2 },
        { label: qsTr("Wednesday"), value: 3 },
        { label: qsTr("Thursday"), value: 4 },
        { label: qsTr("Friday"), value: 5 },
        { label: qsTr("Saturday"), value: 6 },
        { label: qsTr("Sunday"), value: 7 }
    ]

    function calendarFirstDayIndex(value) {
        for (let index = 0; index < calendarFirstDayOptions.length; ++index) {
            if (calendarFirstDayOptions[index].value === value) {
                return index
            }
        }
        return 0
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
            text: qsTr("Card Description Words")
        }

        SpinBox {
            id: cardDescriptionWordCapBox
            from: 0
            to: 500
            value: appSettings.cardDescriptionWordCap
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Limits description text shown in cards across the app. Use 0 to show the full description with no word cap.")
            color: palette.mid
        }

        Label {
            text: qsTr("Card Description Markdown")
        }

        Switch {
            id: cardDescriptionMarkdownSwitch
            checked: appSettings.cardDescriptionMarkdownEnabled
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("When enabled, card descriptions render Markdown formatting. Turn this off to show raw plain text instead.")
            color: palette.mid
        }

        Label {
            text: qsTr("Imported Header Words")
        }

        SpinBox {
            id: importedIdeaTitleWordCapBox
            from: 1
            to: 30
            value: appSettings.importedIdeaTitleWordCap
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("When pasted or imported text does not provide a Markdown heading, SmTool builds the idea header from the first sentence using this many words.")
            color: palette.mid
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

        Label {
            text: qsTr("Calendar Week Starts")
        }

        ComboBox {
            id: calendarFirstDayBox
            Layout.fillWidth: true
            textRole: "label"
            valueRole: "value"
            model: root.calendarFirstDayOptions
            currentIndex: root.calendarFirstDayIndex(appSettings.calendarFirstDayOfWeek)
        }

        Item { }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Locale default uses the first day configured for your system locale.")
            color: palette.mid
        }

        Item {
            Layout.fillHeight: true
        }
    }

    Component.onCompleted: reload()
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "components"

ApplicationWindow {
    id: window

    width: 1560
    height: 960
    visible: true
    title: "SmTool"
    header: ToolBar {
        height: 36
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            Label {
                text: "Search"
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                enabled: tabBar.currentIndex !== 6
                placeholderText: enabled ? "title, description, #tag, t:..., d:..." : "Search disabled on Dashboard"
                onTextEdited: appController.searchQuery = text
            }

            ToolButton {
                text: "x"
                enabled: searchField.enabled && appController.searchQuery.length > 0
                visible: appController.searchQuery.length > 0
                onClicked: appController.clearSearchQuery()
            }
        }
    }

    Connections {
        target: appController

        function onSearchQueryChanged() {
            if (searchField.text !== appController.searchQuery) {
                searchField.text = appController.searchQuery
            }
        }
    }

    property var publicationStatuses: [
        "planned",
        "scheduled",
        "published"
    ]
    property var seriesStatusOptions: [
        { value: "active", label: "Active" },
        { value: "paused", label: "Paused" },
        { value: "completed", label: "Completed" },
        { value: "archived", label: "Archived" }
    ]
    property var allContentSortOptions: [
        { label: "Due Date / Alphabetically", mode: 0 },
        { label: "Priority / Alphabetically", mode: 1 },
        { label: "Alphabetically", mode: 2 },
        { label: "Status / Alphabetically", mode: 3 },
        { label: "Status / Due Date", mode: 4 },
        { label: "Status / First Publish Date", mode: 5 },
        { label: "Pillar / Alphabetically", mode: 6 },
        { label: "Pillar / Due Date", mode: 7 },
        { label: "Pillar / First Publish Date", mode: 8 }
    ]
    property string activeSelectedText: {
        const item = window.activeFocusItem
        if (!item || item.selectedText === undefined || item.selectedText === null) {
            return ""
        }
        return item.selectedText.toString()
    }
    property bool activeHasSelection: activeSelectedText.length > 0
    property var calendarSections: []

    function clippedCardDescription(text) {
        const source = text.trim()
        const wordCap = appSettings.cardDescriptionWordCap
        if (source.length === 0 || wordCap === 0) {
            return source
        }

        const words = source.split(/\s+/).filter(function(word) { return word.length > 0 })
        if (words.length <= wordCap) {
            return source
        }
        return words.slice(0, wordCap).join(" ") + "..."
    }

    function copyCurrentSelection() {
        const item = window.activeFocusItem
        if (!item || !window.activeHasSelection) {
            return
        }

        if (item.copy !== undefined) {
            item.copy()
            return
        }

        appController.copyTextToClipboard(window.activeSelectedText)
    }

    function rebuildCalendarSections() {
        const sections = []
        const model = appController.calendarModel
        const todayKey = Qt.formatDate(new Date(), "yyyy-MM-dd")
        let currentSection = null

        for (let index = 0; index < model.count(); ++index) {
            const entry = model.entryAt(index)
            const dateKey = Qt.formatDate(entry.scheduledAt, "yyyy-MM-dd")

            if (!currentSection || currentSection.dateKey !== dateKey) {
                currentSection = {
                    dateKey: dateKey,
                    isToday: dateKey === todayKey,
                    hasContent: false,
                    hasPublication: false,
                    items: []
                }
                sections.push(currentSection)
            }

            if (entry.sourceType === "publication") {
                currentSection.hasPublication = true
            } else {
                currentSection.hasContent = true
            }

            currentSection.items.push({
                title: entry.title,
                series: entry.series,
                channel: entry.channel,
                sourceType: entry.sourceType,
                isOverdue: entry.isOverdue
            })
        }

        calendarSections = sections
    }

    Component.onCompleted: rebuildCalendarSections()

    Connections {
        target: appController.calendarModel

        function onModelReset() {
            window.rebuildCalendarSections()
        }
    }

    Action {
        id: goalsAction
        text: qsTr("Goals")
        onTriggered: goalsDialog.open()
    }

    Action {
        id: settingsAction
        text: qsTr("Settings")
        shortcut: StandardKey.Preferences
        onTriggered: settingsDialog.open()
    }

    Action {
        id: quitAction
        text: qsTr("Quit")
        shortcut: StandardKey.Quit
        onTriggered: Qt.quit()
    }

    Action {
        id: copyAction
        text: qsTr("Copy")
        shortcut: StandardKey.Copy
        enabled: window.activeHasSelection
        onTriggered: window.copyCurrentSelection()
    }

    Action {
        id: pasteToIdeaAction
        text: qsTr("Paste to Idea")
        shortcut: StandardKey.Paste
        enabled: appController.clipboardHasText
        onTriggered: appController.pasteClipboardToIdea()
    }

    Action {
        id: importFromFileAction
        text: qsTr("Import from File")
        onTriggered: appController.importIdeasFromUserSelectedFile()
    }

    SettingsDialog {
        id: settingsDialog
        parent: Overlay.overlay
    }

    AboutDialog {
        id: aboutDialog
        parent: Overlay.overlay
    }

    Dialog {
        id: deleteContentDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Delete Content")
        width: Math.min(window.width - 80, 420)
        property string contentId: ""
        property string pendingTitle: ""

        function openForContent(contentId, contentTitle) {
            if (!appSettings.confirmContentDeletion) {
                appController.deleteContent(contentId)
                return
            }

            deleteContentDialog.contentId = contentId
            deleteContentDialog.pendingTitle = contentTitle
            open()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: deleteContentDialog.pendingTitle.length > 0
                    ? qsTr("Delete \"%1\" and its related items?").arg(deleteContentDialog.pendingTitle)
                    : qsTr("Delete this content and its related items?")
            }

            DialogButtonBox {
                Layout.fillWidth: true
                standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel

                onAccepted: {
                    if (appController.deleteContent(deleteContentDialog.contentId)) {
                        deleteContentDialog.close()
                    }
                }
                onRejected: deleteContentDialog.close()
            }
        }
    }

    Dialog {
        id: burstDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Create Burst")
        width: Math.min(window.width - 80, 480)
        height: Math.min(Math.max(window.height - 80, 900), 500)
        property var optionsModel: []
        property var selectedKeys: []

        function openForSource() {
            optionsModel = appController.burstTemplateOptions()
            selectedKeys = []
            open()
        }

        function toggleSelection(key, checked) {
            const nextSelection = selectedKeys.filter(function(existingKey) { return existingKey !== key })
            if (checked) {
                nextSelection.push(key)
            }
            selectedKeys = nextSelection
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            Label {
                Layout.fillWidth: true
                text: qsTr("Select the burst alternatives to create.")
                wrapMode: Text.Wrap
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: burstDialog.optionsModel

                        delegate: CheckBox {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.displayName
                            checked: burstDialog.selectedKeys.indexOf(modelData.key) >= 0
                            onToggled: burstDialog.toggleSelection(modelData.key, checked)
                        }
                    }
                }
            }

            DialogButtonBox {
                Layout.fillWidth: true
                standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                onAccepted: {
                    if (appController.createBurstForCurrentSource(burstDialog.selectedKeys)) {
                        burstDialog.close()
                    }
                }
                onRejected: burstDialog.close()
            }
        }
    }

    Dialog {
        id: publicationFanOutDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Publish Fan Out")
        width: Math.min(window.width - 80, 480)
        height: Math.min(Math.max(window.height - 80, 900), 500)
        property string contentId: ""
        property var optionsModel: []
        property var selectedChannelIds: []

        function openForContent(nextContentId) {
            contentId = nextContentId
            optionsModel = appController.publicationFanOutOptions(nextContentId)
            selectedChannelIds = []
            open()
        }

        function toggleSelection(channelId, checked) {
            const nextSelection = selectedChannelIds.filter(function(existingChannelId) { return existingChannelId !== channelId })
            if (checked) {
                nextSelection.push(channelId)
            }
            selectedChannelIds = nextSelection
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            Label {
                Layout.fillWidth: true
                text: qsTr("Select the publication channels to create. Existing channels are shown but cannot be selected.")
                wrapMode: Text.Wrap
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: publicationFanOutDialog.optionsModel

                        delegate: CheckBox {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.displayName + (modelData.alreadyExists ? qsTr(" (already exists)") : "")
                            enabled: !modelData.alreadyExists
                            checked: publicationFanOutDialog.selectedChannelIds.indexOf(modelData.channelId) >= 0
                            onToggled: publicationFanOutDialog.toggleSelection(modelData.channelId, checked)
                        }
                    }
                }
            }

            DialogButtonBox {
                Layout.fillWidth: true
                standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                onAccepted: {
                    if (appController.createPublicationFanOut(publicationFanOutDialog.contentId,
                                                              publicationFanOutDialog.selectedChannelIds)) {
                        quickAddDialog.reloadPublications()
                        publicationFanOutDialog.close()
                    }
                }
                onRejected: publicationFanOutDialog.close()
            }
        }
    }

    Dialog {
        id: quickAddDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        property string editingContentId: ""
        property string presetSeriesId: ""
        property var seriesOptionsModel: []
        property var publicationsModel: []
        property var mediaItems: []
        title: editingContentId.length > 0 ? qsTr("Edit Content") : qsTr("Quick Add")
        width: Math.min(window.width - 40, 900)
        height: Math.min(window.height - 40, 860)

        function resetForm() {
            editingContentId = ""
            publicationsModel = []
            mediaItems = []
            contentMediaEditor.replaceItems(mediaItems)
            inboxTitleField.clear()
            inboxDescriptionField.clear()
            inboxTagsField.clear()
            presetSeriesId = ""
            seriesOptionsModel = appController.contentSeriesOptions()
            priorityBox.value = appSettings.defaultContentPriority
            scheduledAtSelector.value = ""
            if (statusBox.count > 0) {
                statusBox.currentIndex = 0
            }
            if (kindBox.count > 0) {
                kindBox.currentIndex = 0
            }
            if (pillarBox.count > 0) {
                pillarBox.currentIndex = 0
            }
            if (channelBox.count > 0) {
                channelBox.currentIndex = 0
            }
            seriesBox.currentIndex = 0
        }

        function reloadPublications() {
            if (editingContentId.length === 0) {
                publicationsModel = []
                return
            }
            const item = appController.contentDetails(editingContentId)
            publicationsModel = item.publications ? item.publications : []
            mediaItems = item.media ? item.media : []
            contentMediaEditor.replaceItems(mediaItems)
        }

        function openForCreate(seriesId) {
            resetForm()
            presetSeriesId = seriesId ? seriesId : ""
            if (presetSeriesId.length > 0) {
                const presetSeriesIndex = seriesBox.indexOfValue(presetSeriesId)
                if (presetSeriesIndex >= 0) {
                    seriesBox.currentIndex = presetSeriesIndex
                }

                const seriesDetails = appController.currentSeriesDetails
                if (seriesDetails.pillarId && seriesDetails.pillarId.length > 0) {
                    const presetPillarIndex = pillarBox.indexOfValue(seriesDetails.pillarId)
                    if (presetPillarIndex >= 0) {
                        pillarBox.currentIndex = presetPillarIndex
                    }
                }
            }
            open()
        }

        function openForEdit(contentId) {
            const item = appController.contentDetails(contentId)
            if (!item.id) {
                return
            }

            seriesOptionsModel = appController.contentSeriesOptions()
            editingContentId = item.id
            inboxTitleField.text = item.title
            inboxDescriptionField.text = item.description
            inboxTagsField.text = item.tags
            presetSeriesId = item.seriesId ? item.seriesId : ""
            priorityBox.value = item.priority
            scheduledAtSelector.value = item.scheduledAt ? item.scheduledAt.slice(0, 10) : ""

            const statusIndex = statusBox.indexOfValue(item.status)
            if (statusIndex >= 0) {
                statusBox.currentIndex = statusIndex
            }

            const kindIndex = kindBox.indexOfValue(item.kindId)
            if (kindIndex >= 0) {
                kindBox.currentIndex = kindIndex
            }

            const pillarIndex = pillarBox.indexOfValue(item.pillarId)
            if (pillarIndex >= 0) {
                pillarBox.currentIndex = pillarIndex
            }

            const seriesIndex = seriesBox.indexOfValue(item.seriesId)
            seriesBox.currentIndex = seriesIndex >= 0 ? seriesIndex : -1

            if (item.suggestedChannelId && item.suggestedChannelId.length > 0) {
                const channelIndex = channelBox.indexOfValue(item.suggestedChannelId)
                if (channelIndex >= 0) {
                    channelBox.currentIndex = channelIndex
                }
            } else if (channelBox.count > 0) {
                channelBox.currentIndex = 0
            }

            publicationsModel = item.publications ? item.publications : []
            mediaItems = item.media ? item.media : []
            contentMediaEditor.replaceItems(mediaItems)

            open()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: 16

                    Frame {
                        Layout.fillWidth: true
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: "Title"
                                    Layout.preferredWidth: 80
                                }

                                TextField {
                                    id: inboxTitleField
                                    Layout.fillWidth: true
                                    placeholderText: "Capture an idea"
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: "Tags"
                                    Layout.preferredWidth: 80
                                }

                                TextField {
                                    id: inboxTagsField
                                    Layout.fillWidth: true
                                    placeholderText: "#idea product roadmap"
                                }
                            }

                            GridLayout {
                                columns: quickAddDialog.editingContentId.length > 0 ? 10 : 8
                                columnSpacing: 12
                                rowSpacing: 12
                                Layout.fillWidth: true

                                Label {
                                    text: "Kind"
                                    visible: quickAddDialog.editingContentId.length > 0
                                }
                                ComboBox {
                                    id: kindBox
                                    Layout.fillWidth: true
                                    visible: quickAddDialog.editingContentId.length > 0
                                    model: appController.kindModel
                                    textRole: "displayName"
                                    valueRole: "lookupId"
                                }

                                Label { text: "Pillar" }
                                ComboBox {
                                    id: pillarBox
                                    Layout.fillWidth: true
                                    model: appController.pillarModel
                                    textRole: "displayName"
                                    valueRole: "lookupId"
                                }

                                Label { text: "Series" }
                                ComboBox {
                                    id: seriesBox
                                    Layout.fillWidth: true
                                    model: quickAddDialog.seriesOptionsModel
                                    textRole: "displayName"
                                    valueRole: "lookupId"
                                    currentIndex: 0
                                }

                                Label { text: "Ch" }
                                ComboBox {
                                    id: channelBox
                                    Layout.fillWidth: true
                                    model: appController.channelModel
                                    textRole: "displayName"
                                    valueRole: "lookupId"
                                }

                                RowLayout {
                                    spacing: 4

                                    Label { text: "Priority" }

                                    ToolButton {
                                        icon.name: "help-about"
                                        text: "Priority help"
                                        display: AbstractButton.IconOnly
                                        visible: quickAddDialog.editingContentId.length > 0
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Higher numbers mean higher priority and greater urgency."
                                        onClicked: priorityInfoPopup.open()
                                    }
                                }
                                SpinBox {
                                    id: priorityBox
                                    from: 0
                                    to: 100
                                    value: appSettings.defaultContentPriority
                                }
                            }

                            Popup {
                                id: priorityInfoPopup
                                parent: Overlay.overlay
                                anchors.centerIn: Overlay.overlay
                                width: Math.min(window.width - 80, 420)
                                modal: true
                                focus: true
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                                background: Rectangle {
                                    radius: 8
                                    color: "white"
                                    border.color: "#cfcfcf"
                                }

                                contentItem: Label {
                                    width: parent.width
                                    text: "Higher numbers mean higher priority and more urgent work."
                                    wrapMode: Text.Wrap
                                    padding: 16
                                }
                            }

                            GridLayout {
                                columns: 4
                                columnSpacing: 12
                                rowSpacing: 12
                                Layout.fillWidth: true

                                Label { text: "Scheduled At" }
                                DateSelector {
                                    id: scheduledAtSelector
                                    Layout.fillWidth: true
                                }

                                Label { text: "Status" }
                                ComboBox {
                                    id: statusBox
                                    Layout.fillWidth: true
                                    textRole: "displayName"
                                    valueRole: "statusId"
                                    model: appController.contentStatusModel
                                }
                            }
                        }
                    }

                    Frame {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            Label {
                                text: "Description"
                                font.bold: true
                            }

                            ScrollView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 220
                                clip: true
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                                TextArea {
                                    id: inboxDescriptionField
                                    width: parent.width
                                    wrapMode: TextEdit.Wrap
                                    placeholderText: "Optional note"
                                    selectByMouse: true
                                }
                            }

                            MediaAttachmentsEditor {
                                id: contentMediaEditor
                                Layout.fillWidth: true
                                mediaDataDir: appSettings.effectiveMediaDataDir
                                onItemsChanged: quickAddDialog.mediaItems = items
                            }

                            Frame {
                                Layout.fillWidth: true
                                visible: quickAddDialog.editingContentId.length > 0

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true

                                        Label {
                                            text: "Publishing"
                                            font.bold: true
                                        }

                                        Item { Layout.fillWidth: true }

                                        Button {
                                            text: "Add Publication"
                                            onClicked: publicationDialog.openForCreate(quickAddDialog.editingContentId)
                                        }

                                        Button {
                                            text: "Publish Fan Out"
                                            onClicked: publicationFanOutDialog.openForContent(quickAddDialog.editingContentId)
                                        }
                                    }

                                    ListView {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: Math.max(80, Math.min(contentHeight, 240))
                                        clip: true
                                        model: quickAddDialog.publicationsModel
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
                                            width: ListView.view.width
                                            implicitHeight: publicationLayout.implicitHeight + 12
                                            radius: 6
                                            color: "#ffffff"
                                            border.color: "#d6d6d6"

                                            RowLayout {
                                                id: publicationLayout
                                                anchors.fill: parent
                                                anchors.margins: 6
                                                spacing: 8

                                                ColumnLayout {
                                                    Layout.fillWidth: true

                                                    Label {
                                                        text: [modelData.channelName, modelData.status].filter(function(v) { return v.length > 0 }).join(" | ")
                                                        font.bold: true
                                                        Layout.fillWidth: true
                                                    }

                                                    Label {
                                                        text: [
                                                            modelData.scheduledAt.length > 0 ? "Scheduled " + modelData.scheduledAt : "",
                                                            modelData.publishedAt.length > 0 ? "Published " + modelData.publishedAt : ""
                                                        ].filter(function(v) { return v.length > 0 }).join(" | ")
                                                        visible: text.length > 0
                                                        color: "#555555"
                                                        Layout.fillWidth: true
                                                        wrapMode: Text.Wrap
                                                    }

                                                    Label {
                                                        text: modelData.url
                                                        visible: text.length > 0
                                                        color: "#555555"
                                                        Layout.fillWidth: true
                                                        wrapMode: Text.WrapAnywhere
                                                    }
                                                }

                                                ToolButton {
                                                    icon.name: "document-edit"
                                                    text: "Edit publication"
                                                    display: AbstractButton.IconOnly
                                                    ToolTip.visible: hovered
                                                    ToolTip.text: text
                                                    onClicked: publicationDialog.openForEdit(modelData.id)
                                                }

                                                ToolButton {
                                                    icon.name: "edit-delete"
                                                    text: "Delete publication"
                                                    display: AbstractButton.IconOnly
                                                    ToolTip.visible: hovered
                                                    ToolTip.text: text
                                                    onClicked: {
                                                        if (appController.deletePublication(modelData.id)) {
                                                            quickAddDialog.reloadPublications()
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            DialogButtonBox {
                Layout.fillWidth: true
                standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                onAccepted: {
                    const kindId = kindBox.currentIndex >= 0 ? kindBox.currentValue : ""
                    const pillarId = pillarBox.currentIndex >= 0 ? pillarBox.currentValue : ""
                    const seriesId = seriesBox.currentIndex >= 0 ? seriesBox.currentValue : ""
                    const channelId = channelBox.currentIndex >= 0 ? channelBox.currentValue : ""
                    const status = statusBox.currentIndex >= 0 ? statusBox.currentValue : "inbox"
                    const ok = quickAddDialog.editingContentId.length > 0
                        ? appController.updateContent(quickAddDialog.editingContentId,
                                                      inboxTitleField.text,
                                                      inboxDescriptionField.text,
                                                      inboxTagsField.text,
                                                      seriesId,
                                                      kindId,
                                                      pillarId,
                                                      priorityBox.value,
                                                      scheduledAtSelector.value,
                                                      channelId,
                                                      status,
                                                      quickAddDialog.mediaItems,
                                                      appSettings.effectiveMediaDataDir,
                                                      appSettings.fetchAddedUrlTitles)
                        : appController.createInboxItem(inboxTitleField.text,
                                                        inboxDescriptionField.text,
                                                        inboxTagsField.text,
                                                        pillarId,
                                                        seriesId,
                                                        priorityBox.value,
                                                        scheduledAtSelector.value,
                                                        channelId,
                                                        quickAddDialog.mediaItems,
                                                        appSettings.effectiveMediaDataDir,
                                                        appSettings.fetchAddedUrlTitles)
                    if (ok) {
                        quickAddDialog.resetForm()
                        quickAddDialog.close()
                    }
                }

                onRejected: {
                    quickAddDialog.resetForm()
                    quickAddDialog.close()
                }
            }
        }
    }

    Dialog {
        id: publicationDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        property string contentId: ""
        property string editingPublicationId: ""
        property var mediaItems: []
        title: editingPublicationId.length > 0 ? qsTr("Edit Publication") : qsTr("Add Publication")
        width: Math.min(window.width - 80, 760)
        height: Math.min(window.height - 40, 760)

        function resetForm() {
            contentId = ""
            editingPublicationId = ""
            mediaItems = []
            publicationMediaEditor.replaceItems(mediaItems)
            if (publicationChannelBox.count > 0) {
                publicationChannelBox.currentIndex = 0
            }
            publicationStatusBox.currentIndex = 0
            publicationScheduledAtSelector.value = ""
            publicationPublishedAtSelector.value = ""
            publicationUrlField.clear()
        }

        function openForCreate(nextContentId) {
            resetForm()
            contentId = nextContentId
            const item = appController.contentDetails(nextContentId)
            if (item.suggestedChannelId && item.suggestedChannelId.length > 0) {
                const channelIndex = publicationChannelBox.indexOfValue(item.suggestedChannelId)
                if (channelIndex >= 0) {
                    publicationChannelBox.currentIndex = channelIndex
                }
            }
            open()
        }

        function openForEdit(publicationId) {
            const publication = appController.publicationDetails(publicationId)
            if (!publication.id) {
                return
            }

            resetForm()
            contentId = publication.contentId
            editingPublicationId = publication.id
            const channelIndex = publicationChannelBox.indexOfValue(publication.channelId)
            if (channelIndex >= 0) {
                publicationChannelBox.currentIndex = channelIndex
            }
            const statusIndex = publicationStatusBox.indexOfValue(publication.status)
            if (statusIndex >= 0) {
                publicationStatusBox.currentIndex = statusIndex
            }
            publicationScheduledAtSelector.value = publication.scheduledAt.length > 0
                ? publication.scheduledAt.slice(0, 10)
                : ""
            publicationPublishedAtSelector.value = publication.publishedAt.length > 0
                ? publication.publishedAt.slice(0, 10)
                : ""
            publicationUrlField.text = publication.url
            mediaItems = publication.media ? publication.media : []
            publicationMediaEditor.replaceItems(mediaItems)
            open()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width
                    spacing: 16

                    GridLayout {
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 12
                        Layout.fillWidth: true

                        Label { text: "Channel" }
                        ComboBox {
                            id: publicationChannelBox
                            Layout.fillWidth: true
                            model: appController.channelModel
                            textRole: "displayName"
                            valueRole: "lookupId"
                        }

                        Label { text: "Status" }
                        ComboBox {
                            id: publicationStatusBox
                            Layout.fillWidth: true
                            model: window.publicationStatuses
                        }

                        Label { text: "Scheduled At" }
                        DateSelector {
                            id: publicationScheduledAtSelector
                            Layout.fillWidth: true
                        }

                        Label { text: "Published At" }
                        DateSelector {
                            id: publicationPublishedAtSelector
                            Layout.fillWidth: true
                        }

                        Label { text: "URL" }
                        TextField {
                            id: publicationUrlField
                            Layout.fillWidth: true
                            placeholderText: "https://..."
                        }
                    }

                    MediaAttachmentsEditor {
                        id: publicationMediaEditor
                        Layout.fillWidth: true
                        mediaDataDir: appSettings.effectiveMediaDataDir
                        onItemsChanged: publicationDialog.mediaItems = items
                    }
                }
            }

            DialogButtonBox {
                Layout.fillWidth: true
                standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                onAccepted: {
                    const channelId = publicationChannelBox.currentIndex >= 0 ? publicationChannelBox.currentValue : ""
                    const status = publicationStatusBox.currentIndex >= 0 ? publicationStatusBox.currentText : "planned"
                    if (appController.savePublication(publicationDialog.contentId,
                                                      publicationDialog.editingPublicationId,
                                                      channelId,
                                                      status,
                                                      publicationScheduledAtSelector.value,
                                                      publicationPublishedAtSelector.value,
                                                      publicationUrlField.text,
                                                      publicationDialog.mediaItems,
                                                      appSettings.effectiveMediaDataDir,
                                                      appSettings.fetchAddedUrlTitles)) {
                        quickAddDialog.reloadPublications()
                        publicationDialog.resetForm()
                        publicationDialog.close()
                    }
                }

                onRejected: {
                    publicationDialog.resetForm()
                    publicationDialog.close()
                }
            }
        }
    }

    Dialog {
        id: goalsDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Goals")
        width: Math.min(window.width - 40, 1280)
        height: Math.min(window.height - 40, 860)
        property string selectedGoalId: ""
        property string editingGoalId: ""
        property bool editorActive: false
        property var goalTypeOptions: [
            { value: "count", label: "Count" },
            { value: "cadence", label: "Cadence" },
            { value: "balance", label: "Balance" }
        ]
        property var scopeTypeOptions: [
            { value: "pillar", label: "Pillar" },
            { value: "tag", label: "Tag" },
            { value: "channel", label: "Channel" },
            { value: "series", label: "Series" },
            { value: "kind", label: "Kind" }
        ]
        property var periodTypeOptions: [
            { value: "day", label: "day" },
            { value: "week", label: "week" },
            { value: "month", label: "month" },
            { value: "quarter", label: "quarter" },
            { value: "year", label: "year" },
            { value: "rolling_days", label: "days" }
        ]
        property var scopeOptionsModel: []
        property var balanceItemsModel: []

        function metricTypeForScope(scopeType) {
            return scopeType === "channel" ? "publication_count" : "content_count"
        }

        function metricLabel(scopeType, targetValue) {
            const metricType = metricTypeForScope(scopeType)
            if (metricType === "publication_count") {
                return targetValue === 1 ? "publication" : "publications"
            }
            return targetValue === 1 ? "content item" : "content items"
        }

        function periodPreview() {
            const periodType = periodTypeBox.currentValue || "week"
            const periodValue = periodValueBox.value
            if (periodType === "rolling_days") {
                return periodValue + " days"
            }
            if (periodValue > 1) {
                return periodValue + " " + periodType + "s"
            }
            return periodType
        }

        function summaryPreview() {
            const name = goalNameField.text.trim()
            const goalType = goalTypeBox.currentValue || "count"
            const scopeType = scopeTypeBox.currentValue || "pillar"
            if (goalType === "balance") {
                const parts = []
                const total = balanceItemsModel.reduce(function(sum, item) {
                    return item.weight > 0 ? sum + item.weight : sum
                }, 0)
                for (let index = 0; index < balanceItemsModel.length; ++index) {
                    const item = balanceItemsModel[index]
                    if (item.weight <= 0 || total <= 0) {
                        continue
                    }
                    parts.push(item.scopeDisplayName + " " + Math.round((item.weight * 100) / total) + "%")
                }
                return parts.length === 0 ? name : name + ": " + parts.join(", ")
            }

            const scopeName = scopeValueBox.currentIndex >= 0
                ? scopeValueBox.currentText
                : ""
            const targetValue = targetValueBox.value
            const qualifier = goalType === "cadence" ? "every" : "per"
            return scopeName.length > 0
                ? scopeName + ": at least " + targetValue + " " + metricLabel(scopeType, targetValue) + " " + qualifier + " " + periodPreview()
                : name
        }

        function balancePercentage(item) {
            if (!item || item.weight <= 0) {
                return ""
            }
            const total = balanceItemsModel.reduce(function(sum, current) {
                return current.weight > 0 ? sum + current.weight : sum
            }, 0)
            if (total <= 0) {
                return ""
            }
            return Math.round((item.weight * 100) / total) + "%"
        }

        function refreshScopeOptions(preferredScopeId, preferredWeights) {
            const scopeType = scopeTypeBox.currentValue || "pillar"
            scopeOptionsModel = appController.goalScopeOptions(scopeType)

            if ((goalTypeBox.currentValue || "count") === "balance") {
                const weightById = {}
                if (preferredWeights) {
                    for (let index = 0; index < preferredWeights.length; ++index) {
                        const weightedItem = preferredWeights[index]
                        weightById[weightedItem.scopeId] = weightedItem.weight
                    }
                } else {
                    for (let index = 0; index < balanceItemsModel.length; ++index) {
                        const existingItem = balanceItemsModel[index]
                        weightById[existingItem.scopeId] = existingItem.weight
                    }
                }

                balanceItemsModel = scopeOptionsModel.map(function(option, index) {
                    return {
                        id: "",
                        scopeId: option.lookupId,
                        scopeDisplayName: option.displayName,
                        weight: weightById[option.lookupId] !== undefined ? weightById[option.lookupId] : 0,
                        sortOrder: index
                    }
                })
                return
            }

            if (scopeOptionsModel.length === 0) {
                scopeValueBox.currentIndex = -1
                return
            }

            const targetScopeId = preferredScopeId && preferredScopeId.length > 0
                ? preferredScopeId
                : scopeOptionsModel[0].lookupId
            const scopeIndex = scopeValueBox.indexOfValue(targetScopeId)
            scopeValueBox.currentIndex = scopeIndex >= 0 ? scopeIndex : 0
        }

        function resetEditor(goalTypeValue) {
            editorActive = true
            editingGoalId = ""
            goalNameField.clear()
            goalEnabledBox.checked = true
            const nextGoalType = goalTypeValue || "count"
            const goalTypeIndex = goalTypeBox.indexOfValue(nextGoalType)
            goalTypeBox.currentIndex = goalTypeIndex >= 0 ? goalTypeIndex : 0
            scopeTypeBox.currentIndex = 0
            targetValueBox.value = 1
            periodValueBox.value = nextGoalType === "cadence" ? 7 : 1
            const periodIndex = periodTypeBox.indexOfValue(nextGoalType === "cadence" ? "rolling_days" : "week")
            periodTypeBox.currentIndex = periodIndex >= 0 ? periodIndex : 1
            balanceItemsModel = []
            refreshScopeOptions("", [])
        }

        function deactivateEditor() {
            editorActive = false
            editingGoalId = ""
            goalNameField.clear()
            goalEnabledBox.checked = true
            scopeOptionsModel = []
            balanceItemsModel = []
        }

        function editGoal(goalId) {
            const goal = appController.goalDetails(goalId)
            if (!goal.id) {
                return
            }

            editorActive = true
            editingGoalId = goal.id
            selectedGoalId = goal.id
            goalNameField.text = goal.name
            goalEnabledBox.checked = goal.enabled

            let index = goalTypeBox.indexOfValue(goal.goalType)
            goalTypeBox.currentIndex = index >= 0 ? index : 0
            index = scopeTypeBox.indexOfValue(goal.scopeType)
            scopeTypeBox.currentIndex = index >= 0 ? index : 0

            targetValueBox.value = goal.targetValue > 0 ? goal.targetValue : 1
            periodValueBox.value = goal.periodValue > 0 ? goal.periodValue : 1
            index = periodTypeBox.indexOfValue(goal.periodType.length > 0 ? goal.periodType : "week")
            periodTypeBox.currentIndex = index >= 0 ? index : 1

            refreshScopeOptions(goal.scopeId, goal.balanceItems || [])
            if (goal.goalType !== "balance") {
                index = scopeValueBox.indexOfValue(goal.scopeId)
                scopeValueBox.currentIndex = index >= 0 ? index : 0
            }
        }

        function saveEditor() {
            const goalType = goalTypeBox.currentValue || "count"
            const scopeType = scopeTypeBox.currentValue || "pillar"
            const scopeId = goalType === "balance"
                ? ""
                : (scopeValueBox.currentIndex >= 0 ? scopeValueBox.currentValue : "")
            const balanceItems = goalType === "balance"
                ? balanceItemsModel
                : []
            const ok = appController.saveGoal({
                id: editingGoalId,
                name: goalNameField.text,
                goalType: goalType,
                scopeType: scopeType,
                scopeId: scopeId,
                metricType: metricTypeForScope(scopeType),
                targetValue: targetValueBox.value,
                periodType: periodTypeBox.currentValue || "week",
                periodValue: periodValueBox.value,
                enabled: goalEnabledBox.checked
            }, balanceItems)
            if (ok) {
                goalsDialog.deactivateEditor()
            }
        }

        onOpened: goalsDialog.deactivateEditor()

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: "Add Goal"
                    onClicked: goalsDialog.resetEditor("count")
                }

                Button {
                    text: "Add Balance Goal"
                    onClicked: goalsDialog.resetEditor("balance")
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "Close"
                    onClicked: goalsDialog.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                Frame {
                    Layout.preferredWidth: 470
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            text: "Configured Goals"
                            font.bold: true
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: appController.goalsModel
                            spacing: 8

                            delegate: Rectangle {
                                required property string goalId
                                required property string summaryText
                                required property string goalType
                                required property string scopeType
                                required property bool enabled
                                width: ListView.view.width
                                implicitHeight: goalRow.implicitHeight + 12
                                radius: 6
                                color: goalsDialog.selectedGoalId === goalId ? "#eef5ff" : "#ffffff"
                                border.color: goalsDialog.selectedGoalId === goalId ? "#4e79a7" : "#d6d6d6"

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: goalsDialog.selectedGoalId = goalId
                                    onDoubleClicked: goalsDialog.editGoal(goalId)
                                }

                                RowLayout {
                                    id: goalRow
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 8

                                    CheckBox {
                                        checked: enabled
                                        z: 1
                                        onToggled: appController.setGoalEnabled(goalId, checked)
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true

                                        Label {
                                            Layout.fillWidth: true
                                            text: summaryText
                                            wrapMode: Text.Wrap
                                            font.bold: true
                                        }

                                        Label {
                                            text: goalType + " | " + scopeType
                                            color: "#666666"
                                        }
                                    }

                                    Button {
                                        text: "Edit"
                                        z: 1
                                        onClicked: goalsDialog.editGoal(goalId)
                                    }

                                    Button {
                                        text: "Del"
                                        z: 1
                                        onClicked: {
                                            if (appController.deleteGoal(goalId)) {
                                                if (goalsDialog.selectedGoalId === goalId) {
                                                    goalsDialog.selectedGoalId = ""
                                                }
                                                if (goalsDialog.editingGoalId === goalId) {
                                                    goalsDialog.deactivateEditor()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 16

                        Label {
                            text: !goalsDialog.editorActive
                                ? "Goal Editor"
                                : (goalsDialog.editingGoalId.length > 0 ? "Edit Goal" : "New Goal")
                            font.bold: true
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            enabled: goalsDialog.editorActive
                            opacity: goalsDialog.editorActive ? 1.0 : 0.45
                            clip: true
                            contentWidth: availableWidth

                            ColumnLayout {
                                width: parent.width
                                spacing: 16

                                Label {
                                    Layout.fillWidth: true
                                    visible: !goalsDialog.editorActive
                                    wrapMode: Text.Wrap
                                    text: "Select an existing goal to edit, or click Add Goal / Add Balance Goal to start a new one."
                                    color: "#666666"
                                }

                                GridLayout {
                                    columns: 2
                                    columnSpacing: 12
                                    rowSpacing: 12
                                    Layout.fillWidth: true

                                    Label { text: "Name" }
                                    TextField {
                                        id: goalNameField
                                        Layout.fillWidth: true
                                        placeholderText: "Goal name"
                                    }

                                    Label { text: "Goal Type" }
                                    ComboBox {
                                        id: goalTypeBox
                                        Layout.fillWidth: true
                                        model: goalsDialog.goalTypeOptions
                                        textRole: "label"
                                        valueRole: "value"
                                        onActivated: {
                                            if (currentValue === "balance") {
                                                const rollingIndex = periodTypeBox.indexOfValue("rolling_days")
                                                periodTypeBox.currentIndex = rollingIndex >= 0 ? rollingIndex : 0
                                            }
                                            goalsDialog.refreshScopeOptions("", [])
                                        }
                                    }

                                    Label { text: "Track" }
                                    ComboBox {
                                        id: scopeTypeBox
                                        Layout.fillWidth: true
                                        model: goalsDialog.scopeTypeOptions
                                        textRole: "label"
                                        valueRole: "value"
                                        onActivated: goalsDialog.refreshScopeOptions("", [])
                                    }

                                    Label {
                                        text: "Which"
                                        visible: goalTypeBox.currentValue !== "balance"
                                    }
                                    ComboBox {
                                        id: scopeValueBox
                                        Layout.fillWidth: true
                                        visible: goalTypeBox.currentValue !== "balance"
                                        model: goalsDialog.scopeOptionsModel
                                        textRole: "displayName"
                                        valueRole: "lookupId"
                                    }

                                    Label {
                                        text: "Metric"
                                        visible: goalTypeBox.currentValue !== "balance"
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: goalTypeBox.currentValue !== "balance"
                                        text: goalsDialog.metricTypeForScope(scopeTypeBox.currentValue || "pillar") === "publication_count"
                                            ? "Publications"
                                            : "Content items"
                                    }

                                    Label {
                                        text: "Target"
                                        visible: goalTypeBox.currentValue !== "balance"
                                    }
                                    SpinBox {
                                        id: targetValueBox
                                        visible: goalTypeBox.currentValue !== "balance"
                                        from: 1
                                        to: 1000
                                        value: 1
                                    }

                                    Label {
                                        text: "Period"
                                        visible: goalTypeBox.currentValue !== "balance"
                                    }
                                    RowLayout {
                                        visible: goalTypeBox.currentValue !== "balance"

                                        SpinBox {
                                            id: periodValueBox
                                            from: 1
                                            to: 365
                                            value: 1
                                        }

                                        ComboBox {
                                            id: periodTypeBox
                                            Layout.preferredWidth: 180
                                            model: goalsDialog.periodTypeOptions
                                            textRole: "label"
                                            valueRole: "value"
                                        }
                                    }

                                    Label { text: "Enabled" }
                                    CheckBox {
                                        id: goalEnabledBox
                                        checked: true
                                    }
                                }

                                Frame {
                                    Layout.fillWidth: true
                                    visible: goalTypeBox.currentValue === "balance"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 12

                                        Label {
                                            text: "Balance Items"
                                            font.bold: true
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: "Use integer weights. Items with zero weight are excluded from the balance."
                                            wrapMode: Text.Wrap
                                            color: "#666666"
                                        }

                                        Repeater {
                                            model: goalsDialog.balanceItemsModel

                                            delegate: RowLayout {
                                                required property var modelData
                                                required property int index
                                                Layout.fillWidth: true

                                                Label {
                                                    Layout.preferredWidth: 180
                                                    text: modelData.scopeDisplayName
                                                }

                                                Slider {
                                                    Layout.fillWidth: true
                                                    from: 0
                                                    to: 10
                                                    stepSize: 1
                                                    value: modelData.weight
                                                    onMoved: {
                                                        const nextItems = goalsDialog.balanceItemsModel.slice()
                                                        nextItems[index] = {
                                                            id: modelData.id,
                                                            scopeId: modelData.scopeId,
                                                            scopeDisplayName: modelData.scopeDisplayName,
                                                            weight: Math.round(value),
                                                            sortOrder: modelData.sortOrder
                                                        }
                                                        goalsDialog.balanceItemsModel = nextItems
                                                    }
                                                }

                                                SpinBox {
                                                    from: 0
                                                    to: 10
                                                    value: modelData.weight
                                                    onValueModified: {
                                                        const nextItems = goalsDialog.balanceItemsModel.slice()
                                                        nextItems[index] = {
                                                            id: modelData.id,
                                                            scopeId: modelData.scopeId,
                                                            scopeDisplayName: modelData.scopeDisplayName,
                                                            weight: value,
                                                            sortOrder: modelData.sortOrder
                                                        }
                                                        goalsDialog.balanceItemsModel = nextItems
                                                    }
                                                }

                                                Label {
                                                    Layout.preferredWidth: 56
                                                    horizontalAlignment: Text.AlignRight
                                                    text: goalsDialog.balancePercentage(modelData)
                                                }
                                            }
                                        }
                                    }
                                }

                                Frame {
                                    Layout.fillWidth: true

                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 8

                                        Label {
                                            text: "Preview"
                                            font.bold: true
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            wrapMode: Text.Wrap
                                            text: goalsDialog.summaryPreview()
                                        }
                                    }
                                }
                            }
                        }

                        DialogButtonBox {
                            Layout.fillWidth: true
                            enabled: goalsDialog.editorActive
                            standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                            onAccepted: goalsDialog.saveEditor()
                            onRejected: goalsDialog.deactivateEditor()
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: createSeriesDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        property string editingSeriesId: ""
        title: editingSeriesId.length > 0 ? qsTr("Edit Series") : qsTr("Create Series")
        width: Math.min(window.width - 80, 620)

        function resetForm() {
            editingSeriesId = ""
            seriesNameField.clear()
            seriesDescriptionField.clear()
            seriesStatusBox.currentIndex = 0
            if (seriesPillarBox.count > 0) {
                seriesPillarBox.currentIndex = 0
            }
        }

        function openForCreate() {
            resetForm()
            open()
        }

        function openForEdit() {
            const series = appController.currentSeriesDetails
            if (!series.id) {
                return
            }

            editingSeriesId = series.id
            seriesNameField.text = series.name ? series.name : ""
            seriesDescriptionField.text = series.description ? series.description : ""

            const pillarIndex = seriesPillarBox.indexOfValue(series.pillarId ? series.pillarId : "")
            if (pillarIndex >= 0) {
                seriesPillarBox.currentIndex = pillarIndex
            } else if (seriesPillarBox.count > 0) {
                seriesPillarBox.currentIndex = 0
            }

            const statusIndex = seriesStatusBox.indexOfValue(series.status ? series.status : "active")
            seriesStatusBox.currentIndex = statusIndex >= 0 ? statusIndex : 0
            open()
        }

        ColumnLayout {
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
                    id: seriesNameField
                    Layout.fillWidth: true
                    placeholderText: "Series name"
                }

                Label { text: "Description" }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    TextArea {
                        id: seriesDescriptionField
                        width: parent.width
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                    }
                }

                Label { text: "Pillar" }
                ComboBox {
                    id: seriesPillarBox
                    Layout.fillWidth: true
                    model: appController.pillarModel
                    textRole: "displayName"
                    valueRole: "lookupId"
                }

                Label { text: "Status" }
                ComboBox {
                    id: seriesStatusBox
                    Layout.fillWidth: true
                    model: window.seriesStatusOptions
                    textRole: "label"
                    valueRole: "value"
                }
            }

            DialogButtonBox {
                Layout.fillWidth: true
                standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                onAccepted: {
                    const pillarId = seriesPillarBox.currentIndex >= 0 ? seriesPillarBox.currentValue : ""
                    const status = seriesStatusBox.currentIndex >= 0 ? seriesStatusBox.currentValue : "active"
                    if (appController.saveSeries(createSeriesDialog.editingSeriesId,
                                                 seriesNameField.text,
                                                 seriesDescriptionField.text,
                                                 pillarId,
                                                 status)) {
                        createSeriesDialog.resetForm()
                        createSeriesDialog.close()
                    }
                }

                onRejected: {
                    createSeriesDialog.resetForm()
                    createSeriesDialog.close()
                }
            }
        }
    }

    Dialog {
        id: assignSeriesContentDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Add Existing Content")
        width: Math.min(window.width - 80, 720)
        property var contentOptions: []

        function reloadOptions() {
            contentOptions = appController.assignableContentOptionsForCurrentSeries()
            existingSeriesContentBox.currentIndex = contentOptions.length > 0 ? 0 : -1
        }

        function openForCurrentSeries() {
            reloadOptions()
            open()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "Assign an existing content item to the selected series."
            }

            ComboBox {
                id: existingSeriesContentBox
                Layout.fillWidth: true
                model: assignSeriesContentDialog.contentOptions
                textRole: "title"
                valueRole: "contentId"
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "#555555"
                visible: existingSeriesContentBox.currentIndex >= 0
                text: existingSeriesContentBox.currentIndex >= 0
                    ? [assignSeriesContentDialog.contentOptions[existingSeriesContentBox.currentIndex].status,
                       assignSeriesContentDialog.contentOptions[existingSeriesContentBox.currentIndex].series]
                          .filter(function(value) { return value && value.length > 0 })
                          .join(" | ")
                    : ""
            }

            DialogButtonBox {
                Layout.fillWidth: true
                standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                onAccepted: {
                    const contentId = existingSeriesContentBox.currentIndex >= 0 ? existingSeriesContentBox.currentValue : ""
                    if (appController.assignContentToCurrentSeries(contentId)) {
                        assignSeriesContentDialog.close()
                    }
                }

                onRejected: assignSeriesContentDialog.close()
            }
        }
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&App")

            MenuItem { action: importFromFileAction }

            MenuSeparator {}

            MenuItem { action: goalsAction }
            MenuItem { action: settingsAction }

            MenuSeparator {}

            MenuItem { action: quitAction }
        }

        Menu {
            title: qsTr("&Edit")

            MenuItem { action: copyAction }
            MenuItem { action: pasteToIdeaAction }
        }

        Menu {
            title: qsTr("&Help")

            MenuItem {
                text: qsTr("Blog")
                enabled: appInfo.blogUrl.length > 0
                onTriggered: Qt.openUrlExternally(appInfo.blogUrl)
            }

            MenuItem {
                text: qsTr("About")
                onTriggered: aboutDialog.open()
            }
        }
    }

    TabBar {
        id: tabBar
        width: parent.width

        TabButton { text: "Inbox" }
        TabButton { text: "Board" }
        TabButton { text: "Calendar" }
        TabButton { text: "Sources" }
        TabButton { text: "Series" }
        TabButton { text: "All Content" }
        TabButton { text: "Dashboard" }
    }

    StackLayout {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentIndex: tabBar.currentIndex

        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 16

                SectionCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Inbox"

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: appController.inboxModel
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

                        delegate: ContentSummaryCard {
                            required property string itemId
                            required property string title
                            required property string description
                            required property string displayTags
                            required property string pillar
                            required property string kind
                            required property string series
                            required property string suggestedChannel
                            required property int priority
                            width: ListView.view.width
                            titleText: title
                            bodyText: description
                            bodyWordCap: appSettings.cardDescriptionWordCap
                            markdownEnabled: appSettings.cardDescriptionMarkdownEnabled
                            metaText: [pillar, kind, "Pri " + priority, series, suggestedChannel].filter(function(value) { return value.length > 0 }).join(" | ")
                            onEditRequested: quickAddDialog.openForEdit(itemId)
                            onDeleteRequested: deleteContentDialog.openForContent(itemId, title)

                            Label {
                                text: displayTags
                                visible: text.length > 0
                                color: "#2f6f44"
                                wrapMode: Text.Wrap
                            }

                            RowLayout {
                                Button {
                                    text: "Clarifying"
                                    onClicked: appController.moveContentToStatus(itemId, "clarifying")
                                }
                                Button {
                                    text: "Shaping"
                                    onClicked: appController.moveContentToStatus(itemId, "shaping")
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    DropArea {
                        id: inboxImportDropArea
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        implicitHeight: 40

                        onEntered: drag => {
                            const localPath = appController.acceptableIdeaImportPath(drag.urls ? drag.urls : [],
                                                                                     drag.text ? drag.text.toString() : "")
                            if (localPath.length > 0) {
                                drag.accept()
                            }
                        }

                        onDropped: function(drop) {
                            const localPath = appController.acceptableIdeaImportPath(drop.urls ? drop.urls : [],
                                                                                     drop.text ? drop.text.toString() : "")
                            if (localPath.length > 0) {
                                appController.importIdeasFromFile(localPath)
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            implicitHeight: 40
                            radius: 6
                            color: inboxImportDropArea.containsDrag ? "#eef7ee" : "#fafafa"
                            border.width: 1
                            border.color: inboxImportDropArea.containsDrag ? "#2e7d32" : "#d7d7d7"

                            Label {
                                anchors.centerIn: parent
                                text: "Drop .md or .txt here"
                                color: "#666666"
                            }
                        }
                    }

                    Button {
                        height: 40
                        text: "Quick Add"
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: "#2e7d32"
                        }
                        onClicked: quickAddDialog.openForCreate()
                    }

                    Button {
                        height: 40
                        text: "Add from Clipboard"
                        enabled: appController.clipboardHasText
                        contentItem: Text {
                            text: parent.text
                            color: "#202020"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: enabled ? "#f2c94c" : "#d9d0a7"
                        }
                        onClicked: appController.pasteClipboardToIdea()
                    }
                }
            }
        }

        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                RowLayout {
                    Switch {
                        text: "Show archived"
                        checked: appController.boardShowArchived
                        onToggled: appController.boardShowArchived = checked
                    }

                    Button {
                        text: "Refresh"
                        onClicked: appController.refreshAll()
                    }
                }

                Flickable {
                    id: boardFlickable
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: boardRow.width
                    contentHeight: boardRow.height
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.horizontal: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                    }

                    Row {
                        id: boardRow
                        spacing: 12
                        height: boardFlickable.height

                        Repeater {
                            model: appController.contentStatusModel
                            delegate: BoardColumn {
                                required property string statusId
                                required property string displayName
                                required property string info

                                visible: statusId !== "archived" || appController.boardShowArchived
                                columnTitle: displayName
                                statusKey: statusId
                                infoText: info
                                model: appController.boardModelForStatus(statusId)
                                editDialog: quickAddDialog
                                dragLayer: window.contentItem
                                height: boardFlickable.height
                            }
                        }
                    }
                }
            }
        }

        ScrollView {
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 12

                SectionCard {
                    title: "Calendar"

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true

                                Switch {
                                    text: "Show archived"
                                    checked: appController.calendarIncludeArchived
                                    onToggled: appController.calendarIncludeArchived = checked
                                }

                                Switch {
                                    text: "Show published"
                                    checked: appController.calendarIncludePublished
                                    onToggled: appController.calendarIncludePublished = checked
                                }

                                Item {
                                    Layout.fillWidth: true
                                }
                            }

                            ScrollView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 716
                                clip: true

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 12

                                    Repeater {
                                        model: window.calendarSections

                                        delegate: Frame {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            padding: 12
                                            background: Rectangle {
                                                radius: 8
                                                color: "#ffffff"
                                                border.width: modelData.isToday ? 2 : 1
                                                border.color: modelData.isToday ? "#2f9e44" : "#d5d5d5"
                                            }

                                            ColumnLayout {
                                                anchors.fill: parent
                                                spacing: 8

                                                RowLayout {
                                                    Layout.fillWidth: true

                                                    Label {
                                                        text: modelData.dateKey
                                                        font.bold: true
                                                        font.pixelSize: 18
                                                        color: "#111111"
                                                    }

                                                    Rectangle {
                                                        visible: modelData.hasContent
                                                        radius: 10
                                                        color: "#3f6fa8"
                                                        implicitHeight: 24
                                                        implicitWidth: createBadgeLabel.implicitWidth + 18

                                                        Label {
                                                            id: createBadgeLabel
                                                            anchors.centerIn: parent
                                                            text: "Create"
                                                            color: "white"
                                                            font.bold: true
                                                        }
                                                    }

                                                    Rectangle {
                                                        visible: modelData.hasPublication
                                                        radius: 10
                                                        color: "#3d7f4a"
                                                        implicitHeight: 24
                                                        implicitWidth: publishBadgeLabel.implicitWidth + 18

                                                        Label {
                                                            id: publishBadgeLabel
                                                            anchors.centerIn: parent
                                                            text: "Publish"
                                                            color: "white"
                                                            font.bold: true
                                                        }
                                                    }

                                                    Item {
                                                        Layout.fillWidth: true
                                                    }
                                                }

                                                Repeater {
                                                    model: modelData.items

                                                    delegate: Rectangle {
                                                        required property var modelData
                                                        Layout.fillWidth: true
                                                        radius: 6
                                                        color: modelData.sourceType === "publication" ? "#e7f6e9" : "#e8f2ff"
                                                        border.width: modelData.isOverdue ? 2 : 1
                                                        border.color: modelData.isOverdue
                                                            ? "#d94841"
                                                            : (modelData.sourceType === "publication" ? "#b7d9bf" : "#bfd4ee")
                                                        implicitHeight: calendarCardLayout.implicitHeight + 16

                                                        ColumnLayout {
                                                            id: calendarCardLayout
                                                            anchors.fill: parent
                                                            anchors.margins: 8
                                                            spacing: 4

                                                            Rectangle {
                                                                radius: 10
                                                                color: modelData.sourceType === "publication" ? "#3d7f4a" : "#3f6fa8"
                                                                implicitHeight: 24
                                                                implicitWidth: calendarTypeLabel.implicitWidth + 18

                                                                Label {
                                                                    id: calendarTypeLabel
                                                                    anchors.centerIn: parent
                                                                    text: modelData.sourceType === "publication" ? "Publish" : "Create"
                                                                    color: "white"
                                                                    font.bold: true
                                                                }
                                                            }

                                                            Label {
                                                                text: modelData.title
                                                                font.bold: true
                                                                color: "#111111"
                                                                wrapMode: Text.Wrap
                                                                Layout.fillWidth: true
                                                            }

                                                            Label {
                                                                text: [modelData.series, modelData.channel].filter(function(value) { return value.length > 0 }).join(" | ")
                                                                visible: text.length > 0
                                                                color: "#555555"
                                                                wrapMode: Text.Wrap
                                                                Layout.fillWidth: true
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                }
            }
        }

        Item {
            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                SectionCard {
                    Layout.preferredWidth: 420
                    Layout.fillHeight: true
                    title: "Source Content"

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8

                        ListView {
                            id: sourceList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: appController.sourceModel
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

                            delegate: ContentSummaryCard {
                                required property string itemId
                                required property int priority
                                required property string title
                                required property string displayTags
                                required property string kind
                                required property string series
                                required property string status
                                required property string suggestedChannel

                                width: ListView.view.width
                                selected: appController.currentSourceId === itemId
                                borderWidthOverride: appController.contentHasDerivedItems(itemId) ? 2 : 0
                                borderColorOverride: "#2f6f44"
                                titleText: title
                                metaText: [kind, "Pri " + priority, status, series, suggestedChannel].filter(function(value) { return value.length > 0 }).join(" | ")
                                onClicked: appController.currentSourceId = itemId
                                onEditRequested: quickAddDialog.openForEdit(itemId)
                                onDeleteRequested: deleteContentDialog.openForContent(itemId, title)

                                Label {
                                    text: displayTags
                                    visible: text.length > 0
                                    color: "#2f6f44"
                                    wrapMode: Text.Wrap
                                }
                            }
                        }

                        Button {
                            text: "Create Burst From Source"
                            enabled: appController.currentSourceId.length > 0
                            onClicked: burstDialog.openForSource()
                        }
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Derivatives"

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: appController.derivativeModel
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

                        delegate: ContentSummaryCard {
                            required property string itemId
                            required property string title
                            required property string kind
                            required property string outcome
                            required property string suggestedChannel
                            required property string status
                            required property string burstTemplateKey
                            required property string displayTags
                            required property int priority
                            required property string series
                            width: ListView.view.width
                            titleText: title
                            metaText: [kind, "Pri " + priority, series, outcome, suggestedChannel, status].filter(function(value) { return value.length > 0 }).join(" | ")
                            onEditRequested: quickAddDialog.openForEdit(itemId)
                            onDeleteRequested: deleteContentDialog.openForContent(itemId, title)

                            Label {
                                text: displayTags
                                visible: text.length > 0
                                color: "#2f6f44"
                                wrapMode: Text.Wrap
                            }

                            Label {
                                text: "Template: " + burstTemplateKey
                                visible: burstTemplateKey.length > 0
                                color: "#555555"
                            }
                        }
                    }
                }
            }
        }

        ScrollView {
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 16

                SectionCard {
                    Layout.fillWidth: true
                    title: "Series"

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 760
                        spacing: 16

                        ColumnLayout {
                            Layout.preferredWidth: 340
                            Layout.fillHeight: true
                            spacing: 12

                            RowLayout {
                                Layout.fillWidth: true

                                Switch {
                                    text: "Show archived"
                                    checked: appController.seriesShowArchived
                                    onToggled: appController.seriesShowArchived = checked
                                }
                            }

                            TextField {
                                Layout.fillWidth: true
                                placeholderText: "Search series name or description"
                                text: appController.seriesSearchQuery
                                onTextEdited: appController.seriesSearchQuery = text
                            }

                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                model: appController.seriesModel
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
                                    required property string seriesId
                                    required property string name
                                    required property string description
                                    required property string pillar
                                    required property string status
                                    required property int contentCount
                                    required property int scheduledCount
                                    readonly property bool archivedSeries: status === "archived"
                                    width: ListView.view.width
                                    radius: 6
                                    color: appController.currentSeriesId === seriesId ? "#e8f2ff" : "white"
                                    border.color: archivedSeries ? "#d8b400"
                                                                  : appController.currentSeriesId === seriesId ? "#4a80d8"
                                                                                                              : "#d5d5d5"
                                    border.width: archivedSeries ? 2 : 1
                                    implicitHeight: layout.implicitHeight + 16

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: appController.currentSeriesId = seriesId
                                    }

                                    ColumnLayout {
                                        id: layout
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 4

                                        RowLayout {
                                            Layout.fillWidth: true

                                            Label {
                                                Layout.fillWidth: true
                                                text: name
                                                font.bold: true
                                            }

                                            ToolButton {
                                                icon.name: "document-edit"
                                                text: "Edit series"
                                                display: AbstractButton.IconOnly
                                                ToolTip.visible: hovered
                                                ToolTip.text: text
                                                onClicked: {
                                                    appController.currentSeriesId = seriesId
                                                    createSeriesDialog.openForEdit()
                                                }
                                            }

                                            ToolButton {
                                                icon.name: "edit-delete"
                                                text: "Delete series"
                                                display: AbstractButton.IconOnly
                                                ToolTip.visible: hovered
                                                ToolTip.text: text
                                                onClicked: {
                                                    appController.currentSeriesId = seriesId
                                                    appController.deleteCurrentSeries()
                                                }
                                            }
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            wrapMode: Text.Wrap
                                            text: [pillar, status, contentCount + " items", scheduledCount + " scheduled"]
                                                .filter(function(value) { return value.length > 0 })
                                                .join(" | ")
                                        }

                                        Label {
                                            visible: text.length > 0
                                            text: window.clippedCardDescription(description)
                                            textFormat: appSettings.cardDescriptionMarkdownEnabled ? Text.MarkdownText : Text.PlainText
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }
                            }

                            Button {
                                text: "Create Series"
                                Layout.alignment: Qt.AlignRight
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: "#2e7d32"
                                }
                                onClicked: createSeriesDialog.openForCreate()
                            }
                        }

                        Frame {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 12

                                RowLayout {
                                    Layout.fillWidth: true

                                    Button {
                                        text: "Quick Add New Idea"
                                        enabled: appController.currentSeriesId.length > 0
                                        onClicked: quickAddDialog.openForCreate(appController.currentSeriesId)
                                    }

                                    Button {
                                        text: "Add Existing"
                                        enabled: appController.currentSeriesId.length > 0
                                        onClicked: assignSeriesContentDialog.openForCurrentSeries()
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                Label {
                                    text: "Series Content"
                                    font.bold: true
                                }

                                Label {
                                    visible: appController.currentSeriesId.length === 0
                                    text: "Select a series to view its content."
                                    color: "#666666"
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    visible: appController.currentSeriesId.length > 0 && appController.seriesContentModel.rowCount() === 0
                                    text: "This series has no ideas yet. Start by adding a new idea or assigning an existing one."
                                    color: "#666666"
                                    wrapMode: Text.Wrap
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                ListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    model: appController.seriesContentModel
                                    spacing: 8
                                    visible: appController.currentSeriesId.length > 0 && appController.seriesContentModel.rowCount() > 0

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
                                        required property string itemId
                                        required property string title
                                        required property string description
                                        required property string kind
                                        required property string pillar
                                        required property string suggestedChannel
                                        required property string status
                                        required property int priority
                                        required property var seriesPosition
                                        required property date scheduledAt
                                        required property date publishedAt
                                        width: ListView.view.width
                                        radius: 6
                                        color: "#ffffff"
                                        border.color: "#d6d6d6"
                                        implicitHeight: seriesItemLayout.implicitHeight + 12

                                        RowLayout {
                                            id: seriesItemLayout
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 8

                                            Rectangle {
                                                Layout.preferredWidth: 36
                                                Layout.alignment: Qt.AlignTop
                                                radius: 18
                                                color: "#edf3ff"
                                                border.color: "#c9d8f7"
                                                implicitHeight: 36

                                                Label {
                                                    anchors.centerIn: parent
                                                    text: seriesPosition !== undefined ? seriesPosition : "?"
                                                    font.bold: true
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: title
                                                    font.bold: true
                                                    wrapMode: Text.Wrap
                                                }

                                                Label {
                                                    Layout.fillWidth: true
                                                    wrapMode: Text.Wrap
                                                    text: [pillar, kind, "Pri " + priority, status, suggestedChannel]
                                                        .filter(function(value) { return value.length > 0 })
                                                        .join(" | ")
                                                }

                                                Label {
                                                    Layout.fillWidth: true
                                                    visible: description.length > 0
                                                    text: window.clippedCardDescription(description)
                                                    textFormat: appSettings.cardDescriptionMarkdownEnabled ? Text.MarkdownText : Text.PlainText
                                                    wrapMode: Text.Wrap
                                                }

                                                Label {
                                                    Layout.fillWidth: true
                                                    visible: text.length > 0
                                                    color: "#555555"
                                                    text: [
                                                        scheduledAt ? "Scheduled " + Qt.formatDateTime(scheduledAt, "yyyy-MM-dd") : "",
                                                        publishedAt ? "Published " + Qt.formatDateTime(publishedAt, "yyyy-MM-dd") : ""
                                                    ].filter(function(value) { return value.length > 0 }).join(" | ")
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.alignment: Qt.AlignTop
                                                spacing: 4

                                                ToolButton {
                                                    icon.name: "go-up"
                                                    text: "Move up"
                                                    display: AbstractButton.IconOnly
                                                    ToolTip.visible: hovered
                                                    ToolTip.text: text
                                                    onClicked: appController.moveSeriesContent(itemId, -1)
                                                }

                                                ToolButton {
                                                    icon.name: "go-down"
                                                    text: "Move down"
                                                    display: AbstractButton.IconOnly
                                                    ToolTip.visible: hovered
                                                    ToolTip.text: text
                                                    onClicked: appController.moveSeriesContent(itemId, 1)
                                                }

                                                ToolButton {
                                                    icon.name: "document-edit"
                                                    text: "Edit content"
                                                    display: AbstractButton.IconOnly
                                                    ToolTip.visible: hovered
                                                    ToolTip.text: text
                                                    onClicked: quickAddDialog.openForEdit(itemId)
                                                }

                                                ToolButton {
                                                    icon.name: "list-remove"
                                                    text: "Remove from series"
                                                    display: AbstractButton.IconOnly
                                                    ToolTip.visible: hovered
                                                    ToolTip.text: text
                                                    onClicked: appController.removeContentFromCurrentSeries(itemId)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true

                    Switch {
                        text: "Show archived"
                        checked: appController.allContentShowArchived
                        onToggled: appController.allContentShowArchived = checked
                    }

                    Label {
                        text: "Sort"
                    }

                    ComboBox {
                        id: allContentSortBox
                        Layout.preferredWidth: 260
                        model: window.allContentSortOptions
                        textRole: "label"
                        valueRole: "mode"
                        Component.onCompleted: currentIndex = appController.allContentSortMode()
                        onActivated: appController.setAllContentSortMode(currentValue)
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "All Content"

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: appController.allContentModel
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

                        delegate: ContentSummaryCard {
                            required property string itemId
                            required property int priority
                            required property string title
                            required property string description
                            required property string displayTags
                            required property string pillar
                            required property string kind
                            required property string status
                            required property string series
                            required property string suggestedChannel

                            width: ListView.view.width
                            titleText: title
                            bodyText: description
                            bodyWordCap: appSettings.cardDescriptionWordCap
                            markdownEnabled: appSettings.cardDescriptionMarkdownEnabled
                            metaText: [pillar, kind, "Pri " + priority, status, series, suggestedChannel].filter(function(value) { return value.length > 0 }).join(" | ")
                            onEditRequested: quickAddDialog.openForEdit(itemId)
                            onDeleteRequested: deleteContentDialog.openForContent(itemId, title)

                            Label {
                                text: displayTags
                                visible: text.length > 0
                                color: "#2f6f44"
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
        }

        DashboardView {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    footer: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8

            Label {
                text: appController.statusMessage
                color: "#444444"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }
    }
}

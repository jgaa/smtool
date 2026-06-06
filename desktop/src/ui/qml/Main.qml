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
    property var allContentSortOptions: [
        { label: "Due Date / Alphabetically", mode: 0 },
        { label: "Alphabetically", mode: 1 },
        { label: "Status / Alphabetically", mode: 2 },
        { label: "Status / Due Date", mode: 3 },
        { label: "Status / First Publish Date", mode: 4 },
        { label: "Pillar / Alphabetically", mode: 5 },
        { label: "Pillar / Due Date", mode: 6 },
        { label: "Pillar / First Publish Date", mode: 7 }
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
            selectedKeys = optionsModel.map(function(option) { return option.key })
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
        id: quickAddDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        property string editingContentId: ""
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
            priorityBox.value = 0
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

        function openForCreate() {
            resetForm()
            open()
        }

        function openForEdit(contentId) {
            const item = appController.contentDetails(contentId)
            if (!item.id) {
                return
            }

            editingContentId = item.id
            inboxTitleField.text = item.title
            inboxDescriptionField.text = item.description
            inboxTagsField.text = item.tags
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
                                columns: quickAddDialog.editingContentId.length > 0 ? 8 : 6
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

                                Label { text: "Ch" }
                                ComboBox {
                                    id: channelBox
                                    Layout.fillWidth: true
                                    model: appController.channelModel
                                    textRole: "displayName"
                                    valueRole: "lookupId"
                                }

                                Label { text: "Priority" }
                                SpinBox {
                                    id: priorityBox
                                    from: 0
                                    to: 100
                                    value: 0
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

                            TextArea {
                                id: inboxDescriptionField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 220
                                wrapMode: TextEdit.Wrap
                                placeholderText: "Optional note"
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
                    const channelId = channelBox.currentIndex >= 0 ? channelBox.currentValue : ""
                    const status = statusBox.currentIndex >= 0 ? statusBox.currentValue : "inbox"
                    const ok = quickAddDialog.editingContentId.length > 0
                        ? appController.updateContent(quickAddDialog.editingContentId,
                                                      inboxTitleField.text,
                                                      inboxDescriptionField.text,
                                                      inboxTagsField.text,
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
                                                        "",
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
        id: createSeriesDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose
        title: qsTr("Create Series")
        width: Math.min(window.width - 80, 620)

        function resetForm() {
            seriesNameField.clear()
            seriesDescriptionField.clear()
            if (seriesPillarBox.count > 0) {
                seriesPillarBox.currentIndex = 0
            }
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
                TextArea {
                    id: seriesDescriptionField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    wrapMode: TextEdit.Wrap
                }

                Label { text: "Pillar" }
                ComboBox {
                    id: seriesPillarBox
                    Layout.fillWidth: true
                    model: appController.pillarModel
                    textRole: "displayName"
                    valueRole: "lookupId"
                }
            }

            DialogButtonBox {
                Layout.fillWidth: true
                standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel

                onAccepted: {
                    const pillarId = seriesPillarBox.currentIndex >= 0 ? seriesPillarBox.currentValue : ""
                    if (appController.createSeries(seriesNameField.text, seriesDescriptionField.text, pillarId)) {
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

    menuBar: MenuBar {
        Menu {
            title: qsTr("&App")

            MenuItem { action: importFromFileAction }

            MenuSeparator {}

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
                            width: ListView.view.width
                            titleText: title
                            bodyText: description
                            metaText: [pillar, kind, suggestedChannel].filter(function(value) { return value.length > 0 }).join(" | ")
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

                    Item {
                        Layout.fillWidth: true
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
                                required property string title
                                required property string displayTags
                                required property string kind
                                required property string series
                                required property string status
                                required property string suggestedChannel

                                width: ListView.view.width
                                selected: appController.currentSourceId === itemId
                                titleText: title
                                metaText: [kind, status, series, suggestedChannel].filter(function(value) { return value.length > 0 }).join(" | ")
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
                            required property string displayTags
                            width: ListView.view.width
                            titleText: title
                            metaText: [kind, outcome, suggestedChannel, status].filter(function(value) { return value.length > 0 }).join(" | ")
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

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(520, contentHeight)
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
                            width: ListView.view.width
                            radius: 6
                            color: "white"
                            border.color: "#d5d5d5"
                            implicitHeight: layout.implicitHeight + 16

                            ColumnLayout {
                                id: layout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                Label {
                                    text: name
                                    font.bold: true
                                }

                                Label {
                                    text: [pillar, status, contentCount + " items", scheduledCount + " scheduled"].join(" | ")
                                }

                                Label {
                                    text: description
                                    visible: text.length > 0
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        text: "Create Series"
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
                        onClicked: createSeriesDialog.open()
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
                            metaText: [pillar, kind, status, series, suggestedChannel].filter(function(value) { return value.length > 0 }).join(" | ")
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

        ScrollView {
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 16

                Switch {
                    text: "Include archived"
                    checked: appController.dashboardIncludeArchived
                    onToggled: appController.dashboardIncludeArchived = checked
                }

                SectionCard {
                    title: "By Pillar"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        model: appController.dashboardByPillarModel
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
                        delegate: Label { text: label + ": " + value }
                    }
                }

                SectionCard {
                    title: "By Series"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        model: appController.dashboardBySeriesModel
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
                        delegate: Label { text: label + ": " + value + (secondary.length ? " | " + secondary : "") }
                    }
                }

                SectionCard {
                    title: "By Status"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        model: appController.dashboardByStatusModel
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
                        delegate: Label { text: label + ": " + value }
                    }
                }

                SectionCard {
                    title: "Upcoming 14 Days"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        model: appController.dashboardUpcomingModel
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
                        delegate: Label { text: label + " | " + secondary }
                    }
                }

                SectionCard {
                    title: "Published Content 30 Days"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        model: appController.dashboardPublishedContentModel
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
                        delegate: Label { text: label + " | " + secondary }
                    }
                }

                SectionCard {
                    title: "Published Publications 30 Days"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        model: appController.dashboardPublishedPublicationsModel
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
                        delegate: Label { text: label + " | " + secondary }
                    }
                }

                SectionCard {
                    title: "Zero Published Pillars"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        model: appController.dashboardZeroPublishedPillarsModel
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
                        delegate: Label { text: label + (secondary.length ? " | " + secondary : "") }
                    }
                }
            }
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

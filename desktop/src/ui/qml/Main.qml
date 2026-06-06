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

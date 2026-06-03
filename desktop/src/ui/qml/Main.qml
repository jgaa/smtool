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

    property var boardColumns: [
        { title: "Inbox", model: appController.boardInboxModel },
        { title: "Clarifying", model: appController.boardClarifyingModel },
        { title: "Shaping", model: appController.boardShapingModel },
        { title: "Drafting", model: appController.boardDraftingModel },
        { title: "Ready", model: appController.boardReadyModel },
        { title: "Scheduled", model: appController.boardScheduledModel },
        { title: "Published", model: appController.boardPublishedModel },
        { title: "Reviewing", model: appController.boardReviewingModel }
    ]

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8

            Label {
                text: "SmTool"
                font.pixelSize: 20
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: appController.statusMessage
                color: "#444444"
                Layout.preferredWidth: 520
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
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
        TabButton { text: "Dashboard" }
    }

    StackLayout {
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentIndex: tabBar.currentIndex

        ScrollView {
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 16

                SectionCard {
                    title: "Quick Add"

                    GridLayout {
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 12
                        Layout.fillWidth: true

                        Label { text: "Title" }
                        TextField {
                            id: inboxTitleField
                            Layout.fillWidth: true
                            placeholderText: "Capture an idea"
                        }

                        Label { text: "Description" }
                        TextArea {
                            id: inboxDescriptionField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            wrapMode: TextEdit.Wrap
                            placeholderText: "Optional note"
                        }

                        Label { text: "Pillar" }
                        ComboBox {
                            id: pillarBox
                            Layout.fillWidth: true
                            model: appController.pillarModel
                            textRole: "displayName"
                            valueRole: "lookupId"
                        }

                        Label { text: "Suggested Channel" }
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

                        Item { }
                        Button {
                            text: "Add to Inbox"
                            onClicked: {
                                const pillarId = pillarBox.currentIndex >= 0 ? pillarBox.currentValue : ""
                                const channelId = channelBox.currentIndex >= 0 ? channelBox.currentValue : ""
                                if (appController.createInboxItem(inboxTitleField.text, inboxDescriptionField.text, pillarId, "", priorityBox.value, channelId)) {
                                    inboxTitleField.clear()
                                    inboxDescriptionField.clear()
                                    priorityBox.value = 0
                                }
                            }
                        }
                    }
                }

                SectionCard {
                    title: "Inbox"

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 420
                        clip: true
                        model: appController.inboxModel
                        spacing: 8

                        delegate: Rectangle {
                            width: ListView.view.width
                            radius: 6
                            color: "white"
                            border.color: "#d5d5d5"
                            implicitHeight: contentLayout.implicitHeight + 16

                            ColumnLayout {
                                id: contentLayout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                Label {
                                    text: title
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: description
                                    visible: text.length > 0
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: [pillar, kind, suggestedChannel].filter(function(value) { return value.length > 0 }).join(" | ")
                                    color: "#555555"
                                    Layout.fillWidth: true
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
                }
            }
        }

        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                RowLayout {
                    CheckBox {
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: boardRow.implicitWidth
                    contentHeight: boardRow.implicitHeight

                    RowLayout {
                        id: boardRow
                        spacing: 12

                        Repeater {
                            model: window.boardColumns.length
                            delegate: BoardColumn {
                                required property int index
                                property var columnData: window.boardColumns[index]

                                title: columnData.title
                                model: columnData.model
                                height: parent.height
                            }
                        }

                        BoardColumn {
                            visible: appController.boardShowArchived
                            title: "Archived"
                            model: appController.boardArchivedModel
                            height: parent.height
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

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 760
                        clip: true
                        model: appController.calendarModel
                        spacing: 8

                        delegate: Rectangle {
                            width: ListView.view.width
                            radius: 6
                            color: "white"
                            border.color: "#d5d5d5"
                            implicitHeight: infoLayout.implicitHeight + 16

                            ColumnLayout {
                                id: infoLayout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                Label {
                                    text: title
                                    font.bold: true
                                }

                                Label {
                                    text: Qt.formatDateTime(scheduledAt, "yyyy-MM-dd hh:mm") + " | " + sourceType + (channel.length ? " | " + channel : "")
                                }

                                Label {
                                    text: series
                                    visible: text.length > 0
                                    color: "#555555"
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

                            delegate: Rectangle {
                                required property string itemId
                                required property string title
                                required property string kind
                                required property string series
                                required property string status
                                required property string suggestedChannel

                                width: ListView.view.width
                                radius: 6
                                color: appController.currentSourceId === itemId ? "#e8f2ff" : "white"
                                border.color: "#d5d5d5"
                                implicitHeight: sourceLayout.implicitHeight + 16

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: appController.currentSourceId = itemId
                                }

                                ColumnLayout {
                                    id: sourceLayout
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 4

                                    Label {
                                        text: title
                                        font.bold: true
                                    }

                                    Label {
                                        text: [kind, status, series, suggestedChannel].filter(function(value) { return value.length > 0 }).join(" | ")
                                        color: "#555555"
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }

                        Button {
                            text: "Create Burst From Source"
                            enabled: appController.currentSourceId.length > 0
                            onClicked: appController.createBurstForCurrentSource()
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

                        delegate: Rectangle {
                            width: ListView.view.width
                            radius: 6
                            color: "white"
                            border.color: "#d5d5d5"
                            implicitHeight: derivativeLayout.implicitHeight + 16

                            ColumnLayout {
                                id: derivativeLayout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                Label {
                                    text: title
                                    font.bold: true
                                }

                                Label {
                                    text: [kind, outcome, suggestedChannel, status].filter(function(value) { return value.length > 0 }).join(" | ")
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
        }

        ScrollView {
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 16

                SectionCard {
                    title: "Create Series"

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
                            Layout.preferredHeight: 80
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

                        Item { }
                        Button {
                            text: "Create Series"
                            onClicked: {
                                const pillarId = seriesPillarBox.currentIndex >= 0 ? seriesPillarBox.currentValue : ""
                                if (appController.createSeries(seriesNameField.text, seriesDescriptionField.text, pillarId)) {
                                    seriesNameField.clear()
                                    seriesDescriptionField.clear()
                                }
                            }
                        }
                    }
                }

                SectionCard {
                    title: "Series"

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 520
                        clip: true
                        model: appController.seriesModel
                        spacing: 8

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
            }
        }

        ScrollView {
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 16

                CheckBox {
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
                        delegate: Label { text: label + ": " + value }
                    }
                }

                SectionCard {
                    title: "By Series"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        model: appController.dashboardBySeriesModel
                        delegate: Label { text: label + ": " + value + (secondary.length ? " | " + secondary : "") }
                    }
                }

                SectionCard {
                    title: "By Status"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        model: appController.dashboardByStatusModel
                        delegate: Label { text: label + ": " + value }
                    }
                }

                SectionCard {
                    title: "Upcoming 14 Days"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        model: appController.dashboardUpcomingModel
                        delegate: Label { text: label + " | " + secondary }
                    }
                }

                SectionCard {
                    title: "Published Content 30 Days"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        model: appController.dashboardPublishedContentModel
                        delegate: Label { text: label + " | " + secondary }
                    }
                }

                SectionCard {
                    title: "Published Publications 30 Days"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        model: appController.dashboardPublishedPublicationsModel
                        delegate: Label { text: label + " | " + secondary }
                    }
                }

                SectionCard {
                    title: "Zero Published Pillars"
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        model: appController.dashboardZeroPublishedPillarsModel
                        delegate: Label { text: label + (secondary.length ? " | " + secondary : "") }
                    }
                }
            }
        }
    }
}

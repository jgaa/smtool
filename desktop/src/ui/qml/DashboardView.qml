import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "components"

Item {
    id: root

    readonly property var performanceOptions: [
        { label: "Last 30 days", key: "last_30_days" },
        { label: "Last 90 days", key: "last_90_days" },
        { label: "This quarter", key: "this_quarter" },
        { label: "This year", key: "this_year" },
        { label: "Custom", key: "custom" }
    ]
    readonly property var pipelineOptions: [
        { label: "Next 7 days", key: "next_7_days" },
        { label: "Next 30 days", key: "next_30_days" },
        { label: "Next 90 days", key: "next_90_days" },
        { label: "Custom", key: "custom" }
    ]

    function findPeriodIndex(options, key) {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].key === key) {
                return i
            }
        }
        return 0
    }

    function formatValue(value) {
        const rounded = Math.round(value * 10) / 10
        return Math.abs(rounded - Math.round(rounded)) < 0.05 ? Math.round(rounded).toString() : rounded.toFixed(1)
    }

    function percentWidth(percent, availableWidth) {
        return Math.max(8, Math.min(availableWidth, availableWidth * Math.min(percent, 160) / 160.0))
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 16

            SectionCard {
                title: "Dashboard"

                ColumnLayout {
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 24

                        ColumnLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Performance"
                                font.bold: true
                            }

                            ComboBox {
                                id: performancePeriodBox
                                Layout.fillWidth: true
                                textRole: "label"
                                valueRole: "key"
                                model: root.performanceOptions
                                currentIndex: root.findPeriodIndex(root.performanceOptions, appController.dashboardPerformancePeriodKey)
                                onActivated: appController.configureDashboardPerformancePeriod(currentValue,
                                                                                             appController.dashboardPerformanceStartDate,
                                                                                             appController.dashboardPerformanceEndDate)
                            }

                            RowLayout {
                                visible: performancePeriodBox.currentValue === "custom"
                                Layout.fillWidth: true

                                DateSelector {
                                    id: performanceStartSelector
                                    Layout.fillWidth: true
                                    value: appController.dashboardPerformanceStartDate
                                    onValueChanged: {
                                        if (performancePeriodBox.currentValue === "custom") {
                                            appController.configureDashboardPerformancePeriod("custom",
                                                                                             value,
                                                                                             performanceEndSelector.value)
                                        }
                                    }
                                }

                                DateSelector {
                                    id: performanceEndSelector
                                    Layout.fillWidth: true
                                    value: appController.dashboardPerformanceEndDate
                                    onValueChanged: {
                                        if (performancePeriodBox.currentValue === "custom") {
                                            appController.configureDashboardPerformancePeriod("custom",
                                                                                             performanceStartSelector.value,
                                                                                             value)
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Pipeline"
                                font.bold: true
                            }

                            ComboBox {
                                id: pipelinePeriodBox
                                Layout.fillWidth: true
                                textRole: "label"
                                valueRole: "key"
                                model: root.pipelineOptions
                                currentIndex: root.findPeriodIndex(root.pipelineOptions, appController.dashboardPipelinePeriodKey)
                                onActivated: appController.configureDashboardPipelinePeriod(currentValue,
                                                                                          appController.dashboardPipelineStartDate,
                                                                                          appController.dashboardPipelineEndDate)
                            }

                            RowLayout {
                                visible: pipelinePeriodBox.currentValue === "custom"
                                Layout.fillWidth: true

                                DateSelector {
                                    id: pipelineStartSelector
                                    Layout.fillWidth: true
                                    value: appController.dashboardPipelineStartDate
                                    onValueChanged: {
                                        if (pipelinePeriodBox.currentValue === "custom") {
                                            appController.configureDashboardPipelinePeriod("custom",
                                                                                          value,
                                                                                          pipelineEndSelector.value)
                                        }
                                    }
                                }

                                DateSelector {
                                    id: pipelineEndSelector
                                    Layout.fillWidth: true
                                    value: appController.dashboardPipelineEndDate
                                    onValueChanged: {
                                        if (pipelinePeriodBox.currentValue === "custom") {
                                            appController.configureDashboardPipelinePeriod("custom",
                                                                                          pipelineStartSelector.value,
                                                                                          value)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 16

                SectionCard {
                    Layout.fillWidth: true
                    title: "Goal Achievement"

                    Label {
                        text: appController.dashboardPerformancePeriodLabel
                        color: "#666666"
                    }

                    ListView {
                        id: goalAchievementList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(160, Math.min(contentHeight + 8, 360))
                        clip: true
                        spacing: 10
                        model: appController.goalAchievementModel

                        delegate: ColumnLayout {
                            required property string displayName
                            required property string detailText
                            required property string healthColor
                            required property real percent
                            required property real targetValue
                            required property real actualValue

                            width: ListView.view.width
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    Layout.fillWidth: true
                                    text: displayName
                                    font.bold: true
                                }

                                Rectangle {
                                    radius: 10
                                    color: healthColor
                                    implicitWidth: 74
                                    implicitHeight: 28

                                    Label {
                                        anchors.centerIn: parent
                                        color: "white"
                                        text: root.formatValue(percent) + "%"
                                        font.bold: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 10
                                radius: 5
                                color: "#e7e7e7"

                                Rectangle {
                                    width: root.percentWidth(percent, parent.width)
                                    height: parent.height
                                    radius: parent.radius
                                    color: healthColor
                                }
                            }

                            Label {
                                text: detailText
                                color: "#555555"
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    Label {
                        visible: goalAchievementList.count === 0
                        text: "No enabled count or cadence goals yet."
                        color: "#666666"
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    title: "Pipeline Coverage"

                    Label {
                        text: appController.dashboardPipelinePeriodLabel
                        color: "#666666"
                    }

                    ListView {
                        id: pipelineCoverageList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(160, Math.min(contentHeight + 8, 360))
                        clip: true
                        spacing: 10
                        model: appController.pipelineCoverageModel

                        delegate: ColumnLayout {
                            required property string displayName
                            required property string detailText
                            required property string healthColor
                            required property real percent

                            width: ListView.view.width
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    Layout.fillWidth: true
                                    text: displayName
                                    font.bold: true
                                }

                                Rectangle {
                                    radius: 10
                                    color: healthColor
                                    implicitWidth: 74
                                    implicitHeight: 28

                                    Label {
                                        anchors.centerIn: parent
                                        color: "white"
                                        text: root.formatValue(percent) + "%"
                                        font.bold: true
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 10
                                radius: 5
                                color: "#e7e7e7"

                                Rectangle {
                                    width: root.percentWidth(percent, parent.width)
                                    height: parent.height
                                    radius: parent.radius
                                    color: healthColor
                                }
                            }

                            Label {
                                text: detailText
                                color: "#555555"
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    Label {
                        visible: pipelineCoverageList.count === 0
                        text: "No enabled count or cadence goals yet."
                        color: "#666666"
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: 16

                SectionCard {
                    Layout.fillWidth: true
                    title: "Balance Goal Deviation"

                    ListView {
                        id: balanceDeviationList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(150, Math.min(contentHeight + 8, 320))
                        clip: true
                        spacing: 8
                        model: appController.balanceDeviationModel

                        delegate: Rectangle {
                            required property string displayName
                            required property string detailText
                            required property string healthColor
                            required property real targetValue
                            required property real actualValue
                            required property real deviation

                            width: ListView.view.width
                            implicitHeight: 72
                            radius: 8
                            color: "#f3f3f3"
                            border.color: "#dddddd"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10

                                Rectangle {
                                    implicitWidth: 10
                                    Layout.fillHeight: true
                                    radius: 5
                                    color: healthColor
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: displayName
                                        font.bold: true
                                    }

                                    Label {
                                        text: detailText
                                        color: "#555555"
                                        wrapMode: Text.Wrap
                                    }
                                }

                                Label {
                                    text: (deviation > 0 ? "+" : "") + root.formatValue(deviation) + " pts"
                                    color: healthColor
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Label {
                        visible: balanceDeviationList.count === 0
                        text: "No enabled balance goals yet."
                        color: "#666666"
                    }
                }

                SectionCard {
                    Layout.fillWidth: true
                    title: "Neglected Areas"

                    ListView {
                        id: alertsList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(150, Math.min(contentHeight + 8, 320))
                        clip: true
                        spacing: 8
                        model: appController.dashboardAlertsModel

                        delegate: Rectangle {
                            required property string displayName
                            required property string summaryText
                            required property string detailText
                            required property string healthColor

                            width: ListView.view.width
                            implicitHeight: 80
                            radius: 8
                            color: "#fff8f6"
                            border.color: "#ead0c8"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10

                                Label {
                                    text: summaryText
                                    color: healthColor
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    text: detailText
                                    color: "#555555"
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }

                    Label {
                        visible: alertsList.count === 0
                        text: "No urgent dashboard alerts."
                        color: "#666666"
                    }
                }
            }

            SectionCard {
                title: "Recommended Focus"

                ListView {
                    id: focusList
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(150, Math.min(contentHeight + 8, 260))
                    clip: true
                    spacing: 8
                    model: appController.recommendedFocusModel

                    delegate: Rectangle {
                        required property int index
                        required property string displayName
                        required property string detailText
                        required property string healthColor

                        width: ListView.view.width
                        implicitHeight: 72
                        radius: 8
                        color: "#f6f8fb"
                        border.color: "#d9e1ea"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10

                            Rectangle {
                                implicitWidth: 34
                                implicitHeight: 34
                                radius: 17
                                color: healthColor

                                Label {
                                    anchors.centerIn: parent
                                    color: "white"
                                    text: index + 1
                                    font.bold: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: displayName
                                    font.bold: true
                                }

                                Label {
                                    text: detailText
                                    color: "#555555"
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                    }
                }

                Label {
                    visible: focusList.count === 0
                    text: "No immediate focus recommendations."
                    color: "#666666"
                }
            }

            SectionCard {
                title: "Statistics"

                ListView {
                    id: statisticsList
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(180, Math.min(contentHeight + 8, 420))
                    clip: true
                    spacing: 8
                    model: appController.dashboardStatisticsModel

                    delegate: Rectangle {
                        required property string displayName
                        required property string detailText
                        required property real actualValue

                        width: ListView.view.width
                        implicitHeight: 68
                        radius: 8
                        color: "#f7f7f7"
                        border.color: "#dddddd"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: displayName
                                    font.bold: true
                                }

                                Label {
                                    visible: detailText.length > 0
                                    text: detailText
                                    color: "#666666"
                                    wrapMode: Text.Wrap
                                }
                            }

                            Label {
                                text: root.formatValue(actualValue)
                                font.pixelSize: 22
                                font.bold: true
                                color: "#2f6f44"
                            }
                        }
                    }
                }

                Label {
                    visible: statisticsList.count === 0
                    text: "No statistics available yet."
                    color: "#666666"
                }
            }
        }
    }
}

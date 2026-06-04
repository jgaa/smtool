import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string value: ""
    property date selectedDate: todayAtMidnight()

    function formatDate(date) {
        return Qt.formatDate(date, "yyyy-MM-dd")
    }

    function todayAtMidnight() {
        const now = new Date()
        return new Date(now.getFullYear(), now.getMonth(), now.getDate())
    }

    function addDays(date, days) {
        const next = new Date(date)
        next.setDate(next.getDate() + days)
        return next
    }

    function nextWeekday(baseDate, weekday) {
        const candidate = new Date(baseDate)
        const current = candidate.getDay()
        let delta = (weekday - current + 7) % 7
        if (delta === 0) {
            delta = 7
        }
        candidate.setDate(candidate.getDate() + delta)
        return new Date(candidate.getFullYear(), candidate.getMonth(), candidate.getDate())
    }

    function applyPreset(key) {
        const today = todayAtMidnight()
        switch (key) {
        case "none":
            value = ""
            break
        case "custom":
            if (calendar.selectedDate) {
                value = formatDate(calendar.selectedDate)
            }
            break
        case "today":
            value = formatDate(today)
            break
        case "tomorrow":
            value = formatDate(addDays(today, 1))
            break
        case "monday":
            value = formatDate(nextWeekday(today, 1))
            break
        case "tuesday":
            value = formatDate(nextWeekday(today, 2))
            break
        case "wednesday":
            value = formatDate(nextWeekday(today, 3))
            break
        case "thursday":
            value = formatDate(nextWeekday(today, 4))
            break
        case "friday":
            value = formatDate(nextWeekday(today, 5))
            break
        case "saturday":
            value = formatDate(nextWeekday(today, 6))
            break
        case "sunday":
            value = formatDate(nextWeekday(today, 0))
            break
        }
    }

    function syncFromValue() {
        if (value.length === 0) {
            presetBox.currentIndex = 0
            return
        }

        const parsed = new Date(value + "T00:00:00")
        if (!isNaN(parsed.getTime())) {
            selectedDate = parsed
        }
        presetBox.currentIndex = 1
    }

    onValueChanged: syncFromValue()
    Component.onCompleted: syncFromValue()

    RowLayout {
        Layout.fillWidth: true

        ComboBox {
            id: presetBox
            Layout.fillWidth: true
            textRole: "label"
            valueRole: "key"
            model: [
                { label: "None", key: "none" },
                { label: "Date", key: "custom" },
                { label: "Today", key: "today" },
                { label: "Tomorrow", key: "tomorrow" },
                { label: "Next Monday", key: "monday" },
                { label: "Next Tuesday", key: "tuesday" },
                { label: "Next Wednesday", key: "wednesday" },
                { label: "Next Thursday", key: "thursday" },
                { label: "Next Friday", key: "friday" },
                { label: "Next Saturday", key: "saturday" },
                { label: "Next Sunday", key: "sunday" }
            ]
            onActivated: root.applyPreset(currentValue)
        }

        Button {
            visible: presetBox.currentValue === "custom"
            text: root.value.length > 0 ? root.value : "Select date"
            onClicked: calendarPopup.open()
        }
    }

    Popup {
        id: calendarPopup
        modal: true
        focus: true
        width: 320
        height: 360
        padding: 12

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                ToolButton {
                    text: "<"
                    onClicked: {
                        const previous = new Date(root.selectedDate)
                        previous.setMonth(previous.getMonth() - 1)
                        root.selectedDate = previous
                    }
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: Qt.formatDate(root.selectedDate, "MMMM yyyy")
                }

                ToolButton {
                    text: ">"
                    onClicked: {
                        const next = new Date(root.selectedDate)
                        next.setMonth(next.getMonth() + 1)
                        root.selectedDate = next
                    }
                }
            }

            DayOfWeekRow {
                locale: Qt.locale()
                Layout.fillWidth: true
            }

            MonthGrid {
                id: calendar
                Layout.fillWidth: true
                Layout.fillHeight: true
                month: root.selectedDate.getMonth()
                year: root.selectedDate.getFullYear()

                delegate: ItemDelegate {
                    required property var model
                    width: 40
                    height: 36
                    text: model.day
                    enabled: model.month === calendar.month
                    highlighted: model.date.toDateString() === root.selectedDate.toDateString()
                    onClicked: {
                        root.selectedDate = model.date
                        root.value = root.formatDate(model.date)
                        calendarPopup.close()
                    }
                }
            }
        }
    }
}

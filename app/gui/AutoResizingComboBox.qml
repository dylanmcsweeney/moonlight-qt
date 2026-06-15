import QtQuick 2.9
import QtQuick.Controls 2.2

import SdlGamepadKeyNavigation 1.0
import SystemProperties 1.0

// https://stackoverflow.com/questions/45029968/how-do-i-set-the-combobox-width-to-fit-the-largest-item
ComboBox {
    property int textWidth
    property int desiredWidth: leftPadding + textWidth +
                               (indicator ? indicator.width : 0) + rightPadding
    property int maximumWidth: parent ? parent.width : desiredWidth

    implicitWidth: Math.min(desiredWidth, maximumWidth)
    popup.width: width

    TextMetrics {
        id: popupMetrics
    }

    TextMetrics {
        id: textMetrics
    }

    function recalculateWidth() {
        textMetrics.font = font
        popupMetrics.font = popup.font
        textWidth = 0
        for (var i = 0; i < count; i++){
            textMetrics.text = textAt(i)
            popupMetrics.text = textAt(i)
            textWidth = Math.max(textMetrics.width, textWidth)
            textWidth = Math.max(popupMetrics.width, textWidth)
        }
    }

    function scheduleWidthRecalculation() {
        widthRecalculationTimer.restart()
    }

    Timer {
        id: widthRecalculationTimer
        interval: 0
        repeat: false
        onTriggered: recalculateWidth()
    }

    Component.onCompleted: scheduleWidthRecalculation()
    onCountChanged: scheduleWidthRecalculation()
    onModelChanged: scheduleWidthRecalculation()
    onFontChanged: scheduleWidthRecalculation()

    Connections {
        target: model
        ignoreUnknownSignals: true

        function onDataChanged() {
            scheduleWidthRecalculation()
        }

        function onModelReset() {
            scheduleWidthRecalculation()
        }

        function onRowsInserted() {
            scheduleWidthRecalculation()
        }

        function onRowsRemoved() {
            scheduleWidthRecalculation()
        }
    }

    popup.onAboutToShow: {
        recalculateWidth()

        // Switch to normal navigation for combo boxes
        SdlGamepadKeyNavigation.setUiNavMode(false)

        // Override the popup color to improve contrast with the overridden
        // Material 2 background color set in main.qml.
        if (SystemProperties.usesMaterial3Theme) {
            popup.background.color = "#424242"
        }
    }

    popup.onAboutToHide: {
        SdlGamepadKeyNavigation.setUiNavMode(true)
    }

    Keys.onLeftPressed: {
        decrementCurrentIndex()
    }

    Keys.onRightPressed: {
        incrementCurrentIndex()
    }
}

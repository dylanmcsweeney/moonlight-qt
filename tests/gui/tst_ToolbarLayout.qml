import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtTest 1.3

import "../../app/gui" as Moonlight

TestCase {
    id: testCase
    name: "ToolbarLayout"
    when: windowShown
    width: 720
    height: 140
    visible: true

    property real requestedToolbarWidth: 640
    property real requestedToolbarHeight: 60

    Rectangle {
        id: toolbarSurface
        anchors.centerIn: parent
        width: testCase.requestedToolbarWidth
        height: testCase.requestedToolbarHeight
        color: "transparent"

        RowLayout {
            id: toolbarRow
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            Moonlight.NavigableToolButton {
                id: toolbarButton
                objectName: "toolbarButton"
                // Text forces Material's background out of its square-content
                // branch and exercises the background-to-implicit-width path.
                text: "Settings"
                display: AbstractButton.TextBesideIcon
                iconSource: Qt.resolvedUrl("../../app/res/settings.svg")
            }

            Label {
                text: "Toolbar title"
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
            }
        }
    }

    function assertToolbarSettles() {
        var previousWidth = toolbarButton.width
        var previousIconWidth = toolbarButton.icon.width
        var previousIconHeight = toolbarButton.icon.height
        var stableSamples = 0
        var observedLayout = false

        // The old Material background binding grows a few pixels per layout
        // pass. Check bounds before waiting for multiple stable frames so that
        // regression fails promptly instead of letting the feedback run away.
        for (var sample = 0; sample < 20; ++sample) {
            wait(16)

            var buttonWidth = toolbarButton.width
            var iconWidth = toolbarButton.icon.width
            var iconHeight = toolbarButton.icon.height
            if (buttonWidth <= 0 || iconWidth <= 0 || iconHeight <= 0) {
                continue
            }
            observedLayout = true

            if (!(buttonWidth > 0 && buttonWidth < 192)) {
                verify(false, "toolbar button width escaped its normal range: " + buttonWidth)
                return
            }
            if (!(iconWidth > 0 && iconWidth < 128 && iconHeight > 0 && iconHeight < 128)) {
                verify(false, "toolbar icon size escaped its normal range: " +
                               iconWidth + "x" + iconHeight)
                return
            }

            if (Math.abs(buttonWidth - previousWidth) < 0.5 &&
                    Math.abs(iconWidth - previousIconWidth) < 0.5 &&
                    Math.abs(iconHeight - previousIconHeight) < 0.5) {
                ++stableSamples
                if (stableSamples >= 3) {
                    return
                }
            }
            else {
                stableSamples = 0
            }

            previousWidth = buttonWidth
            previousIconWidth = iconWidth
            previousIconHeight = iconHeight
        }

        verify(observedLayout && stableSamples >= 3,
               "toolbar button and icon dimensions did not settle")
    }

    function test_compactAndExpandedToolbarSizesSettle() {
        var sizes = [
            { width: 640, height: 60 },
            { width: 300, height: 48 },
            { width: 220, height: 42 },
            { width: 640, height: 60 },
            { width: 220, height: 42 },
            { width: 640, height: 60 }
        ]

        for (var i = 0; i < sizes.length; ++i) {
            requestedToolbarWidth = sizes[i].width
            requestedToolbarHeight = sizes[i].height
            assertToolbarSettles()
        }
    }
}

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.15

Window {
    id: window

    property int stage: InitialStage
    property var stageNames: ["solid", "material", "shell", "grid", "settings"]
    property bool showHelp: false
    property bool benchmarkEnabled: InitialBenchmark

    width: 1280
    height: 600
    minimumWidth: 500
    minimumHeight: 350
    visible: true
    color: "#303030"
    title: "Moonlight GUI Diagnostic | " + stageNames[stage] +
           " | " + FrameMonitor.measuredFps.toFixed(1) + " FPS" +
           " | p95 " + FrameMonitor.p95FrameTimeMs.toFixed(1) + " ms" +
           " | worst " + FrameMonitor.worstFrameTimeMs.toFixed(1) + " ms" +
           (RedirectionSurfaceDisabled ? " | no redirection" : "") +
           (AlphaBufferEnabled ? " | alpha buffer" : "")

    function selectStage(index) {
        stage = index
        contentLoader.sourceComponent = stageComponents[index]
        FrameMonitor.resetMeasurements()
    }

    function toggleBenchmark() {
        benchmarkEnabled = !benchmarkEnabled
        FrameMonitor.resetMeasurements()
    }

    Component.onCompleted: selectStage(stage)

    Shortcut {
        sequence: "1"
        context: Qt.ApplicationShortcut
        onActivated: selectStage(0)
    }
    Shortcut {
        sequence: "2"
        context: Qt.ApplicationShortcut
        onActivated: selectStage(1)
    }
    Shortcut {
        sequence: "3"
        context: Qt.ApplicationShortcut
        onActivated: selectStage(2)
    }
    Shortcut {
        sequence: "4"
        context: Qt.ApplicationShortcut
        onActivated: selectStage(3)
    }
    Shortcut {
        sequence: "5"
        context: Qt.ApplicationShortcut
        onActivated: selectStage(4)
    }
    Shortcut {
        sequence: "C"
        context: Qt.ApplicationShortcut
        onActivated: FrameMonitor.continuous = !FrameMonitor.continuous
    }
    Shortcut {
        sequence: "B"
        context: Qt.ApplicationShortcut
        onActivated: toggleBenchmark()
    }
    Shortcut {
        sequence: "F1"
        context: Qt.ApplicationShortcut
        onActivated: showHelp = !showHelp
    }

    Loader {
        id: contentLoader
        anchors.fill: parent
        focus: true
    }

    Item {
        id: benchmarkTrack
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        height: 28
        visible: benchmarkEnabled
        z: 90

        Rectangle {
            anchors.fill: parent
            color: "#80000000"
            border.color: "#80ffffff"
            radius: 4
        }

        Rectangle {
            id: benchmarkMarker
            width: 48
            height: benchmarkTrack.height
            color: "#7e57c2"
            radius: 4

            SequentialAnimation on x {
                running: benchmarkEnabled
                loops: Animation.Infinite

                NumberAnimation {
                    from: 0
                    to: Math.max(0, benchmarkTrack.width - benchmarkMarker.width)
                    duration: 1000
                    easing.type: Easing.Linear
                }
                NumberAnimation {
                    from: Math.max(0, benchmarkTrack.width - benchmarkMarker.width)
                    to: 0
                    duration: 1000
                    easing.type: Easing.Linear
                }
            }
        }
    }

    property var stageComponents: [
        solidStage,
        materialStage,
        shellStage,
        gridStage,
        settingsStage
    ]

    Component {
        id: solidStage

        Rectangle {
            color: "#303030"
        }
    }

    Component {
        id: materialStage

        Pane {
            Material.theme: Material.Dark

            Column {
                anchors.centerIn: parent
                spacing: 12

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Material controls"
                    font.pointSize: 24
                }
                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Button"
                }
                CheckBox {
                    text: "Check box"
                    checked: true
                }
                ComboBox {
                    model: ["First option", "Second option", "Third option"]
                }
            }
        }
    }

    Component {
        id: shellStage

        ColumnLayout {
            spacing: 0

            ToolBar {
                Layout.fillWidth: true
                Layout.preferredHeight: 60

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8

                    ToolButton {
                        text: "Back"
                    }
                    Label {
                        text: "Moonlight shell"
                        font.pointSize: 20
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        text: "Help"
                    }
                    ToolButton {
                        text: "Settings"
                    }
                }
            }

            StackView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                initialItem: Pane {
                    Label {
                        anchors.centerIn: parent
                        text: "Toolbar and StackView"
                        font.pointSize: 24
                    }
                }
            }
        }
    }

    Component {
        id: gridStage

        ColumnLayout {
            spacing: 0

            ToolBar {
                Layout.fillWidth: true
                Layout.preferredHeight: 60

                Label {
                    anchors.centerIn: parent
                    text: "Computers"
                    font.pointSize: 20
                }
            }

            GridView {
                id: computerGrid
                Layout.fillWidth: true
                Layout.fillHeight: true
                cellWidth: 310
                cellHeight: 330
                model: 12

                delegate: ItemDelegate {
                    width: 300
                    height: 320

                    Image {
                        id: computerIcon
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: "qrc:/res/desktop_windows-48px.svg"
                        sourceSize.width: 200
                        sourceSize.height: 200
                    }

                    Label {
                        anchors.top: computerIcon.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        text: "Computer " + (index + 1)
                        font.pointSize: 30
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }

                    RoundButton {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 5
                        icon.source: "qrc:/res/settings.svg"
                        icon.width: 30
                        icon.height: 30
                    }
                }
            }
        }
    }

    Component {
        id: settingsStage

        ColumnLayout {
            spacing: 0

            ToolBar {
                Layout.fillWidth: true
                Layout.preferredHeight: 60

                Label {
                    anchors.centerIn: parent
                    text: "Application Settings"
                    font.pointSize: 20
                }
            }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: settingsColumn.implicitHeight + 20
                clip: true

                Column {
                    id: settingsColumn
                    width: Math.min(parent.width - 30, 720)
                    anchors.horizontalCenter: parent.horizontalCenter
                    topPadding: 10
                    spacing: 15

                    Repeater {
                        model: 5

                        GroupBox {
                            required property int index
                            width: settingsColumn.width
                            title: "Settings group " + (index + 1)

                            Column {
                                anchors.fill: parent
                                spacing: 8

                                Label {
                                    width: parent.width
                                    text: "Representative wrapped settings text that must be laid out again when the available width changes."
                                    wrapMode: Text.Wrap
                                }
                                ComboBox {
                                    model: [
                                        "Normal (render only when changed)",
                                        "Match display refresh rate",
                                        "Fixed 120 FPS"
                                    ]
                                }
                                CheckBox {
                                    text: "Enable representative application option"
                                    checked: index % 2 === 0
                                }
                                Slider {
                                    width: parent.width
                                    from: 0
                                    to: 100
                                    value: 50
                                }
                                TextField {
                                    width: parent.width
                                    text: "Editable setting value"
                                }
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    Loader {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        active: showHelp
        z: 100

        sourceComponent: Rectangle {
            width: helpText.implicitWidth + 24
            height: helpText.implicitHeight + 20
            color: "#e0202020"
            border.color: "#808080"
            radius: 4

            Label {
                id: helpText
                anchors.centerIn: parent
                text: "1 Solid   2 Material   3 Shell   4 Grid   5 Settings\n" +
                      "B Deterministic benchmark: " +
                      (benchmarkEnabled ? "ON" : "OFF") + "\n" +
                      "C Continuous rendering: " +
                      (FrameMonitor.continuous ? "ON" : "OFF") + "\n" +
                      "Render loop: " + RenderLoopName +
                      "   Graphics API: " + GraphicsApiName + "\n" +
                      "D3D legacy swapchain: " + D3DLegacySwapchain +
                      "   Max frame latency: " +
                      (D3DMaxFrameLatency.length > 0 ?
                           D3DMaxFrameLatency : "Qt default") + "\n" +
                      "VBlank virtualization disabled: " +
                      VBlankVirtualizationDisabled + "\nF1 Hide help"
            }
        }
    }
}

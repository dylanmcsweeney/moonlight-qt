import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0
import StreamProfileManager 1.0

CenteredGridView {
    property ComputerModel computerModel : createModel()

    id: pcGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 20
    bottomMargin: 5
    itemWidth: 300
    itemHeight: 320
    cellWidth: itemWidth + 10
    cellHeight: itemHeight + 10
    objectName: qsTr("Computers")

    function fitInitialWindow()
    {
        window.fitInitialPcWindow(
            count, cellWidth, cellHeight, itemWidth, itemHeight)
        updateMargins()
    }

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        currentIndex = -1

        // The StackView, model count, and native window are not all finalized
        // during child component completion. Fit on the first event-loop pass
        // so the initial dimensions and margins use the settled geometry.
        Qt.callLater(fitInitialWindow)
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    StackView.onActivated: {
        // Setup signals on CM
        ComputerManager.computerAddCompleted.connect(addComplete)

        if (window.initialPcWindowSizeApplied) {
            window.fitWindowForView(window.initialPcWindowWidth,
                                    window.initialPcWindowHeight,
                                    true)
        }

        // Highlight the first item if a gamepad is connected
        if (currentIndex === -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
        }
    }

    StackView.onDeactivating: {
        ComputerManager.computerAddCompleted.disconnect(addComplete)
    }

    function pairingComplete(error)
    {
        // Close the PIN dialog
        pairDialog.close()

        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.helpText = ""
            errorDialog.open()
        }
    }

    function addComplete(success, detectedPortBlocking)
    {
        if (!success) {
            errorDialog.text = qsTr("Unable to connect to the specified PC.")

            if (detectedPortBlocking) {
                errorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.")
            }
            else {
                errorDialog.helpText = qsTr("Click the Help button for possible solutions.")
            }

            errorDialog.open()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }

    Row {
        anchors.centerIn: parent
        spacing: 5
        visible: pcGrid.count === 0

        BusyIndicator {
            id: searchSpinner
            visible: StreamingPreferences.enableMdns
            running: visible
        }

        Label {
            height: searchSpinner.height
            elide: Label.ElideRight
            text: StreamingPreferences.enableMdns ? qsTr("Searching for compatible hosts on your local network...")
                                                  : qsTr("Automatic PC discovery is disabled. Add your PC manually.")
            font.pointSize: 20
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    model: computerModel

    delegate: NavigableItemDelegate {
        id: pcDelegate
        width: pcGrid.itemWidth
        height: pcGrid.itemHeight
        grid: pcGrid

        property alias pcContextMenu : pcContextMenuLoader.item

        background: Rectangle {
            radius: 5
            color: pcDelegate.highlighted ? "#454752" :
                   (pcDelegate.hovered ? "#333333" : "#292929")
            border.width: 1
            border.color: pcDelegate.highlighted ? "#7f91e8" : "#414141"
        }

        function openProfilesDialog()
        {
            profilesDialog.hostUuid = model.uuid
            profilesDialog.pcName = model.name
            profilesDialog.refresh()
            profilesDialog.open()
        }

        function openActiveProfileEditor()
        {
            window.openProfileEditor(
                StreamProfileManager.createEditor(model.uuid,
                                                  model.activeProfileId,
                                                  false))
        }

        Image {
            id: pcIcon
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/res/desktop_windows-48px.svg"
            sourceSize {
                width: 200
                height: 200
            }
        }

        Image {
            // TODO: Tooltip
            id: stateIcon
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: !model.online ? -18 : -16
            visible: !model.statusUnknown && (!model.online || !model.paired)
            source: !model.online ? "qrc:/res/warning_FILL1_wght300_GRAD200_opsz24.svg" : "qrc:/res/baseline-lock-24px.svg"
            sourceSize {
                width: !model.online ? 75 : 70
                height: !model.online ? 75 : 70
            }
        }

        BusyIndicator {
            id: statusUnknownSpinner
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: -15
            width: 75
            height: 75
            visible: model.statusUnknown
            running: visible
        }

        Label {
            id: pcNameText
            text: model.name

            width: parent.width
            anchors.top: pcIcon.bottom
            anchors.bottom: profileFooter.top
            anchors.bottomMargin: 4
            leftPadding: 8
            rightPadding: 8
            font.pointSize: 24
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
            maximumLineCount: 2

            ToolTip.visible: pcDelegate.hovered && truncated
            ToolTip.delay: 1000
            ToolTip.timeout: 4000
            ToolTip.text: model.name
        }

        Rectangle {
            id: profileFooter
            visible: StreamingPreferences.showPcProfileControls
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 5
            height: visible ? 52 : 0
            radius: 5
            color: pcDelegate.highlighted ? "#454b68" : "#383838"
            border.width: 1
            border.color: pcDelegate.highlighted ? "#899cff" : "#505050"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 2
                spacing: 2

                Button {
                    id: profileSelectorButton
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    focusPolicy: Qt.NoFocus
                    padding: 5
                    flat: true

                    contentItem: Item {
                        Column {
                            id: profileTextColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 0

                            Label {
                                width: parent.width
                                text: qsTr("ACTIVE PROFILE")
                                font.pointSize: 7
                                color: "#a8a8a8"
                            }

                            RowLayout {
                                width: parent.width
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: model.activeProfileName
                                    font.pointSize: 9
                                    color: "white"
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: "\u2026"
                                    font.pointSize: 14
                                    color: "#d0d0d0"
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }

                    background: Rectangle {
                        radius: 4
                        color: profileSelectorButton.down ? "#606060" :
                               (profileSelectorButton.hovered ? "#4b4b4b" :
                                                                "transparent")
                    }

                    onClicked: pcDelegate.openProfilesDialog()
                    ToolTip.visible: hovered
                    ToolTip.delay: 1000
                    ToolTip.timeout: 4000
                    ToolTip.text: qsTr("Open profile list")
                }

                RoundButton {
                    id: profileSettingsButton
                    Layout.preferredWidth: 48
                    Layout.fillHeight: true
                    focusPolicy: Qt.NoFocus
                    icon.source: "qrc:/res/settings.svg"
                    icon.width: 34
                    icon.height: 34

                    background: Rectangle {
                        radius: 4
                        color: profileSettingsButton.down ? "#606060" :
                               (profileSettingsButton.hovered ? "#4b4b4b" :
                                                               "transparent")
                    }

                    onClicked: pcDelegate.openActiveProfileEditor()
                    ToolTip.visible: hovered
                    ToolTip.delay: 1000
                    ToolTip.timeout: 4000
                    ToolTip.text: qsTr("Edit active streaming profile")
                }
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: 2
            visible: parent.highlighted &&
                     SdlGamepadKeyNavigation.getConnectedGamepads() > 0
            text: qsTr("A Open   X Settings   Y Menu   Start Global")
            font.pointSize: 9
            color: "skyblue"
        }

        Loader {
            id: pcContextMenuLoader
            asynchronous: true
            sourceComponent: NavigableMenu {
                id: pcContextMenu
                initiator: pcContextMenuLoader.parent
                MenuItem {
                    text: qsTr("PC Status: %1").arg(model.online ? qsTr("Online") : qsTr("Offline"))
                    font.bold: true
                    enabled: false
                }
                NavigableMenuItem {
                    text: qsTr("View All Apps")
                    onTriggered: {
                        var component = Qt.createComponent("AppView.qml")
                        var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name, "showHiddenGames": true})
                        stackView.push(appView)
                    }
                    visible: model.online && model.paired
                }
                NavigableMenuItem {
                    text: qsTr("Wake PC")
                    onTriggered: computerModel.wakeComputer(index)
                    visible: !model.online && model.wakeable
                }
                NavigableMenuItem {
                    text: qsTr("Test Network")
                    onTriggered: {
                        computerModel.testConnectionForComputer(index)
                        testConnectionDialog.open()
                    }
                }
                NavigableMenuItem {
                    text: qsTr("Profiles…")
                    onTriggered: pcDelegate.openProfilesDialog()
                }
                NavigableMenuItem {
                    text: qsTr("Streaming Settings…")
                    onTriggered: pcDelegate.openActiveProfileEditor()
                }

                NavigableMenuItem {
                    text: qsTr("Rename PC")
                    onTriggered: {
                        renamePcDialog.pcIndex = index
                        renamePcDialog.originalName = model.name
                        renamePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    text: qsTr("Delete PC")
                    onTriggered: {
                        deletePcDialog.pcIndex = index
                        deletePcDialog.pcName = model.name
                        deletePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    text: qsTr("View Details")
                    onTriggered: {
                        showPcDetailsDialog.pcDetails = model.details
                        showPcDetailsDialog.open()
                    }
                }
            }
        }

        onClicked: {
            if (model.online) {
                if (!model.serverSupported) {
                    errorDialog.text = qsTr("The version of GeForce Experience on %1 is not supported by this build of Moonlight. You must update Moonlight to stream from %1.").arg(model.name)
                    errorDialog.helpText = ""
                    errorDialog.open()
                }
                else if (model.paired) {
                    // go to game view
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name})
                    stackView.push(appView)
                }
                else {
                    var pin = computerModel.generatePinString()

                    // Kick off pairing in the background
                    computerModel.pairComputer(index, pin)

                    // Display the pairing dialog
                    pairDialog.pin = pin
                    pairDialog.open()
                }
            } else if (!model.online) {
                // Using open() here because it may be activated by keyboard
                pcContextMenu.open()
            }
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            if (pcContextMenu.popup) {
                pcContextMenu.popup()
            }
            else {
                // Qt 5.9 doesn't have popup()
                pcContextMenu.open()
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton;
            onClicked: {
                parent.pressAndHold()
            }
        }

        Keys.onMenuPressed: {
            // We must use open() here so the menu is positioned on
            // the ItemDelegate and not where the mouse cursor is
            pcContextMenu.open()
        }

        Keys.onPressed: {
            if (event.key === Qt.Key_F2) {
                pcDelegate.openActiveProfileEditor()
                event.accepted = true
            }
        }

        Keys.onDeletePressed: {
            deletePcDialog.pcIndex = index
            deletePcDialog.pcName = model.name
            deletePcDialog.open()
        }
    }

    ErrorMessageDialog {
        id: errorDialog

        // Using Setup-Guide here instead of Troubleshooting because it's likely that users
        // will arrive here by forgetting to enable GameStream or not forwarding ports.
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide"
    }

    NavigableMessageDialog {
        id: pairDialog
        closePolicy: Popup.CloseOnEscape

        // don't allow edits to the rest of the window while open
        property string pin : "0000"
        text:qsTr("Please enter %1 on your host PC. This dialog will close when pairing is completed.").arg(pin)+"\n\n"+
             qsTr("If your host PC is running Sunshine, navigate to the Sunshine web UI to enter the PIN.")
        standardButtons: Dialog.Cancel
        onRejected: {
            // FIXME: We should interrupt pairing here
        }
    }

    NavigableDialog {
        id: profilesDialog
        property string hostUuid
        property string pcName
        property var profileEntries: []
        title: qsTr("Profiles for %1").arg(pcName)
        standardButtons: Dialog.Cancel

        function refresh() {
            profileEntries = StreamProfileManager.profiles(hostUuid)
        }

        onOpened: {
            if (profileButtons.count > 0) {
                profileButtons.itemAt(0).forceActiveFocus(Qt.TabFocus)
            }
        }

        ColumnLayout {
            spacing: 5

            Repeater {
                id: profileButtons
                model: profilesDialog.profileEntries

                Button {
                    Layout.fillWidth: true
                    text: (modelData.active ? "● " : "") + modelData.name
                    highlighted: modelData.active
                    onClicked: {
                        StreamProfileManager.activateProfile(profilesDialog.hostUuid,
                                                             modelData.profileId)
                        profilesDialog.close()
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                opacity: 0.55
                text: qsTr("New Profile")
                onClicked: {
                    var editor = StreamProfileManager.createEditor(
                                     profilesDialog.hostUuid, "", true)
                    profilesDialog.close()
                    window.openProfileEditor(editor)
                }
            }
        }
    }

    NavigableMessageDialog {
        id: deletePcDialog
        // don't allow edits to the rest of the window while open
        property int pcIndex : -1
        property string pcName : ""
        text: qsTr("Are you sure you want to remove '%1'?").arg(pcName)
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: {
            computerModel.deleteComputer(pcIndex)
        }
    }

    NavigableMessageDialog {
        id: testConnectionDialog
        closePolicy: Popup.CloseOnEscape
        standardButtons: Dialog.Ok

        onAboutToShow: {
            testConnectionDialog.text = qsTr("Moonlight is testing your network connection to determine if any required ports are blocked.") + "\n\n" + qsTr("This may take a few seconds…")
            showSpinner = true
        }

        function connectionTestComplete(result, blockedPorts)
        {
            if (result === -1) {
                text = qsTr("The network test could not be performed because none of Moonlight's connection testing servers were reachable from this PC. Check your Internet connection or try again later.")
                imageSrc = "qrc:/res/baseline-warning-24px.svg"
            }
            else if (result === 0) {
                text = qsTr("This network does not appear to be blocking Moonlight. If you still have trouble connecting, check your PC's firewall settings.") + "\n\n" + qsTr("If you are trying to stream over the Internet, install the Moonlight Internet Hosting Tool on your gaming PC and run the included Internet Streaming Tester to check your gaming PC's Internet connection.")
                imageSrc = "qrc:/res/baseline-check_circle_outline-24px.svg"
            }
            else {
                text = qsTr("Your PC's current network connection seems to be blocking Moonlight. Streaming over the Internet may not work while connected to this network.") + "\n\n" + qsTr("The following network ports were blocked:") + "\n"
                text += blockedPorts
                imageSrc = "qrc:/res/baseline-error_outline-24px.svg"
            }

            // Stop showing the spinner and show the image instead
            showSpinner = false
        }
    }

    NavigableDialog {
        id: renamePcDialog
        property string label: qsTr("Enter the new name for this PC:")
        property string originalName
        property int pcIndex : -1;

        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onClosed: {
            editText.clear()
        }

        onAccepted: {
            if (editText.text) {
                computerModel.renameComputer(pcIndex, editText.text)
            }
        }

        ColumnLayout {
            Label {
                text: renamePcDialog.label
                font.bold: true
            }

            TextField {
                id: editText
                placeholderText: renamePcDialog.originalName
                Layout.fillWidth: true
                focus: true

                Keys.onReturnPressed: {
                    renamePcDialog.accept()
                }

                Keys.onEnterPressed: {
                    renamePcDialog.accept()
                }
            }
        }
    }

    NavigableMessageDialog {
        id: showPcDetailsDialog
        property string pcDetails : "";
        text: showPcDetailsDialog.pcDetails
        imageSrc: "qrc:/res/baseline-help_outline-24px.svg"
        standardButtons: Dialog.Ok
    }

    ScrollBar.vertical: ScrollBar {}
}

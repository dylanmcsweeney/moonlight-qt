import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import StreamingPreferences 1.0
import StreamProfileManager 1.0
import SystemProperties 1.0

Flickable {
    id: appSettings
    objectName: qsTr("Application Settings")
    property bool graphicsRestartRequired: false
    property bool appPreferencesDirty: false
    property string closeAction: "back"
    contentWidth: width
    contentHeight: settingsColumn.implicitHeight + 20
    boundsBehavior: Flickable.OvershootBounds

    function savePreferencesNow(afterSave) {
        if (!StreamingPreferences.save()) {
            showAppSettingsSaveError()
            return
        }
        appPreferencesDirty = false
        if (afterSave) {
            afterSave()
        }
    }

    function showAppSettingsSaveError() {
        appSettingsSaveErrorDialog.text = StreamProfileManager.lastError ?
                StreamProfileManager.lastError :
                qsTr("Moonlight could not save the local settings. Check for a newer shared settings change and try again.")
        appSettingsSaveErrorDialog.open()
    }

    function updatePreference(propertyName, value, saveImmediately,
                              afterChange, afterSave) {
        window.requestSettingsAccess(function() {
            if (StreamingPreferences[propertyName] === value) {
                return
            }
            StreamingPreferences[propertyName] = value
            appPreferencesDirty = true
            if (afterChange) {
                afterChange()
            }
            if (saveImmediately) {
                savePreferencesNow(afterSave)
            }
        })
    }

    function selectSavedValue(comboBox, model, value) {
        for (var i = 0; i < model.count; i++) {
            if (model.get(i).value === value) {
                comboBox.currentIndex = i
                return
            }
        }
        comboBox.currentIndex = -1
    }

    function refreshControls() {
        selectSavedValue(languageComboBox, languageModel,
                         StreamingPreferences.language)
        selectSavedValue(displayModeComboBox, displayModeModel,
                         StreamingPreferences.uiDisplayMode)
        selectSavedValue(uiGraphicsBackendComboBox, uiGraphicsBackendModel,
                         StreamingPreferences.uiGraphicsBackend)
        selectSavedValue(uiD3D11SwapchainComboBox, uiD3D11SwapchainModel,
                         StreamingPreferences.uiD3D11SwapchainMode)
        selectSavedValue(uiFramePacingComboBox, uiFramePacingModel,
                         StreamingPreferences.uiFramePacingMode)
    }

    function requestTemplateEditor() {
        window.requestSettingsAccess(function() {
            if (appSettings.appPreferencesDirty ||
                    StreamProfileManager.localSettingsDirty) {
                closeAction = "template"
                unsavedAppSettingsDialog.open()
            } else {
                openTemplateEditor()
            }
        })
    }

    function openTemplateEditor() {
        window.openProfileEditor(function() {
            return StreamProfileManager.createTemplateEditor()
        })
    }

    function requestRestart() {
        window.requestSettingsAccess(function() {
            if (appPreferencesDirty ||
                    StreamProfileManager.localSettingsDirty) {
                closeAction = "restart"
                unsavedAppSettingsDialog.open()
            } else if (!UiRendererManager.restartApplication()) {
                window.showSettingsError(
                            qsTr("Moonlight could not restart automatically."))
            }
        })
    }

    function requestKeepUsingD3D11() {
        window.requestSettingsAccess(function() {
            if (appPreferencesDirty ||
                    StreamProfileManager.localSettingsDirty) {
                closeAction = "keep-d3d11"
                unsavedAppSettingsDialog.open()
            } else if (!UiRendererManager.keepUsingD3D11()) {
                window.showSettingsError(UiRendererManager.recoveryError)
            }
        })
    }

    function requestPreferredRendererRetry() {
        window.requestSettingsAccess(function() {
            if (appPreferencesDirty ||
                    StreamProfileManager.localSettingsDirty) {
                closeAction = "retry-preferred"
                unsavedAppSettingsDialog.open()
            } else if (!UiRendererManager.retryPreferredBackend()) {
                window.showSettingsError(UiRendererManager.recoveryError)
            }
        })
    }

    function requestClose() {
        if (appPreferencesDirty || StreamProfileManager.localSettingsDirty) {
            closeAction = "back"
            unsavedAppSettingsDialog.open()
            return true
        }
        return false
    }

    function requestApplicationClose() {
        if (appPreferencesDirty || StreamProfileManager.localSettingsDirty) {
            closeAction = "quit"
            unsavedAppSettingsDialog.open()
            return true
        }
        return false
    }

    function finishCloseAction() {
        if (closeAction === "quit") {
            window.requestConfirmedQuit()
        } else if (closeAction === "restart") {
            closeAction = "back"
            if (!UiRendererManager.restartApplication()) {
                window.showSettingsError(
                            qsTr("Moonlight could not restart automatically."))
            }
        } else if (closeAction === "template") {
            openTemplateEditor()
            closeAction = "back"
        } else if (closeAction === "keep-d3d11") {
            closeAction = "back"
            if (!UiRendererManager.keepUsingD3D11()) {
                window.showSettingsError(UiRendererManager.recoveryError)
            }
        } else if (closeAction === "retry-preferred") {
            closeAction = "back"
            if (!UiRendererManager.retryPreferredBackend()) {
                window.showSettingsError(UiRendererManager.recoveryError)
            }
        } else {
            stackView.pop()
        }
    }

    function saveAndClose() {
        window.requestSettingsAccess(function() {
            var saved = true
            if (appPreferencesDirty) {
                saved = StreamingPreferences.save()
                if (saved) {
                    appPreferencesDirty = false
                }
            }
            if (saved && !appPreferencesDirty &&
                    StreamProfileManager.localSettingsDirty) {
                saved = StreamProfileManager.saveLocalSnapshot()
            }
            if (!saved) {
                showAppSettingsSaveError()
                return
            }
            appPreferencesDirty = false
            unsavedAppSettingsDialog.close()
            finishCloseAction()
        })
    }

    function discardAndClose() {
        if (!StreamProfileManager.discardLocalChanges()) {
            window.showSettingsError(StreamProfileManager.lastError)
            return
        }
        appPreferencesDirty = false
        unsavedAppSettingsDialog.close()
        finishCloseAction()
    }

    function preferredWindowWidth() {
        return Math.max(window.initialPcWindowWidth, 900)
    }

    function preferredWindowHeight() {
        return window.header.height + contentHeight + 40
    }

    StackView.onActivated: {
        refreshControls()
        SdlGamepadKeyNavigation.setUiNavMode(true)
        Qt.callLater(function() {
            window.fitWindowForView(preferredWindowWidth(),
                                    preferredWindowHeight())
        })
        if (SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            defaultProfileButton.forceActiveFocus(Qt.TabFocus)
        }
    }

    StackView.onDeactivating: {
        SdlGamepadKeyNavigation.setUiNavMode(false)
    }

    Column {
        id: settingsColumn
        width: Math.min(parent.width - 30, 720)
        anchors.horizontalCenter: parent.horizontalCenter
        topPadding: 10
        spacing: 15

        GroupBox {
            width: parent.width
            title: "<font color=\"skyblue\">" + qsTr("Streaming Defaults") + "</font>"

            Column {
                anchors.fill: parent
                spacing: 8

                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("New PCs and new profiles start with the default streaming profile.")
                }

                Button {
                    id: defaultProfileButton
                    text: qsTr("Edit Default Streaming Profile")
                    onClicked: appSettings.requestTemplateEditor()
                }
            }
        }

        GroupBox {
            width: parent.width
            title: "<font color=\"skyblue\">" + qsTr("Application") + "</font>"

            Column {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: qsTr("Language")
                }

                AutoResizingComboBox {
                    id: languageComboBox
                    textRole: "text"
                    model: ListModel {
                        id: languageModel
                        ListElement { text: qsTr("Automatic"); value: StreamingPreferences.LANG_AUTO }
                        ListElement { text: "English"; value: StreamingPreferences.LANG_EN }
                        ListElement { text: "Deutsch"; value: StreamingPreferences.LANG_DE }
                        ListElement { text: "Français"; value: StreamingPreferences.LANG_FR }
                        ListElement { text: "简体中文"; value: StreamingPreferences.LANG_ZH_CN }
                        ListElement { text: "繁體中文"; value: StreamingPreferences.LANG_ZH_TW }
                        ListElement { text: "Español"; value: StreamingPreferences.LANG_ES }
                        ListElement { text: "Português"; value: StreamingPreferences.LANG_PT }
                        ListElement { text: "Português do Brasil"; value: StreamingPreferences.LANG_PT_BR }
                        ListElement { text: "Italiano"; value: StreamingPreferences.LANG_IT }
                        ListElement { text: "Nederlands"; value: StreamingPreferences.LANG_NL }
                        ListElement { text: "Norsk Bokmål"; value: StreamingPreferences.LANG_NB_NO }
                        ListElement { text: "Svenska"; value: StreamingPreferences.LANG_SV }
                        ListElement { text: "Polski"; value: StreamingPreferences.LANG_PL }
                        ListElement { text: "Čeština"; value: StreamingPreferences.LANG_CS }
                        ListElement { text: "Magyar"; value: StreamingPreferences.LANG_HU }
                        ListElement { text: "Türkçe"; value: StreamingPreferences.LANG_TR }
                        ListElement { text: "Ελληνικά"; value: StreamingPreferences.LANG_EL }
                        ListElement { text: "Български"; value: StreamingPreferences.LANG_BG }
                        ListElement { text: "Русский"; value: StreamingPreferences.LANG_RU }
                        ListElement { text: "日本語"; value: StreamingPreferences.LANG_JA }
                        ListElement { text: "한국어"; value: StreamingPreferences.LANG_KO }
                        ListElement { text: "Tiếng Việt"; value: StreamingPreferences.LANG_VI }
                        ListElement { text: "ภาษาไทย"; value: StreamingPreferences.LANG_TH }
                        ListElement { text: "தமிழ்"; value: StreamingPreferences.LANG_TA }
                    }

                    Component.onCompleted: {
                        for (var i = 0; i < languageModel.count; i++) {
                            if (languageModel.get(i).value === StreamingPreferences.language) {
                                currentIndex = i
                                return
                            }
                        }
                    }

                    onActivated: {
                        var language = languageModel.get(currentIndex).value
                        if (language !== StreamingPreferences.language) {
                            appSettings.updatePreference("language", language,
                                    true, null, function() {
                                if (!StreamingPreferences.retranslate()) {
                                    ToolTip.show(qsTr("You must restart Moonlight for this change to take effect"), 5000)
                                } else {
                                    window.clearOnBack = true
                                }
                            })
                        }
                    }
                }

                Label {
                    visible: SystemProperties.hasDesktopEnvironment
                    text: qsTr("GUI display mode")
                }

                AutoResizingComboBox {
                    id: displayModeComboBox
                    visible: SystemProperties.hasDesktopEnvironment
                    textRole: "text"
                    model: ListModel {
                        id: displayModeModel
                        ListElement { text: qsTr("Windowed"); value: StreamingPreferences.UI_WINDOWED }
                        ListElement { text: qsTr("Maximized"); value: StreamingPreferences.UI_MAXIMIZED }
                        ListElement { text: qsTr("Fullscreen"); value: StreamingPreferences.UI_FULLSCREEN }
                    }
                    Component.onCompleted: {
                        for (var i = 0; i < displayModeModel.count; i++) {
                            if (displayModeModel.get(i).value === StreamingPreferences.uiDisplayMode) {
                                currentIndex = i
                                return
                            }
                        }
                    }
                    onActivated: appSettings.updatePreference(
                            "uiDisplayMode",
                            displayModeModel.get(currentIndex).value, false)
                }

                Label {
                    visible: SystemProperties.hasDesktopEnvironment &&
                             Qt.platform.os === "windows"
                    text: qsTr("GUI graphics backend")
                }

                AutoResizingComboBox {
                    id: uiGraphicsBackendComboBox
                    visible: SystemProperties.hasDesktopEnvironment &&
                             Qt.platform.os === "windows"
                    textRole: "text"
                    model: ListModel {
                        id: uiGraphicsBackendModel
                        ListElement { text: qsTr("Automatic (Recommended)"); value: StreamingPreferences.UI_GRAPHICS_AUTOMATIC }
                        ListElement { text: qsTr("Direct3D 12"); value: StreamingPreferences.UI_GRAPHICS_D3D12 }
                        ListElement { text: qsTr("Direct3D 11"); value: StreamingPreferences.UI_GRAPHICS_D3D11 }
                        ListElement { text: qsTr("OpenGL"); value: StreamingPreferences.UI_GRAPHICS_OPENGL }
                    }

                    Component.onCompleted: {
                        for (var i = 0; i < uiGraphicsBackendModel.count; i++) {
                            if (uiGraphicsBackendModel.get(i).value ===
                                    StreamingPreferences.uiGraphicsBackend) {
                                currentIndex = i
                                return
                            }
                        }
                    }

                    onActivated: {
                        appSettings.updatePreference(
                                "uiGraphicsBackend",
                                uiGraphicsBackendModel.get(currentIndex).value,
                                true, function() {
                                    appSettings.graphicsRestartRequired = true
                                })
                    }
                }

                Label {
                    visible: uiGraphicsBackendComboBox.visible &&
                             StreamingPreferences.uiGraphicsBackend ===
                                 StreamingPreferences.UI_GRAPHICS_D3D11
                    text: qsTr("Direct3D 11 presentation mode")
                }

                AutoResizingComboBox {
                    id: uiD3D11SwapchainComboBox
                    visible: uiGraphicsBackendComboBox.visible &&
                             StreamingPreferences.uiGraphicsBackend ===
                                 StreamingPreferences.UI_GRAPHICS_D3D11
                    textRole: "text"
                    model: ListModel {
                        id: uiD3D11SwapchainModel
                        ListElement { text: qsTr("Modern flip model"); value: StreamingPreferences.UI_D3D11_FLIP }
                        ListElement { text: qsTr("Legacy swap chain"); value: StreamingPreferences.UI_D3D11_LEGACY }
                    }

                    Component.onCompleted: {
                        for (var i = 0; i < uiD3D11SwapchainModel.count; i++) {
                            if (uiD3D11SwapchainModel.get(i).value ===
                                    StreamingPreferences.uiD3D11SwapchainMode) {
                                currentIndex = i
                                return
                            }
                        }
                    }

                    onActivated: {
                        appSettings.updatePreference(
                                "uiD3D11SwapchainMode",
                                uiD3D11SwapchainModel.get(currentIndex).value,
                                true, function() {
                                    appSettings.graphicsRestartRequired = true
                                })
                    }
                }

                Label {
                    visible: uiGraphicsBackendComboBox.visible &&
                             UiRendererManager.rendererReady
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: UiRendererManager.adapterName ?
                              qsTr("Active GUI renderer: %1 on %2")
                                  .arg(UiRendererManager.actualBackend)
                                  .arg(UiRendererManager.adapterName) :
                              qsTr("Active GUI renderer: %1")
                                  .arg(UiRendererManager.actualBackend)
                }

                Label {
                    visible: uiGraphicsBackendComboBox.visible
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: qsTr("Saved GUI renderer: %1")
                        .arg(UiRendererManager.savedBackend === StreamingPreferences.UI_GRAPHICS_D3D12 ? qsTr("Direct3D 12") :
                             UiRendererManager.savedBackend === StreamingPreferences.UI_GRAPHICS_D3D11 ? qsTr("Direct3D 11") :
                             UiRendererManager.savedBackend === StreamingPreferences.UI_GRAPHICS_OPENGL ? qsTr("OpenGL") :
                             qsTr("Automatic"))
                }

                Label {
                    visible: UiStartupRecovery.recoveryActive ||
                             UiStartupRecovery.safeUiActive ||
                             UiStartupRecovery.warning.length > 0
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#ffcc80"
                    text: UiStartupRecovery.warning.length > 0 &&
                          !UiStartupRecovery.recoveryActive &&
                          !UiStartupRecovery.safeUiActive ?
                              UiStartupRecovery.warning :
                          UiStartupRecovery.recoveryActive ?
                              qsTr("Startup recovery is using Direct3D 11. The saved renderer remains unchanged until you choose an action.") :
                              qsTr("Safe Startup is using Direct3D 11 for this launch. The saved renderer remains unchanged.")
                }

                Label {
                    visible: UiRendererManager.recoveryError.length > 0
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#ff8080"
                    text: UiRendererManager.recoveryError
                }

                RowLayout {
                    visible: UiStartupRecovery.recoveryActive ||
                             UiStartupRecovery.safeUiActive ||
                             UiStartupRecovery.warning.length > 0
                    Button {
                        visible: UiStartupRecovery.recoveryActive ||
                                 (UiStartupRecovery.warning.length > 0 &&
                                  UiStartupRecovery.effectiveBackend === StreamingPreferences.UI_GRAPHICS_D3D11)
                        text: qsTr("Keep using Direct3D 11")
                        onClicked: appSettings.requestKeepUsingD3D11()
                    }
                    Button {
                        text: qsTr("Retry saved renderer")
                        onClicked: appSettings.requestPreferredRendererRetry()
                    }
                }

                Label {
                    visible: appSettings.graphicsRestartRequired
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#ffcc80"
                    text: qsTr("Restart Moonlight to apply the graphics backend change.")
                }

                Button {
                    visible: appSettings.graphicsRestartRequired
                    text: qsTr("Restart Now")
                    onClicked: appSettings.requestRestart()
                }

                Label {
                    visible: SystemProperties.hasDesktopEnvironment
                    text: qsTr("GUI frame rendering")
                }

                AutoResizingComboBox {
                    id: uiFramePacingComboBox
                    visible: SystemProperties.hasDesktopEnvironment
                    textRole: "text"
                    model: ListModel {
                        id: uiFramePacingModel
                        ListElement { text: qsTr("Normal (render only when changed)"); value: StreamingPreferences.UI_FRAME_PACING_DISABLED }
                        ListElement { text: qsTr("Match display refresh rate"); value: StreamingPreferences.UI_FRAME_PACING_MATCH_DISPLAY }
                        ListElement { text: qsTr("Fixed 60 FPS"); value: StreamingPreferences.UI_FRAME_PACING_60 }
                        ListElement { text: qsTr("Fixed 90 FPS"); value: StreamingPreferences.UI_FRAME_PACING_90 }
                        ListElement { text: qsTr("Fixed 120 FPS"); value: StreamingPreferences.UI_FRAME_PACING_120 }
                        ListElement { text: qsTr("Fixed 144 FPS"); value: StreamingPreferences.UI_FRAME_PACING_144 }
                        ListElement { text: qsTr("Unbounded"); value: StreamingPreferences.UI_FRAME_PACING_UNBOUNDED }
                    }

                    Component.onCompleted: {
                        for (var i = 0; i < uiFramePacingModel.count; i++) {
                            if (uiFramePacingModel.get(i).value === StreamingPreferences.uiFramePacingMode) {
                                currentIndex = i
                                return
                            }
                        }
                    }

                    onActivated: {
                        appSettings.updatePreference(
                                "uiFramePacingMode",
                                uiFramePacingModel.get(currentIndex).value,
                                true)
                    }
                }

                Label {
                    visible: SystemProperties.hasDesktopEnvironment &&
                             StreamingPreferences.uiFramePacingMode !==
                                 StreamingPreferences.UI_FRAME_PACING_DISABLED
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: {
                        if (!UiFramePacer.active || UiFramePacer.measuredFps <= 0) {
                            return qsTr("Measured GUI presentation rate: measuring...")
                        }

                        return qsTr("Measured GUI presentation rate: %1 FPS")
                            .arg(UiFramePacer.measuredFps.toFixed(1))
                    }
                }

                Label {
                    visible: SystemProperties.hasDesktopEnvironment &&
                             StreamingPreferences.uiFramePacingMode ===
                                 StreamingPreferences.UI_FRAME_PACING_UNBOUNDED
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: "#ffcc80"
                    text: qsTr("Unbounded rendering relies on V-Sync or an external frame limiter. Without one, Moonlight may use substantial CPU and GPU resources.")
                }

                CheckBox {
                    text: qsTr("Automatically find PCs on the local network (Recommended)")
                    checked: StreamingPreferences.enableMdns
                    onToggled: {
                        appSettings.updatePreference("enableMdns", checked,
                                false, function() {
                                    if (window.pollingActive) {
                                        ComputerManager.stopPollingAsync()
                                        ComputerManager.startPolling()
                                    }
                                })
                    }
                }

                CheckBox {
                    text: qsTr("Automatically detect blocked connections (Recommended)")
                    checked: StreamingPreferences.detectNetworkBlocking
                    onToggled: appSettings.updatePreference(
                            "detectNetworkBlocking", checked, false)
                }

                CheckBox {
                    visible: SystemProperties.hasDiscordIntegration
                    text: qsTr("Discord Rich Presence integration")
                    checked: StreamingPreferences.richPresence
                    onToggled: appSettings.updatePreference(
                            "richPresence", checked, false)
                }

                CheckBox {
                    text: qsTr("Use Nintendo-style buttons for Moonlight menus")
                    checked: StreamingPreferences.uiSwapFaceButtons
                    onToggled: appSettings.updatePreference(
                            "uiSwapFaceButtons", checked, false)
                }

                CheckBox {
                    text: qsTr("Show streaming profile controls on PC and app pages")
                    checked: StreamingPreferences.showPcProfileControls
                    onToggled: {
                        appSettings.updatePreference(
                                "showPcProfileControls", checked, true)
                    }

                    ToolTip.delay: 1000
                    ToolTip.timeout: 5000
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Hide the profile selector and settings button for a simpler TV interface. They remain available from each PC and app context menu.")
                }
            }
        }
    }

    NavigableDialog {
        id: unsavedAppSettingsDialog
        title: qsTr("Unsaved Settings")
        modal: true

        Label {
            width: 400
            wrapMode: Text.Wrap
            text: qsTr("Save this instance's local settings before continuing?")
        }

        footer: DialogButtonBox {
            Button {
                text: qsTr("Save")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: appSettings.saveAndClose()
            }
            Button {
                text: qsTr("Discard")
                DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
                onClicked: appSettings.discardAndClose()
            }
            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: unsavedAppSettingsDialog.close()
            }
        }
    }

    NavigableMessageDialog {
        id: appSettingsSaveErrorDialog
        text: qsTr("Moonlight could not save the local settings. Check for a newer shared settings change and try again.")
        standardButtons: Dialog.Ok
    }

    ScrollBar.vertical: ScrollBar {}
}

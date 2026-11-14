.pragma library

// Dropping an external-settings prompt also drops its deferred operation.
function cancelExternalSettingsAction(window) {
    if (!window) return
    window.pendingExternalSettingsAction = null
    window.pendingExternalReload = null
}

// A rejected unsaved-settings prompt cancels the requested window close/quit.
function cancelGlobalCloseAction(window) {
    if (!window) return
    window.pendingGlobalCloseAction = ""
}

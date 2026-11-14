# Dialog and navigation regression

Run the controller focus and back-navigation regression from the repository
root with Qt 6.11.1's `qmltestrunner` on `PATH`:

```powershell
$env:QT_QUICK_CONTROLS_STYLE = "Material"
$env:QT_QPA_PLATFORM = "offscreen"
$env:QT_QUICK_BACKEND = "software"
qmltestrunner -input tests/qml/tst_ErrorMessageDialog.qml -import tests/qml/mocks -platform offscreen
```

The test instantiates the real navigable dialog components and focus helpers.
It covers controller confirm/cancel keys, nested dialogs and popups, view
changes while a modal is open, dialog-first back navigation, deferred-action
cancellation, and dialog destruction. The mock modules replace only the
application C++ singletons needed by the combo box popup test.

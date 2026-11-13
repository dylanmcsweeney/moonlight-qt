# GUI QML regression tests

Run the toolbar layout regression from the repository root with Qt 6.11.1's
`qmltestrunner` on `PATH`:

```powershell
$env:QT_QUICK_CONTROLS_STYLE = "Material"
$env:QT_QPA_PLATFORM = "offscreen"
$env:QT_QUICK_BACKEND = "software"
qmltestrunner -input tests/gui
```

The test loads the real `NavigableToolButton`, adds a short text label to force
Material's nonsquare-content background branch, then cycles a toolbar row
through compact and expanded sizes. It checks that button and icon dimensions
stay bounded and settle after each change. This exercises the feedback path;
dragging the app between mixed-DPI monitors remains a separate manual check.

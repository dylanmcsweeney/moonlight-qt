# GUI startup recovery

On Windows, Moonlight records progress while a graphical launch initializes
Qt Quick. A launch is considered healthy after Qt identifies the renderer,
submits a frame, and the UI event loop stays responsive for three seconds.
The journal includes startup stages and elapsed milliseconds so a later
recovery can show where an abandoned launch stopped. Help, version, list, and
headless launches do not create startup attempts.

If a Direct3D 12 launch is abandoned before the UI becomes healthy, the next
graphical launch uses Direct3D 11 for that process. The saved graphics
preference stays unchanged until the user chooses **Keep using Direct3D 11**.
The recovery notice also offers **Retry saved renderer** to try the saved
choice in a fresh process. The app settings page keeps these actions available
after the notice is dismissed.

Users can start a one-off Direct3D 11 session with `Moonlight.exe --safe-ui`.
This does not change saved settings. Restarting from that session removes the
temporary flag, so normal launches return to the saved choice. Windows
portable packages include **Moonlight Safe Startup.cmd** beside the executable
and MSI installs add a matching Start Menu shortcut.

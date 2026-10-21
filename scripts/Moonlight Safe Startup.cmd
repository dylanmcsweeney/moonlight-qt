@echo off
setlocal
cd /d "%~dp0"
start "" "%~dp0Moonlight.exe" --safe-ui

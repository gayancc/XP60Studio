@echo off
rem Starts XPPatchStudio. Double-click this file.
rem If the app has never been built, it builds it first (takes a few minutes).
cd /d "%~dp0"

if not exist "target\release\xppatch-app.exe" (
  echo XPPatchStudio has not been built yet. Building it now...
  call "Rebuild and Run XPPatchStudio.bat"
  exit /b
)

start "" "target\release\xppatch-app.exe"

@echo off
rem Rebuilds XPPatchStudio from the latest source, then starts it. Double-click this file.
rem Use it after the code has changed. It closes a running copy first (a running app locks its own file).
cd /d "%~dp0"

echo Closing XPPatchStudio if it is running...
taskkill /im xppatch-app.exe /f >nul 2>&1

echo.
echo [1/2] Building the interface...
cd ui
call npm run build
if errorlevel 1 goto failed
cd ..

echo.
echo [2/2] Building the application (the first time takes several minutes)...
cargo build -p xppatch-app --release --features custom-protocol
if errorlevel 1 goto failed

echo.
echo Done. Starting XPPatchStudio...
start "" "target\release\xppatch-app.exe"
exit /b 0

:failed
echo.
echo The build failed. Read the messages above, then press any key to close this window.
pause >nul
exit /b 1

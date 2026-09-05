@echo off
setlocal EnableExtensions DisableDelayedExpansion
title XP60Studio Launcher

rem Resolve everything relative to this file, including when opened via a shortcut.
set "XP60_ROOT=%~dp0"
set "XP60_APP=%XP60_ROOT%build-windows\XP60Studio.exe"
set "XP60_QT=%XP60_ROOT%.qt\6.11.2\mingw_64"
set "XP60_MINGW=%XP60_ROOT%.qt\Tools\mingw1310_64\bin"

echo.
choice /C YN /M "Rebuild before run"
if errorlevel 2 goto prepare_run
if errorlevel 1 goto do_rebuild

:do_rebuild
echo.
echo Rebuilding XP60Studio...
powershell -NoProfile -ExecutionPolicy Bypass -File "%XP60_ROOT%tools\build_windows.ps1" -SkipTests
if errorlevel 1 goto rebuild_failed
echo.
echo Rebuild finished.
goto prepare_run

:prepare_run
if not exist "%XP60_APP%" goto missing_app
if not exist "%XP60_QT%\bin\Qt6Core.dll" goto missing_runtime
if not exist "%XP60_QT%\plugins\platforms\qwindows.dll" goto missing_runtime
if not exist "%XP60_MINGW%\libstdc++-6.dll" goto missing_runtime

rem Environment changes apply only to the launcher and the app it starts.
set "PATH=%XP60_QT%\bin;%XP60_MINGW%;%PATH%"
set "QT_PLUGIN_PATH=%XP60_QT%\plugins"
set "QML_IMPORT_PATH=%XP60_QT%\qml"
set "QT_QPA_PLATFORM=windows"
set "QT_QPA_FONTDIR=%WINDIR%\Fonts"
set "XP60STUDIO_SCREENSHOT="

start "" /D "%XP60_ROOT%build-windows" "%XP60_APP%"
if errorlevel 1 goto launch_failed
exit /b 0

:rebuild_failed
echo.
echo Rebuild failed. Fix the build errors, then run this launcher again.
goto failed

:missing_app
echo XP60Studio has not been built yet.
echo.
echo Choose Y when prompted to rebuild, or from PowerShell run:
echo   .\tools\build_windows.ps1
echo.
echo Then double-click Run-XP60Studio.bat again.
goto failed

:missing_runtime
echo The local Qt or MinGW runtime is missing.
echo Follow "Windows setup and revalidation" in README.md, then build the app.
echo Keep this launcher in the project folder beside .qt and build-windows.
goto failed

:launch_failed
echo Windows could not start XP60Studio.
echo Try rebuilding with .\tools\build_windows.ps1 in PowerShell.

:failed
echo.
pause
exit /b 1

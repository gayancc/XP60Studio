@echo off
rem XPPatchStudio launcher. Double-click this file and choose what to do:
rem   1  Run              start the app (builds it first if it has never been built)
rem   2  Build and run    rebuild from the latest source, then start the app
rem   3  Publish          build the Windows installer (setup .exe), then optionally publish it as a GitHub release
setlocal enabledelayedexpansion
cd /d "%~dp0"

:menu
cls
echo.
echo   XPPatchStudio
echo   ------------------------------------------------------------
echo     1  Run               start the app
echo     2  Build and run     rebuild from the latest source, then start
echo     3  Publish           build the installer, then optionally put it on GitHub
echo     Q  Quit
echo   ------------------------------------------------------------
choice /c 123Q /n /m "  Choose 1, 2, 3 or Q: "
if errorlevel 4 exit /b 0
if errorlevel 3 goto publish
if errorlevel 2 goto buildrun
if errorlevel 1 goto run

:run
if not exist "target\release\xppatch-app.exe" (
  echo.
  echo XPPatchStudio has not been built yet. Building it now...
  goto buildrun
)
start "" "target\release\xppatch-app.exe"
exit /b 0

:buildrun
call :closeapp
call :buildui || goto failed
echo.
echo [3/3] Building the application (the first time takes several minutes)...
cargo build -p xppatch-app --release --features custom-protocol
if errorlevel 1 goto failed
echo.
echo Done. Starting XPPatchStudio...
start "" "target\release\xppatch-app.exe"
exit /b 0

:publish
call :closeapp
echo.
echo Building the installer (the first time takes several minutes)...
pushd ui
call npm run installer
set BUILD_RESULT=!errorlevel!
popd
if not "!BUILD_RESULT!"=="0" goto failed

set INSTALLER=
for %%f in ("target\release\bundle\nsis\*-setup.exe") do set INSTALLER=%%~ff
if not defined INSTALLER (
  echo The build finished but no setup file was found in target\release\bundle\nsis.
  goto failed
)
echo.
echo Installer ready:
echo   !INSTALLER!
start "" explorer /select,"!INSTALLER!"

echo.
choice /c YN /n /m "Also publish it as a GitHub release now? (Y/N): "
if errorlevel 2 goto done

rem The release is made on GitHub from what is pushed there, so everything must be committed and pushed first.
for /f "delims=" %%s in ('git status --porcelain') do set DIRTY=1
if defined DIRTY (
  echo.
  echo There are uncommitted changes. Commit and push them first, so the release matches the installer. Nothing was published.
  goto done
)
for /f %%h in ('git rev-parse HEAD') do set LOCALHEAD=%%h
for /f %%h in ('git rev-parse origin/main') do set REMOTEHEAD=%%h
if not "!LOCALHEAD!"=="!REMOTEHEAD!" (
  echo.
  echo The latest commit has not been pushed to GitHub yet. Run "git push" first. Nothing was published.
  goto done
)
for /f "tokens=2 delims==" %%v in ('findstr /b /c:"version = " Cargo.toml') do (
  if not defined VERSION set VERSION=%%v
)
set VERSION=!VERSION: =!
set VERSION=!VERSION:"=!
set TAG=v!VERSION!
echo.
echo This will create the GitHub release !TAG! and upload the installer.
choice /c YN /n /m "Publish !TAG! now? (Y/N): "
if errorlevel 2 goto done
gh release create "!TAG!" "!INSTALLER!" --title "XPPatchStudio !TAG!" --generate-notes --notes "Windows installer: download the setup file, run it, choose the install location and follow the steps."
if errorlevel 1 (
  echo.
  echo Publishing failed. If the release already exists, raise the version in Cargo.toml and tauri.conf.json first.
  goto failed
)
echo.
echo Published !TAG!.

:done
echo.
echo Press any key to close this window.
pause >nul
exit /b 0

:closeapp
echo Closing XPPatchStudio if it is running...
taskkill /im xppatch-app.exe /f >nul 2>&1
exit /b 0

:buildui
echo.
echo [1/3] Building the codec for the interface...
pushd ui
call npm run wasm
if errorlevel 1 ( popd & exit /b 1 )
echo.
echo [2/3] Building the interface...
call npm run build
if errorlevel 1 ( popd & exit /b 1 )
popd
exit /b 0

:failed
echo.
echo The build failed. Read the messages above, then press any key to close this window.
pause >nul
exit /b 1

# Headers only: no system driver, Bluetooth pairing or global toolchain changes.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $projectRoot '.qt/cppwinrt'
$archive = Join-Path $destination 'cppwinrt.pkg.tar.zst'
$url = 'https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-cppwinrt-2.0.250303.1-2-any.pkg.tar.zst'
$sha256 = 'ca3cb1fee300c4b8c8d21437cb964dccccae8b9a131e10aad911fb7fcd351c8d'
# Package hash and version: https://packages.msys2.org/packages/mingw-w64-x86_64-cppwinrt
New-Item -ItemType Directory -Path $destination -Force | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    try { Invoke-WebRequest $url -OutFile $archive } catch { throw 'C++/WinRT download failed; retry the MSYS2 repository later.' }
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $sha256) { throw 'C++/WinRT package checksum mismatch.' }
Push-Location $destination
try {
    & "$projectRoot/.venv-build/Scripts/cmake.exe" -E tar xf $archive
    if ($LASTEXITCODE -ne 0) { throw 'C++/WinRT extraction failed.' }
} finally { Pop-Location }
Write-Output 'C++/WinRT headers installed. tools/build_windows.ps1 will include native Windows Bluetooth MIDI.'

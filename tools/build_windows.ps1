param(
    [switch]$SkipTests,
    [switch]$Run,
    [int]$Jobs = 6
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$qtRoot = Join-Path $projectRoot '.qt/6.11.2/mingw_64'
$compilerBin = Join-Path $projectRoot '.qt/Tools/mingw1310_64/bin'
$toolsBin = Join-Path $projectRoot '.venv-build/Scripts'
$buildDir = Join-Path $projectRoot 'build-windows'
$previousPath = $env:PATH
$previousFontDir = $env:QT_QPA_FONTDIR
try {
    foreach ($required in @("$qtRoot/bin/qmake.exe", "$compilerBin/g++.exe", "$toolsBin/cmake.exe", "$toolsBin/ninja.exe")) {
        if (-not (Test-Path -LiteralPath $required)) { throw "Missing build dependency: $required. See README.md, Windows setup." }
    }
    $env:PATH = "$qtRoot/bin;$compilerBin;$toolsBin;$previousPath"
    $env:QT_QPA_FONTDIR = Join-Path $env:WINDIR 'Fonts'
    $winrtHeaders = Join-Path $projectRoot '.qt/cppwinrt/mingw64/include'
    $midiOptions = @()
    if (Test-Path "$winrtHeaders/winrt/Windows.Devices.Midi.h") {
        $midiOptions = @('-DXP60STUDIO_ENABLE_WINUWP=ON', "-DCPPWINRT_PATH=$winrtHeaders", "-DWINRT_HEADER_PATH=$winrtHeaders")
    } else {
        $midiOptions = @('-DXP60STUDIO_ENABLE_WINUWP=OFF')
        Write-Warning 'Native Bluetooth MIDI is not built: run tools/install_windows_winrt.ps1 to install its headers.'
    }
    & "$toolsBin/cmake.exe" -S $projectRoot -B $buildDir -G Ninja "-DCMAKE_PREFIX_PATH=$qtRoot" '-DCMAKE_BUILD_TYPE=Debug' "-DCMAKE_CXX_COMPILER=$compilerBin/g++.exe" "-DPython3_EXECUTABLE=$toolsBin/python.exe" @midiOptions
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
    & "$toolsBin/cmake.exe" --build $buildDir --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if (-not $SkipTests) {
        & "$toolsBin/ctest.exe" --test-dir $buildDir --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            $failuresFile = Join-Path $buildDir 'Testing/Temporary/LastTestsFailed.log'
            if (Test-Path -LiteralPath $failuresFile) {
                foreach ($failureLine in Get-Content -LiteralPath $failuresFile) {
                    $testName = ($failureLine -split ':', 2)[1]
                    $resultFile = Join-Path $buildDir "$testName-results.txt"
                    if (Test-Path -LiteralPath $resultFile) { Get-Content -LiteralPath $resultFile }
                }
            }
            throw 'Tests failed. Detailed results are in build-windows/*-results.txt.'
        }
    }
    if ($Run) { & "$buildDir/XP60Studio.exe" }
} finally {
    $env:PATH = $previousPath
    $env:QT_QPA_FONTDIR = $previousFontDir
}

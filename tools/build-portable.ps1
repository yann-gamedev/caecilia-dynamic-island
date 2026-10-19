param(
    [string]$Compiler = 'C:\msys64\ucrt64\bin\g++.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if (-not (Test-Path $Compiler)) { throw "Local MinGW compiler not found: $Compiler" }
    $output = Join-Path $root 'artifacts\M1-portable'
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $flags = @('-std=c++20', '-O2', '-Wall', '-Wextra', '-Wconversion', '-Werror',
        '-DUNICODE', '-D_UNICODE', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN',
        '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-Isrc', '-static',
        '-static-libgcc', '-static-libstdc++')
    $sources = @('src/main.cpp', 'src/app/App.cpp', 'src/window/OverlayWindow.cpp',
        'src/render/Renderer.cpp', 'src/render/IslandGeometry.cpp',
        'src/render/Painter.cpp', 'src/render/TextStyles.cpp', 'src/data/DemoSource.cpp',
        'src/motion/Spring.cpp', 'src/state/StateMachine.cpp', 'src/input/HoverTracker.cpp')
    & $Compiler @flags -municode -mwindows @sources -o "$output\CaeciliaNotchIsland.exe" `
        -ld3d11 -ldxgi -ld2d1 -ldcomp -ldwrite -ldxguid -luuid -luser32 -lgdi32 -lole32 -lshell32
    if ($LASTEXITCODE -ne 0) { throw 'Portable application compilation failed' }
    & $Compiler @flags tests/GeometryTests.cpp src/render/IslandGeometry.cpp `
        -o "$output\CaeciliaGeometryTests.exe" -ld2d1 -lole32 -luuid
    if ($LASTEXITCODE -ne 0) { throw 'Geometry test compilation failed' }
    & "$output\CaeciliaGeometryTests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Geometry tests failed' }
    & $Compiler @flags tests/MotionTests.cpp src/motion/Spring.cpp src/state/StateMachine.cpp `
        src/input/HoverTracker.cpp -o "$output\CaeciliaMotionTests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Motion test compilation failed' }
    & "$output\CaeciliaMotionTests.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Motion tests failed' }
    Write-Host "Portable M1 build: $output"
    Write-Host 'Alternative MinGW build only; MSVC /W4 acceptance remains pending.'
} finally {
    Pop-Location
}

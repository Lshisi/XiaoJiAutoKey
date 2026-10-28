param(
    [Parameter(Mandatory = $true)][string]$QtSourceDir,
    [Parameter(Mandatory = $true)][string]$InstallDir,
    [string]$BuildDir = "$InstallDir-build"
)

$ErrorActionPreference = "Stop"

foreach ($tool in @("cl.exe", "cmake.exe", "ninja.exe", "python.exe")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool not found. Run from VS2022 x64 Native Tools with CMake, Ninja and Python on Path."
    }
}
$configure = Join-Path $QtSourceDir "configure.bat"
if (-not (Test-Path $configure -PathType Leaf)) {
    throw "Qt 6 source configure.bat not found: $configure"
}

New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
Push-Location $BuildDir
try {
    $configureArgs = @(
        "-static", "-static-runtime", "-release",
        "-prefix", $InstallDir,
        "-opensource", "-confirm-license",
        "-nomake", "examples", "-nomake", "tests",
        "-submodules", "qtbase,qtshadertools,qtdeclarative,qtsvg"
    )
    & $configure @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "Qt 6 configure failed." }
    & cmake --build . --parallel
    if ($LASTEXITCODE -ne 0) { throw "Qt 6 build failed." }
    & cmake --install .
    if ($LASTEXITCODE -ne 0) { throw "Qt 6 install failed." }
} finally {
    Pop-Location
}

Write-Host "Qt 6 static build installed to: $InstallDir"
Write-Host "Configure XiaoJiAutoKey with -DCMAKE_PREFIX_PATH=$InstallDir"

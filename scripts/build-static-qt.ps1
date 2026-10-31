param(
    [string] $QtVersion = "6.8.3",
    [string] $QtRoot = "D:\Qt",
    [string] $QtSourceDir = "",
    [string] $InstallDir = "",
    [string] $BuildDir = "",
    [string] $CMakePath = "D:\software\cmake-3.31.6\bin\cmake.exe",
    [string] $PythonPath = "",
    [string] $NinjaPath = "",
    [string] $VcVarsAll = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat",
    [string] $Proxy = "",
    [int] $Parallel = 0,
    [switch] $Ltcg,
    [switch] $Incremental,
    [switch] $RemoveIntermediates
)

$ErrorActionPreference = "Stop"

function Get-FullPath {
    param([Parameter(Mandatory = $true)][string] $Path)
    # Join-Path textually prepends the current dir even for drive-qualified
    # paths (D:\Qt), which makes GetFullPath reject the embedded colon.
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Remove-ExactPath {
    param([Parameter(Mandatory = $true)][string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $expectedPath = Get-FullPath $Path
    if (-not [string]::Equals($resolvedPath, $expectedPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unexpected path: $resolvedPath"
    }

    Remove-Item -LiteralPath $resolvedPath -Recurse -Force
}

# Canonicalize all locations before creating or deleting anything.
$QtRoot = Get-FullPath $QtRoot
$sourceArchive = Join-Path $QtRoot "qt-everywhere-src-$QtVersion.zip"
if ([string]::IsNullOrWhiteSpace($QtSourceDir)) {
    $QtSourceDir = Join-Path $QtRoot "qt-everywhere-src-$QtVersion"
}
if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    $InstallDir = Join-Path $QtRoot "$QtVersion-static"
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $QtRoot "qt-$QtVersion-static-build"
}

$QtSourceDir = Get-FullPath $QtSourceDir
$InstallDir = Get-FullPath $InstallDir
$BuildDir = Get-FullPath $BuildDir
$CMakePath = Get-FullPath $CMakePath

$uniqueLocations = @($QtRoot, $QtSourceDir, $InstallDir, $BuildDir) |
    Select-Object -Unique
if ($uniqueLocations.Count -ne 4) {
    throw "Qt root, source, build, and install directories must be different."
}
if ($InstallDir.StartsWith($QtSourceDir, [System.StringComparison]::OrdinalIgnoreCase) -or
    $InstallDir.StartsWith($BuildDir, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The Qt install directory must not be inside the source or build directory."
}

if ($Parallel -le 0) {
    $Parallel = [Math]::Min([Environment]::ProcessorCount, 10)
}

# Enter the MSVC x64 environment when the script was not started from Native Tools.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    if (-not (Test-Path -LiteralPath $VcVarsAll -PathType Leaf)) {
        throw "cl.exe not found and vcvarsall.bat not found: $VcVarsAll"
    }

    $environmentOutput = & cmd.exe /d /c "`"$VcVarsAll`" x64 >NUL 2>&1 && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to initialize the VS2022 x64 environment."
    }

    $importedPath = $null
    foreach ($line in $environmentOutput) {
        if ($line -notmatch '^([^=]+)=(.*)$') {
            continue
        }
        $name = $Matches[1]
        $value = $Matches[2]
        if ($name -ieq "Path") {
            $importedPath = $value
            continue
        }
        [Environment]::SetEnvironmentVariable($name, $value, "Process")
    }

    if (-not $importedPath) {
        throw "vcvarsall.bat did not return a Path environment variable."
    }
    [Environment]::SetEnvironmentVariable("PATH", $null, "Process")
    [Environment]::SetEnvironmentVariable("Path", $importedPath, "Process")
}

# The CMake bundled with VS BuildTools crashes in cmake_autogen on this machine.
$forbiddenCMakeMarker = "Common7\IDE\CommonExtensions\Microsoft\CMake"
if ($CMakePath.IndexOf($forbiddenCMakeMarker, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
    throw "Do not use the CMake bundled with VS BuildTools. Use the standalone CMake."
}
if (-not (Test-Path -LiteralPath $CMakePath -PathType Leaf)) {
    throw "Standalone CMake not found: $CMakePath"
}

if ([string]::IsNullOrWhiteSpace($PythonPath)) {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($pythonCommand) {
        $PythonPath = $pythonCommand.Source
    } else {
        $pythonLauncher = Get-Command py.exe -ErrorAction SilentlyContinue
        if ($pythonLauncher) {
            $PythonPath = (& py.exe -3 -c "import sys; print(sys.executable)") | Select-Object -First 1
        }
    }
}
if ([string]::IsNullOrWhiteSpace($PythonPath) -or -not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
    throw "Python 3 not found. Pass -PythonPath explicitly."
}
$PythonPath = Get-FullPath $PythonPath
& $PythonPath --version
if ($LASTEXITCODE -ne 0) {
    throw "Python validation failed: $PythonPath"
}

$ninjaDownloadDir = Join-Path $QtRoot "tools\ninja"
if ([string]::IsNullOrWhiteSpace($NinjaPath)) {
    $ninjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if ($ninjaCommand) {
        $NinjaPath = $ninjaCommand.Source
    } else {
        $NinjaPath = Join-Path $ninjaDownloadDir "ninja.exe"
    }
}

if (-not (Test-Path -LiteralPath $NinjaPath -PathType Leaf)) {
    New-Item -ItemType Directory -Path $ninjaDownloadDir -Force | Out-Null
    $ninjaArchive = Join-Path $ninjaDownloadDir "ninja-win.zip"
    $curlArguments = @("-L", "--fail", "--retry", "5", "--retry-all-errors")
    if (-not [string]::IsNullOrWhiteSpace($Proxy)) {
        $curlArguments += @("-x", $Proxy)
    }
    $curlArguments += @("-o", $ninjaArchive, "https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip")
    & curl.exe @curlArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to download Ninja."
    }

    Push-Location $ninjaDownloadDir
    try {
        & $CMakePath -E tar xf $ninjaArchive
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to extract Ninja."
        }
    } finally {
        Pop-Location
    }
    Remove-Item -LiteralPath $ninjaArchive -Force
}

$NinjaPath = Get-FullPath $NinjaPath
if (-not (Test-Path -LiteralPath $NinjaPath -PathType Leaf)) {
    throw "ninja.exe not found: $NinjaPath"
}
& $NinjaPath --version
if ($LASTEXITCODE -ne 0) {
    throw "Ninja validation failed: $NinjaPath"
}

# Keep only one casing of Path. MSBuild fails when both PATH and Path exist.
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Process")
if (-not $currentPath) {
    $currentPath = [Environment]::GetEnvironmentVariable("PATH", "Process")
}
$toolPaths = @(
    (Split-Path -Parent $CMakePath),
    (Split-Path -Parent $NinjaPath),
    (Split-Path -Parent $PythonPath)
) | Where-Object { $_ }
[Environment]::SetEnvironmentVariable("PATH", $null, "Process")
[Environment]::SetEnvironmentVariable("Path", (($toolPaths + @($currentPath)) -join ";"), "Process")

# Fail fast when MSVC is not on the final PATH. A Ninja resolved from another
# toolchain (e.g. Strawberry Perl's MinGW) must never let the build proceed.
$clCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $clCommand) {
    throw "MSVC cl.exe not found after environment setup. Check vcvarsall import: $VcVarsAll"
}

New-Item -ItemType Directory -Path $QtRoot -Force | Out-Null

$versionParts = $QtVersion.Split(".")
if ($versionParts.Count -lt 2) {
    throw "Invalid Qt version: $QtVersion"
}
$qtMinorSeries = "$($versionParts[0]).$($versionParts[1])"
$sourceUrl = "https://download.qt.io/archive/qt/$qtMinorSeries/$QtVersion/single/qt-everywhere-src-$QtVersion.zip"

$expectedHashes = @{
    "6.8.3" = "51ACBDB32AA5E74CD8F7A2B30ACEF90739369AD10B665A2D4DD7BA446C1069B0"
}
$expectedHash = $null
if ($expectedHashes.ContainsKey($QtVersion)) {
    $expectedHash = $expectedHashes[$QtVersion]
}

if (-not (Test-Path -LiteralPath $sourceArchive -PathType Leaf)) {
    Write-Host "Downloading Qt $QtVersion source from:"
    Write-Host "  $sourceUrl"
    $curlArguments = @("-L", "--fail", "--retry", "10", "--retry-all-errors", "-C", "-")
    if (-not [string]::IsNullOrWhiteSpace($Proxy)) {
        $curlArguments += @("-x", $Proxy)
    }
    $curlArguments += @("-o", $sourceArchive, $sourceUrl)
    & curl.exe @curlArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to download Qt source."
    }
}

if ($expectedHash) {
    $actualHash = (Get-FileHash -LiteralPath $sourceArchive -Algorithm SHA256).Hash
    if (-not [string]::Equals($actualHash, $expectedHash, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Qt source hash mismatch.`nExpected: $expectedHash`nActual:   $actualHash"
    }
} else {
    Write-Warning "No pinned SHA-256 hash for Qt $QtVersion; source integrity was not verified."
}

$configureBat = Join-Path $QtSourceDir "configure.bat"
if (-not (Test-Path -LiteralPath $configureBat -PathType Leaf)) {
    Push-Location $QtRoot
    try {
        & $CMakePath -E tar xf $sourceArchive
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to extract Qt source."
        }
    } finally {
        Pop-Location
    }
}

if (-not (Test-Path -LiteralPath $configureBat -PathType Leaf)) {
    throw "Qt configure.bat not found after extraction: $configureBat"
}

if (-not $Incremental -and (Test-Path -LiteralPath $BuildDir)) {
    Write-Host "Cleaning Qt build directory: $BuildDir"
    Remove-ExactPath $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null

Push-Location $BuildDir
try {
    $configureArguments = @(
        "-static",
        "-static-runtime",
        "-release",
        "-prefix", $InstallDir,
        "-opensource",
        "-confirm-license",
        "-nomake", "examples",
        "-nomake", "tests",
        "-submodules", "qtbase,qtshadertools,qtdeclarative,qtsvg",
        # Pin MSVC explicitly: with the Ninja generator, CMake may otherwise
        # pick a gcc that appears earlier on PATH than cl.exe.
        "--",
        "-DCMAKE_C_COMPILER=$($clCommand.Source)",
        "-DCMAKE_CXX_COMPILER=$($clCommand.Source)"
    )
    if ($Ltcg) {
        $configureArguments += "-ltcg"
    }

    & $configureBat @configureArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Qt 6 configure failed."
    }

    & $CMakePath --build . --parallel $Parallel
    if ($LASTEXITCODE -ne 0) {
        throw "Qt 6 build failed."
    }

    & $CMakePath --install .
    if ($LASTEXITCODE -ne 0) {
        throw "Qt 6 install failed."
    }
} finally {
    Pop-Location
}

$qtConfig = Join-Path $InstallDir "lib\cmake\Qt6\Qt6Config.cmake"
$qtCoreDll = Join-Path $InstallDir "bin\Qt6Core.dll"
if (-not (Test-Path -LiteralPath $qtConfig -PathType Leaf)) {
    throw "Qt install validation failed: $qtConfig not found."
}
if (Test-Path -LiteralPath $qtCoreDll -PathType Leaf) {
    throw "Qt install validation failed: unexpected dynamic Qt library $qtCoreDll."
}

if ($RemoveIntermediates) {
    Remove-ExactPath $sourceArchive
    Remove-ExactPath $QtSourceDir
    Remove-ExactPath $BuildDir
}

Write-Host ""
Write-Host "Qt $QtVersion static build installed to: $InstallDir"
Write-Host "Build project:"
Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File scripts\package-release.ps1 -QtPrefixPath `"$InstallDir`" -CMakePath `"$CMakePath`""
if (-not $RemoveIntermediates) {
    Write-Host "Clean-only intermediates later with:"
    Write-Host "  Remove-Item -LiteralPath `"$sourceArchive`", `"$QtSourceDir`", `"$BuildDir`" -Recurse -Force"
}

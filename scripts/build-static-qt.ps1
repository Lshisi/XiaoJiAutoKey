# Build Qt 5.15.2 static for MSVC x64
# Prerequisites: Visual Studio 2022 Build Tools, Perl (Strawberry Perl), Python
# This script will download Qt source and compile a minimal static build.
# Run from VS2022 x64 Native Tools Command Prompt or after calling vcvarsall.bat

param(
    [string]$QtVersion = "5.15.2",
    [string]$InstallDir = "C:\Qt\5.15.2-static"
)

$ErrorActionPreference = "Stop"

# Refresh PATH to pick up newly installed tools
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

# Check Perl
if (-not (Get-Command perl -ErrorAction SilentlyContinue)) {
    Write-Host @"

ERROR: Perl is required to build Qt from source.
Please install Strawberry Perl first:
  https://strawberryperl.com/
  (Download and run the MSI installer, then restart your terminal)

"@
    exit 1
}

# Check MSVC environment
if (-not $env:VSINSTALLDIR) {
    Write-Host "Setting up MSVC environment..."
    $vcvarsall = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        throw "vcvarsall.bat not found. Install VS2022 Build Tools."
    }
    # Import environment from vcvarsall
    $envBlock = cmd /c "`"$vcvarsall`" x64 >nul 2>&1 && set" 2>&1
    foreach ($line in $envBlock) {
        if ($line -match '^([^=]+)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

$srcDir = "C:\Qt\qt-src-$QtVersion"
$buildDir = "C:\Qt\qt-build-static"
$archive = "C:\Qt\qt-everywhere-src-$QtVersion.zip"

# Create directories
New-Item -ItemType Directory -Path "C:\Qt" -Force | Out-Null

# Download Qt source if not present
if (-not (Test-Path $srcDir)) {
    $url = "https://download.qt.io/archive/qt/5.15/$QtVersion/single/qt-everywhere-src-$QtVersion.zip"
    if (-not (Test-Path $archive)) {
        Write-Host "Downloading Qt $QtVersion source (~900MB)..."
        Write-Host "URL: $url"
        # Use .NET WebClient for faster download with progress
        $wc = New-Object System.Net.WebClient
        $wc.DownloadFile($url, $archive)
        $wc.Dispose()
        Write-Host "Download complete."
    }
    Write-Host "Extracting Qt source..."
    Expand-Archive -Path $archive -DestinationPath "C:\Qt" -Force
    Rename-Item "C:\Qt\qt-everywhere-src-$QtVersion" $srcDir -ErrorAction SilentlyContinue
}

# Clean build dir
if (Test-Path $buildDir) {
    Remove-Item $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

Write-Host "Configuring Qt static build (this takes a few minutes)..."
Push-Location $buildDir

# Minimal static configure - only what we need
$configArgs = @(
    "-static"
    "-static-runtime"
    "-release"
    "-platform", "win32-msvc"
    "-prefix", $InstallDir
    "-opensource"
    "-confirm-license"
    "-nomake", "examples"
    "-nomake", "tests"
    "-nomake", "tools"
    "-no-opengl"
    "-no-openssl"
    "-no-dbus"
    "-no-sql-sqlite"
    "-no-sql-odbc"
    "-skip", "qtwebengine"
    "-skip", "qt3d"
    "-skip", "qtmultimedia"
    "-skip", "qtlocation"
    "-skip", "qtsensors"
    "-skip", "qtserialport"
    "-skip", "qtconnectivity"
    "-skip", "qtgamepad"
    "-skip", "qtscript"
    "-skip", "qtdatavis3d"
    "-skip", "qtcharts"
    "-skip", "qtpurchasing"
    "-skip", "qtnetworkauth"
    "-skip", "qtremoteobjects"
    "-skip", "qtscxml"
    "-skip", "qtspeech"
    "-skip", "qtvirtualkeyboard"
    "-skip", "qtwebchannel"
    "-skip", "qtwebsockets"
    "-skip", "qtwebview"
    "-skip", "qtquickcontrols"
    "-skip", "qtquickcontrols2"
    "-skip", "qtdeclarative"
)

& "$srcDir\configure.bat" @configArgs
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "Qt configure failed"
}

Write-Host "Building Qt (this will take 30-60 minutes)..."
& nmake
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "Qt build failed"
}

Write-Host "Installing Qt to $InstallDir..."
& nmake install
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "Qt install failed"
}

Pop-Location

Write-Host @"

Qt $QtVersion static build complete!
Installed to: $InstallDir

To use in your project, update CMakeLists.txt:
  set(CMAKE_PREFIX_PATH "$InstallDir")

"@

# Run Qt static build - configure + compile + install
$ErrorActionPreference = "Stop"

# Refresh PATH
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")

# Setup MSVC environment
$vcvarsall = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
$envBlock = cmd /c "`"$vcvarsall`" x64 >nul 2>&1 && set" 2>&1
foreach ($line in $envBlock) {
    if ($line -match '^([^=]+)=(.*)$') {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}
Write-Host "MSVC environment ready."
Write-Host "perl: $(Get-Command perl | Select-Object -ExpandProperty Source)"

$srcDir = "C:\Qt\qt-src-5.15.2"
$buildDir = "C:\Qt\qt-build-static"
$installDir = "C:\Qt\5.15.2-static"

# Clean build dir
if (Test-Path $buildDir) {
    Remove-Item $buildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
Set-Location $buildDir

Write-Host "Configuring Qt static build..."
& "$srcDir\configure.bat" -static -static-runtime -release -platform win32-msvc -prefix $installDir -opensource -confirm-license -nomake examples -nomake tests -nomake tools -no-opengl -no-openssl -no-dbus -no-sql-sqlite -no-sql-odbc -skip qtwebengine -skip qt3d -skip qtmultimedia -skip qtlocation -skip qtsensors -skip qtserialport -skip qtconnectivity -skip qtgamepad -skip qtscript -skip qtdatavis3d -skip qtcharts -skip qtpurchasing -skip qtnetworkauth -skip qtremoteobjects -skip qtscxml -skip qtspeech -skip qtvirtualkeyboard -skip qtwebchannel -skip qtwebsockets -skip qtwebview -skip qtquickcontrols -skip qtquickcontrols2 -skip qtdeclarative

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: configure failed with exit code $LASTEXITCODE"
    exit 1
}

Write-Host "Building Qt (this will take 30-60 minutes)..."
& nmake
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: nmake failed with exit code $LASTEXITCODE"
    exit 1
}

Write-Host "Installing Qt to $installDir..."
& nmake install
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: nmake install failed with exit code $LASTEXITCODE"
    exit 1
}

Write-Host ""
Write-Host "SUCCESS! Qt 5.15.2 static build installed to: $installDir"

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

# MSBuild's .NET process launcher treats Path/PATH as duplicate keys on Windows.
# Normalize the process environment before any compiler tools are started.
$normalizedPath = @(
    "C:\Windows\System32"
    "C:\Windows"
    "C:\Windows\System32\Wbem"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
) -join ";"
[System.Environment]::SetEnvironmentVariable("PATH", $null, "Process")
[System.Environment]::SetEnvironmentVariable("Path", $normalizedPath, "Process")

$repoRoot = Split-Path -Parent $PSScriptRoot
$solution = Join-Path $repoRoot "driver\xiaoji-vhid\XiaoJiVhid.sln"
$msbuildCandidates = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
)
$msbuild = $msbuildCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not $msbuild) {
    throw "MSBuild 2022 was not found."
}
if (-not (Test-Path -LiteralPath $solution)) {
    throw "Driver solution was not found: $solution"
}

$kitRoot = "C:\Program Files (x86)\Windows Kits\10"
$kitVersion = Get-ChildItem (Join-Path $kitRoot "Include") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Where-Object {
        (Test-Path -LiteralPath (Join-Path $_.FullName "km\ntddk.h")) -and
        (Test-Path -LiteralPath (Join-Path $_.FullName "ucrt\corecrt.h")) -and
        (Test-Path -LiteralPath (Join-Path $kitRoot "build\$($_.Name)\WindowsDriver.Default.props"))
    } |
    Select-Object -First 1
if (-not $kitVersion) {
    throw "A matching Windows SDK and Driver Kit were not found. Install the same build number for both kits and the Visual Studio WDK integration."
}
$wdkVersion = $kitVersion.Name

& $msbuild $solution "/t:Build" "/p:Configuration=$Configuration" "/p:Platform=x64" "/p:WindowsTargetPlatformVersion=$wdkVersion" "/p:WDKBuildFolder=$wdkVersion" "/m"
if ($LASTEXITCODE -ne 0) {
    throw "XiaoJiVhid build failed with exit code $LASTEXITCODE."
}

$packageDir = Join-Path $repoRoot "driver\xiaoji-vhid\package\$Configuration"
$requiredFiles = @("XiaoJiVhid.sys", "XiaoJiVhid.inf", "XiaoJiVhid.cat")
$wdkPackageDir = Join-Path $packageDir "XiaoJiVhid"
foreach ($fileName in $requiredFiles) {
    $wdkPackageFile = Join-Path $wdkPackageDir $fileName
    if (Test-Path -LiteralPath $wdkPackageFile -PathType Leaf) {
        Copy-Item -LiteralPath $wdkPackageFile -Destination (Join-Path $packageDir $fileName) -Force
    }
}
foreach ($fileName in $requiredFiles) {
    $path = Join-Path $packageDir $fileName
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Driver package file was not generated: $path"
    }
}

Write-Host "XiaoJiVhid package ready: $packageDir"

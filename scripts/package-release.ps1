param(
    [string]$Configuration = "Release",
    [Parameter(Mandatory = $true)][string]$QtPrefixPath
)

$ErrorActionPreference = "Stop"

# Normalize PATH casing for child tool invocations.
# Some launch environments inject both PATH and Path, which breaks MSBuild/CL startup.
$processPath = [System.Environment]::GetEnvironmentVariable("PATH", "Process")
if (-not $processPath) {
    $machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
    $processPath = (@($machinePath, $userPath) | Where-Object { $_ }) -join ";"
}
[System.Environment]::SetEnvironmentVariable("Path", $null, "Process")
[System.Environment]::SetEnvironmentVariable("PATH", $processPath, "Process")

$repoRoot = Split-Path -Parent $PSScriptRoot
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) {
    throw "cmake.exe not found: $cmake"
}
$qtPrefixPath = (Resolve-Path $QtPrefixPath).Path
if (-not (Test-Path (Join-Path $qtPrefixPath "lib\cmake\Qt6\Qt6Config.cmake"))) {
    throw "Qt 6 prefix not found: $qtPrefixPath"
}
if (Test-Path (Join-Path $qtPrefixPath "bin\Qt6Core.dll")) {
    throw "The release package requires static Qt 6. Build it with scripts/build-static-qt.ps1."
}

$buildDir = Join-Path $repoRoot "build-qt6"
$distDir = Join-Path $repoRoot "dist"
$packageName = "XiaoJiAutoKey"
$packageDir = Join-Path $distDir $packageName
$zipPath = Join-Path $distDir "XiaoJiAutoKey.zip"
$exePath = Join-Path $buildDir "$Configuration\XiaoJiAutoKey.exe"
$driverSrcDir = Join-Path $repoRoot "third_party\dd"
$driverDstDir = Join-Path $packageDir "driver"
$hidDriverSrcDir = Join-Path $driverSrcDir "hid"
$hidDriverDstDir = Join-Path $driverDstDir "hid"
$audioSrcDir = Join-Path $repoRoot "assets\audio"
$audioDstDir = Join-Path $packageDir "audio"
$audioFileNames = @(
    "$([char]0x6309)$([char]0x952E)$([char]0x5F00)$([char]0x542F).wav",
    "$([char]0x6309)$([char]0x952E)$([char]0x5173)$([char]0x95ED).wav",
    "$([char]0x5FAA)$([char]0x73AF)$([char]0x5F00)$([char]0x542F).wav",
    "$([char]0x5FAA)$([char]0x73AF)$([char]0x5173)$([char]0x95ED).wav"
)
$usageGuideName = "$([char]0x4F7F)$([char]0x7528)$([char]0x8BF4)$([char]0x660E).mhtml"
$usageGuideSrc = Join-Path $repoRoot (Join-Path "assets" $usageGuideName)
# Auto update version
$cmakeListsPath = Join-Path $repoRoot "CMakeLists.txt"
# Auto update version (disabled - manual version control)
# $cmakeListsContent = Get-Content $cmakeListsPath -Raw -Encoding UTF8
# if ($cmakeListsContent -match 'project\(\s*XiaoJiAutoKey\s+VERSION\s+(\d+)\.(\d+)\.(\d+)\s+LANGUAGES\s+CXX\s*\)') {
#     $major = [int]$matches[1]
#     $minor = [int]$matches[2]
#     $patch = [int]$matches[3] + 1
#     $newVersion = "$major.$minor.$patch"
#     $newContent = $cmakeListsContent -replace 'project\(\s*XiaoJiAutoKey\s+VERSION\s+\d+\.\d+\.\d+\s+LANGUAGES\s+CXX\s*\)', "project(XiaoJiAutoKey VERSION $newVersion LANGUAGES CXX)"
#     Set-Content -LiteralPath $cmakeListsPath -Value $newContent -NoNewline -Encoding UTF8
#     Write-Host "Updated version to $newVersion"
# } else {
#     Write-Warning "Could not find version string in CMakeLists.txt"
# }
& $cmake -S $repoRoot -B $buildDir -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="$qtPrefixPath"
if ($LASTEXITCODE -ne 0) {
    throw "cmake configure failed"
}

& $cmake --build $buildDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed"
}

if (-not (Test-Path $exePath)) {
    throw "built executable not found: $exePath"
}

New-Item -ItemType Directory -Path $distDir -Force | Out-Null
if (Test-Path $packageDir) {
    Remove-Item $packageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null
Copy-Item $exePath (Join-Path $packageDir "XiaoJiAutoKey.exe") -Force
if (Test-Path $usageGuideSrc) {
    Copy-Item $usageGuideSrc (Join-Path $packageDir $usageGuideName) -Force
} else {
    Write-Warning "usage guide not found: $usageGuideSrc"
}

# Built-in audio files (WAV only)
if (-not (Test-Path $audioSrcDir)) {
    throw "audio source directory not found: $audioSrcDir"
}
New-Item -ItemType Directory -Path $audioDstDir -Force | Out-Null
foreach ($audioFileName in $audioFileNames) {
    $audioSourcePath = Join-Path $audioSrcDir $audioFileName
    if (-not (Test-Path $audioSourcePath -PathType Leaf)) {
        throw "built-in WAV audio file not found: $audioSourcePath"
    }
    Copy-Item $audioSourcePath (Join-Path $audioDstDir $audioFileName) -Force
}

# Driver files
if (Test-Path $driverSrcDir) {
    New-Item -ItemType Directory -Path $driverDstDir -Force | Out-Null

    $dd32Path = Join-Path $driverSrcDir "DD32.dll"
    $dd64Path = Join-Path $driverSrcDir "DD64.dll"
    $hidRuntimePath = Join-Path $hidDriverSrcDir "ddhid.63340.dll"
    if (Test-Path $dd32Path) {
        Copy-Item $dd32Path (Join-Path $driverDstDir "DD32.dll") -Force
    }
    if (Test-Path $dd64Path) {
        Copy-Item $dd64Path (Join-Path $driverDstDir "DD64.dll") -Force
    }
    if (Test-Path $hidRuntimePath) {
        New-Item -ItemType Directory -Path $hidDriverDstDir -Force | Out-Null
        Copy-Item $hidRuntimePath (Join-Path $hidDriverDstDir "ddhid.63340.dll") -Force
    } else {
        Write-Warning "third_party\\dd\\hid\\ddhid.63340.dll not found."
    }

    if (-not (Test-Path $dd32Path) -and -not (Test-Path $dd64Path)) {
        Write-Warning "third_party\\dd\\DD32.dll / DD64.dll not found."
    }
} else {
    Write-Warning "third_party\dd directory not found."
}

# Create zip
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

# Ensure no config.json is included in the package
$configInPackage = Join-Path $packageDir "config.json"
if (Test-Path $configInPackage) {
    Remove-Item $configInPackage -Force
}

Compress-Archive -Path $packageDir -DestinationPath $zipPath -Force

Write-Host "Package ready:"
Write-Host "  Folder: $packageDir"
Write-Host "  Zip:    $zipPath"

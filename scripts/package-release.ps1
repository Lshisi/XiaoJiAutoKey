param(
    [string]$Configuration = "Release",
    [Parameter(Mandatory = $true)][string]$QtPrefixPath,
    [string]$CMakePath = "D:\software\cmake-3.31.6\bin\cmake.exe",
    [string]$XiaojiSignedDriverDir = "",
    [string]$BuildDirectory = "build-qt6",
    [string]$PluginCatalogPublicKey = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot 'plugin-files.ps1')

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
$cmake = $CMakePath
# The CMake bundled with VS BuildTools crashes in cmake_autogen on this machine.
$forbiddenCMakeMarker = "Common7\IDE\CommonExtensions\Microsoft\CMake"
if ($cmake.IndexOf($forbiddenCMakeMarker, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
    throw "Do not use the CMake bundled with VS BuildTools. Use the standalone CMake."
}
if (-not (Test-Path $cmake -PathType Leaf)) {
    throw "cmake.exe not found: $cmake"
}
$qtPrefixPath = (Resolve-Path $QtPrefixPath).Path
if (-not (Test-Path (Join-Path $qtPrefixPath "lib\cmake\Qt6\Qt6Config.cmake"))) {
    throw "Qt 6 prefix not found: $qtPrefixPath"
}
if (Test-Path (Join-Path $qtPrefixPath "bin\Qt6Core.dll")) {
    throw "The release package requires static Qt 6. Build it with scripts/build-static-qt.ps1."
}

$buildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDirectory))
if (-not $buildDir.StartsWith($repoRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The build directory must be inside the repository: $buildDir"
}
$distDir = Join-Path $repoRoot "dist"
$packageName = "XiaoJiAutoKey"
$packageDir = Join-Path $distDir $packageName
$zipPath = Join-Path $distDir "XiaoJiAutoKey.zip"
$exePath = Join-Path $buildDir "$Configuration\XiaoJiAutoKey.exe"
$resolvedXiaojiPackage = ""
if ($XiaojiSignedDriverDir) {
    $resolvedXiaojiPackage = (Resolve-Path -LiteralPath $XiaojiSignedDriverDir).Path
    foreach ($fileName in @("xiaoji.inf", "xiaoji.dll", "xiaoji.cat")) {
        if (-not (Test-Path -LiteralPath (Join-Path $resolvedXiaojiPackage $fileName) -PathType Leaf)) {
            throw "Signed XiaojiDriver package is incomplete: $fileName"
        }
    }
}
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
# Pin Qt6_DIR explicitly: with several Qt installs sharing C:\Qt, bare
# CMAKE_PREFIX_PATH resolution was observed to pick a different kit
# non-deterministically between runs.
$qt6ConfigDir = Join-Path $qtPrefixPath "lib\cmake\Qt6"
$catalogArguments = @()
if ($PSBoundParameters.ContainsKey('PluginCatalogPublicKey')) {
    if ($PluginCatalogPublicKey -and $PluginCatalogPublicKey -notmatch '^B[A-Za-z0-9+/]{86}=$') {
        throw 'The catalog public key must be a Base64 uncompressed P-256 point.'
    }
    $catalogArguments += "-DXIAOJI_PLUGIN_CATALOG_PUBLIC_KEY=$PluginCatalogPublicKey"
}
& $cmake -S $repoRoot -B $buildDir -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="$qtPrefixPath" -DQt6_DIR="$qt6ConfigDir" -DXIAOJI_SIGNED_DRIVER_DIR="$resolvedXiaojiPackage" @catalogArguments
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
$packageAncestor = [System.IO.Path]::GetFullPath($packageDir)
while ($packageAncestor -and $packageAncestor.Length -ge $repoRoot.Length) {
    if ((Test-Path -LiteralPath $packageAncestor) -and
        ((Get-Item -LiteralPath $packageAncestor -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        throw "Linked package directory is not allowed: $packageAncestor"
    }
    $packageAncestor = Split-Path -Parent $packageAncestor
}
if (Test-Path $packageDir) {
    $resolvedPackageDir = [System.IO.Path]::GetFullPath($packageDir)
    $expectedPackageDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "dist\XiaoJiAutoKey"))
    if ($resolvedPackageDir -ne $expectedPackageDir -or
        ((Get-Item -LiteralPath $packageDir).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing to clean an unexpected package directory: $packageDir"
    }
    Remove-Item -LiteralPath $resolvedPackageDir -Recurse -Force
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
}
Get-ChildItem -LiteralPath $audioSrcDir -Filter "*.wav" -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $audioDstDir $_.Name) -Force
}

# Package only the declared current bundled versions, never user installations or caches.
foreach ($pluginName in @('driver-xiaoji')) {
    $sourceManifest = Read-XiaojiPluginManifest (Join-Path $repoRoot "plugins\$pluginName")
    $pluginRelative = 'plugins\builtin\' + $sourceManifest.id + '\' + $sourceManifest.version
    $pluginOutput = Join-Path (Split-Path -Parent $exePath) $pluginRelative
    $outputManifest = Read-XiaojiPluginManifest $pluginOutput
    if ($outputManifest.id -ne $sourceManifest.id -or $outputManifest.version -ne $sourceManifest.version) {
        throw "Built plugin does not match its source manifest: $pluginName"
    }
    Copy-XiaojiPluginFiles $pluginOutput (Join-Path $packageDir $pluginRelative)
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

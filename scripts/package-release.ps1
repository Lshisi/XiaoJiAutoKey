param(
    [string]$Configuration = "Release"
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
$qtCandidates = @(
    "C:\Qt\5.15.2-static",
    "D:\Qt\5.15.2-static",
    "D:\software\anaconda\Library"
)
$qtPrefixPath = $qtCandidates | Where-Object {
    Test-Path (Join-Path $_ "lib\cmake\Qt5\Qt5Config.cmake")
} | Select-Object -First 1

if (-not (Test-Path $cmake)) {
    throw "cmake.exe not found: $cmake"
}
if (-not $qtPrefixPath) {
    throw "Qt5Config.cmake not found. Checked: $($qtCandidates -join ', ')"
}

$buildDir = Join-Path $repoRoot "build"
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
$usageShortcutName = "$([char]0x4F7F)$([char]0x7528)$([char]0x8BF4)$([char]0x660E).url"
$usageShortcutSrc = Join-Path $repoRoot (Join-Path "assets" $usageShortcutName)
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
if (Test-Path $usageShortcutSrc) {
    Copy-Item $usageShortcutSrc (Join-Path $packageDir $usageShortcutName) -Force
} else {
    Write-Warning "usage shortcut not found: $usageShortcutSrc"
}

# Check if this is a static Qt build by looking for Qt DLL dependencies
$exeFile = Join-Path $packageDir "XiaoJiAutoKey.exe"
$exeBytes = [System.IO.File]::ReadAllText($exeFile, [System.Text.Encoding]::GetEncoding(28591))
$isStatic = -not ($exeBytes -match "Qt5Core_conda\.dll|Qt5Core\.dll")

# For dynamic builds, deploy Qt DLLs
if (-not $isStatic) {
    # qt.conf - tell Qt to look for plugins in exe directory
    $qtConfPath = Join-Path $packageDir "qt.conf"
    Set-Content -LiteralPath $qtConfPath -Value @('[Paths]', 'Plugins = .') -Encoding UTF8

    # Deploy Qt DLLs manually (Anaconda Qt uses _conda suffix)
    $qtBinDir = "D:\software\anaconda\Library\bin"
    $qtPluginsDir = "D:\software\anaconda\Library\plugins"

    $qtDlls = @("Qt5Core_conda.dll", "Qt5Gui_conda.dll", "Qt5Widgets_conda.dll", "Qt5Svg_conda.dll")

    Write-Host "Copying Qt DLLs..."
    foreach ($dll in $qtDlls) {
        $src = Join-Path $qtBinDir $dll
        if (Test-Path $src) {
            Copy-Item $src (Join-Path $packageDir $dll) -Force
            Write-Host "  Copied $dll"
        } else {
            Write-Warning "Qt DLL not found: $src"
        }
    }

    # platforms plugin
    $platformsDst = Join-Path $packageDir "platforms"
    New-Item -ItemType Directory -Path $platformsDst -Force | Out-Null
    $qwindowsSrc = Join-Path $qtPluginsDir "platforms\qwindows.dll"
    if (Test-Path $qwindowsSrc) {
        Copy-Item $qwindowsSrc (Join-Path $platformsDst "qwindows.dll") -Force
        Write-Host "  Copied platforms\qwindows.dll"
    } else {
        Write-Warning "qwindows.dll not found at $qwindowsSrc"
    }

    # imageformats plugin
    $imageformatsSrc = Join-Path $qtPluginsDir "imageformats"
    if (Test-Path $imageformatsSrc) {
        $imageformatsDst = Join-Path $packageDir "imageformats"
        New-Item -ItemType Directory -Path $imageformatsDst -Force | Out-Null
        Copy-Item (Join-Path $imageformatsSrc "*") $imageformatsDst -Recurse -Force
    }

    # styles plugin
    $stylesSrc = Join-Path $qtPluginsDir "styles"
    if (Test-Path $stylesSrc) {
        $stylesDst = Join-Path $packageDir "styles"
        New-Item -ItemType Directory -Path $stylesDst -Force | Out-Null
        Copy-Item (Join-Path $stylesSrc "*") $stylesDst -Recurse -Force
    }
} else {
    Write-Host "Static Qt build detected - no Qt DLLs needed!"
}

# Audio files
if (Test-Path $audioSrcDir) {
    New-Item -ItemType Directory -Path $audioDstDir -Force | Out-Null
    Copy-Item (Join-Path $audioSrcDir "*") $audioDstDir -Recurse -Force
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

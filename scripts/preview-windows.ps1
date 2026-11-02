param(
    [string] $QtPrefixPath = 'D:\Qt\6.8.3\msvc2022_64',
    [string] $CMakePath = 'D:\software\cmake-3.31.6\bin\cmake.exe',
    [switch] $BuildOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build-qml-preview-windows'
$processPath = [System.Environment]::GetEnvironmentVariable('Path', 'Process')
[System.Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[System.Environment]::SetEnvironmentVariable('Path', "$(Join-Path $QtPrefixPath 'bin');$processPath", 'Process')

& $CMakePath -S (Join-Path $projectRoot 'preview') -B $buildDir `
    -G 'Visual Studio 17 2022' -A x64 `
    "-DQt6_DIR=$(Join-Path $QtPrefixPath 'lib\cmake\Qt6')"
if ($LASTEXITCODE -ne 0) { throw '预览程序配置失败。' }
& $CMakePath --build $buildDir --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw '预览程序编译失败。' }

if (-not $BuildOnly) {
    & (Join-Path $buildDir 'Release\XiaoJiPreview.exe')
}

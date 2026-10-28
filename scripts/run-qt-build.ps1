param(
    [Parameter(Mandatory = $true)][string]$QtSourceDir,
    [Parameter(Mandatory = $true)][string]$InstallDir,
    [string]$BuildDir = "$InstallDir-build"
)

$ErrorActionPreference = "Stop"
& (Join-Path $PSScriptRoot "build-static-qt.ps1") -QtSourceDir $QtSourceDir -InstallDir $InstallDir -BuildDir $BuildDir

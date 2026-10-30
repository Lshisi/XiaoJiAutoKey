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

# Compatibility wrapper for the familiar one-command Qt build workflow.
$buildArguments = @{}
foreach ($parameterName in $PSBoundParameters.Keys) {
    $buildArguments[$parameterName] = $PSBoundParameters[$parameterName]
}

& (Join-Path $PSScriptRoot "build-static-qt.ps1") @buildArguments
exit $LASTEXITCODE

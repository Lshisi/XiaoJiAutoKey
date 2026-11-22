param(
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [string]$CompilerPath = "$env:SystemRoot\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repoRoot 'third_party\xiaoji'
$payloadRoot = Join-Path $sourceRoot 'driver'
$manifest = Get-Content -LiteralPath (Join-Path $payloadRoot 'payload.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) {
    throw '.NET Framework 4.8 compiler is required to build the local Xiaoji installer.'
}
$resources = @()
foreach ($name in @('xiaoji.inf', 'xiaoji.dll', 'xiaoji.cat')) {
    $path = Join-Path $payloadRoot $name
    $expectedHash = $manifest.files.PSObject.Properties[$name].Value
    # The Windows PowerShell build subprocess may not inherit its utility module
    # search path. Use .NET directly so payload verification needs no modules.
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $actualHash = [System.BitConverter]::ToString(
            $hasher.ComputeHash([System.IO.File]::ReadAllBytes($path))).Replace('-', '')
    } finally {
        $hasher.Dispose()
    }
    if (-not $expectedHash -or $actualHash -ne $expectedHash) {
        throw "Pinned XiaojiDriver payload hash mismatch: $name"
    }
    $resources += "/resource:$path,$name"
}
$absoluteOutput = [System.IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Path (Split-Path -Parent $absoluteOutput) -Force | Out-Null
# Compile only. Never invoke the resulting installer, certificate commands or drivers.
& $CompilerPath /nologo /platform:x64 /optimize+ /codepage:65001 /target:exe `
    /main:XiaojiLocalInstaller "/out:$absoluteOutput" @resources `
    (Join-Path $sourceRoot 'LocalInstaller.cs') (Join-Path $sourceRoot 'LocalSetup.cs') (Join-Path $sourceRoot 'DriverStore.cs')
if ($LASTEXITCODE -ne 0) { throw 'Failed to compile the Xiaoji local installer.' }

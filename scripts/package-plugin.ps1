param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [string]$PackageName = ""
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'plugin-files.ps1')
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$sourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path
$manifest = Read-XiaojiPluginManifest $sourceRoot
if (-not $PackageName) { $PackageName = $manifest.id }
if ($PackageName -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,95}$') { throw 'Invalid package name.' }
$distributionRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'dist\plugins'))
$staging = [System.IO.Path]::GetFullPath((Join-Path $distributionRoot ('staging\' + $manifest.id + '\' + $manifest.version)))
if (-not $staging.StartsWith($distributionRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid plugin staging path.' }
if ($sourceRoot.Equals($staging, [System.StringComparison]::OrdinalIgnoreCase) -or $sourceRoot.StartsWith($staging + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'The plugin source cannot be inside its disposable staging directory.'
}
$ancestor = $staging
while ($ancestor -and $ancestor.Length -ge $repositoryRoot.Length) {
    if (Test-Path -LiteralPath $ancestor) {
        if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw "Linked staging path is not allowed: $ancestor" }
    }
    $ancestor = Split-Path -Parent $ancestor
}
if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

Copy-XiaojiPluginFiles $sourceRoot $staging
$zipPath = Join-Path $distributionRoot ($PackageName + '-v' + $manifest.version + '.zip')
if (Test-Path -LiteralPath $zipPath) { throw 'A version package already exists. Publish a new version instead of overwriting it.' }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($staging, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false, [System.Text.Encoding]::UTF8)
if ((Get-Item -LiteralPath $zipPath).Length -gt 64MB) { throw 'Package exceeds the host download limit of 64 MB; reduce the declared resources before publication.' }
$digest = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText($zipPath + '.sha256', $digest + "`n", [System.Text.UTF8Encoding]::new($false))
Write-Output $zipPath
Write-Output ('SHA256: ' + $digest)

function Read-XiaojiPluginManifest([string]$SourceDirectory) {
    $pluginManifest = Get-Content -LiteralPath (Join-Path $SourceDirectory 'plugin.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $versionPattern = '^(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})$'
    if ($pluginManifest.id -notmatch '^[a-z0-9][a-z0-9.-]{1,94}[a-z0-9]$' -or
        $pluginManifest.id.Contains('..') -or $pluginManifest.version -notmatch $versionPattern) {
        throw 'Invalid plugin identity or version.'
    }
    return $pluginManifest
}

function Copy-XiaojiPluginFiles([string]$SourceDirectory, [string]$DestinationDirectory) {
    $pluginSourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path.TrimEnd('\')
    $pluginDestinationRoot = [System.IO.Path]::GetFullPath($DestinationDirectory).TrimEnd('\')
    $pluginManifest = Read-XiaojiPluginManifest $pluginSourceRoot

    function Copy-PluginTree([string]$Entry, [string]$Destination) {
        $pluginItem = Get-Item -LiteralPath $Entry -Force
        if ($pluginItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Linked plugin entry is not allowed: $Entry"
        }
        if ($pluginItem.PSIsContainer) {
            New-Item -ItemType Directory -Force -Path $Destination | Out-Null
            foreach ($child in Get-ChildItem -LiteralPath $Entry -Force) {
                Copy-PluginTree $child.FullName (Join-Path $Destination $child.Name)
            }
        } else {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
            Copy-Item -LiteralPath $Entry -Destination $Destination -Force
        }
    }

    function Copy-PluginEntry([string]$Relative) {
        $normalized = $Relative.Replace('\', '/')
        $segments = $normalized.Split('/')
        if (-not $normalized -or $normalized.StartsWith('/') -or $normalized.IndexOfAny([char[]]':*?"<>|') -ge 0 -or
            ($segments | Where-Object { $_ -in @('', '.', '..') -or $_.EndsWith('.') -or $_.EndsWith(' ') -or $_ -match '^(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)' })) {
            throw "Invalid plugin entry: $Relative"
        }
        $entryPath = [System.IO.Path]::GetFullPath((Join-Path $pluginSourceRoot $Relative))
        if (-not $entryPath.StartsWith($pluginSourceRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Plugin entry escaped its root: $Relative"
        }
        $entryAncestor = $entryPath
        while ($entryAncestor -and $entryAncestor.Length -ge $pluginSourceRoot.Length) {
            if ((Get-Item -LiteralPath $entryAncestor -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
                throw "Linked plugin path is not allowed: $entryAncestor"
            }
            $entryAncestor = Split-Path -Parent $entryAncestor
        }
        Copy-PluginTree $entryPath (Join-Path $pluginDestinationRoot $Relative)
    }

    Copy-PluginEntry 'plugin.json'
    if ($pluginManifest.native) { Copy-PluginEntry $pluginManifest.native }
    foreach ($page in $pluginManifest.pages) { Copy-PluginEntry $page.file }
    if ($pluginManifest.background.file) { Copy-PluginEntry $pluginManifest.background.file }
    foreach ($window in $pluginManifest.windows) { Copy-PluginEntry $window.file }
    foreach ($resource in $pluginManifest.resources) { Copy-PluginEntry $resource }
    if ($pluginManifest.management) {
        foreach ($operation in $pluginManifest.management.PSObject.Properties) { Copy-PluginEntry $operation.Value.file }
    }
    foreach ($license in Get-ChildItem -LiteralPath $pluginSourceRoot -File -Filter 'LICENSE*') { Copy-PluginEntry $license.Name }
    if (Test-Path -LiteralPath (Join-Path $pluginDestinationRoot 'config.json')) { throw 'User config.json cannot be packaged.' }
}

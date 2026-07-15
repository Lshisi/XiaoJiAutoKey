param(
    [switch]$NoClean
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$cli = Join-Path $repoRoot "tools\arduino-cli\arduino-cli.exe"
$configDir = Join-Path $repoRoot "tools\ch552-arduino-data"
$sketchDir = Join-Path $repoRoot "firmware\ch552"
$buildDir = Join-Path $repoRoot "build\ch552-firmware"
$outputDir = Join-Path $sketchDir "build"
$fqbn = "CH55xDuino:mcs51:ch552:clock=24internal,usb_settings=user266,upload_method=usb,bootloader_pin=p36"

if (-not (Test-Path $cli)) {
    throw "Arduino CLI was not found at $cli. See firmware/ch552/README.md."
}
if (-not (Test-Path (Join-Path $configDir "packages\CH55xDuino"))) {
    throw "CH55xduino is not installed under $configDir. See firmware/ch552/README.md."
}

# CH55xduino 0.0.26 ships Bash array syntax but invokes this wrapper with
# busybox ash on Windows. Apply the equivalent POSIX form to the local tool.
$wrapperRoot = Join-Path $configDir "packages\CH55xDuino\tools\MCS51Tools"
$wrapper = Get-ChildItem $wrapperRoot -Recurse -Filter "sdcc.sh" |
    Where-Object { $_.FullName -match "wrapper" } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if ($wrapper) {
    $text = [IO.File]::ReadAllText($wrapper.FullName)
    if ($text.Contains('FILTERED_ARGS=()')) {
        $text = $text.Replace('FILTERED_ARGS=()', 'FILTERED_ARGS=""')
        $text = $text.Replace('-c|-MMD|-MF)', '-MF)')
        $text = $text.Replace("`t`t`t`tcontinue`n`t`t`t`t;;`n`t`tesac", "`t`t`t`tcontinue`n`t`t`t`t;;`n`t`t`t-c|-MMD)`n`t`t`t`tcontinue`n`t`t`t`t;;`n`t`tesac")
        $text = $text.Replace('FILTERED_ARGS+=("$arg")', 'FILTERED_ARGS="$FILTERED_ARGS $arg"')
        $text = $text.Replace('"${FILTERED_ARGS[@]}"', '$FILTERED_ARGS')
        [IO.File]::WriteAllText($wrapper.FullName, $text, (New-Object Text.UTF8Encoding($false)))
    }
}

New-Item -ItemType Directory -Force -Path $buildDir, $outputDir | Out-Null
$arguments = @(
    "compile",
    "--fqbn", $fqbn,
    "--build-path", $buildDir,
    "--output-dir", $outputDir,
    "--config-dir", $configDir,
    "--warnings", "all"
)
if (-not $NoClean) {
    $arguments += "--clean"
}
$arguments += $sketchDir

& $cli @arguments
if ($LASTEXITCODE -ne 0) {
    throw "CH552 firmware build failed with exit code $LASTEXITCODE."
}

$generatedHex = Join-Path $outputDir "ch552.ino.hex"
$releaseHex = Join-Path $outputDir "xiaoji_ch552.hex"
if (-not (Test-Path $generatedHex) -or (Get-Item $generatedHex).Length -eq 0) {
    throw "The CH552 HEX output is missing or empty: $generatedHex"
}
Copy-Item $generatedHex $releaseHex -Force

$hasEndRecord = $false
foreach ($line in Get-Content $releaseHex) {
    if (-not $line.StartsWith(":")) {
        throw "Invalid Intel HEX record: $line"
    }
    $bytes = for ($index = 1; $index -lt $line.Length; $index += 2) {
        [Convert]::ToByte($line.Substring($index, 2), 16)
    }
    $sum = 0
    foreach ($value in $bytes) {
        $sum = ($sum + $value) -band 0xff
    }
    if ($sum -ne 0) {
        throw "Intel HEX checksum failed: $line"
    }
    if ($bytes.Count -ge 5 -and $bytes[3] -eq 1) {
        $hasEndRecord = $true
    }
}
if (-not $hasEndRecord) {
    throw "Intel HEX end-of-file record is missing."
}
Write-Host "CH552 firmware: $releaseHex"

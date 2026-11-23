[CmdletBinding()]
param(
    [string[]]$Driver = @('recording'),
    [ValidateSet('all','regression','single','composite','complex','validation','stress','config','external','K1','K2','K3','K4','K5','related','macro')]
    [string]$Suite = 'all',
    [string]$Case = '',
    [string]$Tag = '',
    [uint32]$Seed = 117,
    [ValidateRange(1,20)][int]$Repeat = 1,
    [ValidateRange(10,7200)][int]$TimeoutSeconds = 900,
    [string]$QtPrefixPath = 'D:\Qt\6.8.3-static',
    [string]$CMakePath = 'D:\software\cmake-3.31.6\bin\cmake.exe',
    [string]$BuildDirectory = '',
    [string]$OutputRoot = '',
    [string]$CatalogPath = '',
    [string]$DdRuntimeDirectory = '',
    [switch]$BuildOnly,
    [switch]$List,
    [switch]$AllowRealInput
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = Join-Path $repo 'verification\engine'
$fixtures = Join-Path $repo 'verification\fixtures'
$catalog = if ($CatalogPath) { [IO.Path]::GetFullPath($CatalogPath) } else { Join-Path $repo 'verification\scenarios\catalog.json' }
if (!$BuildDirectory) { $BuildDirectory = Join-Path $repo 'build-qt6\engine-verification' }
if (!$OutputRoot) { $OutputRoot = Join-Path $BuildDirectory 'runs' }
if (!$DdRuntimeDirectory) { $DdRuntimeDirectory = Join-Path $repo 'plugins\driver-dd\driver' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$drivers = @($Driver | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim().ToLowerInvariant() } | Select-Object -Unique)
foreach ($name in $drivers) {
    if ($name -notin @('recording','dd','xiaoji')) { throw "Unknown driver: $name" }
}
if (!$drivers.Count) { throw 'At least one driver is required.' }
if ($BuildDirectory -eq $repo) { throw 'The repository root cannot be the build directory.' }
if (!$BuildOnly -and !$List -and @($drivers | Where-Object { $_ -ne 'recording' }).Count -and !$AllowRealInput) {
    throw 'Real drivers require -AllowRealInput. The default recording backend sends no system input.'
}
if ($CMakePath -match 'CommonExtensions[\\/]Microsoft[\\/]CMake') {
    throw 'Use standalone CMake, not the VS BuildTools bundled CMake.'
}
if (!(Test-Path -LiteralPath $CMakePath -PathType Leaf)) { throw "CMake not found: $CMakePath" }
if (!(Test-Path -LiteralPath $QtPrefixPath -PathType Container)) { throw "Qt not found: $QtPrefixPath" }

function Write-Json($Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 100), [Text.UTF8Encoding]::new($false))
}
function Get-HashMap($Paths) {
    $map = [ordered]@{}
    foreach ($p in @($Paths | Sort-Object -Unique)) {
        $map[$p] = if (Test-Path -LiteralPath $p -PathType Leaf) { (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash } else { $null }
    }
    return $map
}
function Quote-NativeArgument([string]$Value) {
    # CommandLineToArgvW quoting, including embedded quotes and trailing slashes.
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}
function Invoke-CMake([string[]]$NativeArguments, [string]$LogName) {
    $argumentLine = ($NativeArguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
    $stdout = Join-Path $BuildDirectory "$LogName.stdout.log"
    $stderr = Join-Path $BuildDirectory "$LogName.stderr.log"
    $buildProcess = Start-Process -FilePath $CMakePath -ArgumentList $argumentLine -WorkingDirectory $repo `
        -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null = $buildProcess.Handle
    $buildProcess.WaitForExit()
    $buildProcess.Refresh()
    $code = $buildProcess.ExitCode
    $lines = @(Get-Content -LiteralPath $stdout -Encoding UTF8)
    $lines += @(Get-Content -LiteralPath $stderr -Encoding UTF8)
    $lines | Tee-Object -FilePath (Join-Path $BuildDirectory "$LogName.log") | Write-Host
    if ($null -eq $code -or $code -ne 0) { throw "CMake $LogName failed: $code" }
}

$guardPaths = @((Join-Path $repo 'config.json'), (Join-Path $repo 'build-qt6\Release\config.json'),
    (Join-Path $DdRuntimeDirectory 'config.json'), $catalog)
$guardPaths += @(Get-ChildItem -LiteralPath $fixtures -File | ForEach-Object { $_.FullName })
$before = Get-HashMap $guardPaths
New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
$originalPath = [Environment]::GetEnvironmentVariable('Path','Process')
$originalPathExt = [Environment]::GetEnvironmentVariable('PATHEXT','Process')
try {
    [Environment]::SetEnvironmentVariable('PATH',$null,'Process')
    [Environment]::SetEnvironmentVariable('Path',"$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem;$PSHOME",'Process')
    [Environment]::SetEnvironmentVariable('PATHEXT',".COM;.EXE;.BAT;.CMD;$originalPathExt",'Process')
    Invoke-CMake @('-S',$source,'-B',$BuildDirectory,'-G','Visual Studio 17 2022','-A','x64',"-DCMAKE_PREFIX_PATH=$QtPrefixPath") 'configure'
    Invoke-CMake @('--build',$BuildDirectory,'--config','Release','--target','engine-checks','--parallel','8') 'build'
} finally {
    [Environment]::SetEnvironmentVariable('PATH',$null,'Process')
    [Environment]::SetEnvironmentVariable('Path',$originalPath,'Process')
    [Environment]::SetEnvironmentVariable('PATHEXT',$originalPathExt,'Process')
}
if ($BuildOnly) {
    Write-Host "Build completed. No verification executable was started: $BuildDirectory\Release\engine-checks.exe"
    return
}

$exe = Join-Path $BuildDirectory 'Release\engine-checks.exe'
$git = (Get-Command git.exe -ErrorAction SilentlyContinue).Source
if (!$git -and (Test-Path -LiteralPath 'D:\software\Git\cmd\git.exe')) { $git = 'D:\software\Git\cmd\git.exe' }
if (!$git) { throw 'Git is required to record source provenance.' }
try {
    [Environment]::SetEnvironmentVariable('PATHEXT',".COM;.EXE;.BAT;.CMD;$originalPathExt",'Process')
    $commit = (& $git -C $repo rev-parse HEAD | Out-String).Trim()
    if ($LASTEXITCODE) { throw 'Cannot identify current commit.' }
    $statusBefore = (& $git -C $repo status --porcelain | Out-String)
    if ($LASTEXITCODE) { throw 'Cannot record working tree state.' }
} finally {
    [Environment]::SetEnvironmentVariable('PATHEXT',$originalPathExt,'Process')
}
$run = Join-Path $OutputRoot ((Get-Date).ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $run | Out-Null
Copy-Item -LiteralPath $catalog -Destination (Join-Path $run 'catalog.snapshot.json')
$runs = @()
$exitStatus = 0
$stop = $false
$manifest = [ordered]@{
    schemaVersion = 1; commit = $commit; sourceDirty = [bool]$statusBefore.Trim(); trackedStatus = $statusBefore
    startedAt = [DateTime]::UtcNow.ToString('o'); suite = $Suite; caseFilter = $Case; tag = $Tag; seed = $Seed
    exeSHA256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    catalogSHA256 = (Get-FileHash -LiteralPath $catalog -Algorithm SHA256).Hash
    catalogPath = $catalog; catalogOnly = [bool]$CatalogPath
    requestedDrivers = $drivers; repeat = $Repeat; listOnly = [bool]$List; userFilesBefore = $before
}
Write-Json (Join-Path $run 'manifest.json') $manifest
try {
    if (!$List -and (Get-Process -Name XiaoJiAutoKey,engine-checks -ErrorAction SilentlyContinue)) {
        throw 'Close the main application and other verification processes before running.'
    }
    foreach ($driverName in $drivers) {
        if ($stop) { break }
        for ($iteration=1; $iteration -le $Repeat; $iteration++) {
            $output = Join-Path $run "$driverName-$iteration"
            New-Item -ItemType Directory -Path $output | Out-Null
            $arguments = @('--driver',$driverName,'--suite',$Suite,'--fixtures',$fixtures,'--catalog',$catalog,
                '--runtime',$DdRuntimeDirectory,'--output',$output,'--commit',$commit,'--seed',"$Seed",'--timeout-ms',"$($TimeoutSeconds*1000)")
            if ($Case) { $arguments += @('--case',$Case) }
            if ($Tag) { $arguments += @('--tag',$Tag) }
            if ($List) { $arguments += '--list' }
            if ($CatalogPath) { $arguments += '--catalog-only' }
            if ($AllowRealInput) { $arguments += '--allow-real-input' }
            if ($statusBefore.Trim()) { $arguments += '--source-dirty' }
            if ($driverName -ne 'recording' -and !$List) {
                Write-Warning "Real $driverName input will be intercepted. Do not use the keyboard/mouse during this run. Escape aborts."
                Start-Sleep -Seconds 3
            }
            $argumentLine = ($arguments | ForEach-Object { Quote-NativeArgument $_ }) -join ' '
            $process = Start-Process -FilePath $exe -ArgumentList $argumentLine -WorkingDirectory $output -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $output 'console.log') -RedirectStandardError (Join-Path $output 'stderr.log')
            # Windows PowerShell 5.1 otherwise loses ExitCode after WaitForExit on a detached child.
            $null = $process.Handle
            $timedOut = !$process.WaitForExit(($TimeoutSeconds+10)*1000)
            if ($timedOut) {
                [IO.File]::WriteAllText((Join-Path $output 'cancel.flag'),'watchdog timeout')
                if (!$process.WaitForExit(10000)) {
                    $process.Kill(); $process.WaitForExit()
                    Write-Warning 'The process was terminated. Input release is UNCONFIRMED; no further driver run will start.'
                }
                $stop = $true
            }
            $process.WaitForExit()
            $process.Refresh()
            $code = $process.ExitCode
            if ($null -eq $code) { $code = 2 }
            if ($timedOut) { $code = 2 }
            $reportFile = Join-Path $output 'results.json'
            $report = if (Test-Path -LiteralPath $reportFile) { Get-Content -LiteralPath $reportFile -Raw -Encoding UTF8 | ConvertFrom-Json } else { $null }
            $runs += [ordered]@{ driver=$driverName; iteration=$iteration; exitCode=$code; timedOut=$timedOut
                report=$reportFile; overall=$(if ($timedOut -or !$report) { 'ABORTED' } else { $report.overall }); counts=$report.counts }
            if ($List -and $report) { $report.results | Select-Object id,description | Format-Table -AutoSize | Out-Host }
            else { Get-Content -LiteralPath (Join-Path $output 'console.log') -Encoding UTF8 | Write-Host }
            Write-Host "$driverName / $iteration : $($runs[-1].overall), exit $code"
            if ($code -ne 0 -and ($exitStatus -eq 0 -or $code -in @(1,2,5,6))) { $exitStatus=$code }
            if ($code -in @(2,5)) { $stop=$true; break }
        }
    }
} catch {
    $exitStatus=2
    $manifest['error']=$_.Exception.Message
    Write-Warning "Verification aborted: $($_.Exception.Message)"
} finally {
    $after = Get-HashMap $guardPaths
    $unchanged = $true
    foreach ($p in $before.Keys) { if ($before[$p] -ne $after[$p]) { $unchanged=$false } }
    $manifest['completedAt']=[DateTime]::UtcNow.ToString('o')
    $manifest['runs']=$runs
    $manifest['userFilesAfter']=$after
    $manifest['guardedFilesUnchanged']=$unchanged
    try {
        [Environment]::SetEnvironmentVariable('PATHEXT',".COM;.EXE;.BAT;.CMD;$originalPathExt",'Process')
        $manifest['trackedStatusAfter']=(& $git -C $repo status --porcelain | Out-String)
        if ($LASTEXITCODE) { throw 'Cannot record final working tree state.' }
    } catch {
        $exitStatus=2
        $manifest['finalStatusError']=$_.Exception.Message
        Write-Warning "Cannot record final working tree state: $($_.Exception.Message)"
    } finally {
        [Environment]::SetEnvironmentVariable('PATHEXT',$originalPathExt,'Process')
    }
    if (!$unchanged) { $exitStatus=2; Write-Warning 'Guarded files changed; they have NOT been restored or overwritten.' }
    $manifest['exitCode']=$exitStatus
    Write-Json (Join-Path $run 'manifest.json') $manifest
    $lines = @('# Engine verification report','',"Commit: ``$commit``", "Suite: ``$Suite``; seed: $Seed; list only: $([bool]$List)",
        '', '| Driver | Iteration | Outcome | Exit |', '| --- | ---: | --- | ---: |')
    foreach ($r in $runs) { $lines += "| $($r.driver) | $($r.iteration) | $($r.overall) | $($r.exitCode) |" }
    $lines += @('',"Guarded files unchanged: $unchanged",'',
        'Each driver directory contains results.json (expectations, observed states, generated random actions and coverage), events.csv and owners.csv.',
        'Recording output is NOT real-driver evidence. NOT_VERIFIED/BLOCKED/CONTAMINATED are not passes. No automatic retries are performed.',
        'Game acceptance, physical WM_INPUT ingress, full application feedback filtering and real foreground recognition are not certified.')
    [IO.File]::WriteAllLines((Join-Path $run 'report.md'),$lines,[Text.UTF8Encoding]::new($false))
    Write-Host "Reports: $run"
}
if ($exitStatus) { exit $exitStatus }

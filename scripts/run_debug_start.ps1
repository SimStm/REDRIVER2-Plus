[CmdletBinding()]
param(
    # Path to the debug executable. Defaults to the Release_dev build.
    [string]$Exe = "",

    # Read mission/car/position/players/chase from developer_debug_start.ini.
    [switch]$FromSnapshot,

    # Explicit reproducible session.
    [int]$Mission = -1,
    [int]$GameType = -1,
    [int]$Level = -1,
    [int]$Car = -1,
    [int]$X = [int]::MinValue,
    [int]$Z = [int]::MinValue,
    [int]$StartDir = [int]::MinValue,
    [int]$Players = 1,
    [int]$Chase = 0,
    [string]$Replay = "",

    # Extra configuration passed with -ini. Defaults to the executable's config.ini.
    [string]$Config = "",

    # Extra game arguments appended verbatim to every launch, in both normal and
    # -Capture modes. Pass one string or an array; each element is one argument.
    # Example: -ExtraArgs '-drawregion' prepares the native region draw so a
    # RED2_NATIVE_RENDERER=1 run has a scene to capture.
    [string[]]$ExtraArgs = @(),

    # Capture mode: launch twice (texture overrides on and off), press F12 in
    # each run and store SCREENSHOT.BMP for comparison.
    [switch]$Capture,
    [int]$DelaySeconds = 20,

    # Anchor the capture to the simulation tick count instead of wall-clock
    # seconds. When greater than 0 the game freezes the simulation at this many
    # ticks after the first presented frame, captureAfterSeconds is forced to 0,
    # and both renderer paths capture the identical simulation state.
    [int]$DelayTicks = 0,
    [string]$OutDir = "",

    # Keep the process attached to this script instead of returning immediately.
    [switch]$Wait
)

$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Exe))
{
    $Exe = Join-Path $scriptRoot 'src_rebuild\bin\Release_dev\REDRIVER2_dev.exe'
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$workDir = Split-Path -Parent $Exe

if ([string]::IsNullOrWhiteSpace($Config))
{
    $Config = Join-Path $workDir 'config.ini'
}
$Config = (Resolve-Path -LiteralPath $Config).Path

if ([string]::IsNullOrWhiteSpace($OutDir))
{
    $OutDir = Join-Path $scriptRoot 'src_rebuild\build\debug-start-captures'
}

$snapshotPath = Join-Path $workDir 'developer_debug_start.ini'

function Read-Snapshot
{
    if (-not (Test-Path -LiteralPath $snapshotPath))
    {
        throw "No snapshot found at $snapshotPath. Save one from the Developer Graphics Panel first."
    }

    $snapshot = @{}
    foreach ($line in Get-Content -LiteralPath $snapshotPath)
    {
        if ($line -match '^\s*([A-Za-z]+)\s*=\s*(-?\d+)\s*$')
        {
            $snapshot[$Matches[1]] = [int]$Matches[2]
        }
    }

    if ($snapshot['enabled'] -ne 1)
    {
        Write-Warning "$snapshotPath exists but is disabled (enabled=0). The game will not auto-start."
    }

    return $snapshot
}

function New-StartArguments
{
    $arguments = @()

    if (-not [string]::IsNullOrWhiteSpace($Replay))
    {
        $arguments = @('-nointro', '-replay', $Replay)
    }
    else
    {
        if ($Mission -lt 0)
        {
            throw 'Specify -Replay or -Mission (or use -FromSnapshot).'
        }

        $arguments = @('-nointro', '-mission', "$Mission")
        if ($GameType -ge 0) { $arguments += @('-gametype', "$GameType") }
        if ($Level -ge 0) { $arguments += @('-level', "$Level") }
        if ($Car -ge 0) { $arguments += @('-playercar', "$Car") }
        if ($X -ne [int]::MinValue -and $Z -ne [int]::MinValue)
        {
            $arguments += @('-startpos', "$X", "$Z")
            if ($StartDir -ne [int]::MinValue) { $arguments += @('-startdir', "$StartDir") }
        }
        if ($Players -gt 0) { $arguments += @('-players', "$Players") }
        if ($Chase -ne 0) { $arguments += @('-chase', "$Chase") }
    }

    if ($ExtraArgs.Count -gt 0)
    {
        $arguments += $ExtraArgs
    }

    return $arguments
}

if ($FromSnapshot)
{
    $snapshot = Read-Snapshot
    if (-not [string]::IsNullOrWhiteSpace($Replay))
    {
        Write-Warning '-Replay overrides the snapshot mission.'
    }
    else
    {
        $Mission = [int]$snapshot['mission']
        $GameType = [int]$snapshot['gameType']
        $Level = [int]$snapshot['level']
        $Car = [int]$snapshot['car']
        $X = [int]$snapshot['startX']
        $Z = [int]$snapshot['startZ']
        if ($snapshot.ContainsKey('startDir')) { $StartDir = [int]$snapshot['startDir'] }
        $Players = [int]$snapshot['players']
        $Chase = [int]$snapshot['chase']
    }
}

if (-not $Capture)
{
    $arguments = New-StartArguments
    Write-Host "Launching: $Exe $($arguments -join ' ')"
    if ($Wait)
    {
        Start-Process -FilePath $Exe -ArgumentList $arguments -WorkingDirectory $workDir -Wait
    }
    else
    {
        Start-Process -FilePath $Exe -ArgumentList $arguments -WorkingDirectory $workDir | Out-Null
    }
    exit 0
}

# ---------------------------------------------------------------------------
# Capture mode: the same deterministic session with texture overrides on/off.
# The game saves SCREENSHOT.BMP itself after captureAfterSeconds, so no window
# focus or key injection is required.
# ---------------------------------------------------------------------------

$screenshot = Join-Path $workDir 'SCREENSHOT.BMP'
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$baseArguments = New-StartArguments
$configText = Get-Content -LiteralPath $Config -Raw
$captured = @()

foreach ($enabled in @(1, 0))
{
    $captureConfig = Join-Path $workDir 'config.capture.ini'
    $captureText = $configText
    $captureText = if ($captureText -match '(?im)^\s*textureOverrides\s*=')
    {
        [regex]::Replace($captureText, '(?im)^\s*textureOverrides\s*=.*$', "textureOverrides=$enabled")
    }
    else
    {
        "$captureText`r`n[render]`r`ntextureOverrides=$enabled`r`n"
    }

    # Tick-anchored mode freezes the simulation, so the wall-clock trigger must
    # not fire earlier than the freeze.
    $secondsValue = if ($DelayTicks -gt 0) { 0 } else { $DelaySeconds }

    if ($captureText -match '(?im)^\s*captureAfterSeconds\s*=')
    {
        $captureText = [regex]::Replace($captureText, '(?im)^\s*captureAfterSeconds\s*=.*$', "captureAfterSeconds=$secondsValue")
    }
    else
    {
        $captureText = "$captureText`r`n[game]`r`ncaptureAfterSeconds=$secondsValue`r`n"
    }

    if ($DelayTicks -gt 0)
    {
        # Tick mode: the freeze owns the capture moment, so put the tick key in
        # the same [game] section as the (zeroed) wall trigger.
        if ($captureText -match '(?im)^\s*captureAfterTicks\s*=')
        {
            $captureText = [regex]::Replace($captureText, '(?im)^\s*captureAfterTicks\s*=.*$', "captureAfterTicks=$DelayTicks")
        }
        else
        {
            $captureText = "$captureText`r`n[game]`r`ncaptureAfterTicks=$DelayTicks`r`n"
        }
    }

    Set-Content -LiteralPath $captureConfig -Value $captureText -Encoding ascii

    if (Test-Path -LiteralPath $screenshot) { Remove-Item -LiteralPath $screenshot -Force }

    $arguments = @('-ini', 'config.capture.ini') + $baseArguments
    Write-Host "Capturing (textureOverrides=$enabled): $Exe $($arguments -join ' ')"

    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -WorkingDirectory $workDir -PassThru

    $deadline = if ($DelayTicks -gt 0)
    {
        (Get-Date).AddSeconds([math]::Ceiling($DelayTicks / 4.0) + 240)
    }
    else
    {
        (Get-Date).AddSeconds($DelaySeconds + 60)
    }
    while ((Get-Date) -lt $deadline -and -not (Test-Path -LiteralPath $screenshot))
    {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        if ($process.HasExited) { break }
    }

    if (-not (Test-Path -LiteralPath $screenshot))
    {
        if (-not $process.HasExited) { $process.Kill() }
        Remove-Item -LiteralPath $captureConfig -Force -ErrorAction SilentlyContinue
        $anchor = if ($DelayTicks -gt 0) { "$DelayTicks ticks" } else { "$DelaySeconds seconds" }
        throw "No SCREENSHOT.BMP after $anchor with textureOverrides=$enabled. The session may not have reached gameplay."
    }

    $destination = Join-Path $OutDir ("overrides-{0}.bmp" -f $enabled)
    Copy-Item -LiteralPath $screenshot -Destination $destination -Force
    $captured += $destination

    if (-not $process.HasExited)
    {
        $process.Kill()
        $process.WaitForExit(5000) | Out-Null
    }
}

Remove-Item -LiteralPath (Join-Path $workDir 'config.capture.ini') -Force -ErrorAction SilentlyContinue

Write-Host ''
Write-Host 'Captured frames:'
$captured | ForEach-Object { Write-Host "  $_" }
Write-Host 'Compare the two images; with the cutout fix transparent regions must not render black in either.'

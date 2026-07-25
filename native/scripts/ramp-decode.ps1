<#
  ramp-decode.ps1 - sweep the camera-free decode ceiling and RECORD every result.

  Runs vms_grid --test-pattern at a series of tile counts, captures each run's
  full console output to a log file, parses the SUMMARY(test-pattern) line plus
  the peak RSS / last CPU%, and appends one row per run to a CSV. This is the
  measurement half of native increment 4a - no governor policy, just data.

  It answers one question per hardware tier: how many simultaneous 25 fps streams
  can this machine DECODE before the aggregate fps stops climbing? That ceiling is
  the empirical input the P3-03 / P0-05 governor discussion needs.

  Usage (from anywhere; paths are resolved relative to this script):
    # Dev box (has H.265 hardware decode):
    .\ramp-decode.ps1 -Label devbox -Codec h265

    # Low-end i5 tier (no H.265 HW decode -> measure the H.264 ceiling instead):
    .\ramp-decode.ps1 -Label lowend-i5 -Codec h264 -Counts 2,4,6,9,12,16

  Params:
    -Label    short machine name, stamped into the CSV + log names
    -Codec    h265 (default) or h264
    -Counts   tile counts to sweep (default 4,9,16,25,36,49,64)
    -Seconds  run length per count (default 20)
    -Srcw/-Srch  encoded source resolution (default 1920x1080; use 640 480 for a sub-stream)
    -GstRoot  GStreamer root (default C:\Program Files\gstreamer\1.0\msvc_x86_64)
    -Exe      path to vms_grid.exe (default <native>\build\vms_grid.exe)
#>
param(
  [string]$Label   = "unlabeled",
  [ValidateSet("h265","h264")][string]$Codec = "h265",
  [int[]]$Counts   = @(4,9,16,25,36,49,64),
  [int]$Seconds    = 20,
  [int]$Srcw       = 1920,
  [int]$Srch       = 1080,
  [string]$GstRoot = "C:\Program Files\gstreamer\1.0\msvc_x86_64",
  [string]$Exe     = ""
)

$ErrorActionPreference = "Stop"

# Resolve paths relative to this script: <native>\scripts\ramp-decode.ps1
$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$nativeDir  = Split-Path -Parent $scriptDir
if ([string]::IsNullOrEmpty($Exe)) { $Exe = Join-Path $nativeDir "build\vms_grid.exe" }
$resultsDir = Join-Path $nativeDir "results"
$logsDir    = Join-Path $resultsDir "logs"
New-Item -ItemType Directory -Force -Path $logsDir | Out-Null

if (-not (Test-Path $Exe)) {
  Write-Error "vms_grid.exe not found at '$Exe'. Build it first (cmake --build build)."
  exit 1
}
if (-not (Test-Path $GstRoot)) {
  Write-Error "GStreamer not found at '$GstRoot'. Pass -GstRoot with the correct path."
  exit 1
}

# Bring GStreamer's DLLs and plugins into scope for the child process.
$env:PATH = (Join-Path $GstRoot "bin") + ";" + $env:PATH
$env:GST_PLUGIN_PATH = Join-Path $GstRoot "lib\gstreamer-1.0"

$stamp   = Get-Date -Format "yyyyMMdd-HHmmss"
$csvPath = Join-Path $resultsDir "decode-ceiling.csv"
if (-not (Test-Path $csvPath)) {
  "timestamp,label,codec,src,tiles,seconds,agg_fps,sustainable_streams,cpu_pct,rss_mb,log" |
    Out-File -FilePath $csvPath -Encoding utf8
}

Write-Host ""
Write-Host "=== decode-ceiling ramp: label=$Label codec=$Codec src=${Srcw}x${Srch} ===" -ForegroundColor Cyan
Write-Host "exe: $Exe"
Write-Host "results CSV: $csvPath"
Write-Host ""

$rows = @()
foreach ($n in $Counts) {
  Write-Host ("-- running {0} tiles ({1}s) ..." -f $n, $Seconds) -ForegroundColor Yellow
  $log = Join-Path $logsDir ("{0}_{1}_{2}_n{3}.log" -f $Label, $Codec, $stamp, $n)

  $runArgs = @("--test-pattern","--count",$n,"--codec",$Codec,
               "--srcw",$Srcw,"--srch",$Srch,"--seconds",$Seconds)
  # Merge stderr, coerce every line to a plain string (PS 5.1 wraps native stderr
  # in ErrorRecord objects otherwise), keep the full log, and scan it.
  $out = & $Exe @runArgs 2>&1 | ForEach-Object { $_.ToString() }
  $out | Out-File -FilePath $log -Encoding utf8

  $summary = $out | Select-String -Pattern "SUMMARY\(test-pattern\)" | Select-Object -Last 1
  $aggFps = ""; $streams = ""; $tiles = $n
  if ($summary) {
    $line = $summary.ToString()
    if ($line -match "aggregate decode fps=(\d+)") { $aggFps  = $Matches[1] }
    if ($line -match "\(~(\d+) sustainable")       { $streams = $Matches[1] }
    if ($line -match "tiles=(\d+)")                { $tiles   = $Matches[1] }
  } else {
    Write-Host "   (no SUMMARY line - see $log)" -ForegroundColor Red
  }

  # Peak working set + last reported CPU% from the per-second stat ticks.
  $rss = ($out | Select-String -Pattern "rss=(\d+)MB" -AllMatches |
          ForEach-Object { $_.Matches } | ForEach-Object { [int]$_.Groups[1].Value } |
          Measure-Object -Maximum).Maximum
  $cpuMatch = $out | Select-String -Pattern "cpu=(\d+)%" | Select-Object -Last 1
  $cpu = ""
  if ($cpuMatch -and ($cpuMatch.ToString() -match "cpu=(\d+)%")) { $cpu = $Matches[1] }

  $row = [pscustomobject]@{
    tiles = $tiles; agg_fps = $aggFps; streams = $streams; cpu_pct = $cpu; rss_mb = $rss
  }
  $rows += $row
  Write-Host ("   tiles={0}  agg_fps={1}  ~streams={2}  cpu={3}%  rss={4}MB" -f `
              $tiles, $aggFps, $streams, $cpu, $rss) -ForegroundColor Green

  "$stamp,$Label,$Codec,${Srcw}x${Srch},$tiles,$Seconds,$aggFps,$streams,$cpu,$rss,$log" |
    Out-File -FilePath $csvPath -Append -Encoding utf8
}

Write-Host ""
Write-Host "=== ramp complete: $Label / $Codec ===" -ForegroundColor Cyan
$rows | Format-Table -AutoSize
Write-Host ""
Write-Host "The ceiling is the tile count where agg_fps stops rising (and dropped/cpu climb)."
Write-Host "All rows appended to: $csvPath"
Write-Host "Per-run logs in:       $logsDir"

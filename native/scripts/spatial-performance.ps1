<#
  spatial-performance.ps1 - run the P3-15 64-tile latency regression gate
  from a normal PowerShell, with the Qt/GStreamer runtime paths supplied.

  Usage:
    .\spatial-performance.ps1
    .\spatial-performance.ps1 -Samples 5000
    .\spatial-performance.ps1 -BuildDir ..\out\perf-release -Samples 5000
#>
param(
  [ValidateRange(50, 100000)]
  [int]$Samples = 600,
  [string]$BuildDir = "..\build"
)

$ErrorActionPreference = "Stop"
$scriptsDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $scriptsDir $BuildDir))
$workspaceExe = Join-Path $resolvedBuildDir "vms_workspace.exe"

if (-not (Test-Path -LiteralPath $workspaceExe)) {
  Write-Error "vms_workspace.exe not found: $workspaceExe (run build.ps1 first)"
  exit 2
}

$qtBin = "C:\Qt\6.11.1\msvc2022_64\bin"
$gstBin = "C:\Program Files\gstreamer\1.0\msvc_x86_64\bin"
if (-not (Test-Path -LiteralPath $qtBin)) {
  Write-Error "Qt runtime not found: $qtBin"
  exit 2
}

$env:PATH = "$qtBin;$gstBin;" + $env:PATH
& $workspaceExe --spatial-benchmark $Samples
exit $LASTEXITCODE

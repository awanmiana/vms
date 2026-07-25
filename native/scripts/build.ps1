<#
  build.ps1 - build the native targets from ANY PowerShell (no Developer shell needed).

  cmake and cl live inside the VS Build Tools and are not on the default PATH.
  This sources vcvars64.bat and uses the bundled CMake to configure (first time)
  and build. Run it from anywhere.

  Usage:
    .\build.ps1                     # build everything
    .\build.ps1 -Target vms_govtest # build one target
    .\build.ps1 -Configure          # force a fresh configure first

  Params:
    -Target     CMake target to build (default: all)
    -Configure  re-run the configure step before building
#>
param(
  [string]$Target = "",
  [switch]$Configure
)

$ErrorActionPreference = "Stop"

$nativeDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$buildDir  = Join-Path $nativeDir "build"

$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$cmake  = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $vcvars)) { Write-Error "vcvars64.bat not found: $vcvars"; exit 1 }
if (-not (Test-Path $cmake))  { Write-Error "bundled cmake not found: $cmake"; exit 1 }

$qtPrefix = "C:/Qt/6.11.1/msvc2022_64"
$gstRoot  = "C:/Program Files/gstreamer/1.0/msvc_x86_64"

if ($Configure -or -not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
  Write-Host "== configuring ==" -ForegroundColor Cyan
  cmd /c "`"$vcvars`" >nul 2>&1 && `"$cmake`" -S `"$nativeDir`" -B `"$buildDir`" -G Ninja -DCMAKE_PREFIX_PATH=`"$qtPrefix`" -DGSTREAMER_ROOT=`"$gstRoot`" 2>&1"
  if ($LASTEXITCODE -ne 0) { Write-Error "configure failed ($LASTEXITCODE)"; exit 1 }
}

$targetArg = ""
$targetLabel = "all"
if (-not [string]::IsNullOrEmpty($Target)) { $targetArg = "--target $Target"; $targetLabel = $Target }
Write-Host "== building $targetLabel ==" -ForegroundColor Cyan
cmd /c "`"$vcvars`" >nul 2>&1 && `"$cmake`" --build `"$buildDir`" $targetArg 2>&1"
if ($LASTEXITCODE -ne 0) { Write-Error "build failed ($LASTEXITCODE)"; exit 1 }

Write-Host "== build OK ==" -ForegroundColor Green
Write-Host "binaries in: $buildDir"

# Flash + serial monitor - Akasha Companion (Phase 6)
#
# Examples:
#   .\scripts\flash.ps1
#   .\scripts\flash.ps1 -Port COM5 -NoMonitor
#   .\scripts\flash.ps1 -BuildOnly
#   .\scripts\flash.ps1 -MonitorOnly -Port COM5
#   .\scripts\flash.ps1 -Env fnk0104b

param(
  [switch]$MonitorOnly,
  [switch]$BuildOnly,
  [switch]$NoMonitor,
  [string]$Port = "",
  [string]$Env = "fnk0104b"
)

$ErrorActionPreference = "Stop"

function Find-Pio {
  $candidate = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
  if (Test-Path $candidate) { return $candidate }
  $cmd = Get-Command pio -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "PlatformIO (pio) not found. Install: https://platformio.org/install"
}

function Resolve-SerialPort([string]$preferred) {
  if ($preferred) { return $preferred }
  try {
    $list = & $script:pio device list 2>$null | Out-String
    # Prefer Espressif USB CDC (VID 303A)
    if ($list -match '(COM\d+)[\s\S]{0,200}?303A') {
      return $Matches[1]
    }
    if ($list -match 'COM(\d+)') {
      return "COM$($Matches[1])"
    }
  } catch {}
  return ""
}

$script:pio = Find-Pio
$firmware = Join-Path $PSScriptRoot "..\firmware"
Set-Location $firmware

if (-not (Test-Path "include\secrets.h")) {
  Copy-Item "include\secrets.example.h" "include\secrets.h"
  Write-Host "Created include\secrets.h from example - edit WiFi / daemon host before use."
  Write-Host "Tip: .\scripts\provision.ps1 -Wifi 'SSID' -WifiPass 'pass' -DaemonHost '192.168.1.10'"
}

$allowed = @("fnk0104b", "fnk0104a", "fnk0104n", "fnk0104s")
if ($allowed -notcontains $Env) {
  throw "Unknown -Env '$Env'. Allowed: $($allowed -join ', ')"
}
if ($Env -ne "fnk0104b") {
  Write-Host "WARNING: env '$Env' is experimental / may not compile (product support = fnk0104b)." -ForegroundColor Yellow
}

$portResolved = Resolve-SerialPort $Port
if ($portResolved) {
  $env:PLATFORMIO_UPLOAD_PORT = $portResolved
  $env:PLATFORMIO_MONITOR_PORT = $portResolved
  Write-Host "Serial port: $portResolved"
} elseif (-not $BuildOnly -and -not $MonitorOnly) {
  Write-Host "No -Port given and none auto-detected; pio will try default." -ForegroundColor Yellow
}

$envArgs = @("run", "-e", $Env)

if ($MonitorOnly) {
  $monArgs = @("device", "monitor", "-b", "115200")
  if ($portResolved) { $monArgs += @("--port", $portResolved) }
  & $script:pio @monArgs
  exit $LASTEXITCODE
}

if ($BuildOnly) {
  & $script:pio @envArgs
  exit $LASTEXITCODE
}

& $script:pio @envArgs -t upload
if ($LASTEXITCODE -ne 0) {
  Write-Host ""
  Write-Host "Upload failed. If the error is 'Could not open COMx':" -ForegroundColor Red
  Write-Host "  - Close any 'pio device monitor' / Serial Monitor using that port"
  Write-Host "  - Unplug/replug USB-C, then retry with -Port COMx"
  exit $LASTEXITCODE
}

if (-not $NoMonitor) {
  $monArgs = @("device", "monitor", "-b", "115200")
  if ($portResolved) { $monArgs += @("--port", $portResolved) }
  & $script:pio @monArgs
  exit $LASTEXITCODE
}

Write-Host "Flash OK (-NoMonitor)."
exit 0

# Provision firmware/include/secrets.h for Akasha Companion (Phase 6)
#
# Examples:
#   .\scripts\provision.ps1
#   .\scripts\provision.ps1 -Wifi "Maison" -WifiPass "secret" -DaemonHost "192.168.1.168" -DaemonPort 3876
#   .\scripts\provision.ps1 -PairSecret "my-pair" -Force

param(
  [string]$Wifi = "",
  [string]$WifiPass = "",
  [string]$DaemonHost = "",
  [int]$DaemonPort = 0,
  [string]$Token = "",
  [string]$PairSecret = "",
  [switch]$Force
)

$ErrorActionPreference = "Stop"
$firmware = Join-Path $PSScriptRoot "..\firmware"
$example = Join-Path $firmware "include\secrets.example.h"
$target = Join-Path $firmware "include\secrets.h"

if (-not (Test-Path $example)) {
  throw "Missing $example"
}

$hasPatch = $Wifi -or $WifiPass -or $DaemonHost -or ($DaemonPort -gt 0) -or $Token -or $PairSecret
if ((Test-Path $target) -and -not $Force -and -not $hasPatch) {
  Write-Host "secrets.h already exists. Use -Force to overwrite from example, or pass -Wifi/-DaemonHost/... to patch."
  Write-Host ""
  Write-Host "Daemon reminder: set AKASHA_BIND=0.0.0.0 so the ESP can reach the daemon on the LAN."
  exit 0
}

if (-not (Test-Path $target) -or $Force) {
  Copy-Item $example $target -Force
  Write-Host "Wrote $target from secrets.example.h"
}

function Set-DefineString([string]$name, [string]$value) {
  $lines = Get-Content $target
  $escaped = $value.Replace('\', '\\').Replace('"', '\"')
  $repl = "#define $name `"$escaped`""
  $found = $false
  $out = foreach ($line in $lines) {
    if ($line -match "^#define\s+$([regex]::Escape($name))\s+") {
      $found = $true
      $repl
    } else {
      $line
    }
  }
  if (-not $found) { $out = @($out) + $repl }
  $out | Set-Content $target
}

function Set-DefineRaw([string]$name, [string]$value) {
  $lines = Get-Content $target
  $repl = "#define $name $value"
  $found = $false
  $out = foreach ($line in $lines) {
    if ($line -match "^#define\s+$([regex]::Escape($name))\s+") {
      $found = $true
      $repl
    } else {
      $line
    }
  }
  if (-not $found) { $out = @($out) + $repl }
  $out | Set-Content $target
}

if ($Wifi) { Set-DefineString "WIFI_SSID" $Wifi }
if ($WifiPass) { Set-DefineString "WIFI_PASS" $WifiPass }
if ($DaemonHost) { Set-DefineString "AKASHA_HOST" $DaemonHost }
if ($DaemonPort -gt 0) { Set-DefineRaw "AKASHA_PORT" "$DaemonPort" }
if ($PSBoundParameters.ContainsKey("Token")) { Set-DefineString "AKASHA_TOKEN" $Token }
if ($PairSecret) { Set-DefineString "AKASHA_PAIR_SECRET" $PairSecret }

Write-Host "Provision OK: $target"
Write-Host ""
Write-Host "Next:"
Write-Host "  1. Start daemon with AKASHA_BIND=0.0.0.0 (LAN) + STT/TTS in voice_router.yaml"
Write-Host "  2. Flash: .\scripts\flash.ps1 -Port COMx -NoMonitor"
Write-Host "  3. See docs/user/getting-started.md"

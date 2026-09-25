# Open Windows Firewall for Akasha Companion LAN access (port 3876).
# Run once as Administrator:
#   powershell -ExecutionPolicy Bypass -File .\scripts\open-lan-firewall.ps1

$ErrorActionPreference = "Stop"
$name = "Akasha Daemon LAN 3876"

$existing = Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue
if ($existing) {
  Enable-NetFirewallRule -DisplayName $name
  Write-Host "Enabled existing rule: $name"
} else {
  New-NetFirewallRule -DisplayName $name -Direction Inbound -Protocol TCP -LocalPort 3876 `
    -Action Allow -Profile Any -Description "Allow ESP32 Companion to reach akasha-daemon on LAN" | Out-Null
  Write-Host "Created rule: $name (TCP 3876 inbound)"
}

Write-Host "Test from PC: Invoke-RestMethod http://192.168.1.168:3876/api/status"
Write-Host "Then reboot / wait for Companion hb d=1"

[CmdletBinding()]
param(
    [string]$InstallRoot = "$env:ProgramFiles\HydrogenHttpd",
    [string]$DataRoot = "$env:ProgramData\HydrogenHttpd",
    [switch]$PurgeData
)

$ErrorActionPreference = "Stop"

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run this script from an elevated PowerShell session."
}

$Binary = Join-Path $InstallRoot "bin\hydrogen_httpd.exe"

Stop-Service -Name HydrogenHttpd -Force -ErrorAction SilentlyContinue

if (Test-Path $Binary) {
    & $Binary --uninstall-service | Out-Null
} else {
    & sc.exe delete HydrogenHttpd | Out-Null
}

if ($PurgeData -and (Test-Path $DataRoot)) {
    Remove-Item $DataRoot -Recurse -Force
}

Write-Host "HydrogenHttpd service removed."
if (-not $PurgeData) {
    Write-Host "Configuration and runtime data were preserved in $DataRoot"
}

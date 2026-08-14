[CmdletBinding()]
param(
    [string]$InstallRoot = "$env:ProgramFiles\HydrogenHttpd",
    [string]$DataRoot = "$env:ProgramData\HydrogenHttpd",
    [switch]$NoStart
)

$ErrorActionPreference = "Stop"

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Run this script from an elevated PowerShell session."
    }
}

Assert-Administrator

$Binary = Join-Path $InstallRoot "bin\hydrogen_httpd.exe"
$TemplateRoot = Join-Path $InstallRoot "share\hydrogenhttpd"
$ConfigTemplate = Join-Path $TemplateRoot "config\server.conf"
$ConfigPath = Join-Path $DataRoot "server.conf"

if (-not (Test-Path $Binary)) {
    throw "HydrogenHttpd executable not found: $Binary"
}

$directories = @(
    $DataRoot,
    (Join-Path $DataRoot "www"),
    (Join-Path $DataRoot "logs"),
    (Join-Path $DataRoot "certs"),
    (Join-Path $DataRoot "secrets"),
    (Join-Path $DataRoot "uploads"),
    (Join-Path $DataRoot "tmp\uploads"),
    (Join-Path $DataRoot "sql")
)

foreach ($directory in $directories) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

if (-not (Test-Path $ConfigPath)) {
    Copy-Item $ConfigTemplate $ConfigPath
}

$SecretsPath = Join-Path $DataRoot "secrets\auth.tokens"
if (-not (Test-Path $SecretsPath)) {
    New-Item -ItemType File -Path $SecretsPath -Force | Out-Null
}

$ExampleIndex = Join-Path $DataRoot "www\index.html"
if (-not (Test-Path $ExampleIndex)) {
    Copy-Item (Join-Path $TemplateRoot "www\*") (Join-Path $DataRoot "www") -Recurse -Force
}

# LocalService SID: S-1-5-19. Grant modify only to runtime data.
& icacls.exe $DataRoot /inheritance:r | Out-Null
& icacls.exe $DataRoot /grant:r `
    "*S-1-5-32-544:(OI)(CI)F" `
    "*S-1-5-18:(OI)(CI)F" `
    "*S-1-5-19:(OI)(CI)M" /T /C | Out-Null

# Keep the main config and secret store read-only for LocalService.
& icacls.exe $ConfigPath /inheritance:r | Out-Null
& icacls.exe $ConfigPath /grant:r `
    "*S-1-5-32-544:F" `
    "*S-1-5-18:F" `
    "*S-1-5-19:R" | Out-Null

& icacls.exe $SecretsPath /inheritance:r | Out-Null
& icacls.exe $SecretsPath /grant:r `
    "*S-1-5-32-544:F" `
    "*S-1-5-18:F" `
    "*S-1-5-19:R" | Out-Null

& $Binary --check-config --config $ConfigPath
if ($LASTEXITCODE -ne 0) {
    throw "Configuration validation failed."
}

& $Binary --uninstall-service | Out-Null
& $Binary --install-service --config $ConfigPath
if ($LASTEXITCODE -ne 0) {
    throw "Windows service registration failed."
}

Set-Service -Name HydrogenHttpd -StartupType Automatic

if (-not $NoStart) {
    Start-Service -Name HydrogenHttpd
}

Write-Host "HydrogenHttpd installed as a Windows service."
Write-Host "Configuration: $ConfigPath"

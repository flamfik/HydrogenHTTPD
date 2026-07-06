$ErrorActionPreference = "Stop"
$Target = "E:\HydrogenHttpd"

if (!(Test-Path "E:\")) {
    throw "Drive E: does not exist or is not mounted."
}

if (Test-Path $Target) {
    $Backup = "E:\HydrogenHttpd_backup_" + (Get-Date -Format "yyyyMMdd_HHmmss")
    Move-Item $Target $Backup
    Write-Host "Backup created: $Backup"
}

New-Item -ItemType Directory -Force -Path $Target | Out-Null
Copy-Item -Path ".\*" -Destination $Target -Recurse -Force
Write-Host "Project copied to $Target"

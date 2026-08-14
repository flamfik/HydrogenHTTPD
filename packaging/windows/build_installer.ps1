[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StageDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$StageDirectory = (Resolve-Path $StageDirectory).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path

$Nsis = Get-Command makensis.exe -ErrorAction SilentlyContinue
if (-not $Nsis) {
    throw "makensis.exe was not found. Install NSIS first."
}

$Script = Join-Path $PSScriptRoot "HydrogenHttpd.nsi"

& $Nsis.Source `
    "/DSTAGE_DIR=$StageDirectory" `
    "/DOUTPUT_DIR=$OutputDirectory" `
    $Script

if ($LASTEXITCODE -ne 0) {
    throw "NSIS build failed."
}

$Portable = Join-Path $OutputDirectory "HydrogenHttpd-1.9.0-Windows-x64-Portable.zip"
if (Test-Path $Portable) {
    Remove-Item $Portable -Force
}
Compress-Archive -Path (Join-Path $StageDirectory "*") -DestinationPath $Portable

Get-FileHash (Join-Path $OutputDirectory "HydrogenHttpd-1.9.0-Windows-x64-Setup.exe") -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash.ToLower())  HydrogenHttpd-1.9.0-Windows-x64-Setup.exe" } |
    Set-Content (Join-Path $OutputDirectory "HydrogenHttpd-1.9.0-Windows-x64-Setup.exe.sha256")

Get-FileHash $Portable -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash.ToLower())  HydrogenHttpd-1.9.0-Windows-x64-Portable.zip" } |
    Set-Content "$Portable.sha256"

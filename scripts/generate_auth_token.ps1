param(
    [string]$Name = "upload",
    [string]$Scopes = "upload",
    [int]$TokenBytes = 32,
    [int]$TtlDays = 0
)

$ErrorActionPreference = "Stop"

if ($TokenBytes -lt 24) { throw "TokenBytes must be at least 24." }
if ($TtlDays -lt 0) { throw "TtlDays cannot be negative." }
if ([string]::IsNullOrWhiteSpace($Name)) { throw "Name cannot be empty." }
if ([string]::IsNullOrWhiteSpace($Scopes)) { throw "Scopes cannot be empty." }

$bytes = New-Object byte[] $TokenBytes
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
try {
    $rng.GetBytes($bytes)
} finally {
    $rng.Dispose()
}
$token = [Convert]::ToBase64String($bytes).TrimEnd('=').Replace('+', '-').Replace('/', '_')

$sha = [System.Security.Cryptography.SHA256]::Create()
try {
    $hashBytes = $sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($token))
} finally {
    $sha.Dispose()
}
$hash = -join ($hashBytes | ForEach-Object { $_.ToString('x2') })

$expires = if ($TtlDays -eq 0) {
    "never"
} else {
    [DateTimeOffset]::UtcNow.AddDays($TtlDays).ToUnixTimeSeconds().ToString()
}

Write-Host "TOKEN (store securely; shown only now):"
Write-Host $token
Write-Host ""
Write-Host "SECRETS FILE ENTRY:"
Write-Host "token.$Name = sha256:$hash | $Scopes | $expires"

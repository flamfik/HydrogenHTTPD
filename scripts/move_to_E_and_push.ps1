param(
    [string]$RepoUrl = ""
)

$ErrorActionPreference = "Stop"
$Target = "E:\HydrogenHttpd"

Write-Host "Preparing target folder: $Target"

if (!(Test-Path "E:\")) {
    throw "Drive E: does not exist or is not mounted."
}

if (Test-Path $Target) {
    Write-Host "Existing E:\HydrogenHttpd found. Creating backup..."
    $Backup = "E:\HydrogenHttpd_backup_" + (Get-Date -Format "yyyyMMdd_HHmmss")
    Write-Host "Backup created: $Backup"
}


Set-Location $Target

if (!(Test-Path ".git")) {
    git init
}

git config user.name "flamfik"
git config user.email "arclite@o2.pl"

git add .
git commit -m "initial commit: HydrogenHttpd"

if ($LASTEXITCODE -ne 0) {
    Write-Host "No new files to commit or commit already exists."
}

git branch -M main

if ($RepoUrl -ne "") {
    $existing = git remote get-url origin 2>$null
    if ($LASTEXITCODE -eq 0) {
        git remote set-url origin $RepoUrl
    } else {
        git remote add origin $RepoUrl
    }

    git push -u origin main
    Write-Host "Pushed to GitHub: $RepoUrl"
} else {
    Write-Host ""
    Write-Host "Project moved and committed locally at: $Target"
    Write-Host "To push later:"
    Write-Host "  cd E:\HydrogenHttpd"
    Write-Host "  git remote add origin git@github.com:flamfik/hydrogenhttpd.git"
    Write-Host "  git push -u origin main"
}

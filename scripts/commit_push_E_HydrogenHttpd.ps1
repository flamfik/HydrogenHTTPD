param(
    [string]$Message = "update HydrogenHttpd project",
    [string]$ProjectPath = "E:\HydrogenHttpd",
    [string]$Branch = "main",
    [string]$RemoteName = "origin",
    [string]$RepoUrl = ""
)

$ErrorActionPreference = "Stop"

Write-Host "HydrogenHttpd commit-and-push script"
Write-Host "Project path: $ProjectPath"
Write-Host "Branch: $Branch"
Write-Host "Remote: $RemoteName"
Write-Host "Commit message: $Message"
Write-Host ""

if (!(Test-Path $ProjectPath)) {
    throw "Project folder does not exist: $ProjectPath"
}

Set-Location $ProjectPath

if (!(Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git is not installed or not available in PATH."
}

if (!(Test-Path ".git")) {
    Write-Host "No .git folder found. Initializing repository..."
    git init
}

git config user.name "flamfik"
git config user.email "arclite@o2.pl"

Write-Host "Switching branch to $Branch..."
git branch -M $Branch

if ($RepoUrl -ne "") {
    Write-Host "Configuring remote $RemoteName -> $RepoUrl"

    $existingRemote = git remote get-url $RemoteName 2>$null

    if ($LASTEXITCODE -eq 0) {
        git remote set-url $RemoteName $RepoUrl
    } else {
        git remote add $RemoteName $RepoUrl
    }
}

$remoteUrl = git remote get-url $RemoteName 2>$null

if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($remoteUrl)) {
    throw "Remote '$RemoteName' is not configured. Run with -RepoUrl, for example: .\commit_push_E_HydrogenHttpd.ps1 -RepoUrl `"git@github.com:flamfik/hydrogenhttpd.git`""
}

Write-Host ""
Write-Host "Remote URL:"
git remote get-url $RemoteName
Write-Host ""

Write-Host "Current git status:"
git status --short
Write-Host ""

git add .

$changes = git status --short

if ([string]::IsNullOrWhiteSpace($changes)) {
    Write-Host "No changes to commit. Pushing current branch anyway..."
} else {
    git commit -m $Message
}

Write-Host ""
Write-Host "Pushing to $RemoteName/$Branch..."
git push -u $RemoteName $Branch --force

Write-Host ""
Write-Host "Done."
Write-Host "Latest commit:"
git log -1 --oneline

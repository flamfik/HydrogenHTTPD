param(
    [string]$Message = "update HydrogenHttpd project",
    [string]$ProjectPath = "E:\HydrogenHttpd"
)

$ErrorActionPreference = "Stop"

Write-Host "HydrogenHttpd commit-only script"
Write-Host "Project path: $ProjectPath"
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
    git branch -M main
}

# Optional local identity for this repository.
# Change these values if you want a different author.
git config user.name "Bartosz Suchy"
git config user.email "post.apo@gmail.com"

Write-Host "Current git status:"
git status --short
Write-Host ""

git add .

$changes = git status --short

if ([string]::IsNullOrWhiteSpace($changes)) {
    Write-Host "No changes to commit."
    exit 0
}

git commit -m $Message

Write-Host ""
Write-Host "Commit created successfully."
Write-Host ""
Write-Host "Latest commit:"
git log -1 --oneline

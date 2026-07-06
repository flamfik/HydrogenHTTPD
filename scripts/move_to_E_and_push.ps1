param(
    [string]$RepoUrl = ""
)


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
    Write-Host "  git remote add origin git@github.com:flamfik/HydrogenHTTPD.git"
    Write-Host "  git push -u origin main"
}

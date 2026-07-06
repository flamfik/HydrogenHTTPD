# GitHub upload steps

The GitHub connector can commit to an existing repository, but no accessible `hydrogenhttpd` repository was found for the connected account `flamfik`.

## Recommended flow

1. Create an empty GitHub repository named:

```txt
hydrogenhttpd
```

2. Unzip this package.
3. Open PowerShell in the extracted folder.
4. Run:

```powershell
.\scripts\move_to_E_and_push.ps1 -RepoUrl "git@github.com:flamfik/hydrogenhttpd.git"
```

## Move to E: only

```powershell
.\scripts\move_to_E_only.ps1
```

## Manual GitHub push

```powershell
cd E:\HydrogenHttpd
git init
git add .
git commit -m "initial commit: HydrogenHttpd"
git branch -M main
git remote add origin git@github.com:flamfik/hydrogenhttpd.git
git push -u origin main
```

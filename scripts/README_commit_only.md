# HydrogenHttpd commit-only script

Ten skrypt wykonuje wyłącznie lokalny commit z folderu:

```txt
E:\HydrogenHttpd
```

Nie robi `git push`.

## Użycie

Otwórz PowerShell i uruchom:

```powershell
.\commit_E_HydrogenHttpd.ps1
```

Z własnym komunikatem commita:

```powershell
.\commit_E_HydrogenHttpd.ps1 -Message "feat: update HydrogenHttpd modules"
```

Z inną ścieżką projektu:

```powershell
.\commit_E_HydrogenHttpd.ps1 -ProjectPath "E:\HydrogenHttpd"
```

## Co robi skrypt

- przechodzi do `E:\HydrogenHttpd`,
- sprawdza, czy istnieje repo `.git`,
- jeśli nie istnieje, wykonuje `git init`,
- ustawia lokalnie autora commita,
- wykonuje `git add .`,
- jeśli są zmiany, wykonuje `git commit`,
- nie wykonuje `git push`.

#!/usr/bin/env bash
set -euo pipefail

git init
git add .
git commit -m "initial commit: HydrogenHttpd"
git branch -M main

echo "Repository initialized."
echo "Create an empty GitHub repository, then run:"
echo "git remote add origin git@github.com:YOUR_USERNAME/hydrogenhttpd.git"
echo "git push -u origin main"

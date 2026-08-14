#!/bin/bash
set -euo pipefail

PURGE=0
if [[ "${1:-}" == "--purge" ]]; then
    PURGE=1
fi

if [[ "$(id -u)" -ne 0 ]]; then
    echo "Run this uninstaller with sudo." >&2
    exit 1
fi

launchctl bootout system/com.hydrogenhttpd.server >/dev/null 2>&1 || true
rm -f /Library/LaunchDaemons/com.hydrogenhttpd.server.plist
rm -f /usr/local/bin/hydrogen_httpd
rm -rf /usr/local/share/hydrogenhttpd

if [[ "$PURGE" -eq 1 ]]; then
    rm -rf "/Library/Application Support/HydrogenHttpd"
    rm -rf "/Library/Logs/HydrogenHttpd"
    dscl . -delete /Users/_hydrogenhttpd >/dev/null 2>&1 || true
    dscl . -delete /Groups/_hydrogenhttpd >/dev/null 2>&1 || true
fi

echo "HydrogenHttpd removed."
[[ "$PURGE" -eq 0 ]] && echo "Configuration and runtime data were preserved."

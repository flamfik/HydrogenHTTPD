#!/bin/bash
set -euo pipefail

if [[ "$(id -u)" -ne 0 ]]; then
    echo "Run this installer with sudo." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PACKAGE_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
DATA_ROOT="/Library/Application Support/HydrogenHttpd"
LOG_ROOT="/Library/Logs/HydrogenHttpd"
PLIST="/Library/LaunchDaemons/com.hydrogenhttpd.server.plist"

create_service_account() {
    local gid
    if dscl . -read /Groups/_hydrogenhttpd PrimaryGroupID >/dev/null 2>&1; then
        gid=$(dscl . -read /Groups/_hydrogenhttpd PrimaryGroupID | awk '{print $2}')
    else
        gid=399
        while dscl . -search /Groups PrimaryGroupID "$gid" 2>/dev/null | grep -q .; do
            gid=$((gid - 1))
        done
        dscl . -create /Groups/_hydrogenhttpd
        dscl . -create /Groups/_hydrogenhttpd PrimaryGroupID "$gid"
        dscl . -create /Groups/_hydrogenhttpd Password '*'
    fi

    if ! dscl . -read /Users/_hydrogenhttpd >/dev/null 2>&1; then
        local uid=399
        while dscl . -search /Users UniqueID "$uid" 2>/dev/null | grep -q .; do
            uid=$((uid - 1))
        done
        dscl . -create /Users/_hydrogenhttpd
        dscl . -create /Users/_hydrogenhttpd UniqueID "$uid"
        dscl . -create /Users/_hydrogenhttpd PrimaryGroupID "$gid"
        dscl . -create /Users/_hydrogenhttpd UserShell /usr/bin/false
        dscl . -create /Users/_hydrogenhttpd NFSHomeDirectory "$DATA_ROOT"
        dscl . -create /Users/_hydrogenhttpd RealName "HydrogenHttpd Service"
        dscl . -create /Users/_hydrogenhttpd Password '*'
    fi
}

launchctl bootout system/com.hydrogenhttpd.server >/dev/null 2>&1 || true
create_service_account

if [[ "$PACKAGE_ROOT" != "/usr/local" ]]; then
    install -m 0755 "$PACKAGE_ROOT/bin/hydrogen_httpd" /usr/local/bin/hydrogen_httpd
    mkdir -p /usr/local/share/hydrogenhttpd
    cp -a "$PACKAGE_ROOT/share/hydrogenhttpd/." /usr/local/share/hydrogenhttpd/
fi

install -d -m 0750 "$DATA_ROOT"
install -d -m 0750 "$DATA_ROOT"/{www,uploads,tmp/uploads,sql,certs,secrets}
install -d -m 0750 "$LOG_ROOT"

if [[ ! -f "$DATA_ROOT/server.conf" ]]; then
    install -m 0640 "$PACKAGE_ROOT/share/hydrogenhttpd/config/server.conf" \
        "$DATA_ROOT/server.conf"
fi

if [[ ! -f "$DATA_ROOT/secrets/auth.tokens" ]]; then
    install -m 0640 /dev/null "$DATA_ROOT/secrets/auth.tokens"
fi

if [[ ! -f "$DATA_ROOT/www/index.html" ]]; then
    cp -a "$PACKAGE_ROOT/share/hydrogenhttpd/www/." "$DATA_ROOT/www/"
fi

install -m 0644 "$PACKAGE_ROOT/share/hydrogenhttpd/macos/com.hydrogenhttpd.server.plist" \
    "$PLIST"

chown -R _hydrogenhttpd:_hydrogenhttpd "$DATA_ROOT" "$LOG_ROOT"
chown root:wheel /usr/local/bin/hydrogen_httpd "$PLIST"
chmod 0755 /usr/local/bin/hydrogen_httpd
chmod 0644 "$PLIST"

/usr/local/bin/hydrogen_httpd --check-config --config "$DATA_ROOT/server.conf"
launchctl bootstrap system "$PLIST"
launchctl enable system/com.hydrogenhttpd.server

echo "HydrogenHttpd installed."
echo "Configuration: $DATA_ROOT/server.conf"

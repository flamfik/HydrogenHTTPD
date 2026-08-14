#!/usr/bin/env sh
set -eu

PURGE=0
if [ "${1:-}" = "--purge" ]; then
    PURGE=1
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this uninstaller as root." >&2
    exit 1
fi

systemctl disable --now hydrogenhttpd.service >/dev/null 2>&1 || true
rm -f /etc/systemd/system/hydrogenhttpd.service
rm -f /etc/logrotate.d/hydrogenhttpd
rm -f /usr/local/bin/hydrogen_httpd
rm -rf /usr/local/share/hydrogenhttpd
systemctl daemon-reload >/dev/null 2>&1 || true

if [ "$PURGE" -eq 1 ]; then
    rm -rf /etc/hydrogenhttpd /var/lib/hydrogenhttpd /var/log/hydrogenhttpd
    userdel hydrogenhttpd >/dev/null 2>&1 || true
    groupdel hydrogenhttpd >/dev/null 2>&1 || true
fi

echo "HydrogenHttpd removed."
[ "$PURGE" -eq 0 ] && echo "Configuration and runtime data were preserved."

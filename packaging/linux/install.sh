#!/usr/bin/env sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this installer as root." >&2
    exit 1
fi

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACKAGE_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)

install -m 0755 "$PACKAGE_ROOT/bin/hydrogen_httpd" /usr/local/bin/hydrogen_httpd
install -d /usr/local/share/hydrogenhttpd
cp -a "$PACKAGE_ROOT/share/hydrogenhttpd/." /usr/local/share/hydrogenhttpd/

if ! getent group hydrogenhttpd >/dev/null 2>&1; then
    groupadd --system hydrogenhttpd
fi
if ! getent passwd hydrogenhttpd >/dev/null 2>&1; then
    useradd --system --gid hydrogenhttpd --home-dir /var/lib/hydrogenhttpd \
        --shell /usr/sbin/nologin hydrogenhttpd
fi

install -d -m 0750 -o root -g hydrogenhttpd /etc/hydrogenhttpd/{certs,secrets}
install -d -m 0750 -o hydrogenhttpd -g hydrogenhttpd \
    /var/lib/hydrogenhttpd/{www,uploads,tmp/uploads,sql} \
    /var/log/hydrogenhttpd

if [ ! -f /etc/hydrogenhttpd/server.conf ]; then
    install -m 0640 -o root -g hydrogenhttpd \
        "$PACKAGE_ROOT/share/hydrogenhttpd/config/server.conf" \
        /etc/hydrogenhttpd/server.conf
fi

if [ ! -f /etc/hydrogenhttpd/secrets/auth.tokens ]; then
    install -m 0640 -o root -g hydrogenhttpd /dev/null \
        /etc/hydrogenhttpd/secrets/auth.tokens
fi

if [ ! -f /var/lib/hydrogenhttpd/www/index.html ]; then
    cp -a "$PACKAGE_ROOT/share/hydrogenhttpd/www/." /var/lib/hydrogenhttpd/www/
    chown -R hydrogenhttpd:hydrogenhttpd /var/lib/hydrogenhttpd/www
fi

install -m 0644 "$PACKAGE_ROOT/lib/systemd/system/hydrogenhttpd.service" \
    /etc/systemd/system/hydrogenhttpd.service
install -m 0644 "$PACKAGE_ROOT/etc/logrotate.d/hydrogenhttpd" \
    /etc/logrotate.d/hydrogenhttpd

systemctl daemon-reload
systemctl enable hydrogenhttpd.service
/usr/local/bin/hydrogen_httpd --check-config --config /etc/hydrogenhttpd/server.conf
systemctl restart hydrogenhttpd.service

echo "HydrogenHttpd installed."
echo "Configuration: /etc/hydrogenhttpd/server.conf"

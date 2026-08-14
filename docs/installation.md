# HydrogenHttpd v1.9.0 Installation

HydrogenHttpd provides native installation workflows for Windows, Linux and macOS.

## Default installation posture

The installed configuration starts with:

- HTTP on port `8080`,
- TLS disabled,
- uploads disabled,
- admin endpoint disabled,
- endpoint authorization disabled,
- server banner hidden,
- static cache and asynchronous access logging enabled.

This makes first startup predictable while avoiding deployment with placeholder certificates or tokens.

---

## Windows

### Native installer

Run:

```powershell
HydrogenHttpd-1.9.0-Windows-x64-Setup.exe
```

The installer:

- installs binaries under `C:\Program Files\HydrogenHttpd`,
- creates runtime data under `C:\ProgramData\HydrogenHttpd`,
- validates the configuration,
- registers the `HydrogenHttpd` Windows service,
- runs the service as `LocalService`,
- grants `LocalService` write access only to runtime data,
- starts the service automatically.

Configuration:

```text
C:\ProgramData\HydrogenHttpd\server.conf
```

Service commands:

```powershell
Get-Service HydrogenHttpd
Restart-Service HydrogenHttpd
Stop-Service HydrogenHttpd
Start-Service HydrogenHttpd
```

Configuration test:

```powershell
& "C:\Program Files\HydrogenHttpd\bin\hydrogen_httpd.exe" `
  --check-config `
  --config "C:\ProgramData\HydrogenHttpd\server.conf"
```

Standard uninstallation preserves `C:\ProgramData\HydrogenHttpd`.

### Portable ZIP

Extract the portable ZIP and run the bundled elevated script:

```powershell
.\share\hydrogenhttpd\windows\install-service.ps1 `
  -InstallRoot $PWD
```

---

## Linux

### Debian/Ubuntu package

```bash
sudo apt install ./hydrogenhttpd_1.9.0_amd64.deb
```

The package:

- creates the `hydrogenhttpd` system user,
- installs a hardened systemd service,
- installs configuration in `/etc/hydrogenhttpd`,
- stores website and databases in `/var/lib/hydrogenhttpd`,
- stores logs in `/var/log/hydrogenhttpd`,
- enables and starts the service.

Configuration:

```text
/etc/hydrogenhttpd/server.conf
```

Service commands:

```bash
sudo systemctl status hydrogenhttpd
sudo systemctl restart hydrogenhttpd
sudo journalctl -u hydrogenhttpd
```

Configuration test:

```bash
sudo -u hydrogenhttpd \
  /usr/bin/hydrogen_httpd \
  --check-config \
  --config /etc/hydrogenhttpd/server.conf
```

### Portable TGZ

Extract the archive and run:

```bash
sudo ./libexec/hydrogenhttpd/install.sh
```

Uninstall while preserving data:

```bash
sudo /usr/local/libexec/hydrogenhttpd/uninstall.sh
```

A full data purge requires:

```bash
sudo uninstall.sh --purge
```

---

## macOS

### Native PKG

```bash
sudo installer \
  -pkg HydrogenHttpd-1.9.0-macOS-arm64.pkg \
  -target /
```

The package:

- installs the binary in `/usr/local/bin`,
- creates the `_hydrogenhttpd` service account,
- stores data in `/Library/Application Support/HydrogenHttpd`,
- stores logs in `/Library/Logs/HydrogenHttpd`,
- installs `com.hydrogenhttpd.server` as a LaunchDaemon,
- validates the configuration before launch.

Configuration:

```text
/Library/Application Support/HydrogenHttpd/server.conf
```

Service commands:

```bash
sudo launchctl print system/com.hydrogenhttpd.server
sudo launchctl kickstart -k system/com.hydrogenhttpd.server
```

Configuration test:

```bash
sudo -u _hydrogenhttpd \
  /usr/local/bin/hydrogen_httpd \
  --check-config \
  --config "/Library/Application Support/HydrogenHttpd/server.conf"
```

Unsigned development packages may trigger Gatekeeper warnings. Public distribution should use Developer ID Installer signing and Apple notarization.

---

## Enabling TLS

Place certificates outside the public document root, update:

```ini
enable_tls = true
tls_cert_file = certs/fullchain.pem
tls_key_file = certs/privkey.pem
force_https = true
```

Then validate configuration before restarting the service.

## Enabling protected endpoints

Generate scoped tokens using:

```bash
python3 generate_auth_token.py --name admin --scopes upload,admin --ttl-days 30
```

Put the generated hash entry in the platform's `secrets/auth.tokens` file, then enable:

```ini
enable_endpoint_auth = true
enable_admin_status = true
```

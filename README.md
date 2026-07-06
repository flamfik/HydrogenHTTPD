# HydrogenHttpd

**HydrogenHttpd** is an experimental lightweight HTTP/HTTPS server written in C++20.

It is designed as a learning-oriented, security-conscious mini web server with modular features inspired by larger servers such as Apache HTTP Server, while intentionally keeping the implementation small and conservative.

> Status: experimental / MVP. Not intended as a production replacement for Apache, nginx, Caddy, or other hardened production servers.

## Features

- HTTP/1.0 and HTTP/1.1 request handling
- Static file serving
- Optional HTTPS/TLS via OpenSSL
- Worker pool instead of thread-per-connection
- Rate limiting per client
- Read timeouts for slow-client protection
- Path traversal protection
- Safe default security headers
- Virtual hosts by `Host` header
- Limited `.htaccess` support
- Optional PHP support via FastCGI / PHP-FPM
- Optional SQLite module
- CTest-based unit and integration tests
- GitHub Actions CI

## Security-first design choices

HydrogenHttpd intentionally avoids several dangerous features by default:

- no shell execution for PHP
- no direct `php-cgi` spawning
- no arbitrary SQL-over-HTTP endpoint
- no CGI execution
- no dynamic module loading
- no `.htaccess` script handlers
- no automatic directory listing
- no public serving of `.htaccess`

PHP is delegated to PHP-FPM through FastCGI. SQL is exposed only as an internal module with prepared-statement examples.

## Supported `.htaccess` subset

HydrogenHttpd supports only a small whitelist:

```apache
Require all denied
Require all granted
Options -Indexes
Header set X-Custom-Header value
Header set X-Custom-Header "quoted value"
```

Unsupported by design:

- `RewriteRule`
- `RewriteCond`
- `AddHandler`
- `SetHandler`
- `ProxyPass`
- CGI
- PHP handler configuration
- script execution

## Requirements

Core build:

- C++20 compiler
- CMake 3.20+
- Boost.System

Optional:

- OpenSSL for TLS
- SQLite3 for the SQL module
- Python 3 for integration tests
- OpenSSL CLI for HTTPS integration test cert generation
- PHP-FPM for runtime PHP support

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

### Build without TLS

```bash
cmake .. -DHYDROGENHTTPD_ENABLE_TLS=OFF
cmake --build .
```

### Build without SQLite

```bash
cmake .. -DHYDROGENHTTPD_ENABLE_SQLITE=OFF
cmake --build .
```

## Run

```bash
./hydrogen_httpd ../server.conf
```

Windows:

```powershell
.\Release\hydrogen_httpd.exe ..\server.conf
```

Then open:

```txt
http://localhost:8080/
```

## Enable HTTPS

Generate a local development certificate:

```bash
bash scripts_generate_self_signed_cert.sh
```

Set in `server.conf`:

```ini
enable_tls = true
```

Run:

```bash
./hydrogen_httpd ../server.conf
```

Test:

```bash
curl -k -i https://localhost:8443/
```

## Enable PHP through PHP-FPM

Start PHP-FPM, for example with Docker:

```bash
docker run --rm -p 9000:9000 -v "$PWD/../www:/var/www/html" php:8.3-fpm
```

Set in `server.conf`:

```ini
enable_php = true
php_fastcgi_host = 127.0.0.1
php_fastcgi_port = 9000
```

Test:

```bash
curl -i http://localhost:8080/info.php
```

## Enable SQLite module

Set in `server.conf`:

```ini
enable_sql = true
sqlite_database = sql/hydrogen.db
```

The SQL module is internal. It does not expose arbitrary SQL execution over HTTP.

## Configuration overview

See [`docs/configuration.md`](docs/configuration.md).

## Security policy

See [`SECURITY.md`](SECURITY.md).

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Roadmap

See [`docs/roadmap.md`](docs/roadmap.md).

## License

MIT License. See [`LICENSE`](LICENSE).

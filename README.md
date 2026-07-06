# HydrogenHttpd

**HydrogenHttpd** is an experimental lightweight HTTP/HTTPS server written in C++20.

Current package: **v0.10 GCC build fixes**.

It is designed as a learning-oriented, security-conscious mini web server with modular features inspired by larger servers such as Apache HTTP Server, while intentionally keeping the implementation small and conservative.

> Status: experimental / MVP. Not intended as a production replacement for Apache, nginx, Caddy, or other hardened production servers.

## v1.5.88 hardening status

This version is a **production-hardening baseline**. It is more defensive than the earlier MVP, but it is not a drop-in replacement for Apache/nginx/Caddy without independent audit and operational testing. See [`docs/production-hardening.md`](docs/production-hardening.md).

## Features

- HTTP/1.0 and HTTP/1.1 request handling
- Static file serving
- Optional HTTPS/TLS via OpenSSL
- Worker pool instead of thread-per-connection
- Rate limiting per client
- Read timeouts for slow-client protection
- Path traversal protection
- Safe default security headers
- Strict HTTP parser with header/URI/method limits
- Hidden and sensitive file blocking
- Optional HTTP-to-HTTPS redirect
- Server header disabled by default
- Virtual hosts by `Host` header
- Limited `.htaccess` support
- Optional PHP support via FastCGI / PHP-FPM
- Optional SQLite module
- CTest-based unit and integration tests
- GitHub Actions CI






## v1.5.88 scoped tokens and audit log

HydrogenHttpd v1.5.88 adds scoped endpoint authorization and audit logging.

### Scoped tokens

Recommended format:

```ini
enable_endpoint_auth = true

auth_token.upload = change-upload-token | upload
auth_token.admin = change-admin-token | upload,admin
```

The old single-token format is still supported for compatibility:

```ini
auth_bearer_token = legacy-token
```

The legacy token is treated as a broad upload/admin token. New deployments should use `auth_token.*`.

### Available baseline scopes

```txt
upload
admin
*
```

At this stage the upload endpoint requires:

```txt
upload
```

### Audit log

Protected endpoint authorization attempts are written to:

```ini
audit_log = logs/audit.log
```

Logged events include:

```txt
auth_missing
auth_denied
auth_allowed
```

Example:

```txt
event=auth_denied ip=127.0.0.1 target="/__hydrogen/upload" required_scope=upload token="adminonly" status=403
```

### Tests

```bash
ctest --test-dir build -R integration_upload_auth_tests --output-on-failure
```

## Streaming uploads and endpoint authorization

HydrogenHttpd v1.5.88 adds two hardening steps:

### v0.15 streaming upload spool

Upload request bodies are no longer buffered as one large in-memory string for the upload endpoint.

Flow:

1. Read request headers.
2. Detect protected upload endpoint.
3. Stream request body to `upload_spool_directory`.
4. Parse multipart data from the spool file.
5. Write accepted files to `upload_directory`.
6. Remove the spool file after processing.

Configuration:

```ini
upload_spool_directory = tmp/uploads
```

### v1.5.88 endpoint authorization

Protected internal endpoints such as uploads can require a bearer token:

```ini
enable_endpoint_auth = true
auth_bearer_token = change-this-token
```

Request:

```http
Authorization: Bearer change-this-token
```

Unauthorized requests return:

```txt
401 Unauthorized
```

Run the new auth test:

```bash
ctest --test-dir build -R integration_upload_auth_tests --output-on-failure
```

## Multipart upload baseline

HydrogenHttpd v1.5.88 adds a guarded `multipart/form-data` upload module.

The upload endpoint is disabled by default:

```ini
enable_uploads = false
```

Enable explicitly:

```ini
enable_uploads = true
upload_endpoint = /__hydrogen/upload
upload_directory = uploads
max_multipart_parts = 16
max_upload_file_bytes = 1048576
max_upload_field_bytes = 16384
```

Security design:

- uploads are written to `upload_directory`,
- uploads are not automatically served as static files,
- dangerous extensions such as `.php`, `.phtml`, `.cgi`, `.sh`, `.exe`, `.dll`, `.conf`, `.ini`, `.db`, `.key`, `.pem` are blocked,
- uploaded filenames are sanitized,
- path separators are stripped from uploaded filenames,
- multipart part count is limited,
- file size and field size are limited,
- endpoint returns JSON metadata only.

Run upload tests:

```bash
ctest --test-dir build -R integration_upload_tests --output-on-failure
```

## Safe POST/body baseline

HydrogenHttpd v1.5.88 adds safe request-body handling for `Content-Length` requests.

Design choices:

- `Transfer-Encoding` is still rejected with `501 Not Implemented`.
- Bodies are accepted only through explicit `Content-Length`.
- `max_body_bytes` limits accepted body size.
- `GET`/`HEAD` with a non-empty body are rejected.
- `POST` to static files returns `405 Method Not Allowed`.
- `POST` to PHP is supported through FastCGI/PHP-FPM when `enable_php = true`.
- PHP source disclosure remains blocked when PHP is disabled.

Configuration:

```ini
max_body_bytes = 1048576
```

Run the POST/body test only:

```bash
ctest --test-dir build -R integration_post_body_tests --output-on-failure
```

## Exploit regression suite

HydrogenHttpd includes a defensive regression suite inspired by public httpd vulnerability classes.

The suite validates that the server rejects or safely handles:

- plain and encoded path traversal,
- double-encoded traversal markers,
- encoded slash/backslash separator tricks,
- hidden files such as `.env`, `.git/config`, `.htaccess`,
- sensitive extensions such as `.key`, `.pem`, `.db`, `.conf`, `.ini`, `.log`,
- symlink escapes outside document root,
- PHP source disclosure when PHP module is disabled,
- duplicate or missing `Host`,
- `Transfer-Encoding` before chunked support exists,
- request body before safe body handling exists,
- overly long URI/header lines,
- too many headers,
- obsolete folded headers,
- unsafe methods such as `TRACE` and `CONNECT`.

Run:

```bash
ctest --test-dir build -R exploit_regression_tests --output-on-failure
```

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

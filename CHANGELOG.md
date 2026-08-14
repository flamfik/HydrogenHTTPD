# Changelog

## v1.9.0 Cross-Platform Installer Edition

- Added configuration-relative filesystem path resolution.
- Added shared application launcher with `--config`, `--check-config`, `--version` and `--help`.
- Added graceful SIGINT/SIGTERM handling on Linux and macOS.
- Added native Windows Service integration using the Windows Service Control Manager.
- Added elevated PowerShell service installation and uninstallation scripts.
- Added NSIS Windows setup definition and portable ZIP builder.
- Added hardened systemd unit, tmpfiles and logrotate integration.
- Added Debian DEB packaging with lifecycle scripts.
- Added RPM packaging scripts and Rocky Linux build workflow.
- Added portable Linux TGZ builder.
- Added macOS LaunchDaemon, dedicated service-user creation and native PKG builder.
- Added cross-platform GitHub Actions installer workflow.
- Added platform-specific installed configuration templates.
- Default installed webroot now contains only `index.html`.
- Added installation and installer-building documentation.
- Verified 21/21 tests with TLS/SQLite.
- Verified 19/19 tests without TLS/SQLite.


## v1.8.0 High Load Baseline

- Added sharded `StaticFileCache` with concurrent read locks.
- Added bounded entry, byte and file-size cache limits.
- Added configurable cache revalidation interval.
- Added ETag generation and `If-None-Match` / `304 Not Modified` support.
- Changed static response writes to separate header/body buffers to avoid rebuilding a second full response string.
- Added asynchronous bounded access logging with batch flush.
- Added access-log sampling, disable switch and dropped-line metric.
- Added sharded rate limiter and idle-bucket cleanup.
- Added optional local rate-limiter disable switch for trusted upstream deployments.
- Added automatic worker sizing with `worker_threads = 0`.
- Added configurable `io_threads`, listen backlog and socket options.
- Added thread-pool active/completed/rejected metrics.
- Added static-cache and logger metrics to admin status.
- Added `server.high-load.conf`.
- Added benchmark scripts and high-load documentation.
- Added static cache unit test and ETag integration regression.
- Verified 21/21 tests with TLS/SQLite and 19/19 without TLS/SQLite.

## v1.7.0 Runtime Security Operations

- Added thread-safe `AuthRuntimeStore`.
- Added hot reload for `auth_secrets_file`.
- Added immediate token revocation without server restart.
- Added last-known-good behavior when a changed secrets file is malformed.
- Added file removal handling that revokes all file-backed tokens.
- Added `auth_hot_reload`.
- Added `auth_reload_interval_seconds`.
- Added authentication store generation and reload timestamp to admin status.
- Added `auth_store_reloaded` and `auth_store_reload_failed` audit events.
- Added internal audit log rotation without restart.
- Added `audit_rotate_bytes`.
- Added `audit_rotate_keep`.
- Added atomic `scripts/manage_auth_tokens.py` rotation/revocation tool.
- Added `auth_runtime_store_tests`.
- Added `logger_tests`.
- Added `integration_auth_hot_reload_tests`.
- Verified 20 tests with TLS/SQLite and 18 tests without TLS/SQLite.

## v1.6.0 Security Operations

- Added internal SHA-256 implementation independent of TLS/OpenSSL.
- Added constant-time comparison for plaintext compatibility tokens and token hashes.
- Added separate hashed token store through `auth_secrets_file`.
- Added token rule format with scopes and optional Unix-epoch expiration.
- Added distinct authorization outcomes:
  - missing,
  - invalid,
  - expired,
  - insufficient scope,
  - allowed.
- Added audit events:
  - `auth_missing`,
  - `auth_invalid`,
  - `auth_expired`,
  - `auth_scope_denied`,
  - `auth_allowed`.
- Added protected `/__hydrogen/admin/status` endpoint requiring `admin` scope.
- Added non-secret status data: uptime, worker count, queue depth and enabled modules.
- Added Python and PowerShell token generators.
- Added `secrets/auth.tokens.example` and Git ignore rules for real token stores.
- Added `auth_tests` and `integration_admin_status_tests`.
- Verified 17/17 tests with TLS + SQLite and 15/15 without TLS/SQLite.

## v1.5.88 Scoped token authorization and audit log

- Added scoped bearer tokens:
  - `auth_token.<name> = <secret> | <scope1,scope2>`
- Added baseline scopes:
  - `upload`
  - `admin`
  - `*`
- Upload endpoint now requires the `upload` scope.
- Kept backward compatibility with `auth_bearer_token`.
- Added `audit_log = logs/audit.log`.
- Added audit events for protected endpoint authorization:
  - `auth_missing`
  - `auth_denied`
  - `auth_allowed`
- Added audit assertions to `integration_upload_auth_tests`.
- Added test for a token that is valid but lacks the required `upload` scope.

## v0.16 Endpoint authorization baseline

- Added protected endpoint authorization using `Authorization: Bearer <token>`.
- Added `enable_endpoint_auth`.
- Added `auth_bearer_token`.
- Applied endpoint auth to upload endpoint.
- Added `integration_upload_auth_tests`.

## v0.15 Streaming upload spool baseline

- Upload endpoint no longer buffers the full body in memory.
- Added header-first routing for the upload endpoint.
- Added streaming upload body copy to spool file.
- Added `upload_spool_directory`.
- Added multipart parsing from spool file.
- Added spool cleanup after successful or failed upload handling.
- Added tests for spool parser and authorized streaming uploads.

## v0.14 Multipart upload baseline

- Added `Multipart` parser module.
- Added guarded `multipart/form-data` upload endpoint.
- Added upload configuration:
  - `enable_uploads`
  - `upload_endpoint`
  - `upload_directory`
  - `max_multipart_parts`
  - `max_upload_file_bytes`
  - `max_upload_field_bytes`
- Added filename sanitization.
- Added dangerous upload extension blocking.
- Added upload storage isolation from document root.
- Added `multipart_tests`.
- Added `integration_upload_tests`.
- Kept upload module disabled by default.

## v0.13 Safe POST/body baseline

- Added bounded request body reading for `Content-Length`.
- Added `max_body_bytes` configuration.
- Added safe `POST` handling baseline.
- Added `integration_post_body_tests`.
- Added FastCGI `STDIN` forwarding for PHP request bodies.
- Added FastCGI `CONTENT_LENGTH` and `CONTENT_TYPE` handling.
- Kept `Transfer-Encoding` / chunked requests rejected until fully implemented.
- Kept PHP source disclosure protection when PHP is disabled.
- Fixed strict parser blank-line handling.
- Preserved exploit regression suite from v0.12.

## v0.12 Exploit regression suite

- Added `integration_exploit_regression_tests`.
- Added CVE/Exploit-DB-inspired defensive regression tests.
- Added double-encoded traversal rejection.
- Added encoded slash/backslash traversal rejection.
- Added PHP source disclosure protection when PHP module is disabled.
- Added hidden/sensitive file regression tests.
- Added symlink escape regression test.
- Added parser ambiguity regression tests.
- Added `.well-known` allow-list regression test.

## v0.11 production-hardening baseline

- Added strict HTTP parser.
- Added header count, header line, URI and method limits.
- Added duplicate Host rejection and HTTP/1.1 Host requirement.
- Added Transfer-Encoding rejection until body handling is implemented.
- Added non-zero request body rejection until body handling is implemented.
- Added hidden-file and sensitive-file blocking.
- Added optional HTTP-to-HTTPS redirect.
- Disabled Server header by default.
- Corrected HEAD Content-Length behavior.
- Added production profile `server.production.conf`.
- Added production hardening documentation.
- Updated security headers.

## v0.10 GCC build fixes

- Fixed CMake Boost discovery on newer GCC/CMake/Boost setups.
- Removed hard dependency on `Boost::system` CMake component.
- Enabled header-only Boost.System via `BOOST_ERROR_CODE_HEADER_ONLY`.
- Fixed worker pool task submission for move-only `boost::asio::ip::tcp::socket`.
- Replaced move-only lambda capture inside `std::function` with `std::shared_ptr<tcp::socket>`.
- Verified GCC build with TLS + SQLite enabled.
- Verified GCC build with TLS and SQLite disabled.
- Verified all CTest tests pass.

## v0.9 GitHub-ready

- Added GitHub Actions CI.
- Added `.gitignore`.
- Added MIT license.
- Added issue templates.
- Added PR template.
- Added `CONTRIBUTING.md`.
- Added `SECURITY.md`.
- Added documentation in `docs/`.
- Improved public README.

## v0.8 PHP and SQL modules

- Added PHP FastCGI/PHP-FPM module.
- Added optional SQLite module.
- Added sample `info.php`.
- Added SQL schema example.

## v0.7 HTTPS tests and .htaccess

- Added HTTPS integration test.
- Added limited `.htaccess` support.

## v0.6 TLS worker pool

- Reintegrated TLS with worker pool.

## v0.5 Worker pool

- Renamed project to HydrogenHttpd.
- Added fixed worker pool.

## Earlier versions

- Basic HTTP server.
- Security tests.
- TLS support.
- Timeouts.
- Integration tests.

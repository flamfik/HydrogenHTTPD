# Changelog

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

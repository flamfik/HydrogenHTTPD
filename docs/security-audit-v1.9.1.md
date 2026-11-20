# HydrogenHttpd v1.9.1 — Focused Security Audit

## Scope

This is a source-level review of the HydrogenHttpd codebase, using Apache HTTP Server,
PHP and NGINX security advisories as references for vulnerability classes. It is not
a claim that every CVE in those products applies to HydrogenHttpd, nor a certification
that this server is immune to AI-assisted attacks.

## Changes in this hardening patch

- Require exactly one non-empty Host field for HTTP/1.1.
- Reject every duplicate Content-Length field, even if values match.
- Continue to reject Transfer-Encoding because chunked request framing is not supported.
- Add regression tests for empty Host, duplicate and conflicting Content-Length, and
  Transfer-Encoding combined with Content-Length.
- Reduce the shipped PHP FastCGI smoke-test endpoint to a fixed response, avoiding
  disclosure of PHP_VERSION.
- Document PHP-FPM isolation, least privilege, supported PHP branches, and production
  error-display settings.

## Relevant upstream vulnerability classes

### Apache HTTP Server

The Apache security tracker includes vulnerabilities in HTTP/2 memory management,
proxy modules, request/response smuggling and path handling. HydrogenHttpd does not
implement Apache modules such as mod_http2, mod_proxy_uwsgi, mod_dav or mod_rewrite;
their CVEs are not directly transferable. Their broader lessons—strict parsing,
bounded allocation, trust boundaries and regression testing—are relevant.

### PHP

PHP's security posture depends on the exact runtime branch, SAPI, extensions and
application code. HydrogenHttpd delegates PHP execution to PHP-FPM over FastCGI.
Operators must use a supported PHP branch, keep FPM private, isolate its pool user,
and bound workers, memory and execution time. The web server cannot repair unsafe
PHP application code or unsafe extensions.

### NGINX

NGINX advisories cover HTTP/2 and HTTP/3 memory safety, alias/rewrite path handling,
proxy parsing and request-body processing. HydrogenHttpd currently does not implement
HTTP/2, HTTP/3, NGINX's alias/rewrite directives, mail proxy or its dynamic module
system. The transferable controls are parser differential resistance, path confinement,
bounded body processing and denial-of-service limits.

## Remaining high-priority work

1. Fuzz StrictHttpParser, Security::resolveSafePath and multipart parsing with
   libFuzzer/AFL++ and ASan/UBSan.
2. Bound FastCGI response bytes and enforce real connect/write/read deadlines.
3. Audit all upload error paths for partial-file cleanup and spool quotas.
4. Reduce filesystem TOCTOU risk using descriptor-relative opens and no-follow
   semantics where supported.
5. Add request-rate and concurrent-upload quotas per authenticated principal.
6. Review TLS protocol/cipher defaults and certificate reload behavior.
7. Run external penetration testing and a platform matrix (Linux, Windows, macOS).

## Validation

The v1.9.1 changes were built with GCC, TLS and SQLite enabled, with warnings treated
as errors. All 21 CTest tests passed in the local Linux environment. Windows and macOS
native builds and an external security assessment remain outstanding.

## Meaning of “AI-resistant”

No defensible security claim can guarantee resistance to every AI-generated payload.
The practical objective is to make automated discovery and exploitation harder through
strict protocol handling, memory safety instrumentation, fuzzing, resource budgets,
least privilege, observability and fast patch turnaround.

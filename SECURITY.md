# Security Policy

HydrogenHttpd is experimental software and should not be used as a production-facing web server without an independent security review.

## Supported versions

Only the latest version in the main branch is considered for security fixes.

## Reporting a vulnerability

Please report vulnerabilities privately. Do not open a public issue with exploit details.

Suggested report contents:

- affected version or commit,
- operating system,
- build options,
- configuration,
- reproduction steps,
- expected behavior,
- actual behavior,
- impact assessment.

## Security model

HydrogenHttpd aims to reduce attack surface by:

- using a worker pool,
- limiting request size,
- applying read timeouts,
- preventing path traversal,
- disabling directory listing,
- blocking direct `.htaccess` access,
- using PHP-FPM instead of shell execution,
- avoiding arbitrary SQL execution from HTTP requests,
- sending safe default headers.

## Out of scope

The following are intentionally not supported at this stage:

- CGI execution,
- shell command execution,
- dynamic module loading,
- arbitrary SQL-over-HTTP,
- `.htaccess` rewrite engine,
- PHP handler configuration through `.htaccess`.

## Known limitations

- HTTP parser is not yet fully RFC 9110/9112 compliant.
- Keep-alive is not implemented.
- Request body and POST handling are limited.
- PHP FastCGI module is basic.
- TLS configuration needs further hardening before production use.

# Security Model

HydrogenHttpd is built around conservative defaults and a small attack surface.

## Threats considered

- path traversal,
- direct access to configuration files,
- slow clients,
- unlimited thread creation,
- arbitrary script execution,
- arbitrary SQL execution,
- unsafe `.htaccess` directives,
- legacy TLS protocols.

## Mitigations

| Threat | Mitigation |
|---|---|
| Path traversal | canonical path checks |
| Slowloris-style clients | read timeout |
| Too many connections | worker pool and bounded queue |
| `.htaccess` exposure | direct access returns 403 |
| Dangerous `.htaccess` directives | whitelist-only parser |
| PHP shell execution | PHP-FPM/FastCGI only |
| SQL injection | no SQL-over-HTTP endpoint; prepared-statement example |
| Old TLS | SSLv2, SSLv3, TLS 1.0, TLS 1.1 disabled |

## Current limitations

- HTTP parser needs RFC hardening.
- Request body handling is minimal.
- PHP module needs broader CGI variable support.
- TLS cipher configuration needs refinement.
- No privilege dropping yet.
- No chroot/seccomp/sandbox yet.

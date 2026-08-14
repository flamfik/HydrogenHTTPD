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
- sending safe default headers,
- storing production bearer tokens as SHA-256 hashes,
- using constant-time token comparison,
- separating token secrets from the main server configuration,
- supporting token expiration and scopes,
- auditing protected endpoint access without logging token secrets,
- hot-reloading hashed token rules with last-known-good fallback,
- rotating audit logs without restarting the server,
- bounded high-load queues and cache budgets,
- preserving synchronous error/audit logging while batching only access logs.

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
- Request body, multipart parsing and upload streaming remain conservative and intentionally limited.
- PHP FastCGI module is basic.
- TLS configuration needs further hardening before production use.


## Authentication guidance

- Prefer `auth_secrets_file` with `sha256:` entries.
- Treat inline `auth_token.*` plaintext entries and `auth_bearer_token` as compatibility modes only.
- Keep real `secrets/auth.tokens` files out of Git.
- Use HTTPS for every authenticated endpoint.
- Use short-lived tokens where practical and rotate them regularly.
- Protect `logs/audit.log` with operating-system file permissions.
- Do not expose `/__hydrogen/admin/status` without `enable_endpoint_auth = true`.

- Use `scripts/manage_auth_tokens.py` for atomic token rotation and revocation.


## High-load guidance

- Do not increase queues and body limits without setting memory budgets.
- Keep `max_pending_connections`, `access_log_queue_capacity` and static cache byte limits bounded.
- Disable the local rate limiter only when a trusted upstream enforces equivalent or stronger limits.
- Access-log loss under queue saturation is observable through `dropped_access_logs`; security audit logs are not placed in that lossy queue.
- The server remains blocking-worker based and should still be protected by a mature load balancer for serious Internet-facing deployments.

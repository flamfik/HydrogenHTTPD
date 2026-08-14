# Production Hardening Notes

HydrogenHttpd v0.12 is a stronger hardened baseline, but it is still experimental software.

## Added in v0.12

- exploit-regression integration test suite,
- double-encoded traversal rejection,
- encoded slash/backslash rejection,
- PHP source disclosure blocking when PHP is disabled,
- symlink escape regression coverage,
- sensitive file exposure regression coverage.

## Recommended deployment stance

- Run behind a mature reverse proxy until HydrogenHttpd receives more audit time.
- Keep `enable_php = false` unless PHP-FPM is explicitly configured.
- Keep `enable_uploads = false` unless an upload endpoint is intentionally needed.
- Use `enable_endpoint_auth = true` for uploads/admin endpoints when enabled.
- Keep `enable_htaccess = false` in production unless needed.
- Keep `force_https = true` when TLS is enabled.
- Keep `expose_server_header = false`.
- Use a dedicated low-privilege user.
- Do not put secrets inside the document root.
- Do not expose SQLite databases inside the document root.
- Run all CTest tests in CI before deployment.

## Still needed before serious production use

- complete RFC 9110/9112 parser review,
- request body support with hard limits,
- fuzzing of parser and path handling,
- privilege dropping,
- chroot/seccomp or platform sandboxing,
- full TLS cipher policy,
- static file sendfile support,
- keep-alive implementation with connection-level limits,
- professional external security review.


## Scoped authorization

Prefer:

```ini
auth_token.upload = long-random-upload-token | upload
auth_token.admin = long-random-admin-token | upload,admin
```

Avoid using the legacy `auth_bearer_token` in new deployments.


## Authentication secrets

- Use `auth_secrets_file` instead of plaintext inline tokens.
- Set filesystem permissions so only the service account can read the token store.
- Use token expiration and separate upload/admin scopes.
- Enable `/__hydrogen/admin/status` only with endpoint authentication.

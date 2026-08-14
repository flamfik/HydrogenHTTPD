# HydrogenHttpd v1.6.0 — Security Operations

This release adds an operational authentication layer on top of the streaming upload and scoped-token foundations.

## Highlights

- SHA-256 token hashes without requiring OpenSSL in non-TLS builds.
- Separate `auth_secrets_file` outside the main server configuration.
- Token scopes and Unix-epoch expiration.
- Constant-time token comparison.
- Protected `/__hydrogen/admin/status` endpoint.
- Detailed authorization audit events without logging secrets.
- Token generation scripts for Python and PowerShell.

## Verification

- TLS + SQLite profile: 17 tests passed.
- No-TLS + no-SQLite profile: 15 tests passed.
- GCC warnings are treated as errors.

## Migration

Existing plaintext token settings continue to work, but new deployments should migrate to:

```ini
enable_endpoint_auth = true
auth_secrets_file = secrets/auth.tokens
```

and hashed rules:

```ini
token.upload = sha256:<hash> | upload | <expiry>
token.admin = sha256:<hash> | upload,admin | <expiry>
```

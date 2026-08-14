# Security Operations v1.6.0

This release moves HydrogenHttpd from a single-token baseline toward an operationally usable authorization model.

## Delivered controls

- SHA-256 hashed bearer-token store,
- token scopes,
- token expiration,
- constant-time comparison,
- separate secrets file,
- admin status endpoint,
- authorization audit events,
- token generation scripts,
- positive and negative integration tests.

## Recommended production configuration

```ini
enable_tls = true
force_https = true
expose_server_header = false

enable_endpoint_auth = true
auth_secrets_file = secrets/auth.tokens

enable_admin_status = true
admin_status_endpoint = /__hydrogen/admin/status

audit_log = logs/audit.log
```

## Recommended token separation

```ini
token.upload = sha256:<hash> | upload | <expiry>
token.admin = sha256:<hash> | admin | <expiry>
token.operations = sha256:<hash> | upload,admin | <expiry>
```

Avoid assigning `*` unless a broad emergency token is explicitly required.

## Remaining work

This is not a complete identity platform. Remaining production hardening includes:

- token-store reload,
- revocation without restart,
- token-specific quotas,
- mTLS,
- structured audit output and rotation,
- external identity-provider integration,
- independent security audit.

# Scoped Auth and Audit Log

HydrogenHttpd v1.5.88 adds scoped bearer tokens and audit logging for protected internal endpoints.

## Why scoped tokens?

The previous baseline supported a single bearer token. That worked as a first step, but it did not distinguish between upload-only access and future admin API access.

Scoped tokens allow safer separation:

```ini
auth_token.upload = upload-secret | upload
auth_token.admin = admin-secret | upload,admin
auth_token.super = root-secret | *
```

## Configuration

```ini
enable_endpoint_auth = true
audit_log = logs/audit.log

auth_token.upload = change-upload-token | upload
auth_token.admin = change-admin-token | upload,admin
```

## Legacy compatibility

Still supported:

```ini
auth_bearer_token = legacy-token
```

The legacy token is treated as a broad token for compatibility. Prefer `auth_token.*` in new deployments.

## Upload endpoint

The upload endpoint requires:

```txt
upload
```

Examples:

```http
Authorization: Bearer change-upload-token
```

A token with only `admin` scope cannot upload unless it also has `upload`.

## Audit events

Audit log path:

```ini
audit_log = logs/audit.log
```

Events:

```txt
auth_missing
auth_scope_denied
auth_allowed
```

Example log lines:

```txt
event=auth_missing ip=127.0.0.1 target="/__hydrogen/upload" required_scope=upload token="-" status=401
event=auth_scope_denied ip=127.0.0.1 target="/__hydrogen/upload" required_scope=upload token="adminonly" status=403
event=auth_allowed ip=127.0.0.1 target="/__hydrogen/upload" required_scope=upload token="upload" status=200
```

## Security notes

- Use long random tokens.
- Do not commit production tokens.
- Use HTTPS.
- Rotate tokens periodically.
- Keep audit logs outside public document root.
- Protect audit logs with file permissions.
- Treat legacy `auth_bearer_token` as transitional.

## Future hardening

- hashed token storage,
- token files outside main config,
- token expiration,
- multiple auth backends,
- mTLS for admin endpoints,
- per-endpoint scope mapping.

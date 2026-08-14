# Administrative Status Endpoint

HydrogenHttpd v1.7.0 adds a protected operational status endpoint.

## Configuration

```ini
enable_endpoint_auth = true
auth_secrets_file = secrets/auth.tokens

enable_admin_status = true
admin_status_endpoint = /__hydrogen/admin/status
```

Enabling the status endpoint without endpoint authentication is rejected during configuration loading.

## Required scope

```txt
admin
```

A token with only `upload` receives `403 Forbidden`.

## Request

```http
GET /__hydrogen/admin/status HTTP/1.1
Host: example.com
Authorization: Bearer <admin-token>
```

## Response

```json
{
  "service": "HydrogenHttpd",
  "version": "1.6.0",
  "uptime_seconds": 42,
  "worker_threads": 8,
  "queued_tasks": 0,
  "max_pending_connections": 512,
  "tls_enabled": true,
  "uploads_enabled": false,
  "php_enabled": false,
  "endpoint_auth_enabled": true,
  "configured_token_rules": 2,
  "auth_store_generation": 2,
  "auth_last_reload_epoch": 1785440000,
  "auth_hot_reload_enabled": true
}
```

The response intentionally excludes:

- token values,
- token hashes,
- token names,
- document-root paths,
- certificate paths,
- database paths.

## Method policy

Only `GET` is accepted. Other methods return `405 Method Not Allowed`.

## Audit events

Requests are recorded in `audit_log` as one of:

```txt
auth_missing
auth_invalid
auth_expired
auth_scope_denied
auth_allowed
```

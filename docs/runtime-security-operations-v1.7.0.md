# Runtime Security Operations — v1.7.0

HydrogenHttpd v1.7.0 focuses on security changes that must take effect without restarting the process.

## Delivered

- thread-safe runtime authentication store,
- hot reload of the hashed token file,
- immediate token revocation,
- atomic token rotation/revocation CLI,
- last-known-good handling for malformed replacements,
- audit events for successful and failed store reloads,
- audit log size rotation with retained backups,
- additional non-secret authentication metadata in the admin status response.

## New configuration

```ini
auth_hot_reload = true
auth_reload_interval_seconds = 0

audit_rotate_bytes = 10485760
audit_rotate_keep = 5
```

## New admin status fields

```json
{
  "configured_token_rules": 3,
  "auth_store_generation": 2,
  "auth_last_reload_epoch": 1785440000,
  "auth_hot_reload_enabled": true
}
```

## New tests

```txt
auth_runtime_store_tests
logger_tests
integration_auth_hot_reload_tests
```

The integration test changes the secrets file while the server is running and verifies:

- the previous token stops working,
- the replacement token starts working,
- removing all file rules revokes the token,
- a later valid file recovers the store,
- audit log rotation occurs,
- the number of retained backup logs is bounded.

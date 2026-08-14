# Authentication Store Hot Reload

HydrogenHttpd v1.7.0 can reload `auth_secrets_file` while the server is running.

## Configuration

```ini
enable_endpoint_auth = true
auth_secrets_file = secrets/auth.tokens
auth_hot_reload = true
auth_reload_interval_seconds = 0
```

`auth_reload_interval_seconds = 0` checks the file before every protected request. A positive value limits filesystem checks to the configured interval.

## Reload behavior

### Valid changed file

The new token set replaces the previous file-backed token set atomically.

```txt
event=auth_store_reloaded generation=2 token_count=3
```

### Removed secrets file

All file-backed tokens are revoked. Inline `auth_token.*` compatibility rules remain active.

### Malformed changed file

The last known good token set remains active. The server records:

```txt
event=auth_store_reload_failed
```

The detailed parser error is written to `error_log`, not copied into the audit log.

The same malformed file version is not retried on every request. Editing or replacing the file triggers another attempt.

## Immediate revocation

Use the management tool:

```bash
python3 scripts/manage_auth_tokens.py \
  --file secrets/auth.tokens \
  revoke --name upload
```

The tool uses atomic file replacement. With reload interval `0`, the removed token is rejected by the next protected request.

## Rotation

```bash
python3 scripts/manage_auth_tokens.py \
  --file secrets/auth.tokens \
  rotate --name upload --scopes upload --ttl-days 30
```

The plaintext token is displayed once. Only its SHA-256 hash is stored.

## Thread safety

The runtime store protects token snapshots and reload metadata with a mutex. Requests either use the previous complete snapshot or the new complete snapshot; they do not observe a partially loaded file.

# Audit Log Rotation

HydrogenHttpd v1.7.0 can rotate its audit log without restarting.

## Configuration

```ini
audit_log = logs/audit.log
audit_rotate_bytes = 10485760
audit_rotate_keep = 5
```

`audit_rotate_bytes = 0` disables internal size-based rotation.

When the current audit log would exceed the configured size, files are shifted:

```txt
audit.log.4 -> audit.log.5
audit.log.3 -> audit.log.4
audit.log.2 -> audit.log.3
audit.log.1 -> audit.log.2
audit.log   -> audit.log.1
```

A new `audit.log` is opened immediately.

## Security properties

- Rotation runs under the logger mutex.
- Protected endpoint requests do not require a server restart.
- The configured backup count is enforced.
- Token plaintext and token hashes are never written to the audit log.
- Reload parser details are written to `error_log`; the audit log receives only a stable event code.

## External log rotation

The internal mechanism is recommended for this release. When using an external service such as logrotate, coordinate file ownership and reopening behavior carefully.

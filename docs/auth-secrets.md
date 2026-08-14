# Hashed Authentication Secrets

HydrogenHttpd v1.7.0 can load bearer-token rules from a file separate from the main server configuration.

## Main configuration

```ini
enable_endpoint_auth = true
auth_secrets_file = secrets/auth.tokens
```

The path is resolved relative to the main configuration file when it is not absolute.

## Token store format

```ini
token.upload = sha256:<64-hex-hash> | upload | never
token.admin = sha256:<64-hex-hash> | upload,admin | 1893456000
```

Each rule contains:

1. a SHA-256 token hash,
2. one or more comma-separated scopes,
3. an expiration value.

Expiration values:

- `never` or `0`: no expiration,
- a positive integer: Unix epoch seconds in UTC.

## Generate tokens

Linux/macOS:

```bash
python3 scripts/generate_auth_token.py \
  --name upload \
  --scopes upload \
  --ttl-days 30
```

Windows PowerShell:

```powershell
.\scripts\generate_auth_token.ps1 `
  -Name admin `
  -Scopes "upload,admin" `
  -TtlDays 30
```

The generator prints:

- the plaintext token once,
- the hashed token-store entry.

Store the plaintext in the client or a secret manager. Store only the generated hash entry on the server.

## Compatibility modes

The following older formats still work:

```ini
auth_bearer_token = legacy-token
auth_token.upload = plaintext-token | upload | never
```

They are not recommended for new production deployments because the main configuration contains the plaintext token.

## Runtime decisions

HydrogenHttpd distinguishes:

- missing credentials,
- invalid credentials,
- expired credentials,
- insufficient scope,
- allowed credentials.

A token hash is compared in constant time. Token plaintext is never written to access, error or audit logs.

## File handling

Recommended permissions on Linux:

```bash
chmod 600 secrets/auth.tokens
```

Keep the file outside the document root and outside version control.


## Hot reload

See [`auth-hot-reload.md`](auth-hot-reload.md). File-backed token changes can take effect without restarting the server.

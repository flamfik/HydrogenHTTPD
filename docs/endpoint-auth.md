# Endpoint Authorization

HydrogenHttpd v1.6.0 adds a small authorization baseline for protected internal endpoints.

At this stage it protects the upload endpoint.

## Configuration

```ini
enable_endpoint_auth = true
auth_bearer_token = change-this-token
```

## Request

```http
POST /__hydrogen/upload HTTP/1.1
Host: localhost
Authorization: Bearer change-this-token
Content-Type: multipart/form-data; boundary=...
Content-Length: ...
```

## Unauthorized response

```txt
401 Unauthorized
```

with:

```http
WWW-Authenticate: Bearer
```

## Security notes

This is a baseline, not a full identity system.

Recommended production practices:

- use a long random token,
- keep the token outside the document root,
- use HTTPS,
- rotate the token periodically,
- do not commit production tokens to Git,
- put HydrogenHttpd behind a mature reverse proxy for production deployments.

## Future hardening

- hashed token storage,
- multiple tokens,
- scoped tokens,
- admin/user roles,
- audit log for protected endpoints,
- token rotation file,
- optional mTLS.


## v1.5.88 scoped token format

```ini
auth_token.upload = upload-secret | upload
auth_token.admin = admin-secret | upload,admin
```

The upload endpoint requires the `upload` scope.


## v1.6.0 hashed token store

Recommended configuration:

```ini
enable_endpoint_auth = true
auth_secrets_file = secrets/auth.tokens
```

```ini
token.upload = sha256:<hash> | upload | never
token.admin = sha256:<hash> | upload,admin | <unix-expiry>
```

Authorization now distinguishes missing, invalid, expired and insufficient-scope tokens.

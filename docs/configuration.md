# Configuration Reference

HydrogenHttpd uses a simple `key = value` config format.

## Network

```ini
port = 8080
```

## TLS

```ini
enable_tls = false
tls_port = 8443
tls_cert_file = certs/server.crt
tls_key_file = certs/server.key
```

Generate development certs:

```bash
bash scripts_generate_self_signed_cert.sh
```

## Document root

```ini
default_root = www
```

## Logs

```ini
access_log = logs/access.log
error_log = logs/error.log
```

## Limits

```ini
max_request_bytes = 16384
max_body_bytes = 1048576
rate_limit_per_minute = 120
read_timeout_seconds = 5
```

## Worker pool

```ini
worker_threads = 4
max_pending_connections = 256
```

## `.htaccess`

```ini
enable_htaccess = true
htaccess_filename = .htaccess
max_htaccess_depth = 8
```

Supported directives:

```apache
Require all denied
Require all granted
Options -Indexes
Header set X-Custom-Header value
```

## PHP FastCGI

```ini
enable_php = false
php_extension = .php
php_fastcgi_host = 127.0.0.1
php_fastcgi_port = 9000
php_fastcgi_connect_timeout_seconds = 3
php_fastcgi_read_timeout_seconds = 10
```

## SQLite

```ini
enable_sql = false
sqlite_database = sql/hydrogen.db
```

## Virtual hosts

```ini
vhost.localhost = www
vhost.127.0.0.1 = www
```


## Uploads

```ini
enable_uploads = false
upload_endpoint = /__hydrogen/upload
upload_directory = uploads
upload_spool_directory = tmp/uploads
max_multipart_parts = 16
max_upload_file_bytes = 1048576
max_upload_field_bytes = 16384
```


## Endpoint authorization

```ini
enable_endpoint_auth = false
auth_bearer_token =
```


## Hashed token store

```ini
enable_endpoint_auth = true
auth_secrets_file = secrets/auth.tokens
```

Token file entries:

```ini
token.upload = sha256:<hash> | upload | never
token.admin = sha256:<hash> | upload,admin | 1893456000
```

## Admin status endpoint

```ini
enable_admin_status = false
admin_status_endpoint = /__hydrogen/admin/status
```

`enable_admin_status = true` requires `enable_endpoint_auth = true`.


## High-load runtime

```ini
enable_rate_limiter = true
rate_limiter_shards = 64
io_threads = 2
worker_threads = 0
worker_thread_multiplier = 4
max_pending_connections = 4096
listen_backlog = 4096
tcp_no_delay = true
tcp_keep_alive = true
socket_receive_buffer_bytes = 0
socket_send_buffer_bytes = 0

async_access_log = true
enable_access_log = true
access_log_sample_rate = 1
access_log_queue_capacity = 65536
access_log_flush_interval_ms = 100

enable_static_cache = true
static_cache_shards = 32
static_cache_max_entries = 4096
static_cache_max_bytes = 268435456
static_cache_max_file_bytes = 4194304
static_cache_revalidate_ms = 1000
static_cache_control = public, max-age=60
```

See `server.high-load.conf` and `docs/high-load-performance.md`.

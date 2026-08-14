# High-load performance baseline — v1.8.0

HydrogenHttpd v1.8.0 improves the existing blocking-worker architecture without weakening the security defaults introduced in earlier releases.

It is still an experimental server. The changes below improve throughput and reduce contention, but they do not turn the current HTTP/1.1 implementation into a fully asynchronous nginx-class architecture.

## Changes

### Sharded static file cache

Small and medium static files can be retained in memory:

```ini
enable_static_cache = true
static_cache_shards = 32
static_cache_max_entries = 4096
static_cache_max_bytes = 268435456
static_cache_max_file_bytes = 4194304
static_cache_revalidate_ms = 1000
static_cache_control = public, max-age=60
```

The cache:

- uses multiple shards,
- allows concurrent read access through `std::shared_mutex`,
- validates file modification time after the configured interval,
- has entry, total-byte and per-file limits,
- evicts old entries when a shard exceeds its budget,
- generates an ETag from file size and modification time.

Static response bodies are passed to Boost.Asio as a separate shared buffer. The server no longer has to concatenate headers and a cached body into another full response string.

### Conditional requests

Static responses include:

```http
ETag: "..."
```

A matching request:

```http
If-None-Match: "..."
```

returns:

```http
304 Not Modified
```

This reduces network transfer and application work for clients and reverse proxies with cached assets.

### Asynchronous access log

```ini
async_access_log = true
enable_access_log = true
access_log_sample_rate = 1
access_log_queue_capacity = 65536
access_log_flush_interval_ms = 100
```

Request workers enqueue access-log lines into a bounded queue. A dedicated logger thread writes them in batches.

When the queue is full, access-log entries may be dropped instead of blocking request processing. The number is exposed as `dropped_access_logs` in the protected admin status endpoint. Error and security audit logs remain synchronous because their durability is more important than raw request throughput.

For extreme load, sample access logs:

```ini
access_log_sample_rate = 10
```

or disable them only when an upstream proxy already provides trustworthy request logging:

```ini
enable_access_log = false
```

### Sharded rate limiter

```ini
enable_rate_limiter = true
rate_limiter_shards = 64
```

Client buckets are split across independent locks. Idle buckets are periodically removed.

When a trusted upstream already performs rate limiting, the local limiter can be disabled:

```ini
enable_rate_limiter = false
```

Do not disable both upstream and local rate limiting on an Internet-facing deployment.

### Runtime concurrency

```ini
io_threads = 2
worker_threads = 0
worker_thread_multiplier = 4
max_pending_connections = 4096
listen_backlog = 4096
```

`worker_threads = 0` selects:

```text
max(4, hardware_concurrency × worker_thread_multiplier)
```

The Boost.Asio `io_context` can now run on multiple threads. The accept backlog and bounded worker queue are independently configurable.

### Socket tuning

```ini
tcp_no_delay = true
tcp_keep_alive = true
socket_receive_buffer_bytes = 0
socket_send_buffer_bytes = 0
```

A buffer value of `0` leaves operating-system autotuning enabled, which is usually the best default.

## High-load profile

Start with:

```bash
./build/hydrogen_httpd server.high-load.conf
```

The profile assumes operation behind a trusted load balancer or reverse proxy. Review its disabled modules and rate-limiter setting before using it.

## Admin metrics

The protected admin status endpoint now includes:

```json
{
  "active_workers": 8,
  "completed_tasks": 100000,
  "rejected_tasks": 0,
  "rate_limiter_enabled": true,
  "rate_limiter_buckets": 12,
  "dropped_access_logs": 0,
  "static_cache_entries": 20,
  "static_cache_bytes": 1048576,
  "static_cache_hits": 90000,
  "static_cache_misses": 20,
  "static_cache_evictions": 0
}
```

## Remaining architectural limits

The current server still has important scaling limits:

- one blocking worker is occupied while a connection is being read or processed,
- HTTP keep-alive is not implemented,
- HTTP pipelining is not supported,
- HTTP/2 and HTTP/3 are not implemented,
- plain HTTP does not yet use `sendfile`,
- TLS operations remain synchronous inside worker tasks,
- PHP-FPM and filesystem latency can still consume worker capacity.

The next major throughput step would be an asynchronous connection/session state machine with bounded per-connection buffers and worker offload only for filesystem, FastCGI and application work.

# Safe POST and Request Body Handling

HydrogenHttpd v0.13 introduces the first safe baseline for request bodies.

## Goals

- Accept request bodies only when the size is explicit.
- Keep body parsing bounded by configuration.
- Avoid partial or unsafe chunked implementation.
- Enable future form handling and PHP-FPM POST requests.
- Preserve exploit-regression hardening.

## Supported

```http
POST /submit.php HTTP/1.1
Host: localhost
Content-Length: 7
Content-Type: application/x-www-form-urlencoded

x=hello
```

Body forwarding to PHP-FPM is supported when:

```ini
enable_php = true
```

## Rejected

### Chunked requests

```http
Transfer-Encoding: chunked
```

Result:

```txt
501 Not Implemented
```

Chunked support is intentionally postponed until it can be implemented and tested properly.

### GET or HEAD with body

Result:

```txt
413 Payload Too Large
```

### POST to static resources

Result:

```txt
405 Method Not Allowed
```

### Oversized body

Controlled by:

```ini
max_body_bytes = 1048576
```

Result:

```txt
413 Payload Too Large
```

## Implementation notes

The read path now works in two stages:

1. Read headers up to `max_request_bytes`.
2. Extract `Content-Length` from headers only.
3. Read at most `max_body_bytes`.
4. Run strict HTTP parser on the complete request.

This avoids the bug where the strict parser was asked to validate a request body before the body had been read.

## Tests

```txt
tests/integration_post_body_tests.py
```

Run:

```bash
ctest --test-dir build -R integration_post_body_tests --output-on-failure
```

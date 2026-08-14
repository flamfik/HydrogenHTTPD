# Roadmap

## Near term

- Better FastCGI response header mapping
- HTTPS integration test improvements
- Per-vhost PHP-FPM configuration
- Better MIME type handling
- Range requests for static files

## Security hardening

- RFC 9110/9112-aligned parser
- Fuzzing for request parsing and path handling
- Privilege dropping
- Optional chroot-like document root isolation
- TLS cipher suite configuration
- More negative tests for malformed requests

## Performance

- Static file cache metadata
- sendfile support
- keep-alive
- connection pooling for FastCGI
- async file I/O exploration

## Developer experience

- vcpkg/conan examples
- Dockerfile
- devcontainer
- packaged releases


## After v0.13

- Chunked transfer decoding with strict limits.
- Multipart form parsing.
- FastCGI integration test with PHP-FPM container.


## After v0.14

- Per-directory upload policy.
- Quotas and cleanup jobs.
- Optional content scanning hook.


## After v1.6.0

- Reload token store without full server restart.
- Hashed-token identifiers and revocation lists.
- Per-token rate limits and upload quotas.
- Audit log rotation and structured JSON audit format.
- Optional mTLS for administrative endpoints.
- Health/readiness separation for orchestration platforms.

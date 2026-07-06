# Roadmap

## Near term

- POST body support with strict limits
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

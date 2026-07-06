# PHP FastCGI Module

HydrogenHttpd supports PHP by delegating `.php` files to PHP-FPM over FastCGI.

## Why FastCGI?

This avoids:

- shell execution,
- spawning `php-cgi` per request,
- mixing PHP interpreter lifecycle with the HTTP server process.

## Docker example

From the repository root:

```bash
docker run --rm -p 9000:9000 -v "$PWD/www:/var/www/html" php:8.3-fpm
```

Then enable PHP:

```ini
enable_php = true
php_fastcgi_host = 127.0.0.1
php_fastcgi_port = 9000
```

Run:

```bash
./build/hydrogen_httpd server.conf
```

Test:

```bash
curl -i http://localhost:8080/info.php
```

## Limitations

Current implementation focuses on basic GET support. Future work:

- POST body forwarding,
- full HTTP header mapping,
- `PATH_INFO`,
- FastCGI connection pooling,
- per-vhost PHP-FPM pools.

# PHP example

Start PHP-FPM through Docker:

```bash
docker run --rm -p 9000:9000 -v "$PWD/www:/var/www/html" php:8.3-fpm
```

Enable in `server.conf`:

```ini
enable_php = true
```

Run HydrogenHttpd and open:

```txt
http://localhost:8080/info.php
```

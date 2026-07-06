# Multipart Upload Handling

HydrogenHttpd v0.14 adds a guarded baseline for `multipart/form-data` uploads.

This module is intentionally small and conservative.

## Configuration

Uploads are disabled by default:

```ini
enable_uploads = false
```

Enable explicitly:

```ini
enable_uploads = true
upload_endpoint = /__hydrogen/upload
upload_directory = uploads
upload_spool_directory = tmp/uploads
max_multipart_parts = 16
max_upload_file_bytes = 1048576
max_upload_field_bytes = 16384
```

## Endpoint

```http
POST /__hydrogen/upload HTTP/1.1
Host: localhost
Content-Type: multipart/form-data; boundary=...
Content-Length: ...
```

Successful response:

```json
{
  "ok": true,
  "files": [
    {
      "field": "file",
      "original": "notes.txt",
      "stored": "1760000000000000_notes.txt",
      "size": 12
    }
  ]
}
```

## Security model

The upload module:

- is disabled by default,
- accepts only `POST`,
- requires `multipart/form-data`,
- requires already-bounded body handling from v0.13,
- limits number of multipart parts,
- limits uploaded file size,
- limits text field size,
- sanitizes filenames,
- strips path separators from filenames,
- prefixes stored filenames with a timestamp-like value,
- blocks dangerous extensions,
- stores files in `upload_directory`,
- does not make uploads public automatically.

## Blocked extensions

Examples:

```txt
.php
.phtml
.phar
.cgi
.pl
.py
.rb
.sh
.bat
.cmd
.ps1
.exe
.dll
.so
.conf
.ini
.db
.sqlite
.key
.pem
.crt
.log
```

## Tests

Unit test:

```txt
tests/test_multipart.cpp
```

Integration test:

```txt
tests/integration_upload_tests.py
```

Run:

```bash
ctest --test-dir build -R multipart_tests --output-on-failure
ctest --test-dir build -R integration_upload_tests --output-on-failure
```

## Current limitations

- No streaming upload to disk yet; body is still buffered within `max_body_bytes`.
- No multipart nested parsing.
- No antivirus/content scanning hook.
- No upload authentication/authorization layer.
- No per-user quotas.
- No MIME sniffing beyond extension policy.


## v0.15 streaming note

The upload endpoint now streams request bodies to `upload_spool_directory` instead of buffering the whole upload body in memory.

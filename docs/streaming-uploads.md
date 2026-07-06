# Streaming Uploads

HydrogenHttpd v0.15 changes the upload endpoint flow so upload request bodies are not buffered as one large in-memory string.

## Previous flow

```txt
socket -> memory string -> multipart parser -> uploaded files
```

## New flow

```txt
socket -> headers only
socket body -> spool file
spool file -> multipart parser
multipart file parts -> upload directory
spool cleanup
```

## Configuration

```ini
enable_uploads = true
upload_endpoint = /__hydrogen/upload
upload_directory = uploads
upload_spool_directory = tmp/uploads
max_body_bytes = 1048576
max_upload_file_bytes = 1048576
```

## Why a spool file?

A direct streaming multipart parser is more complex and riskier to implement quickly.  
The spool baseline gives an immediate memory-safety improvement:

- the server does not hold the entire upload body in RAM,
- limits still apply,
- multipart parsing happens from disk,
- accepted files are written to a separate upload directory,
- spool files are removed after processing.

## Current limitations

- Multipart parsing is still line-oriented and conservative.
- Binary edge cases need more tests before large arbitrary binary upload support.
- No resumable upload support.
- No direct-to-final streaming yet.
- No antivirus/content scanning hook yet.

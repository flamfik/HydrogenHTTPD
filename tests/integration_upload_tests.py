#!/usr/bin/env python3
import json
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def wait_for_port(port: int, timeout: float = 5.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("server did not open port")


def send_raw(port: int, request: bytes, timeout: float = 3.0) -> bytes:
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as s:
        s.settimeout(timeout)
        s.sendall(request)
        chunks = []
        while True:
            try:
                part = s.recv(4096)
            except socket.timeout:
                break
            if not part:
                break
            chunks.append(part)
        return b"".join(chunks)


def status_code(response: bytes) -> int:
    first = response.split(b"\r\n", 1)[0]
    parts = first.split()
    if len(parts) < 2:
        raise AssertionError(f"bad response line: {first!r}; response={response[:200]!r}")
    return int(parts[1])


def response_body(response: bytes) -> bytes:
    return response.split(b"\r\n\r\n", 1)[1] if b"\r\n\r\n" in response else b""


def assert_status(response: bytes, expected: int, label: str) -> None:
    got = status_code(response)
    if got != expected:
        raise AssertionError(f"{label}: expected {expected}, got {got}; response={response[:500]!r}")


def multipart(boundary: str, filename: str, content: bytes, field: str = "file") -> bytes:
    return (
        f"--{boundary}\r\n"
        f"Content-Disposition: form-data; name=\"{field}\"; filename=\"{filename}\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
    ).encode() + content + f"\r\n--{boundary}--\r\n".encode()


def request(path: str, body: bytes, boundary: str) -> bytes:
    return (
        f"POST {path} HTTP/1.1\r\n"
        "Host: localhost\r\n"
        f"Content-Type: multipart/form-data; boundary={boundary}\r\n"
        f"Content-Length: {len(body)}\r\n"
        "\r\n"
    ).encode() + body


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: integration_upload_tests.py <hydrogen_httpd_exe>", file=sys.stderr)
        return 2

    exe = Path(sys.argv[1]).resolve()

    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        www = work / "www"
        logs = work / "logs"
        uploads = work / "isolated_uploads"
        www.mkdir()
        logs.mkdir()
        (www / "index.html").write_text("ok", encoding="utf-8")

        cfg = work / "server.conf"
        cfg.write_text("\n".join([
            "port = 18084",
            "enable_tls = false",
            f"default_root = {www}",
            f"access_log = {logs / 'access.log'}",
            f"error_log = {logs / 'error.log'}",
            "max_request_bytes = 4096",
            "max_body_bytes = 2048",
            "max_header_line_bytes = 1024",
            "max_headers = 32",
            "max_uri_bytes = 512",
            "max_method_bytes = 16",
            "rate_limit_per_minute = 1000",
            "read_timeout_seconds = 1",
            "worker_threads = 2",
            "max_pending_connections = 16",
            "enable_htaccess = false",
            "enable_php = false",
            "enable_uploads = true",
            "upload_endpoint = /__hydrogen/upload",
            f"upload_directory = {uploads}",
            "max_multipart_parts = 4",
            "max_upload_file_bytes = 32",
            "max_upload_field_bytes = 32",
            f"vhost.localhost = {www}",
            "",
        ]), encoding="utf-8")

        proc = subprocess.Popen([str(exe), str(cfg)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=str(work))
        try:
            wait_for_port(18084)

            body = multipart("abc", "notes.txt", b"hello-upload")
            res = send_raw(18084, request("/__hydrogen/upload", body, "abc"))
            assert_status(res, 200, "valid upload")
            data = json.loads(response_body(res).decode())
            assert data["ok"] is True
            assert len(data["files"]) == 1
            stored = uploads / data["files"][0]["stored"]
            assert stored.exists(), stored
            assert stored.read_bytes() == b"hello-upload"
            assert not (www / data["files"][0]["stored"]).exists()

            blocked = multipart("abc", "shell.php", b"<?php echo 1; ?>")
            assert_status(send_raw(18084, request("/__hydrogen/upload", blocked, "abc")), 415, "blocked php upload")

            too_big = multipart("abc", "big.txt", b"X" * 40)
            assert_status(send_raw(18084, request("/__hydrogen/upload", too_big, "abc")), 413, "oversized upload")

            bad_type = (
                b"POST /__hydrogen/upload HTTP/1.1\r\n"
                b"Host: localhost\r\n"
                b"Content-Type: text/plain\r\n"
                b"Content-Length: 3\r\n"
                b"\r\n"
                b"abc"
            )
            assert_status(send_raw(18084, bad_type), 415, "wrong content type")

            assert_status(send_raw(
                18084,
                b"GET /__hydrogen/upload HTTP/1.1\r\nHost: localhost\r\n\r\n"
            ), 405, "GET upload endpoint")

            print("integration upload tests passed")
            return 0
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=3)


if __name__ == "__main__":
    raise SystemExit(main())

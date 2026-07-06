#!/usr/bin/env python3
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


def assert_status(response: bytes, expected: int, label: str) -> None:
    got = status_code(response)
    if got != expected:
        raise AssertionError(f"{label}: expected {expected}, got {got}; response={response[:300]!r}")


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: integration_post_body_tests.py <hydrogen_httpd_exe>", file=sys.stderr)
        return 2

    exe = Path(sys.argv[1]).resolve()

    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        www = work / "www"
        logs = work / "logs"
        www.mkdir()
        logs.mkdir()
        (www / "index.html").write_text("ok", encoding="utf-8")
        (www / "submit.php").write_text("<?php echo $_POST['x'] ?? 'missing'; ?>", encoding="utf-8")

        cfg = work / "server.conf"
        cfg.write_text("\n".join([
            "port = 18083",
            "enable_tls = false",
            f"default_root = {www}",
            f"access_log = {logs / 'access.log'}",
            f"error_log = {logs / 'error.log'}",
            "max_request_bytes = 4096",
            "max_body_bytes = 8",
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
            f"vhost.localhost = {www}",
            "",
        ]), encoding="utf-8")

        proc = subprocess.Popen([str(exe), str(cfg)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=str(work))
        try:
            wait_for_port(18083)

            assert_status(send_raw(
                18083,
                b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\nContent-Type: text/plain\r\n\r\nabc"
            ), 405, "POST static resource")

            assert_status(send_raw(
                18083,
                b"GET / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 1\r\n\r\nX"
            ), 413, "GET body rejected")

            assert_status(send_raw(
                18083,
                b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 9\r\n\r\n123456789"
            ), 413, "oversized POST body")

            assert_status(send_raw(
                18083,
                b"POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n"
            ), 501, "chunked rejected")

            assert_status(send_raw(
                18083,
                b"POST /submit.php HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\nx=1"
            ), 403, "PHP source disclosure still blocked")

            print("integration POST/body tests passed")
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

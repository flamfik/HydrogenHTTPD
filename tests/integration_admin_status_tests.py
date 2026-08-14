#!/usr/bin/env python3
import hashlib
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


def send_raw(port: int, token: str | None, method: str = "GET") -> bytes:
    auth = f"Authorization: Bearer {token}\r\n" if token is not None else ""
    request = (
        f"{method} /__hydrogen/admin/status HTTP/1.1\r\n"
        "Host: localhost\r\n"
        f"{auth}"
        "\r\n"
    ).encode()

    with socket.create_connection(("127.0.0.1", port), timeout=3.0) as sock:
        sock.settimeout(3.0)
        sock.sendall(request)
        chunks = []
        while True:
            try:
                part = sock.recv(4096)
            except socket.timeout:
                break
            if not part:
                break
            chunks.append(part)
        return b"".join(chunks)


def status_code(response: bytes) -> int:
    first = response.split(b"\r\n", 1)[0]
    return int(first.split()[1])


def response_body(response: bytes) -> bytes:
    return response.split(b"\r\n\r\n", 1)[1] if b"\r\n\r\n" in response else b""


def assert_status(response: bytes, expected: int, label: str) -> None:
    got = status_code(response)
    if got != expected:
        raise AssertionError(f"{label}: expected {expected}, got {got}; response={response[:500]!r}")


def token_hash(value: str) -> str:
    return hashlib.sha256(value.encode()).hexdigest()


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: integration_admin_status_tests.py <hydrogen_httpd_exe>", file=sys.stderr)
        return 2

    exe = Path(sys.argv[1]).resolve()

    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        www = work / "www"
        logs = work / "logs"
        www.mkdir()
        logs.mkdir()
        (www / "index.html").write_text("ok", encoding="utf-8")

        secrets = work / "auth.tokens"
        secrets.write_text("\n".join([
            f"token.upload = sha256:{token_hash('upload-secret')} | upload | never",
            f"token.admin = sha256:{token_hash('admin-secret')} | upload,admin | never",
            f"token.expired = sha256:{token_hash('expired-secret')} | admin | 1",
            "",
        ]), encoding="utf-8")

        cfg = work / "server.conf"
        cfg.write_text("\n".join([
            "port = 18086",
            "enable_tls = false",
            f"default_root = {www}",
            f"access_log = {logs / 'access.log'}",
            f"error_log = {logs / 'error.log'}",
            f"audit_log = {logs / 'audit.log'}",
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
            "enable_uploads = false",
            "enable_endpoint_auth = true",
            "auth_secrets_file = auth.tokens",
            "enable_admin_status = true",
            "admin_status_endpoint = /__hydrogen/admin/status",
            f"vhost.localhost = {www}",
            "",
        ]), encoding="utf-8")

        proc = subprocess.Popen(
            [str(exe), str(cfg)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=str(work),
        )
        try:
            wait_for_port(18086)

            assert_status(send_raw(18086, None), 401, "missing token")
            assert_status(send_raw(18086, "wrong-secret"), 401, "invalid token")
            assert_status(send_raw(18086, "upload-secret"), 403, "insufficient scope")
            assert_status(send_raw(18086, "expired-secret"), 401, "expired token")
            assert_status(send_raw(18086, "admin-secret", method="POST"), 405, "admin status method")

            response = send_raw(18086, "admin-secret")
            assert_status(response, 200, "admin status")
            data = json.loads(response_body(response).decode())
            assert data["service"] == "HydrogenHttpd"
            assert data["version"] == "1.9.0"
            assert data["worker_threads"] == 2
            assert data["endpoint_auth_enabled"] is True
            assert data["configured_token_rules"] == 3
            assert data["auth_store_generation"] == 1
            assert data["auth_hot_reload_enabled"] is True
            assert isinstance(data["uptime_seconds"], int)

            audit_text = (logs / "audit.log").read_text(encoding="utf-8")
            assert "event=auth_missing" in audit_text
            assert "event=auth_invalid" in audit_text
            assert "event=auth_scope_denied" in audit_text
            assert "event=auth_expired" in audit_text
            assert "event=auth_allowed" in audit_text
            assert "required_scope=admin" in audit_text
            assert 'token="admin"' in audit_text
            assert 'token="expired"' in audit_text

            print("integration admin status tests passed")
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

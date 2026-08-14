#!/usr/bin/env python3
import hashlib
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def token_hash(value: str) -> str:
    return hashlib.sha256(value.encode()).hexdigest()


def write_token(path: Path, name: str, secret: str) -> None:
    path.write_text(
        f"token.{name} = sha256:{token_hash(secret)} | admin | never\n",
        encoding="utf-8",
    )
    os.utime(path, None)
    time.sleep(0.03)


def wait_for_port(port: int, timeout: float = 5.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("server did not open port")


def send_status(port: int, token: str | None) -> bytes:
    auth = f"Authorization: Bearer {token}\r\n" if token is not None else ""
    request = (
        "GET /__hydrogen/admin/status HTTP/1.1\r\n"
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
    return int(response.split(b"\r\n", 1)[0].split()[1])


def response_body(response: bytes) -> bytes:
    return response.split(b"\r\n\r\n", 1)[1]


def assert_status(response: bytes, expected: int, label: str) -> None:
    actual = status_code(response)
    if actual != expected:
        raise AssertionError(f"{label}: expected {expected}, got {actual}; {response[:500]!r}")


def read_all_audit_files(audit: Path) -> str:
    files = [audit]
    files.extend(sorted(audit.parent.glob(audit.name + ".*")))
    return "\n".join(
        item.read_text(encoding="utf-8", errors="replace")
        for item in files
        if item.exists()
    )


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: integration_auth_hot_reload_tests.py <hydrogen_httpd_exe>", file=sys.stderr)
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
        write_token(secrets, "old", "old-secret")

        audit = logs / "audit.log"
        cfg = work / "server.conf"
        cfg.write_text("\n".join([
            "port = 18087",
            "enable_tls = false",
            f"default_root = {www}",
            f"access_log = {logs / 'access.log'}",
            f"error_log = {logs / 'error.log'}",
            f"audit_log = {audit}",
            "audit_rotate_bytes = 600",
            "audit_rotate_keep = 2",
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
            "auth_hot_reload = true",
            "auth_reload_interval_seconds = 0",
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
            wait_for_port(18087)

            initial = send_status(18087, "old-secret")
            assert_status(initial, 200, "initial token")
            initial_data = json.loads(response_body(initial).decode())
            assert initial_data["auth_store_generation"] == 1
            assert initial_data["configured_token_rules"] == 1
            assert initial_data["auth_hot_reload_enabled"] is True

            write_token(secrets, "new", "new-secret")

            assert_status(send_status(18087, "old-secret"), 401, "old token revoked")
            new_response = send_status(18087, "new-secret")
            assert_status(new_response, 200, "new token accepted")
            new_data = json.loads(response_body(new_response).decode())
            assert new_data["auth_store_generation"] >= 2

            secrets.write_text("", encoding="utf-8")
            os.utime(secrets, None)
            time.sleep(0.03)
            assert_status(send_status(18087, "new-secret"), 401, "token removed from store")

            write_token(secrets, "recovered", "recovered-secret")
            assert_status(send_status(18087, "recovered-secret"), 200, "token store recovered")

            for _ in range(24):
                assert_status(send_status(18087, "invalid-secret"), 401, "invalid token for rotation")

            write_token(secrets, "final", "final-secret")
            assert_status(send_status(18087, "final-secret"), 200, "final token after rotation")

            assert (logs / "audit.log.1").exists(), "audit rotation did not create audit.log.1"
            assert not (logs / "audit.log.3").exists(), "rotation exceeded configured backup count"

            audit_text = read_all_audit_files(audit)
            assert "event=auth_store_reloaded" in audit_text
            assert "event=auth_invalid" in audit_text
            assert "generation=" in audit_text

            print("integration auth hot reload tests passed")
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

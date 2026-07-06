#!/usr/bin/env python3
import socket, subprocess, sys, tempfile, time
from pathlib import Path

def wait_for_port(port, timeout=5.0):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("server did not open port")

def send_raw(port, req, timeout=3.0):
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as s:
        s.settimeout(timeout)
        s.sendall(req)
        out = []
        while True:
            try: part = s.recv(4096)
            except socket.timeout: break
            if not part: break
            out.append(part)
        return b"".join(out)

def assert_status(res, status):
    first = res.split(b"\r\n", 1)[0]
    expected = f"HTTP/1.1 {status} ".encode()
    if not first.startswith(expected):
        raise AssertionError((status, first, res[:200]))

def main():
    exe = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        www = work / "www"; logs = work / "logs"
        www.mkdir(); logs.mkdir()
        (www / "index.html").write_text("ok", encoding="utf-8")
        (www / ".htaccess").write_text('Header set X-Test yes\n', encoding="utf-8")
        (www / "private").mkdir()
        (www / "private" / "secret.txt").write_text("secret", encoding="utf-8")
        (www / "private" / ".htaccess").write_text("Require all denied\n", encoding="utf-8")
        cfg = work / "server.conf"
        cfg.write_text("\n".join([
            "port = 18080",
            "enable_tls = false",
            f"default_root = {www}",
            f"access_log = {logs/'access.log'}",
            f"error_log = {logs/'error.log'}",
            "max_request_bytes = 2048",
            "rate_limit_per_minute = 100",
            "read_timeout_seconds = 1",
            "expose_server_header = true",
            "worker_threads = 2",
            "max_pending_connections = 8",
            "enable_htaccess = true",
            "htaccess_filename = .htaccess",
            "max_htaccess_depth = 8",
            f"vhost.localhost = {www}",
            ""
        ]), encoding="utf-8")
        proc = subprocess.Popen([str(exe), str(cfg)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=str(work))
        try:
            wait_for_port(18080)
            res = send_raw(18080, b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
            assert_status(res, 200)
            assert b"Server: HydrogenHttpd" in res
            assert b"X-Test: yes" in res
            res = send_raw(18080, b"GET /.htaccess HTTP/1.1\r\nHost: localhost\r\n\r\n")
            assert_status(res, 403)
            res = send_raw(18080, b"GET /private/secret.txt HTTP/1.1\r\nHost: localhost\r\n\r\n")
            assert_status(res, 403)
            res = send_raw(18080, b"GET /%2e%2e/server.conf HTTP/1.1\r\nHost: localhost\r\n\r\n")
            assert_status(res, 403)
            print("integration HTTP tests passed")
            return 0
        finally:
            proc.terminate()
            try: proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill(); proc.wait(timeout=3)
if __name__ == "__main__":
    raise SystemExit(main())

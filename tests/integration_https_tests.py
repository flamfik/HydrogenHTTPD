#!/usr/bin/env python3
import os, socket, ssl, subprocess, sys, tempfile, time
from pathlib import Path

def wait_for_port(port, timeout=8.0):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError("server did not open HTTPS port")

def send_https(port, req):
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    with socket.create_connection(("127.0.0.1", port), timeout=3) as raw:
        with ctx.wrap_socket(raw, server_hostname="localhost") as s:
            s.settimeout(3)
            s.sendall(req)
            chunks = []
            while True:
                try: part = s.recv(4096)
                except socket.timeout: break
                if not part: break
                chunks.append(part)
            return b"".join(chunks)

def assert_status(res, status):
    first = res.split(b"\r\n", 1)[0]
    expected = f"HTTP/1.1 {status} ".encode()
    if not first.startswith(expected):
        raise AssertionError((status, first, res[:200]))

def main():
    exe = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        www = work / "www"; logs = work / "logs"; certs = work / "certs"
        www.mkdir(); logs.mkdir(); certs.mkdir()
        (www / "index.html").write_text("tls ok", encoding="utf-8")
        crt = certs / "server.crt"; key = certs / "server.key"

        subprocess.check_call([
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-sha256",
            "-days", "1", "-nodes", "-keyout", str(key), "-out", str(crt),
            "-subj", "/CN=localhost"
        ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        cfg = work / "server.conf"
        cfg.write_text("\n".join([
            "port = 18081",
            "enable_tls = true",
            "tls_port = 18443",
            f"tls_cert_file = {crt}",
            f"tls_key_file = {key}",
            f"default_root = {www}",
            f"access_log = {logs/'access.log'}",
            f"error_log = {logs/'error.log'}",
            "max_request_bytes = 2048",
            "rate_limit_per_minute = 100",
            "read_timeout_seconds = 2",
            "worker_threads = 2",
            "max_pending_connections = 8",
            "enable_htaccess = true",
            f"vhost.localhost = {www}",
            ""
        ]), encoding="utf-8")

        proc = subprocess.Popen([str(exe), str(cfg)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, cwd=str(work))
        try:
            wait_for_port(18443)
            res = send_https(18443, b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
            assert_status(res, 200)
            assert b"tls ok" in res
            assert b"Strict-Transport-Security:" in res
            assert b"Server: HydrogenHttpd" in res
            print("integration HTTPS tests passed")
            return 0
        finally:
            proc.terminate()
            try: proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill(); proc.wait(timeout=3)

if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Small cross-platform HTTP/1.1 benchmark for HydrogenHttpd.

This client opens one connection per request because HydrogenHttpd v1.8.0 does
not implement keep-alive yet. For serious testing also use wrk, h2load, hey or ab
from a separate machine.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import socket
import statistics
import time
from dataclasses import dataclass


@dataclass
class Result:
    ok: bool
    latency_ms: float
    status: int


def request(host: str, port: int, path: str, timeout: float) -> Result:
    started = time.perf_counter()
    status = 0
    try:
        with socket.create_connection((host, port), timeout=timeout) as sock:
            sock.settimeout(timeout)
            raw = (
                f"GET {path} HTTP/1.1\r\n"
                f"Host: {host}\r\n"
                "Connection: close\r\n\r\n"
            ).encode("ascii")
            sock.sendall(raw)
            response = bytearray()
            while True:
                chunk = sock.recv(65536)
                if not chunk:
                    break
                response.extend(chunk)
        first = bytes(response).split(b"\r\n", 1)[0].split()
        status = int(first[1]) if len(first) >= 2 else 0
        ok = 200 <= status < 400
    except (OSError, ValueError, IndexError):
        ok = False
    return Result(ok, (time.perf_counter() - started) * 1000.0, status)


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, int(round((len(ordered) - 1) * fraction)))
    return ordered[index]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--path", default="/")
    parser.add_argument("--requests", type=int, default=10000)
    parser.add_argument("--concurrency", type=int, default=100)
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args()

    if args.requests <= 0 or args.concurrency <= 0:
        parser.error("requests and concurrency must be greater than zero")

    started = time.perf_counter()
    results: list[Result] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency) as pool:
        futures = [
            pool.submit(request, args.host, args.port, args.path, args.timeout)
            for _ in range(args.requests)
        ]
        for future in concurrent.futures.as_completed(futures):
            results.append(future.result())

    elapsed = time.perf_counter() - started
    latencies = [result.latency_ms for result in results]
    succeeded = sum(result.ok for result in results)
    statuses: dict[int, int] = {}
    for result in results:
        statuses[result.status] = statuses.get(result.status, 0) + 1

    print(f"requests={len(results)} succeeded={succeeded} failed={len(results) - succeeded}")
    print(f"elapsed_seconds={elapsed:.3f}")
    print(f"requests_per_second={len(results) / elapsed:.2f}")
    print(f"latency_mean_ms={statistics.mean(latencies):.3f}")
    print(f"latency_p50_ms={percentile(latencies, 0.50):.3f}")
    print(f"latency_p95_ms={percentile(latencies, 0.95):.3f}")
    print(f"latency_p99_ms={percentile(latencies, 0.99):.3f}")
    print("statuses=" + ",".join(f"{code}:{count}" for code, count in sorted(statuses.items())))
    return 0 if succeeded == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())

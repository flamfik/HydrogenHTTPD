#!/usr/bin/env python3
"""Generate a random HydrogenHttpd bearer token and its SHA-256 config entry."""

from __future__ import annotations

import argparse
import hashlib
import secrets
import time


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="upload", help="Token rule name")
    parser.add_argument("--scopes", default="upload", help="Comma-separated scopes")
    parser.add_argument("--bytes", type=int, default=32, dest="token_bytes", help="Random token bytes (minimum 24)")
    parser.add_argument("--ttl-days", type=int, default=0, help="Expiration in days; 0 means never")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.token_bytes < 24:
        raise SystemExit("--bytes must be at least 24")
    if args.ttl_days < 0:
        raise SystemExit("--ttl-days cannot be negative")

    name = args.name.strip()
    scopes = ",".join(scope.strip().lower() for scope in args.scopes.split(",") if scope.strip())
    if not name or not scopes:
        raise SystemExit("name and scopes cannot be empty")

    token = secrets.token_urlsafe(args.token_bytes)
    digest = hashlib.sha256(token.encode("utf-8")).hexdigest()
    expires = 0 if args.ttl_days == 0 else int(time.time()) + args.ttl_days * 86400
    expires_text = "never" if expires == 0 else str(expires)

    print("TOKEN (store securely; shown only now):")
    print(token)
    print()
    print("SECRETS FILE ENTRY:")
    print(f"token.{name} = sha256:{digest} | {scopes} | {expires_text}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

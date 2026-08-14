#!/usr/bin/env python3
"""Atomically list, rotate, add, or revoke HydrogenHttpd hashed bearer tokens."""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import secrets
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path

TOKEN_RE = re.compile(
    r"^\s*token\.([A-Za-z0-9_.-]+)\s*=\s*sha256:([0-9a-fA-F]{64})\s*"
    r"\|\s*([^|]+?)\s*\|\s*(never|0|[0-9]+)\s*$"
)
NAME_RE = re.compile(r"^[A-Za-z0-9_.-]+$")
SCOPE_RE = re.compile(r"^[A-Za-z0-9_.:-]+$")


@dataclass(frozen=True)
class TokenEntry:
    name: str
    digest: str
    scopes: tuple[str, ...]
    expires: str

    def serialize(self) -> str:
        return f"token.{self.name} = sha256:{self.digest} | {','.join(self.scopes)} | {self.expires}"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--file", type=Path, default=Path("secrets/auth.tokens"))

    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("list", help="List token names, scopes, and expiration")

    rotate = sub.add_parser("rotate", help="Create or replace a token and print its plaintext once")
    rotate.add_argument("--name", required=True)
    rotate.add_argument("--scopes", required=True, help="Comma-separated scopes")
    rotate.add_argument("--bytes", type=int, default=32, dest="token_bytes")
    rotate.add_argument("--ttl-days", type=int, default=0)

    revoke = sub.add_parser("revoke", help="Remove a token by name")
    revoke.add_argument("--name", required=True)

    return parser.parse_args()


def validate_name(name: str) -> str:
    value = name.strip()
    if not NAME_RE.fullmatch(value):
        raise SystemExit("token name may contain only letters, numbers, dot, underscore, and dash")
    return value


def parse_scopes(value: str) -> tuple[str, ...]:
    scopes = tuple(scope.strip().lower() for scope in value.split(",") if scope.strip())
    if not scopes:
        raise SystemExit("at least one scope is required")
    if any(not SCOPE_RE.fullmatch(scope) for scope in scopes):
        raise SystemExit("invalid scope")
    return scopes


def load_entries(path: Path) -> tuple[list[str], dict[str, TokenEntry]]:
    preserved: list[str] = []
    entries: dict[str, TokenEntry] = {}

    if not path.exists():
        return preserved, entries

    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        stripped = raw.strip()
        if not stripped or stripped.startswith("#"):
            preserved.append(raw)
            continue

        match = TOKEN_RE.fullmatch(raw)
        if not match:
            raise SystemExit(f"invalid token rule at {path}:{line_number}")

        name, digest, scopes_text, expires = match.groups()
        if name in entries:
            raise SystemExit(f"duplicate token name: {name}")

        entries[name] = TokenEntry(
            name=name,
            digest=digest.lower(),
            scopes=parse_scopes(scopes_text),
            expires=expires.lower(),
        )

    return preserved, entries


def atomic_write(path: Path, preserved: list[str], entries: dict[str, TokenEntry]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    lines = list(preserved)
    if lines and lines[-1].strip():
        lines.append("")
    lines.extend(entries[name].serialize() for name in sorted(entries))
    payload = "\n".join(lines).rstrip() + "\n"

    fd, temp_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    temp_path = Path(temp_name)

    try:
        if os.name != "nt":
            os.fchmod(fd, 0o600)

        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())

        os.replace(temp_path, path)
    finally:
        if temp_path.exists():
            temp_path.unlink()


def command_list(entries: dict[str, TokenEntry]) -> int:
    if not entries:
        print("No token rules.")
        return 0

    for name in sorted(entries):
        entry = entries[name]
        print(f"{entry.name}\tscopes={','.join(entry.scopes)}\texpires={entry.expires}")
    return 0


def command_rotate(args: argparse.Namespace, preserved: list[str], entries: dict[str, TokenEntry]) -> int:
    name = validate_name(args.name)
    scopes = parse_scopes(args.scopes)

    if args.token_bytes < 24:
        raise SystemExit("--bytes must be at least 24")
    if args.ttl_days < 0:
        raise SystemExit("--ttl-days cannot be negative")

    plaintext = secrets.token_urlsafe(args.token_bytes)
    digest = hashlib.sha256(plaintext.encode("utf-8")).hexdigest()
    expires_epoch = 0 if args.ttl_days == 0 else int(time.time()) + args.ttl_days * 86400
    expires = "never" if expires_epoch == 0 else str(expires_epoch)

    entries[name] = TokenEntry(name, digest, scopes, expires)
    atomic_write(args.file, preserved, entries)

    print("TOKEN (store securely; shown only now):")
    print(plaintext)
    print()
    print(f"Updated token.{name} in {args.file}")
    return 0


def command_revoke(args: argparse.Namespace, preserved: list[str], entries: dict[str, TokenEntry]) -> int:
    name = validate_name(args.name)
    if name not in entries:
        raise SystemExit(f"token not found: {name}")

    del entries[name]
    atomic_write(args.file, preserved, entries)
    print(f"Revoked token.{name} in {args.file}")
    return 0


def main() -> int:
    args = parse_args()
    preserved, entries = load_entries(args.file)

    if args.command == "list":
        return command_list(entries)
    if args.command == "rotate":
        return command_rotate(args, preserved, entries)
    if args.command == "revoke":
        return command_revoke(args, preserved, entries)

    raise SystemExit("unsupported command")


if __name__ == "__main__":
    raise SystemExit(main())

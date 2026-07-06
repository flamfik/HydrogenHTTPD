# Contributing to HydrogenHttpd

Thanks for your interest in contributing.

HydrogenHttpd is an experimental C++ HTTP/HTTPS server with a security-conscious design. Contributions should preserve the project goals:

- small, readable codebase,
- conservative defaults,
- secure-by-default behavior,
- no shell execution,
- no arbitrary SQL-over-HTTP,
- minimal attack surface.

## Development setup

```bash
mkdir build
cd build
cmake ..
cmake --build .
ctest --output-on-failure
```

## Code style

- Use C++20.
- Prefer simple standard library code.
- Keep functions small where reasonable.
- Avoid global mutable state.
- Do not add dependencies unless clearly justified.
- Handle errors explicitly.
- Prefer deny-by-default security behavior.

## Pull request checklist

Before opening a PR:

- build passes locally,
- `ctest --output-on-failure` passes,
- new behavior has tests,
- README/docs updated if needed,
- no secrets, certificates, private keys, logs, or databases committed.

## Security-sensitive changes

For features touching request parsing, path resolution, TLS, PHP/FastCGI, `.htaccess`, SQL, file access, or process execution:

- add tests,
- document threat model,
- avoid shell execution,
- avoid interpreting untrusted input as code or SQL,
- preserve safe defaults.

## Commit style

Recommended style:

```txt
feat: add FastCGI PHP module
fix: block .htaccess direct access
test: add HTTPS integration test
docs: update configuration reference
```

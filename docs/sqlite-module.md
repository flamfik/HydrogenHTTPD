# SQLite Module

HydrogenHttpd includes an optional SQLite module.

## Purpose

The SQLite module is intended for internal server/app data, such as:

- structured events,
- local metadata,
- future demo apps,
- safe prepared-statement examples.

## Not supported

HydrogenHttpd does not expose arbitrary SQL execution over HTTP.

This is intentional. A generic SQL-over-HTTP endpoint would be an injection risk and poor default design.

## Build

```bash
cmake -S . -B build -DHYDROGENHTTPD_ENABLE_SQLITE=ON
cmake --build build
```

Disable:

```bash
cmake -S . -B build -DHYDROGENHTTPD_ENABLE_SQLITE=OFF
```

## Config

```ini
enable_sql = true
sqlite_database = sql/hydrogen.db
```

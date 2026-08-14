# HydrogenHttpd v1.9.0 Release Notes

## Cross-platform installation

This release turns the source project into an installable server distribution for Windows, Linux and macOS.

### Windows

- native Windows Service mode,
- NSIS setup project,
- portable ZIP workflow,
- Program Files / ProgramData separation,
- LocalService runtime identity,
- configuration validation during installation.

### Linux

- DEB package,
- RPM release workflow,
- portable TGZ,
- dedicated `hydrogenhttpd` user,
- hardened systemd service,
- logrotate and tmpfiles integration.

### macOS

- PKG builder,
- LaunchDaemon,
- dedicated `_hydrogenhttpd` account,
- Application Support / Library Logs separation,
- portable archive workflow.

## Operational improvements

- relative configuration paths are anchored to the configuration file,
- configuration can be checked before service startup,
- Linux/macOS termination signals stop the io_context gracefully,
- standard uninstall preserves website content, databases, uploads, logs and configuration.

## Verification

- TLS + SQLite: 21/21 tests passed,
- no TLS / no SQLite: 19/19 tests passed,
- GCC 14.2.0 with warnings treated as errors,
- DEB package contents and lifecycle scripts inspected,
- Linux portable archive extracted and checked,
- macOS plist parsed successfully,
- shell installer scripts passed syntax validation.

Windows EXE and macOS PKG are built by the included target-native GitHub Actions workflow because native installer toolchains are not available in the Linux preparation environment.

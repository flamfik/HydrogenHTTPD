# Building Native Installers

## Linux

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DHYDROGENHTTPD_BUILD_TESTS=ON \
  -DHYDROGENHTTPD_ENABLE_TLS=ON \
  -DHYDROGENHTTPD_ENABLE_SQLITE=ON

cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G DEB
./packaging/linux/build_portable.sh "$PWD/build" "$PWD/dist"
```

## Windows

Requirements:

- Visual Studio C++ build tools,
- CMake,
- Ninja,
- vcpkg,
- NSIS.

After building and staging:

```powershell
cmake --install build --prefix "$PWD\stage"

.\packaging\windows\build_installer.ps1 `
  -StageDirectory "$PWD\stage" `
  -OutputDirectory "$PWD\dist"
```

## macOS

Requirements:

- Xcode command line tools,
- CMake,
- Ninja,
- Boost,
- OpenSSL,
- SQLite,
- `pkgbuild`,
- `productbuild`.

```bash
DESTDIR="$PWD/stage" cmake --install build --prefix /usr/local
./packaging/macos/build_pkg.sh "$PWD/stage" "$PWD/dist" "$(uname -m)"
```

## Automated builds

`.github/workflows/installers.yml` builds:

- Linux DEB and portable TGZ,
- Windows NSIS setup EXE and portable ZIP,
- macOS PKG and portable TGZ.

The workflow runs manually or when a Git tag matching `v*` is pushed.

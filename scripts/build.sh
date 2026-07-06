#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build \
  -DHYDROGENHTTPD_BUILD_TESTS=ON \
  -DHYDROGENHTTPD_ENABLE_TLS=ON \
  -DHYDROGENHTTPD_ENABLE_SQLITE=ON

cmake --build build --parallel
ctest --test-dir build --output-on-failure

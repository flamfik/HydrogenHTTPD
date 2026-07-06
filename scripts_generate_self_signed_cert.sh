#!/usr/bin/env bash
set -euo pipefail
mkdir -p certs
openssl req -x509 \
  -newkey rsa:2048 \
  -sha256 \
  -days 365 \
  -nodes \
  -keyout certs/server.key \
  -out certs/server.crt \
  -subj "/CN=localhost"
echo "Generated certs/server.crt and certs/server.key"
echo "Now set enable_tls = true in server.conf"

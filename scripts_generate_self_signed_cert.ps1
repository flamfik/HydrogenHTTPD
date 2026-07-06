New-Item -ItemType Directory -Force -Path certs | Out-Null
openssl req -x509 `
  -newkey rsa:2048 `
  -sha256 `
  -days 365 `
  -nodes `
  -keyout certs/server.key `
  -out certs/server.crt `
  -subj "/CN=localhost"
Write-Host "Generated certs/server.crt and certs/server.key"
Write-Host "Now set enable_tls = true in server.conf"

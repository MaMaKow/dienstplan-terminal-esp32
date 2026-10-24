#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CERT_DIR="$SCRIPT_DIR/certs"
PHP_PORT="${MOCK_PHP_PORT:-8088}"
TLS_PORT="${MOCK_TLS_PORT:-8443}"
PHP_BIN="${PHP_BIN:-php}"
STUNNEL_BIN="${STUNNEL_BIN:-stunnel}"

stunnel_pid=''
config_file=''
san_config=''
cleanup() {
  local status=$?
  if [[ -n "$stunnel_pid" ]] && kill -0 "$stunnel_pid" 2>/dev/null; then
    kill "$stunnel_pid" 2>/dev/null || true
    wait "$stunnel_pid" 2>/dev/null || true
  fi
  [[ -z "$config_file" ]] || rm -f -- "$config_file"
  [[ -z "$san_config" ]] || rm -f -- "$san_config"
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

fail() {
  printf 'Fehler: %s\n' "$*" >&2
  exit 1
}
for command in "$PHP_BIN" "$STUNNEL_BIN" openssl hostname; do
  command -v "$command" >/dev/null 2>&1 || fail "Programm nicht gefunden: $command"
done

hostname_fqdn="$(hostname -f 2>/dev/null | tr -d '\r\n' || true)"
[[ -n "$hostname_fqdn" ]] || hostname_fqdn="$(hostname | tr -d '\r\n')"
hostname_short="$(hostname -s 2>/dev/null | tr -d '\r\n' || true)"
[[ -n "$hostname_short" ]] || hostname_short="$hostname_fqdn"

# Prefer the source address of the default route; fall back to the first LAN address.
lan_ip="${MOCK_IP:-}"
if [[ -z "$lan_ip" ]] && command -v ip >/dev/null 2>&1; then
  lan_ip="$(ip -o route get 1.1.1.1 2>/dev/null | awk '{for (i=1;i<=NF;i++) if ($i=="src") {print $(i+1); exit}}')"
fi
if [[ -z "$lan_ip" ]]; then
  lan_ip="$(hostname -I 2>/dev/null | awk '{for (i=1;i<=NF;i++) if ($i !~ /^127\./ && $i !~ /^169\.254\./) {print $i; exit}}')"
fi
[[ -n "$lan_ip" ]] || fail "Keine LAN-IP gefunden. Setze MOCK_IP auf die Adresse des Netzwerkinterfaces."
[[ "$lan_ip" =~ ^[0-9a-fA-F:.]+$ ]] || fail "Ungültige IP-Adresse: $lan_ip"
[[ "$hostname_fqdn" =~ ^[A-Za-z0-9.-]+$ ]] || fail "Hostname enthält nicht unterstützte Zeichen: $hostname_fqdn"
[[ "$hostname_short" =~ ^[A-Za-z0-9.-]+$ ]] || fail "Hostname enthält nicht unterstützte Zeichen: $hostname_short"

mkdir -p "$CERT_DIR"
ca_key="$CERT_DIR/dev-ca.key"
ca_cert="$CERT_DIR/dev-ca.pem"
server_key="$CERT_DIR/mock-server.key"
server_csr="$CERT_DIR/mock-server.csr"
server_cert="$CERT_DIR/mock-server.crt"
san_config="$(mktemp "$CERT_DIR/.san-runtime.XXXXXX")"

# The CA private key is local-only. Create a matching CA pair if either half is absent.
if [[ ! -s "$ca_key" || ! -s "$ca_cert" ]] || ! openssl verify -CAfile "$ca_cert" "$ca_cert" >/dev/null 2>&1; then
  printf 'Erzeuge lokale Dev-CA (dev-ca.pem). Firmware muss diese CA vertrauen.\n'
  openssl req -x509 -newkey rsa:2048 -sha256 -keyout "$ca_key" -out "$ca_cert" \
    -days 3650 -nodes -subj "/CN=Dienstplan Dev CA"
  chmod 600 "$ca_key"
fi
# A mismatched local CA key/certificate cannot sign valid server certificates.
ca_key_pub="$(openssl pkey -in "$ca_key" -pubout 2>/dev/null | openssl dgst -sha256)"
ca_cert_pub="$(openssl x509 -in "$ca_cert" -pubkey -noout | openssl dgst -sha256)"
if [[ "$ca_key_pub" != "$ca_cert_pub" ]]; then
  printf 'Vorhandener CA-Key passt nicht zum CA-Zertifikat; erzeuge ein neues CA-Paar. Firmware muss die neue dev-ca.pem übernehmen.\n' >&2
  openssl req -x509 -newkey rsa:2048 -sha256 -keyout "$ca_key" -out "$ca_cert" \
    -days 3650 -nodes -subj "/CN=Dienstplan Dev CA"
  chmod 600 "$ca_key"
fi

if [[ ! -s "$server_key" ]]; then
  openssl genrsa -out "$server_key" 2048
  chmod 600 "$server_key"
fi
openssl req -new -key "$server_key" -out "$server_csr" -subj "/CN=$hostname_fqdn"
# Alle nicht-Loopback-, nicht-Link-Local-IPv4-Adressen einsammeln
mapfile -t lan_ips < <(
  ip -o -4 addr show scope global \
    | awk '{split($4,a,"/"); print a[1]}' \
    | grep -Ev '^(127\.|169\.254\.)'
)
[[ ${#lan_ips[@]} -gt 0 ]] || fail "Keine LAN-IPv4-Adresse gefunden."
[[ -z "${MOCK_IP:-}" ]] || lan_ips=("$MOCK_IP")

san_entries=""
for ip in "${lan_ips[@]}"; do
  san_entries+="IP:$ip,"
done
san_entries+="IP:127.0.0.1,DNS:localhost,DNS:$hostname_short"
printf 'subjectAltName = %s\n' "$san_entries" >"$san_config"
openssl x509 -req -sha256 -in "$server_csr" -CA "$ca_cert" -CAkey "$ca_key" \
  -CAcreateserial -out "$server_cert" -days 825 -extfile "$san_config"
chmod 644 "$server_cert" "$ca_cert"

# Keep the user's stunnel settings, but use runtime ports and the generated cert paths.
# Running stunnel in foreground lets this script cleanly stop it when PHP exits.
config_file="$(mktemp "${TMPDIR:-/tmp}/dienstplan-stunnel.XXXXXX.conf")"
cat >"$config_file" <<EOF
foreground = yes
[https]
accept = 0.0.0.0:$TLS_PORT
connect = 127.0.0.1:$PHP_PORT
cert = $server_cert
key = $server_key
EOF

printf '\nDienstplan Mock-Server startet\n'
printf 'Hostname: %s\n' "$hostname_fqdn"
printf 'LAN-IP:   %s\n' "$lan_ip"
printf 'HTTPS:    https://%s:%s/v1\n' "$lan_ip" "$TLS_PORT"
printf 'CA:       %s\n' "$ca_cert"
printf 'PHP:      127.0.0.1:%s\n\n' "$PHP_PORT"
printf 'Für die Firmware muss dev-ca.pem als vertrauenswürdige CA eingebunden sein.\n'
printf 'Beenden mit Ctrl+C.\n\n'

"$STUNNEL_BIN" "$config_file" &
stunnel_pid=$!
# Wait briefly for stunnel to bind before starting the backend.
for _ in {1..30}; do
  kill -0 "$stunnel_pid" 2>/dev/null || fail "stunnel konnte nicht gestartet werden."
  if command -v ss >/dev/null 2>&1 && ss -ltn 2>/dev/null | grep -q ":$TLS_PORT "; then break; fi
  sleep 0.1
done

cd "$SCRIPT_DIR"
"$PHP_BIN" -S "127.0.0.1:$PHP_PORT" router.php

#!/bin/bash
# Project 2 OTA test helper (run on the Mac).
#
# Usage:
#   ./ota_test.sh <path-to-firmware.bin>            # correct hash   (Stage 6 / 8)
#   ./ota_test.sh <path-to-firmware.bin> badhash    # wrong hash     (Stage 7)
#
# What it does: copies the .bin to a temp folder, serves it over HTTP on port 8000,
# then publishes the OTA message to the broker. Keep this window open until the
# board has finished downloading (watch the idf.py monitor), then press Enter.
#
# Needs: the Docker broker container (default name lgs_mosquitto) running, and the
# Mac + board on the same WiFi.  Override with:  MAC_IP=10.0.0.21 BROKER_CONTAINER=...

set -e
BIN="$1"
MODE="$2"
TOPIC="${OTA_TOPIC:-topic_ota_1}"           # must match TOPIC_OTA in Config.h
BROKER_CONTAINER="${BROKER_CONTAINER:-lgs_mosquitto}"
PORT=8000

if [ -z "$BIN" ] || [ ! -f "$BIN" ]; then
  echo "Usage: $0 <path-to-firmware.bin> [badhash]"; exit 1
fi

MAC_IP="${MAC_IP:-$(ipconfig getifaddr en0 2>/dev/null || true)}"
if [ -z "$MAC_IP" ]; then echo "Could not detect Mac IP. Run: MAC_IP=x.x.x.x $0 ..."; exit 1; fi

WWW="$(mktemp -d)"
cp "$BIN" "$WWW/firmware.bin"
SIZE=$(wc -c < "$WWW/firmware.bin" | tr -d ' ')
HASH=$(shasum -a 256 "$WWW/firmware.bin" | awk '{print $1}')

if [ "$MODE" = "badhash" ]; then
  HASH="0000000000000000000000000000000000000000000000000000000000000000"
  echo ">>> Using a deliberately WRONG hash"
fi

echo "Mac IP : $MAC_IP"
echo "Size   : $SIZE bytes"
echo "Hash   : $HASH"

python3 -m http.server "$PORT" --directory "$WWW" >/dev/null 2>&1 &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null; rm -rf "$WWW"' EXIT
sleep 1

URL="http://$MAC_IP:$PORT/firmware.bin"
if ! curl -sfI "$URL" >/dev/null; then echo "Cannot fetch $URL from this Mac - firewall?"; exit 1; fi
echo "Serving: $URL"

MSG="{\"url\":\"$URL\",\"hash\":\"$HASH\"}"
echo "Publishing to $TOPIC: $MSG"
docker exec "$BROKER_CONTAINER" mosquitto_pub -t "$TOPIC" -m "$MSG"

echo
echo "Watch the board's monitor. Press Enter when the download has finished."
read -r _

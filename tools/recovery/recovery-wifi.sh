#!/bin/bash
# <wtl> project: wtlwireless | file: tools/recovery/recovery-wifi.sh | author: watchthelight </wtl>
# Wi-Fi from macOS Recovery or a USB installer with WTLWireless. Run from Terminal (Utilities > Terminal):
#   bash /Volumes/<EFI or stick>/WTLWireless/recovery-wifi.sh [ID]
# With the native part engaged, the Wi-Fi menu in the menu bar does the work and this only records what happened.
# Without it (another macOS build, or the core alone), it scans, asks which network and its password, and joins.
# Nothing is stored: the password goes from the keyboard to the driver as a PMK and is gone at the next restart.
# Then it runs wifi.sh (DHCP, ping, captive.apple.com) and writes a capture to wifi-<ID>/ next to this script.
ID="${1:-recovery}"
DIR="$(cd "$(dirname "$0")" && pwd)"
MTK="$DIR/mtkwifi"
D="$DIR/wifi-$ID"
mkdir -p "$D" || { echo "cannot write $D"; exit 1; }

[ -x "$MTK" ] || chmod +x "$MTK" 2>/dev/null
if ! ioreg -r -c WTLWireless -d 1 | grep -q WTLWireless; then
  echo "WTLWireless is not loaded: check that WTLWireless.kext is in EFI/OC/Kexts and enabled in config.plist"
  kmutil showloaded 2>/dev/null | grep -i watchthelight > "$D/kexts.txt"
  exit 1
fi
"$MTK" status | tee "$D/status-before.txt"

if grep -q "front end:  native" "$D/status-before.txt"; then
  echo
  echo "The native front end is up: use the Wi-Fi menu at the top right to pick a network."
  echo "Press Return here once you are connected (or to just save the capture)."
  read -r _
else
  echo
  echo "== scanning (up to 25 s)"
  "$MTK" scan --wait | tee "$D/scan.txt"
  echo
  printf "network name (exactly as listed, empty = skip): "
  read -r SSID
  if [ -n "$SSID" ]; then
    printf "is it open, with no password? [y/N] "
    read -r OPEN
    if [ "$OPEN" = y ] || [ "$OPEN" = Y ]; then "$MTK" join "$SSID" --open | tee "$D/join.txt"
    else "$MTK" join "$SSID" --ask | tee "$D/join.txt"; fi
  fi
fi

"$MTK" status > "$D/status-after.txt" 2>&1
ioreg -r -c WTLWireless -l -w0 > "$D/wtlwireless.txt" 2>&1
ioreg -r -c WTLWirelessNative -l -w0 > "$D/wtlwirelessnative.txt" 2>&1
/usr/bin/log show --last 10m --predicate 'process == "airportd" OR process == "WiFiAgent" OR sender BEGINSWITH "WTLWireless"' \
  > "$D/log.txt" 2>&1
sync
# DHCP, router ping, captive portal check and a short speed test (also captures ifconfig and routes)
[ -f "$DIR/wifi.sh" ] && bash "$DIR/wifi.sh" "$ID"
sync
echo "capture: $D"

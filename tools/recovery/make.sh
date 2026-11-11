#!/bin/bash
# <wtl> project: wtlwireless | file: tools/recovery/make.sh | author: watchthelight </wtl>
# Assembles the Recovery folder (recovery-wifi.sh, mtkwifi, wifi.sh, README) in out/recovery/WTLWireless, ready to copy to
# the root of the EFI partition or the USB stick. Files are overwritten in place.
set -euo pipefail
cd "$(dirname "$0")/../.."
bash tools/mtkwifi/build.sh >/dev/null
O=out/recovery/WTLWireless
mkdir -p "$O"
cp tools/recovery/recovery-wifi.sh tools/recovery/README.md tools/wifi.sh tools/mtkwifi/out/mtkwifi "$O/"
chmod +x "$O/recovery-wifi.sh" "$O/wifi.sh" "$O/mtkwifi"
echo "$O: $(ls "$O" | tr '\n' ' ')"

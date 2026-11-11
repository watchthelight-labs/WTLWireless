#!/bin/bash
# <wtl> project: wtlwireless | file: tools/bundle/make-bundle.sh | author: watchthelight </wtl>
# Packs the release zip from the bundle build.sh left in out/: WTLWireless.kext, INSTALL.md, the config snippet, the
# Recovery folder, the licence files and SHA256SUMS.txt, as WTLWireless-<version>-RELEASE.zip.
#   ./build.sh && tools/bundle/make-bundle.sh [out dir]
set -euo pipefail
cd "$(dirname "$0")/../.."
K=out/WTLWireless.kext
[ -d "$K/Contents/PlugIns/WTLWirelessBt.kext" ] || { echo "FAIL: no bundle in $K, run ./build.sh first"; exit 1; }
V=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$K/Contents/Info.plist")
OUT=${1:-$PWD/out}; W=$(mktemp -d); B="$W/WTLWireless-$V"; mkdir -p "$B" "$OUT"
cp -Rp "$K" "$B/"
cp -R LICENSE NOTICE.md LICENSES tools/bundle/INSTALL.md tools/bundle/config-snippet.plist "$B/"
# the Recovery / installer helper folder (mtkwifi is built here)
bash tools/recovery/make.sh >/dev/null && cp -R out/recovery/WTLWireless "$B/Recovery"
{ echo "WTLWireless $V, packed $(date -u +%F)"; echo
  (cd "$B" && find . -type f -path '*/MacOS/*' | sort | sed 's|^\./||' | while read -r f; do shasum -a 256 "$f"; done)
  (cd "$B" && shasum -a 256 Recovery/mtkwifi); } > "$B/SHA256SUMS.txt"
Z="$OUT/WTLWireless-$V-RELEASE.zip"; rm -f "$Z"
(cd "$W" && zip -qr -X "$Z" "WTLWireless-$V")
echo "$Z"; unzip -l "$Z" | tail -1; cat "$B/SHA256SUMS.txt"

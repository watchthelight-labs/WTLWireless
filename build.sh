#!/bin/bash
# <wtl> project: wtlwireless | file: build.sh | author: watchthelight </wtl>
# Builds WTLWireless.kext on macOS with the Command Line Tools (no Xcode project, no SDK clone, no Linux box):
#   out/WTLWireless.kext                           core: PCI driver, firmware, Ethernet fallback (src/core)
#     Contents/PlugIns/WTLWirelessNative.kext      the macOS Wi-Fi menu front end (src/native)
#     Contents/PlugIns/WTLWirelessBt.kext          Bluetooth firmware loader (src/bt)
# The three parts share one version and the script stops when they disagree. Nothing is loaded or installed.
#   ./build.sh               the native part is built for the macOS build this Mac runs
#   ABI=25G416 ./build.sh    ... or against the headers in abi/25G416
# NO_KMUTIL=1 and NO_LINKCHECK=1 skip the two checks that read the installed kernel collections.
# The build is reproducible: the same sources with the same Apple clang give the same three binaries.
set -euo pipefail
cd "$(dirname "$0")"
PB=/usr/libexec/PlistBuddy
ver() { $PB -c 'Print :CFBundleVersion' "src/$1/Info.plist"; }
bid() { $PB -c 'Print :CFBundleIdentifier' "src/$1/Info.plist"; }

V=$(ver core)
for p in native bt; do
  [ "$(ver $p)" = "$V" ] || { echo "FAIL: src/$p is $(ver $p), the core is $V"; exit 1; }
done
grep -q "KEXT_VERSION := $V\$" Makefile || { echo "FAIL: Makefile KEXT_VERSION differs from $V"; exit 1; }
grep -q "MTK_NATIVE_VERSION \"$V\"" src/native/WTLWirelessNative.cpp || { echo "FAIL: MTK_NATIVE_VERSION is not $V"; exit 1; }
grep -q "\"MtkBtVersion\", \"$V\"" src/bt/WTLWirelessBt.cpp || { echo "FAIL: MtkBtVersion is not $V"; exit 1; }
NEED=$($PB -c 'Print :OSBundleLibraries:org.watchthelight.WTLWireless' src/native/Info.plist)
[ "$NEED" = "$V" ] || { echo "FAIL: the native part asks for core $NEED, this is $V"; exit 1; }
ABI=${ABI:-$(sw_vers -buildVersion)}
if [ ! -d "abi/$ABI" ]; then
  echo "FAIL: no ABI headers in abi/$ABI. Shipped: $(ls abi | tr '\n' ' ')"
  echo "  Build against one of them (ABI=<build> ./build.sh): the native part then loads only where its allow-list matches"
  echo "  and the core's Ethernet port takes over elsewhere. For a new macOS build see tools/kcabi/README.md."
  exit 1
fi

# macOS 15 Sequoia (24x builds) has the older IO80211 generation: other slots and payload layouts (src/native/MtkAbiCompat.h)
case "$ABI" in
  24*) ABIDEF="-DMTK_ABI_SEQUOIA=1"; GEN=sequoia ;;
  *)   ABIDEF=""; GEN=tahoe ;;
esac

SDK=$(xcrun --sdk macosx --show-sdk-path)
K=$SDK/System/Library/Frameworks/Kernel.framework/Headers
RES=$(clang -print-resource-dir)
rm -rf out/parts out/obj out/WTLWireless.kext
mkdir -p out/obj/core out/obj/native out/obj/bt out/gen

# the SDK marks IOPCIDevice and IOEthernet* deprecated (DriverKit); MacKernelSDK in the Linux build does not
FLAGS="-arch x86_64 -mmacosx-version-min=10.15 -mkernel -nostdinc -fno-builtin -fno-common -fno-stack-protector
 -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -isystem $RES/include -I$K -O2 -g0 -Wall -Wno-unused-parameter
 -Wno-deprecated-declarations"
CXXFLAGS="$FLAGS -fapple-kext -std=gnu++17 -nostdinc++ -fno-exceptions -fno-rtti -fno-use-cxa-atexit
 -fno-register-global-dtors-with-atexit -Wno-inconsistent-missing-override"

# link <part> <name> <objects...>: kmod_info with the part's bundle ID, then the kext binary and its plist
link() {
  local part=$1 name=$2; shift 2
  local c=out/parts/$name.kext/Contents
  mkdir -p "$c/MacOS"
  clang $FLAGS -std=gnu11 -DKEXT_ID="$(bid "$part")" -DKEXT_VERSION="\"$V\"" -c src/kmod_info.c -o "out/obj/$part/kmod_info.o"
  # the SDK's libkmod objects say macOS 15.5, which ld notes against the 10.15 minimum (harmless)
  clang++ -arch x86_64 -mmacosx-version-min=10.15 -nostdlib -Xlinker -kext -Xlinker -export_dynamic \
    "$@" "out/obj/$part/kmod_info.o" -L"$SDK/usr/lib" -lkmodc++ -lkmod "$RES/lib/darwin/libclang_rt.cc_kext.a" \
    -o "$c/MacOS/$name" 2>&1 | grep -v 'was built for newer' >&2 || true
  [ -f "$c/MacOS/$name" ] || { echo "FAIL: link of $name"; exit 1; }
  cp "src/$part/Info.plist" "$c/Info.plist"
  # offline checks: MH_KEXT_BUNDLE, kmod_info, no C++ runtime symbols the kernel lacks
  otool -hv "$c/MacOS/$name" | grep -q KEXTBUNDLE || { echo "FAIL: $name is not MH_KEXT_BUNDLE"; exit 1; }
  nm "$c/MacOS/$name" | grep -q ' _kmod_info$' || { echo "FAIL: $name has no kmod_info"; exit 1; }
  nm -u "$c/MacOS/$name" | sort > "out/undefined-$part.txt"
  if grep -E '^_{2,3}(cxa_|Unwind|gxx_)|^__ZTV.*__cxxabiv|^__Znw|^__Zdl' "out/undefined-$part.txt"; then
    echo "FAIL: $name needs C++ runtime symbols the kernel does not export"; exit 1
  fi
  echo "$name: $(wc -l < "out/undefined-$part.txt" | tr -d ' ') undefined symbols (out/undefined-$part.txt)"
}

echo "=== core"
# one source list for this build and the Linux one: the SRCS lines of the Makefile
SRCS=$(awk '/^SRCS *:=/{sub(/^SRCS *:= */,""); s=$0; while (s ~ /\\$/) {sub(/\\$/,"",s); getline; s=s $0}; print s}' Makefile)
# Wi-Fi firmware from firmware/mt7925 and firmware/mt7927, checked against their SHA256SUMS
python3 -I tools/gen_firmware.py out/gen/MtkFirmware.c
OBJS=""
for s in $SRCS; do
  o=out/obj/core/$(basename "${s%.*}").o
  case "$s" in
    *.cpp) clang++ $CXXFLAGS -Isrc/core -c "$s" -o "$o" ;;
    *.c)   clang $FLAGS -Isrc/core -std=gnu11 -c "$s" -o "$o" ;;
  esac
  OBJS="$OBJS $o"
done
link core WTLWireless $OBJS
# which OSBundleLibraries the undefined symbols need (kmutil reads the installed kernel collections, changes nothing)
if [ -n "${NO_KMUTIL:-}" ]; then
  echo "kmutil libraries: skipped (NO_KMUTIL)"
elif kmutil libraries -p out/parts/WTLWireless.kext --xml > out/libraries.plist 2> out/libraries.err; then
  plutil -p out/libraries.plist > out/libraries.txt
  echo "kmutil libraries: out/libraries.txt"
else
  echo "kmutil libraries: failed (out/libraries.err)"
fi

echo "=== native ($ABI, $GEN)"
OBJS=""
for s in MtkNativeTrace WTLWirelessNative WTLWirelessNativeIf WTLWirelessNativeReq; do
  clang++ $CXXFLAGS $ABIDEF -Iabi/$ABI -Isrc/core -Isrc/native -c src/native/$s.cpp -o out/obj/native/$s.o
  OBJS="$OBJS out/obj/native/$s.o"
done
link native WTLWirelessNative $OBJS
# a Sequoia native part carries only the Sequoia builds in its allow-list (src/native/Info.plist lists the Tahoe ones)
[ "$GEN" = sequoia ] && python3 -I tools/abi-plist.py out/parts/WTLWirelessNative.kext/Contents/Info.plist "abi/$ABI/abi.json"
# every undefined symbol must be exported by this Mac's kernel collection or by the core
if [ -n "${NO_LINKCHECK:-}" ]; then
  echo "link check: skipped (NO_LINKCHECK)"
elif [ -f /System/Library/KernelCollections/BootKernelExtensions.kc ]; then
  python3 -I tools/kcabi/linkcheck.py out/parts/WTLWirelessNative.kext --baseline out/parts/WTLWireless.kext \
    --own-lib org.watchthelight.WTLWireless || echo "(link check: see above)"
fi

echo "=== bluetooth"
# Bluetooth firmware from firmware/mt7925, checked against its SHA256SUMS and the prefix pinned in the script
python3 -I tools/gen_bt_firmware.py out/gen/MtkBtFirmware.c
BTFLAGS="-arch x86_64 -mmacosx-version-min=10.15 -mkernel -nostdinc -fno-builtin -fno-common -fno-stack-protector
 -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT -isystem $RES/include -I$K -O2 -Wall -Wno-unused-parameter"
clang++ $BTFLAGS -fapple-kext -std=gnu++17 -nostdinc++ -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wno-inconsistent-missing-override \
  -c src/bt/WTLWirelessBt.cpp -o out/obj/bt/WTLWirelessBt.o
clang $BTFLAGS -std=gnu11 -c out/gen/MtkBtFirmware.c -o out/obj/bt/MtkBtFirmware.o
link bt WTLWirelessBt out/obj/bt/WTLWirelessBt.o out/obj/bt/MtkBtFirmware.o

B=out/WTLWireless.kext
cp -Rp out/parts/WTLWireless.kext "$B"
mkdir -p "$B/Contents/PlugIns"
cp -Rp out/parts/WTLWirelessNative.kext out/parts/WTLWirelessBt.kext "$B/Contents/PlugIns/"
echo "=== bundle $B ($V, native part for $ABI, $GEN)"
# OpenCore wants one Kernel > Add entry per kext, the core before its plugins
for k in WTLWireless.kext WTLWireless.kext/Contents/PlugIns/WTLWirelessNative.kext WTLWireless.kext/Contents/PlugIns/WTLWirelessBt.kext; do
  P=out/$k/Contents
  EXE=$($PB -c 'Print :CFBundleExecutable' "$P/Info.plist")
  echo "BundlePath $k"
  echo "  ExecutablePath Contents/MacOS/$EXE, $($PB -c 'Print :CFBundleIdentifier' "$P/Info.plist") $V"
  echo "  sha256 $(shasum -a 256 "$P/MacOS/$EXE" | cut -c1-64)"
done

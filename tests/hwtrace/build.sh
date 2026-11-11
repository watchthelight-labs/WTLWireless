#!/bin/bash
# <wtl> project: mtkwlan | file: tests/hwtrace/build.sh | author: watchthelight </wtl>
# build.sh <driver-dir> <build-dir>: compile the driver sources that <driver-dir>/Makefile lists, plus the harness
# from this working tree, into <build-dir>/hwtrace. Host clang only, UBSan on, nothing outside <build-dir>.
# The sources sit beside the Makefile or in src/ under it; both layouts work.
set -eu -o pipefail
HW="$(cd "$(dirname "$0")" && pwd)"
MAIN="$(cd "$HW/../.." && pwd)"
DRV="$(cd "$1" && pwd)"
mkdir -p "$2"
B="$(cd "$2" && pwd)"
FWDIR="$MAIN/out/tests/hwtrace/fw"
mkdir -p "$FWDIR" "$B/obj"

# firmware: the generated file the kext build uses, else generate it here (script and blobs are in the repo)
FWC="$MAIN/out/gen/MtkFirmware.c"
if [ ! -f "$FWC" ]; then
    FWC="$FWDIR/MtkFirmware.c"
    GEN="$MAIN/gen_firmware.py"; [ -f "$GEN" ] || GEN="$MAIN/tools/gen_firmware.py"
    [ -f "$FWC" ] || (cd "$MAIN" && python3 "$GEN" "$FWC" >/dev/null)
fi
if [ ! -f "$FWDIR/MtkFirmware.o" ] || [ "$FWC" -nt "$FWDIR/MtkFirmware.o" ]; then
    clang -std=gnu11 -O0 -w -c "$FWC" -o "$FWDIR/MtkFirmware.o"
fi

# the kext's own source list, minus the IOKit networking front ends and the firmware blob
SRCS=$(awk '/^SRCS *:=/ { f = 1; sub(/^SRCS *:= */, "") } f { c = sub(/\\$/, ""); printf "%s ", $0; if (!c) exit }' "$DRV/Makefile")
UBSAN="-fsanitize=undefined -fno-sanitize-recover=undefined"
INC="$DRV"; [ -d "$DRV/src" ] && INC="$DRV/src"
CXXF="-std=gnu++17 -O0 -g $UBSAN -include $HW/shim/prelude.h -I$HW/shim -I$INC -w"
CF="-std=gnu11 -O0 -g $UBSAN -I$HW/shim -I$INC -w"
pids=(); objs=()
for s in $SRCS; do
    case "$(basename "$s")" in MtkWlanEth.cpp|MtkUserClient.cpp|MtkFirmware.c) continue;; esac
    o="$B/obj/$(basename "$s").o"; objs+=("$o")
    case "$s" in
        *.cpp) clang++ $CXXF -finstrument-functions -c "$DRV/$s" -o "$o" & ;;
        *.c)   clang   $CF   -finstrument-functions -c "$DRV/$s" -o "$o" & ;;
    esac
    pids+=($!)
done
for h in model.cpp host_shim.cpp trace_main.cpp; do
    o="$B/obj/hw_$h.o"; objs+=("$o")
    clang++ $CXXF -c "$HW/$h" -o "$o" &
    pids+=($!)
done
fail=0
for p in "${pids[@]}"; do wait "$p" || fail=1; done
[ $fail = 0 ] || { echo "build.sh: compile failed" >&2; exit 1; }
clang++ $UBSAN "${objs[@]}" "$FWDIR/MtkFirmware.o" -o "$B/hwtrace"

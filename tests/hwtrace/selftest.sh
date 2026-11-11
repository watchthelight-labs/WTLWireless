#!/bin/bash
# <wtl> project: mtkwlan | file: tests/hwtrace/selftest.sh | author: watchthelight </wtl>
# Sensitivity check: mutate a scratch copy of the driver sources one constant at a time and require that each
# mutation changes the trace. The real sources are never touched. Exit 0 when every mutation is seen.
set -eu -o pipefail
HW="$(cd "$(dirname "$0")" && pwd)"
MAIN="$(cd "$HW/../.." && pwd)"
OUT="$MAIN/out/tests/hwtrace/selftest"
SRC="$MAIN"; SUB=.; [ -d "$MAIN/src" ] && { SRC="$MAIN/src"; SUB=src; }   # sources beside the Makefile, or in src/
mkdir -p "$OUT"

"$HW/build.sh" "$MAIN" "$OUT/ref-build"
"$OUT/ref-build/hwtrace" "$OUT/ref.txt"

fail=0
# name | file | sed expression (BSD sed) | what it stands for
run_case() {
    local name="$1" file="$2" expr="$3" what="$4"
    local d="$OUT/$name"
    mkdir -p "$d/src/$SUB"
    cp "$MAIN/Makefile" "$d/src/"
    for f in "$SRC"/*.cpp "$SRC"/*.c "$SRC"/*.h "$SRC"/*.hpp; do cp "$f" "$d/src/$SUB/"; done
    if [ "$expr" != "-" ]; then
        sed -i '' "$expr" "$d/src/$SUB/$file"
        if cmp -s "$SRC/$file" "$d/src/$SUB/$file"; then echo "selftest: $name: the sed expression matched nothing" >&2; fail=1; return; fi
    fi
    "$HW/build.sh" "$d/src" "$d/build"
    "$d/build/hwtrace" "$d/trace.txt"
    if cmp -s "$OUT/ref.txt" "$d/trace.txt"; then
        if [ "$expr" = "-" ]; then echo "selftest: $name: identical, as it must be ($what)"
        else echo "selftest: $name: NOT DETECTED ($what)"; fail=1; fi
    else
        if [ "$expr" = "-" ]; then echo "selftest: $name: DIFFERS but nothing was changed ($what)"; fail=1
        else echo "selftest: $name: detected, $(diff "$OUT/ref.txt" "$d/trace.txt" | grep -c '^[<>]') changed lines ($what)"; fi
    fi
}

run_case control MtkRegs.cpp - "unmodified copy"
run_case regmap MtkChip.cpp 's/{ 0x820e4000, 0x021000,/{ 0x820e4000, 0x021004,/' "fixed register map: WF_LMAC_TOP BN0 TMAC base"
run_case prefetch MtkChip.cpp 's/{ 0x684, pf(0x0040, 0x4) }/{ 0x684, pf(0x0040, 0x8) }/' "prefetch table: depth of one ring"
run_case ringid MtkFw.cpp 's/TXQ_MCU_WM = 15/TXQ_MCU_WM = 14/' "DMA ring id: MCU command ring"
run_case rxring MtkChip.cpp 's/^        0, 2,$/        0, 3,/' "chip table: RX data ring number"
run_case rxirq MtkChip.cpp 's/^        1u << 0, 1u << 2,$/        1u << 0, 1u << 3,/' "chip table: RX data interrupt bit"
run_case irqmask MtkFw.cpp 's/INT_MCU_CMD = 1u << 29/INT_MCU_CMD = 1u << 28/' "IRQ enable mask: MCU command bit"
[ $fail = 0 ] && echo "selftest: all mutations detected" || { echo "selftest: FAILED" >&2; exit 1; }

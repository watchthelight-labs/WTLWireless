#!/bin/bash
# <wtl> project: wtlwireless | file: tests/hwtrace/selftest.sh | author: watchthelight </wtl>
# Sensitivity check: mutate a scratch copy of the driver sources one constant at a time and require that each
# mutation changes the trace. The real sources are never touched. Exit 0 when every mutation is seen.
set -eu -o pipefail
HW="$(cd "$(dirname "$0")" && pwd)"
MAIN="$(cd "$HW/../.." && pwd)"
OUT="$MAIN/out/tests/hwtrace/selftest"
SRC="$MAIN"; SUB=.; [ -d "$MAIN/src" ] && { SRC="$MAIN/src"; SUB=src; }   # sources beside the Makefile, in src/,
[ -d "$MAIN/src/core" ] && { SRC="$MAIN/src/core"; SUB=src/core; }                # or in src/core/
mkdir -p "$OUT"

"$HW/build.sh" "$MAIN" "$OUT/ref-build"
"$OUT/ref-build/hwtrace" "$OUT/ref.txt"
A7927="--pcidev 7927 --chip 7927 --bootarg mtk7927=1 --lnkctl 43"
"$OUT/ref-build/hwtrace" "$OUT/ref7927.txt" $A7927

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
    local args="" ref="$OUT/ref.txt"
    case "$name" in m7927-*) args="$A7927"; ref="$OUT/ref7927.txt";; esac
    "$d/build/hwtrace" "$d/trace.txt" $args
    if cmp -s "$ref" "$d/trace.txt"; then
        if [ "$expr" = "-" ]; then echo "selftest: $name: identical, as it must be ($what)"
        else echo "selftest: $name: NOT DETECTED ($what)"; fail=1; fi
    else
        if [ "$expr" = "-" ]; then echo "selftest: $name: DIFFERS but nothing was changed ($what)"; fail=1
        else echo "selftest: $name: detected, $(diff "$ref" "$d/trace.txt" | grep -c '^[<>]') changed lines ($what)"; fi
    fi
}

run_case control MtkRegs.cpp - "unmodified copy"
run_case regmap MtkChip.cpp 's/{ 0x820e4000, 0x021000,/{ 0x820e4000, 0x021004,/' "fixed register map: WF_LMAC_TOP BN0 TMAC base"
run_case prefetch MtkChip.cpp 's/{ 0x684, pf(0x0040, 0x4) }/{ 0x684, pf(0x0040, 0x8) }/' "prefetch table: depth of one ring"
run_case ringid MtkFw.cpp 's/TXQ_MCU_WM = 15/TXQ_MCU_WM = 14/' "DMA ring id: MCU command ring"
run_case rxring MtkChip.cpp 's/^        0, 2,$/        0, 3,/' "chip table: RX data ring number"
run_case rxirq MtkChip.cpp 's/^        1u << 0, 1u << 2,$/        1u << 0, 1u << 3,/' "chip table: RX data interrupt bit"
run_case irqmask MtkFw.cpp 's/INT_MCU_CMD = 1u << 29/INT_MCU_CMD = 1u << 28/' "IRQ enable mask: MCU command bit"
# the MT7927 row, against the trace of the opt-in run
run_case m7927-prefetch MtkChip.cpp 's/{ 0x698, pf(0x0080, 0x8) }/{ 0x698, pf(0x0080, 0x4) }/' "MT7927 prefetch table: depth of RX ring 6"
run_case m7927-rxring MtkChip.cpp 's/^        6, 4,  /        6, 5,  /' "MT7927 row: RX data ring number"
run_case m7927-irq MtkChip.cpp 's/^        1u << 14, 1u << 12,  /        1u << 20, 1u << 12,  /' "MT7927 row: MCU RX interrupt bit (outside GENMASK(18, 4))"
run_case m7927-cbtop MtkChip.cpp 's/0x1f8600, 0x1f5034, 0xc1604,/0x1f8600, 0x1f5038, 0xc1604,/' "MT7927 CBTOP reset: MCU own register"
run_case m7927-quirk MtkChip.cpp 's/MTK_Q_ASPM_OFF | MTK_Q_WFDMA_EXT | MTK_Q_BAND_IDX | MTK_Q_DBDC,/MTK_Q_ASPM_OFF | MTK_Q_WFDMA_EXT | MTK_Q_BAND_IDX,/' "MT7927 row: DBDC quirk off"
[ $fail = 0 ] && echo "selftest: all mutations detected" || { echo "selftest: FAILED" >&2; exit 1; }

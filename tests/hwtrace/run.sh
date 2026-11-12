#!/bin/bash
# <wtl> project: wtlwireless | file: tests/hwtrace/run.sh | author: watchthelight </wtl>
# Build the harness against the working-tree driver, run it twice, fail if the two traces differ.
# Writes out/tests/hwtrace/{trace.txt,functions-run.txt,functions-not-run.txt}.
set -eu -o pipefail
HW="$(cd "$(dirname "$0")" && pwd)"
MAIN="$(cd "$HW/../.." && pwd)"
OUT="$MAIN/out/tests/hwtrace"
B="$OUT/main"
mkdir -p "$OUT"

"$HW/build.sh" "$MAIN" "$B"
"$B/hwtrace" "$OUT/trace.txt" --cov "$OUT/functions-run.txt"
"$B/hwtrace" "$OUT/trace.2.txt"
if ! cmp -s "$OUT/trace.txt" "$OUT/trace.2.txt"; then
    echo "hwtrace: two runs differ" >&2
    diff -u "$OUT/trace.txt" "$OUT/trace.2.txt" | head -40 >&2 || true
    exit 1
fi
rm "$OUT/trace.2.txt"
LC_ALL=C sort -u -o "$OUT/functions-run.txt" "$OUT/functions-run.txt"

# every non-static driver function in the compiled objects, minus the ones that ran
nm -g -U $(ls "$B"/obj/*.o | grep -v '/hw_') | awk 'NF==3 && $2=="T" {print $3}' | sed 's/^_//' | c++filt \
    | sed 's/(.*//' | grep -E '^(WTLWireless::|MtkWlan::|mtk_)' | LC_ALL=C sort -u > "$OUT/functions-all.txt"
LC_ALL=C comm -23 "$OUT/functions-all.txt" "$OUT/functions-run.txt" > "$OUT/functions-not-run.txt"
LC_ALL=C comm -12 "$OUT/functions-all.txt" "$OUT/functions-run.txt" > "$OUT/functions-run-public.txt"

w=$(grep -c '^W ' "$OUT/trace.txt" || true)
r=$(grep -c '^R ' "$OUT/trace.txt" || true)
m=$(grep -c '^MCU ' "$OUT/trace.txt" || true)
f=$(grep -c '^FWDL ' "$OUT/trace.txt" || true)
n=$(wc -l < "$OUT/trace.txt" | tr -d ' ')
u=$(grep -c '^UNEXPECTED\|^CRASH' "$OUT/trace.txt" || true)
echo "hwtrace: ok, two runs identical, UBSan clean"
echo "hwtrace: $n events: W $w, R $r, MCU $m, FWDL $f, unexpected $u"
echo "hwtrace: functions run $(wc -l < "$OUT/functions-run-public.txt" | tr -d ' ') of $(wc -l < "$OUT/functions-all.txt" | tr -d ' ') (lists in $OUT)"
[ "$u" = 0 ] || { echo "hwtrace: the trace holds UNEXPECTED or CRASH lines" >&2; exit 1; }

# ops v6 (chip facts, channels with a band) must agree with the older calls
"$B/hwtrace" "$OUT/trace.ops6.txt" --ops6
if grep -q '^UNEXPECTED' "$OUT/trace.ops6.txt" || [ "$(grep -c '^OPS6 ' "$OUT/trace.ops6.txt")" != 3 ]; then
  echo "hwtrace: ops v6 check failed" >&2; grep '^UNEXPECTED\|^OPS6' "$OUT/trace.ops6.txt" >&2; exit 1
fi
grep '^OPS6 ' "$OUT/trace.ops6.txt" | sed 's/^/hwtrace: /'

# a card without a chip table row (an ID no row lists) must not be touched at all
"$B/hwtrace" "$OUT/trace.unknown.txt" --pcidev 7961
t=$(grep -c -v '^PHASE \|^STEP \|^RESULT ' "$OUT/trace.unknown.txt" || true)
[ "$t" = 0 ] || { echo "hwtrace: unknown card: $t hardware events, want 0" >&2; grep -v '^PHASE \|^STEP \|^RESULT ' "$OUT/trace.unknown.txt" | head -5 >&2; exit 1; }
echo "hwtrace: unknown card (14c3:7961): no hardware access"

# the MT7927 row is matched but opt-in: without boot-arg mtk7927=1 it is report only, no register or config access
"$B/hwtrace" "$OUT/trace.7927off.txt" --pcidev 7927 --chip 7927 --lnkctl 43 --props
t=$(grep -c -v '^PHASE \|^STEP \|^RESULT \|^PROP ' "$OUT/trace.7927off.txt" || true)
if [ "$t" != 0 ] || ! grep -q '^PROP MtkChipMatched MT7927' "$OUT/trace.7927off.txt" || ! grep -q '^PROP MtkChipUnsupported' "$OUT/trace.7927off.txt"; then
  echo "hwtrace: MT7927 without boot-arg: $t hardware events (want 0) or properties missing" >&2; head -8 "$OUT/trace.7927off.txt" >&2; exit 1
fi
echo "hwtrace: MT7927 (14c3:7927) without mtk7927=1: report only, no hardware access"
# a wrong value is no opt-in either
"$B/hwtrace" "$OUT/trace.7927two.txt" --pcidev 7927 --chip 7927 --bootarg mtk7927=2
[ "$(grep -c -v '^PHASE \|^STEP \|^RESULT ' "$OUT/trace.7927two.txt" || true)" = 0 ] || { echo "hwtrace: mtk7927=2 touched the card" >&2; exit 1; }

# the boot-arg changes nothing for an MT7925
"$B/hwtrace" "$OUT/trace.7925arg.txt" --bootarg mtk7927=1
cmp -s "$OUT/trace.txt" "$OUT/trace.7925arg.txt" || { echo "hwtrace: mtk7927=1 changed the MT7925 trace" >&2; exit 1; }
echo "hwtrace: MT7925 trace unchanged with mtk7927=1"

# opt-in run: whole bring-up, join and reload against the fake MT7927, checked against the Linux-derived golden sequence
"$B/hwtrace" "$OUT/trace.7927.txt" --pcidev 7927 --chip 7927 --bootarg mtk7927=1 --lnkctl 43 --props
if grep -q '^UNEXPECTED\|^CRASH' "$OUT/trace.7927.txt" || ! grep -q '^STEP W3 pass' "$OUT/trace.7927.txt" || ! grep -q '^RESULT up 1 ' "$OUT/trace.7927.txt"; then
  echo "hwtrace: MT7927 opt-in run failed" >&2; grep '^UNEXPECTED\|^CRASH\|^STEP\|^RESULT' "$OUT/trace.7927.txt" | head -12 >&2; exit 1
fi
python3 -I "$HW/golden.py" "$HW/golden-mt7927.txt" "$OUT/trace.7927.txt"
# ASPM L0s and L1 cleared in Link Control (0x43 -> 0x40), rings 6, 4 and 7 only, firmware named for MT6639, DBDC sent
grep -q '^PCI cfgw16 0x90 0x0040$' "$OUT/trace.7927.txt" || { echo "hwtrace: MT7927: ASPM not cleared" >&2; exit 1; }
if grep -q '^RING rx [0-3] ' "$OUT/trace.7927.txt" || [ "$(grep -c '^RING rx ' "$OUT/trace.7927.txt")" != 6 ]; then
  echo "hwtrace: MT7927: unexpected RX rings" >&2; grep '^RING rx' "$OUT/trace.7927.txt" >&2; exit 1
fi
grep -q '^PROP MtkFwPatchSha256 7a5aacf68ed8526c1d94ba962f84046496b3909562decf8bb690cfbd8de5b65e$' "$OUT/trace.7927.txt" || { echo "hwtrace: MT7927: wrong patch firmware" >&2; exit 1; }
grep -q '^PROP MtkW4Dbdc 0$' "$OUT/trace.7927.txt" || { echo "hwtrace: MT7927: DBDC command missing" >&2; exit 1; }
grep -q '^MCU uni cid 0x28 ' "$OUT/trace.7927.txt" || grep -q '^MCU uni cid 40 ' "$OUT/trace.7927.txt" || { echo "hwtrace: MT7927: no UNI command 0x28" >&2; exit 1; }
echo "hwtrace: MT7927 opt-in run: join and reload ok, golden sequence, ASPM off, rings 6/4/7, MT6639 firmware, DBDC"
# the other CHIPID words boards report are taken, an unknown one is not
"$B/hwtrace" "$OUT/trace.7927alt.txt" --pcidev 7927 --chip 7927 --chipid 6639 --bootarg mtk7927=1 --props
grep -q '^STEP W3 pass' "$OUT/trace.7927alt.txt" && grep -q '^PROP MtkChipIdRaw ' "$OUT/trace.7927alt.txt" || { echo "hwtrace: MT7927 with CHIPID 6639 did not come up" >&2; exit 1; }
"$B/hwtrace" "$OUT/trace.7927bad.txt" --pcidev 7927 --chip 7927 --chipid 7925 --bootarg mtk7927=1 --props
if grep -q '^STEP W2' "$OUT/trace.7927bad.txt" || ! grep -q '^PROP MtkChipUnsupported chip id does not match' "$OUT/trace.7927bad.txt" || grep -q '^W 0x0d' "$OUT/trace.7927bad.txt"; then
  echo "hwtrace: MT7927 with a wrong CHIPID went on" >&2; exit 1
fi
echo "hwtrace: MT7927 CHIPID words: 6639 taken, 7925 refused before any DMA setup"

# with the older MtkWlan class in the kernel the core must leave the card alone and say why
"$B/hwtrace" "$OUT/trace.oldset.txt" --oldset --props
t=$(grep -c '^W \|^R \|^MCU \|^FWDL ' "$OUT/trace.oldset.txt" || true)
if [ "$t" != 0 ] || ! grep -q 'MtkConflict' "$OUT/trace.oldset.txt"; then
  echo "hwtrace: old kext set present: $t hardware events (want 0) or no MtkConflict" >&2; head -5 "$OUT/trace.oldset.txt" >&2; exit 1
fi
echo "hwtrace: older MtkWlan class present: no hardware access, MtkConflict published"

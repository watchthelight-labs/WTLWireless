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

# a card without a chip table row (here the MT7927 ID) must not be touched at all
"$B/hwtrace" "$OUT/trace.unknown.txt" --pcidev 7927
t=$(grep -c -v '^PHASE \|^STEP \|^RESULT ' "$OUT/trace.unknown.txt" || true)
[ "$t" = 0 ] || { echo "hwtrace: unknown card: $t hardware events, want 0" >&2; grep -v '^PHASE \|^STEP \|^RESULT ' "$OUT/trace.unknown.txt" | head -5 >&2; exit 1; }
echo "hwtrace: unknown card (14c3:7927): no hardware access"

# with the older MtkWlan class in the kernel the core must leave the card alone and say why
"$B/hwtrace" "$OUT/trace.oldset.txt" --oldset --props
t=$(grep -c '^W \|^R \|^MCU \|^FWDL ' "$OUT/trace.oldset.txt" || true)
if [ "$t" != 0 ] || ! grep -q 'MtkConflict' "$OUT/trace.oldset.txt"; then
  echo "hwtrace: old kext set present: $t hardware events (want 0) or no MtkConflict" >&2; head -5 "$OUT/trace.oldset.txt" >&2; exit 1
fi
echo "hwtrace: older MtkWlan class present: no hardware access, MtkConflict published"

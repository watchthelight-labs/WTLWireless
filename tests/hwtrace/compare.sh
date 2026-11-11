#!/bin/bash
# <wtl> project: wtlwireless | file: tests/hwtrace/compare.sh | author: watchthelight </wtl>
# compare.sh [git-ref]: hardware trace of the driver sources at git-ref (default HEAD, via a temporary git worktree)
# against the working tree. Harness sources always come from the working tree. Exit 0 identical, 1 different.
set -eu -o pipefail
REF="${1:-HEAD}"
HW="$(cd "$(dirname "$0")" && pwd)"
MAIN="$(cd "$HW/../.." && pwd)"
OUT="$MAIN/out/tests/hwtrace"
mkdir -p "$OUT"
PREFIX="$(git -C "$MAIN" rev-parse --show-prefix)"
[ -n "$PREFIX" ] || PREFIX=.
WT="$(mktemp -d "$OUT/wt.XXXXXX")"
cleanup() {
    git -C "$MAIN" worktree remove --force "$WT" 2>/dev/null || true
    rmdir "$WT" 2>/dev/null || true
}
trap cleanup EXIT

# only the driver folder is checked out in the worktree
git -C "$MAIN" worktree add --no-checkout --detach "$WT" "$REF" >/dev/null 2>&1
git -C "$WT" checkout "$REF" -- "$PREFIX" >/dev/null 2>&1
BASE="$WT/$PREFIX"

"$HW/build.sh" "$BASE" "$OUT/base"
"$HW/build.sh" "$MAIN" "$OUT/work"
"$OUT/base/hwtrace" "$OUT/trace.base.txt"
"$OUT/work/hwtrace" "$OUT/trace.work.txt"

if cmp -s "$OUT/trace.base.txt" "$OUT/trace.work.txt"; then
    echo "hwtrace: identical ($(wc -l < "$OUT/trace.work.txt" | tr -d ' ') events)"
    exit 0
fi
diff -u --label "driver at $REF" --label "driver in working tree" "$OUT/trace.base.txt" "$OUT/trace.work.txt" || true
exit 1

#!/bin/bash
# <wtl> project: wtlwireless | file: tests/native/run.sh | author: watchthelight </wtl>
# Host test for the channel encoding of the native front end (host clang with UBSan, not the kext flags).
# Writes only to the out/tests/ folder of the build.
set -euo pipefail
cd "$(dirname "$0")"
OUT=../out/tests; [ -d ../../src/native ] && OUT=../../out/tests   # beside the sources, or in tests/native/ of the bundle repo
mkdir -p "$OUT"
clang -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=undefined chanspec_test.c -o "$OUT/chanspec_test"
"$OUT/chanspec_test"

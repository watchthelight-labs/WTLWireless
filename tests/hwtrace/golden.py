# <wtl> project: wtlwireless | file: tests/hwtrace/golden.py | author: watchthelight </wtl>
"""golden.py <golden.txt> <trace.txt>: every golden line must appear in the trace, in order ("*" matches one token).
Prints the first line that is missing and exits 1."""
import sys


def match(g, t):
    a, b = g.split(), t.split()
    return len(a) == len(b) and all(x == "*" or x == y for x, y in zip(a, b))


def main(gold, trace):
    want = [l.strip() for l in open(gold, encoding="utf-8") if l.strip() and not l.startswith("#")]
    have = [l.rstrip("\n") for l in open(trace, encoding="utf-8") if not l.startswith("PROP ")]
    i = 0
    for n, w in enumerate(want):
        while i < len(have) and not match(w, have[i]):
            i += 1
        if i == len(have):
            print(f"golden: line {n + 1} not found in order: {w}", file=sys.stderr)
            return 1
        i += 1
    print(f"golden: all {len(want)} lines found in order")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))

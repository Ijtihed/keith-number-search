#!/usr/bin/env python3
"""Audit a search output against expectations recomputed here.

    check_results.py FILE LOWER UPPER

Rebuilds the whole equation list for [LOWER, UPPER] from scratch and requires
the file to match it exactly, then re-derives every candidate. Nothing is
hardcoded and no assert is used, so python -O cannot turn the checks off.
Accepts output from either exhaustive.cpp or search.cpp.
"""

import csv
import sys


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def base4(n: int) -> list[int]:
    d = []
    while n:
        d.append(n % 4)
        n //= 4
    return d[::-1] or [0]


def hit_index(n: int) -> int | None:
    d = base4(n)
    k = len(d)
    if k < 2:
        return None
    terms = d[:]
    while terms[-1] < n:
        terms.append(sum(terms[-k:]))
    return len(terms) if terms[-1] == n else None


def expected_equations(lower: int, upper: int) -> list[tuple[int, int, int, int]]:
    """Every (k, m, lo, hi) whose recurrence term can land in [lower, upper]."""
    out = []
    for k in range(len(base4(lower)), len(base4(upper)) + 1):
        lo, hi = max(lower, 4 ** (k - 1)), min(upper, 4**k - 1)
        if lo > hi:
            continue
        rows = [[1 if i == j else 0 for i in range(k)] for j in range(k)]
        m = k
        while True:
            m += 1
            c = [sum(rows[-b][i] for b in range(1, k + 1)) for i in range(k)]
            rows.append(c)
            if c[0] > hi:  # smallest legal value already past the block
                break
            if 3 * sum(c) >= lo:  # largest legal value reaches the block
                out.append((k, m, lo, hi))
    return out


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: check_results.py FILE LOWER UPPER")
    path, lower, upper = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])

    with open(path, newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    require(rows, "no result rows")

    got = [(int(r["k"]), int(r["m"]), int(r["lower"]), int(r["upper"])) for r in rows]
    require(got == expected_equations(lower, upper), "equation list does not match")

    # Every width in range is covered, once, with no gap between blocks.
    blocks = sorted({(k, lo, hi) for k, _, lo, hi in got})
    require(blocks[0][1] == lower, "first block does not start at LOWER")
    require(blocks[-1][2] == upper, "last block does not end at UPPER")
    for (_, _, end), (_, start, _) in zip(blocks, blocks[1:]):
        require(start == end + 1, f"gap in coverage at {end}")

    found = []
    for row in rows:
        k, m, lo, hi = int(row["k"]), int(row["m"]), int(row["lower"]), int(row["upper"])
        for text in (row["candidates"] or "").split(","):
            if not text:
                continue
            n = int(text)
            require(lo <= n <= hi, f"{n} is outside its block")
            require(len(base4(n)) == k, f"{n} is not {k} digits wide")
            require(hit_index(n) == m, f"{n} does not first appear at position {m}")
            found.append(n)

    require(len(set(found)) == len(found), "a candidate was reported twice")
    found.sort()
    print(f"PASS: {len(rows)} equations, {len(found)} candidates in [{lower}, {upper}]")
    for n in found:
        print(f"  {n}")


if __name__ == "__main__":
    main()

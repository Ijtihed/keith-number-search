#!/usr/bin/env python3
"""Check every term in terms.tsv and b188196.txt directly.

Uses explicit exceptions, not assert, so python -O cannot turn the checks off.
"""

import csv
from pathlib import Path

HERE = Path(__file__).parent
FIRST_NEW = 34
PREVIOUS = 24_453_922_692  # OEIS A188196 a(33), the last published term


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL: {message}")


def base4(n: int) -> str:
    out = ""
    while n:
        out = str(n % 4) + out
        n //= 4
    return out or "0"


def hit_index(n: int) -> int | None:
    """One-based position at which n appears in its own Keith sequence."""
    digits = [int(c) for c in base4(n)]
    k = len(digits)
    if k < 2:
        return None
    terms = digits[:]
    while terms[-1] < n:
        terms.append(sum(terms[-k:]))
    return len(terms) if terms[-1] == n else None


def check_terms() -> list[int]:
    rows = list(csv.DictReader((HERE / "terms.tsv").open(encoding="utf-8"), delimiter="\t"))
    require(rows, "terms.tsv is empty")

    values = []
    previous = PREVIOUS
    for offset, row in enumerate(rows):
        index, n = int(row["index"]), int(row["decimal"])
        require(index == FIRST_NEW + offset, f"index {index} is out of sequence")
        require(n > previous, f"a({index}) = {n} does not exceed the previous term")
        require(base4(n) == row["base4"], f"a({index}) base-4 digits do not match")
        require(len(row["base4"]) == int(row["digits"]), f"a({index}) width does not match")
        require(hit_index(n) == int(row["hit_index"]), f"a({index}) hit index does not match")
        previous = n
        values.append(n)
    return values


def check_bfile(new_terms: list[int]) -> int:
    lines = [l for l in (HERE / "b188196.txt").read_text(encoding="ascii").splitlines() if l.strip()]
    require(lines, "b188196.txt is empty")

    values = []
    for expected_index, line in enumerate(lines, start=1):
        parts = line.split(" ")
        require(len(parts) == 2, f"b-file line {expected_index} is not 'n a(n)'")
        index, n = int(parts[0]), int(parts[1])
        require(index == expected_index, f"b-file index {index} is out of sequence")
        require(hit_index(n) is not None, f"b-file a({index}) = {n} is not a base-4 Keith number")
        values.append(n)

    require(all(a < b for a, b in zip(values, values[1:])), "b-file is not strictly increasing")
    require(values[FIRST_NEW - 2] == PREVIOUS, "b-file a(33) is not the last published term")
    require(values[FIRST_NEW - 1:] == new_terms, "b-file and terms.tsv disagree")
    return len(values)


def main() -> None:
    new_terms = check_terms()
    total = check_bfile(new_terms)
    print(f"PASS: {total} base-4 Keith numbers, {len(new_terms)} of them new")


if __name__ == "__main__":
    main()

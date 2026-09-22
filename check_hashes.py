#!/usr/bin/env python3
"""Check that minimality.json still describes the sources next to it.

Line endings are normalised to LF first, so the hashes are the same whether the
repo was checked out on Linux or Windows. Run with --write to refresh them.
"""

import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).parent
SOURCES = ("search.cpp", "exhaustive.cpp", "verify.py", "check_results.py")


def digest(name: str) -> str:
    data = (HERE / name).read_bytes().replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    path = HERE / "minimality.json"
    record = json.loads(path.read_text(encoding="utf-8"))
    current = {name: digest(name) for name in SOURCES}

    if "--write" in sys.argv:
        record["source_sha256"] = current
        path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8", newline="\n")
        print("updated minimality.json")
        return

    stored = record.get("source_sha256", {})
    wrong = [n for n in SOURCES if stored.get(n) != current[n]]
    if wrong:
        for name in wrong:
            print(f"FAIL: {name}\n  recorded {stored.get(name)}\n  actual   {current[name]}")
        raise SystemExit(1)
    print(f"PASS: {len(SOURCES)} source hashes match")


if __name__ == "__main__":
    main()

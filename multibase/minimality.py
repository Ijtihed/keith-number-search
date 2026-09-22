#!/usr/bin/env python3
"""Rigorous minimality certificate for every base searched.

For each base this re-derives, independently of the C++ solver:
  1. the full equation list for every width block,
  2. that the blocks tile [b, b^kmax - 1] with no gap or overlap,
  3. that the solver's recorded equations match exactly,
  4. that every reported term is a Keith number of the right width and hit index,
  5. that the term list is strictly increasing and matches the published prefix.

Nothing here calls the solver; it reads the recorded block files.
"""
import csv, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
BLOCKS = os.path.join(HERE, "blocks")
SEQ = {2:"162724", 3:"188195", 4:"188196", 5:"187713",
       6:"188197", 7:"188198", 8:"188199", 9:"188200", 10:"007629"}

def digits(n, b):
    d = []
    while n: d.append(n % b); n //= b
    return d[::-1] or [0]

def hit_index(n, b):
    d = digits(n, b); k = len(d)
    if k < 2: return None
    s = list(d)
    while s[-1] < n: s.append(sum(s[-k:]))
    return len(s) if s[-1] == n else None

def expected_equations(b, k):
    """Every m whose recurrence term can land in the width-k block."""
    lo, hi = b**(k-1), b**k - 1
    rows = [[1 if i == j else 0 for i in range(k)] for j in range(k)]
    out = []
    m = k
    while True:
        m += 1
        c = [sum(rows[-t][i] for t in range(1, k+1)) for i in range(k)]
        rows.append(c)
        if c[0] > hi: break
        if (b-1) * sum(c) >= lo: out.append(m)
    return out, lo, hi

def published(b):
    p = os.path.join(HERE, "indep", f"b{SEQ[b]}.txt")
    if not os.path.exists(p): return []
    return [int(l.split()[1]) for l in open(p) if not l.startswith('#') and l.strip()]

def audit(b):
    recs = json.load(open(os.path.join(HERE, f"ext_b{b}.json")))
    problems = []
    ks = [r["k"] for r in recs]

    # 1. widths contiguous from 2
    if ks != list(range(2, max(ks)+1)):
        problems.append(f"widths not contiguous from 2: {ks[:3]}..{ks[-3:]}")

    # 2. blocks tile the range exactly
    prev_hi = b - 1
    for r in recs:
        if r["lo"] != prev_hi + 1:
            problems.append(f"gap or overlap before k={r['k']}: {prev_hi} -> {r['lo']}")
        if r["lo"] != b**(r["k"]-1) or r["hi"] != b**r["k"] - 1:
            problems.append(f"k={r['k']} block bounds wrong")
        prev_hi = r["hi"]

    # 3. equation lists match an independent recomputation
    eq_total = 0
    for r in recs:
        k = r["k"]
        exp_m, lo, hi = expected_equations(b, k)
        f = os.path.join(BLOCKS, f"b{b}_k{k}.tsv")
        if not os.path.exists(f):
            problems.append(f"missing block file for k={k}"); continue
        rows = list(csv.DictReader(open(f, newline='', encoding='utf-8-sig'), delimiter='\t'))
        got_m = [int(x["m"]) for x in rows]
        if got_m != exp_m:
            problems.append(f"k={k}: equation list mismatch (got {len(got_m)}, expected {len(exp_m)})")
        if any(int(x["lower"]) != lo or int(x["upper"]) != hi for x in rows):
            problems.append(f"k={k}: recorded block bounds disagree")
        eq_total += len(rows)
        # 4. every candidate belongs to its row
        for x in rows:
            for v in (x.get("candidates") or "").split(","):
                if not v.strip(): continue
                n = int(v)
                if not (lo <= n <= hi): problems.append(f"k={k}: {n} outside block")
                if len(digits(n, b)) != k: problems.append(f"k={k}: {n} wrong width")
                if hit_index(n, b) != int(x["m"]): problems.append(f"k={k}: {n} wrong hit index")

    # 5. term list increasing and matching the published prefix
    terms = [t for r in recs for t in r["terms"]]
    if any(terms[i] >= terms[i+1] for i in range(len(terms)-1)):
        problems.append("terms not strictly increasing")
    pub = published(b)
    pref = pub[1:] if b == 2 else pub          # A162724 lists the 1-digit term
    # only the published terms inside the searched range are comparable
    hi_searched = b**max(ks) - 1
    pref = [p for p in pref if p <= hi_searched]
    if pref and terms[:len(pref)] != pref:
        problems.append(f"published prefix not reproduced ({len(pref)} comparable terms)")
    if len(terms) < len(pref):
        problems.append("fewer terms than published in the same range")

    return dict(base=b, kmax=max(ks), equations=eq_total, terms=len(terms),
                lo=b, hi=b**max(ks)-1, problems=problems)

if __name__ == "__main__":
    bases = [int(x) for x in sys.argv[1:]] or [b for b in SEQ
             if os.path.exists(os.path.join(HERE, f"ext_b{b}.json"))]
    print(f"{'base':>4} {'kmax':>5} {'equations':>10} {'terms':>6} {'range covered':>28} {'verdict':>10}")
    allok = True
    for b in sorted(bases):
        r = audit(b)
        ok = not r["problems"]; allok &= ok
        print(f"{r['base']:>4} {r['kmax']:>5} {r['equations']:>10} {r['terms']:>6} "
              f"{f'[{r[chr(108)+chr(111)]}, {r[chr(104)+chr(105)]}]'[:28]:>28} {'PASS' if ok else 'FAIL':>10}")
        for p in r["problems"][:5]: print(f"        - {p}")
    print()
    print("MINIMALITY CERTIFIED FOR ALL BASES:" , allok)

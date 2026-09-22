#!/usr/bin/env python3
"""Build a ready-to-paste OEIS submission pack for each extended sequence.

Two things to get right:
  * the published term count is the b-file length, not the DATA field, which is
    truncated to about three lines;
  * OEIS says that once DATA holds about three lines there is no point adding
    more terms to it, so DATA is left alone and the b-file carries the rest.
"""
import json, os, subprocess

REPO = r"C:\Users\Ijtihed\base4-keith-verification"
OUT  = os.path.join(REPO, "multibase", "submissions")
os.makedirs(OUT, exist_ok=True)

SEQ  = {3:"A188195", 4:"A188196", 5:"A187713", 6:"A188197",
        7:"A188198", 8:"A188199", 9:"A188200"}
DATE = "Sep 12 2026"
NAME = "Ijtihed Kilani"

def fetch(url):
    return subprocess.run(["curl","-s",url], capture_output=True, text=True).stdout

def published_terms(seq):
    """The real published list: the b-file if there is one, else DATA."""
    raw = fetch(f"https://oeis.org/{seq}/b{seq[1:]}.txt")
    if raw and not raw.lstrip().startswith("#"):
        return [int(l.split()[1]) for l in raw.splitlines() if l.strip()], "b-file"
    d = json.loads(fetch(f"https://oeis.org/search?q=id:{seq}&fmt=json"))
    rec = d[0] if isinstance(d, list) else d["results"][0]
    return [int(x) for x in rec["data"].split(",")], "DATA (synthesized b-file)"

def digits(n,b):
    o=[]
    while n: o.append(n%b); n//=b
    return o[::-1] or [0]

def is_keith(n,b):
    d=digits(n,b); k=len(d)
    if k<2: return False
    s=list(d)
    while s[-1]<n: s.append(sum(s[-k:]))
    return s[-1]==n

rows=[]
for b, seq in sorted(SEQ.items()):
    pub, src = published_terms(seq)
    # base 4 is the original submission, so its b-file sits at the repo root
    bpath = (os.path.join(REPO, "b188196.txt") if b == 4
             else os.path.join(REPO, "multibase", "bfiles", f"b{seq[1:]}.txt"))
    ours = [int(l.split()[1]) for l in open(bpath)]

    assert ours[:len(pub)] == pub, f"{seq}: published prefix mismatch"
    assert all(is_keith(x,b) for x in ours), f"{seq}: a term fails the recurrence"
    assert all(ours[i] < ours[i+1] for i in range(len(ours)-1)), f"{seq}: not increasing"

    rows.append(dict(
        base=b, seq=seq, published=len(pub), source=src, total=len(ours),
        new=len(ours)-len(pub), first_new=len(pub)+1,
        link=f'_{NAME}_, <a href="/{seq}/b{seq[1:]}.txt">Table of n, a(n) for n = 1..{len(ours)}</a>',
        extensions=f"a({len(pub)+1})-a({len(ours)}) from _{NAME}_, {DATE}",
        bfile=os.path.relpath(bpath, REPO).replace("\\", "/"),
        largest=str(ours[-1]),
    ))

with open(os.path.join(OUT,"README.md"),"w",newline="\n") as fh:
    fh.write("# OEIS submission pack\n\n")
    fh.write(f"{len(rows)} sequences, one submission each. Submit **one at a time** and wait\n")
    fh.write("for each to be approved before sending the next.\n\n")
    fh.write("Every entry below was checked against the live OEIS data when this file was\n")
    fh.write("generated: the published terms form a prefix of our list, every term satisfies\n")
    fh.write("the Keith recurrence in its base, and the list is strictly increasing.\n\n")
    fh.write("DATA is deliberately left unchanged. OEIS advises that once DATA holds about\n")
    fh.write("three lines there is no point in adding more terms to it, so the b-file\n")
    fh.write("carries the extension.\n\n")
    fh.write("| base | sequence | published | after | new | largest term |\n")
    fh.write("|---:|---|---:|---:|---:|---|\n")
    for r in rows:
        fh.write(f"| {r['base']} | {r['seq']} | {r['published']} | {r['total']} | "
                 f"+{r['new']} | {r['largest'][:24]}{'...' if len(r['largest'])>24 else ''} |\n")
    fh.write("\n---\n")
    for r in rows:
        fh.write(f"\n## {r['seq']} (base {r['base']})\n\n")
        fh.write(f"Published: {r['published']} terms (source: {r['source']}). "
                 f"After: {r['total']}. New: {r['new']}.\n\n")
        if r["seq"] == "A188196":
            fh.write("A follow-up: this entry already carries a b-file and a LINKS entry from\n")
            fh.write("the earlier submission, so replace them rather than adding new ones.\n\n")
            fh.write(f"1. Upload `{r['bfile']}` to replace the existing b-file.\n")
            fh.write("2. Leave DATA unchanged.\n")
            fh.write(f"3. Update the existing LINKS entry to read:\n\n```\n{r['link']}\n```\n\n")
            fh.write(f"4. Add a second EXTENSIONS line below the existing one:\n\n```\n{r['extensions']}\n```\n")
        else:
            fh.write(f"1. Upload `{r['bfile']}` as the b-file.\n")
            fh.write("2. Leave DATA unchanged.\n")
            fh.write(f"3. Add to LINKS:\n\n```\n{r['link']}\n```\n\n")
            fh.write(f"4. Add to EXTENSIONS:\n\n```\n{r['extensions']}\n```\n")

json.dump(rows, open(os.path.join(OUT,"pack.json"),"w"), indent=1)
print(f"{'seq':>9} {'published':>10} {'source':>26} {'after':>6} {'new':>5}")
for r in rows:
    print(f"{r['seq']:>9} {r['published']:>10} {r['source']:>26} {r['total']:>6} {r['new']:>5}")
print(f"\nwrote {OUT}")

#!/usr/bin/env python3
"""Assemble the result tables from the recorded per-block files."""
import json, os, math, glob
from math import comb

HERE = os.path.dirname(os.path.abspath(__file__))
SEQ  = {2:"A162724", 3:"A188195", 4:"A188196", 5:"A187713",
        6:"A188197", 7:"A188198", 8:"A188199", 9:"A188200"}

def digits(n,b):
    d=[]
    while n: d.append(n%b); n//=b
    return d[::-1] or [0]

def hit(n,b):
    d=digits(n,b); k=len(d)
    if k<2: return None
    s=list(d)
    while s[-1]<n: s.append(sum(s[-k:]))
    return len(s) if s[-1]==n else None

def published(b):
    p=os.path.join(HERE,"indep",f"b{SEQ[b][1:]}.txt")
    if not os.path.exists(p): return []
    return [int(l.split()[1]) for l in open(p) if not l.startswith('#') and l.strip()]

def load(b):
    f=os.path.join(HERE,f"ext_b{b}.json")
    if not os.path.exists(f): return None
    return json.load(open(f))

def binom_two_sided(k,n,p=0.5):
    pk=comb(n,k)*p**k*(1-p)**(n-k)
    return sum(comb(n,i)*p**i*(1-p)**(n-i) for i in range(n+1)
               if comb(n,i)*p**i*(1-p)**(n-i) <= pk*(1+1e-12))

def main():
    print("="*78)
    print("TABLE 1  Extension summary")
    print("="*78)
    print(f"{'base':>4} {'seq':>9} {'pub':>4} {'pub max':>12} {'searched to':>16} {'total':>6} {'NEW':>5} {'prefix ok':>10}")
    summary={}
    for b in sorted(SEQ):
        recs=load(b)
        if not recs: continue
        pub=published(b)
        terms=[t for r in recs for t in r["terms"]]
        kmax=recs[-1]["k"]
        # base 2 published list includes the 1-digit term a(1)=1
        pref = pub[1:] if b==2 else pub
        okpref = terms[:len(pref)]==pref
        new=[t for t in terms if t>pub[-1]]
        summary[b]=dict(recs=recs,terms=terms,pub=pub,new=new,kmax=kmax)
        print(f"{b:>4} {SEQ[b]:>9} {len(pub):>4} {pub[-1]:>12.4g} {f'{b}^{kmax}-1':>16} "
              f"{len(terms):>6} {len(new):>5} {str(okpref):>10}")

    print()
    print("="*78)
    print("TABLE 2  Search cost growth per digit  (theory: b^(1/2))")
    print("="*78)
    print(f"{'base':>4} {'sqrt(b)':>9} {'measured':>10} {'widths used':>14} {'total nodes':>18}")
    for b,S in summary.items():
        recs=[r for r in S["recs"] if r["nodes"]>0]
        tail=recs[-6:] if len(recs)>=7 else recs[-3:]
        gs=[tail[i]["nodes"]/tail[i-1]["nodes"] for i in range(1,len(tail))
            if tail[i-1]["nodes"]>0]
        g=sum(gs)/len(gs) if gs else float('nan')
        tot=sum(r["nodes"] for r in S["recs"])
        print(f"{b:>4} {math.sqrt(b):>9.3f} {g:>10.3f} {f'k={tail[0][chr(107)]}..{tail[-1][chr(107)]}':>14} {tot:>18,}")

    print()
    print("="*78)
    print("TABLE 3  Equations per width  (theory: Theta(log k))")
    print("="*78)
    for b,S in summary.items():
        eqs=[r["equations"] for r in S["recs"] if r["k"]>=10]
        ks=[r["k"] for r in S["recs"] if r["k"]>=10]
        if eqs:
            print(f"  base {b}: equations range {min(eqs)}..{max(eqs)} over k={min(ks)}..{max(ks)}"
                  f"   log_b(({b}-1)k) at k={max(ks)} = {math.log((b-1)*max(ks),b):.2f}")

    print()
    print("="*78)
    print("TABLE 4  Parity of terms per base  (Violette's question, generalised)")
    print("="*78)
    print(f"{'base':>4} {'n':>5} {'odd':>5} {'frac':>7} {'p(two-sided)':>13}  {'small terms':>12} {'large terms':>12}")
    for b,S in summary.items():
        t=[x for x in S["terms"]]
        if not t: continue
        o=sum(1 for x in t if x%2)
        half=len(t)//2
        os_,ol = sum(1 for x in t[:half] if x%2), sum(1 for x in t[half:] if x%2)
        print(f"{b:>4} {len(t):>5} {o:>5} {o/len(t):>7.3f} {binom_two_sided(o,len(t)):>13.4f}"
              f"  {f'{os_}/{half}':>12} {f'{ol}/{len(t)-half}':>12}")

    print()
    print("="*78)
    print("TABLE 5  Hit-position law  m = k+1+(k ln b - ln S)/ln(lambda_k) + O(1)")
    print("="*78)
    def lam(k):
        lo,hi=1.0,2.0
        for _ in range(200):
            mid=(lo+hi)/2
            if mid**(k+1)-2*mid**k+1>0: hi=mid
            else: lo=mid
        return (lo+hi)/2
    for b,S in summary.items():
        errs=[]
        for n in S["terms"]:
            d=digits(n,b); k=len(d)
            if k<8: continue
            m=hit(n,b); Ssum=sum(d)
            errs.append(m-(k+1+(k*math.log(b)-math.log(Ssum))/math.log(lam(k))))
        if errs:
            mu=sum(errs)/len(errs)
            sd=(sum((e-mu)**2 for e in errs)/len(errs))**.5
            print(f"  base {b}: n={len(errs):>3}  mean err {mu:+.3f}  sd {sd:.3f}  max|err| {max(abs(e) for e in errs):.2f}")

    json.dump({str(b):{"kmax":S["kmax"],"total":len(S["terms"]),"new":len(S["new"]),
                       "new_terms":[str(x) for x in S["new"]]}
               for b,S in summary.items()},
              open(os.path.join(HERE,"summary.json"),"w"), indent=1)
    print("\nwrote summary.json")

if __name__ == "__main__":
    main()

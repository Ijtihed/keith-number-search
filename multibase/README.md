# Other bases

`../multibase.cpp` is the same solver with the base as its first argument.

```bash
g++ -O3 -std=c++17 -pthread ../multibase.cpp -o multibase
./multibase 3 3 92709463147897837085761925410586 4 --parity-first --mod2 > b3.tsv
```

## What was searched

| base | OEIS | published | searched through | found | new |
|---:|---|---:|---|---:|---:|
| 3 | A188195 | 46 | 3^67 - 1 | 114 | +68 |
| 4 | A188196 | 33 | 4^41 - 1 | 69 | +36 |
| 5 | A187713 | 42 | 5^32 - 1 | 99 | +57 |
| 6 | A188197 | 58 | 6^26 - 1 | 86 | +28 |
| 7 | A188198 | 53 | 7^24 - 1 | 81 | +28 |
| 8 | A188199 | 55 | 8^23 - 1 | 75 | +20 |
| 9 | A188200 | 68 | 9^22 - 1 | 92 | +24 |

Every published term was reproduced from scratch first.

Base 10 was a control, not an extension. A007629 is already known to 45 digits by
lattice reduction, which beats this method above base 4. Our run to 10^17 returned
exactly the 63 known terms in that range.

## Filters

Optional, and all must give the same terms. That agreement is part of the
checking.

| flag | what it does | worth |
|---|---|---|
| `--parity` | parity theorem | 2x in odd bases, nothing in even ones |
| `--mod` | residues mod 64 the rest of the digits can reach | 2.2x to 8.6x |
| `--mod2` | adds a second modulus, 63 | 3.2x to 18.9x |

Best is `--parity-first --mod2`. On base 4 through `4^34-1` it cuts the search
from 10.7 to 2.7 billion nodes. It is a constant factor and does not change how
the cost grows.

## Files

- `terms-b<base>.tsv` -- every term, with digit length and hit index
- `bfiles/` -- OEIS-format b-files, ASCII with LF endings
- `submissions/` -- what to paste into each OEIS entry
- `minimality.py` -- rebuilds every equation list without using the solver,
  checks the digit-length blocks tile the range with no gap, re-runs the
  recurrence on every term
- `analyze.py` -- the tables
- `submitpack.py` -- regenerates `submissions/`

## Certificate

```
base  kmax  equations  terms
  3    67       552     114
  4    41       363      69
  5    32       282      99
  6    26       235      86
  7    24       226      81
  8    23       234      75
  9    22       223      92
```

2115 equations, all passing. Run `python minimality.py`. It needs the per-width
block files, which are not committed; regenerate them with the solver first.

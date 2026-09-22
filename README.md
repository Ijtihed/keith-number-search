# Keith numbers

Exhaustive searches for Keith numbers in bases 3 to 10.

Base 4 is [A188196](https://oeis.org/A188196). The published list ended at
`a(33) = 24453922692`. This repo found `a(34)` through `a(69)`. Of those,
`a(34)` to `a(57)` are now in OEIS.

The same solver runs in any base. Every published term was reproduced from
scratch first, then the search carried on.

| base | OEIS | was | now | new | searched through |
|---:|---|---:|---:|---:|---|
| 3 | [A188195](https://oeis.org/A188195) | 46 | 114 | +68 | `3^67 - 1` |
| 4 | [A188196](https://oeis.org/A188196) | 33 | 69 | +36 | `4^41 - 1` |
| 5 | [A187713](https://oeis.org/A187713) | 42 | 99 | +57 | `5^32 - 1` |
| 6 | [A188197](https://oeis.org/A188197) | 58 | 86 | +28 | `6^26 - 1` |
| 7 | [A188198](https://oeis.org/A188198) | 53 | 81 | +28 | `7^24 - 1` |
| 8 | [A188199](https://oeis.org/A188199) | 55 | 75 | +20 | `8^23 - 1` |
| 9 | [A188200](https://oeis.org/A188200) | 68 | 92 | +24 | `9^22 - 1` |
| | | **355** | **616** | **+261** | |

Base 10 was a control, not an extension. [A007629](https://oeis.org/A007629) is
already known to 45 digits by lattice reduction, which beats this method above
base 4. The run to `10^17` returned exactly the 63 known terms in that range.

Details and the minimality certificate are in [multibase](multibase).

## Check the terms

```bash
python verify.py
```

Runs the recurrence on every term and checks the two files against each other.

## Check nothing was missed

```bash
g++ -O3 -std=c++17 -pthread search.cpp -o search
./search 24453922693 4835703278458516698824703 4 > all.tsv
python check_results.py all.tsv 24453922693 4835703278458516698824703
```

This covers every integer from `a(33)+1` to `4^41 - 1`, so the terms are
consecutive and not a selection. About forty minutes on four threads.

`check_results.py` rebuilds the expected equation list on its own, so it fails if
the search skipped a position.

## Second opinion

`exhaustive.cpp` solves the same equations by meet in the middle. It agrees up to
`4^28 - 1`, which is as far as its table fits in memory.

```bash
g++ -O3 -std=c++17 -pthread exhaustive.cpp -o exhaustive
./exhaustive 24453922693 72057594037927935 4 > mitm.tsv
python check_results.py mitm.tsv 24453922693 72057594037927935
```

## Files

| | |
|---|---|
| `search.cpp` | branch and bound, base 4 |
| `multibase.cpp` | same solver, any base |
| `exhaustive.cpp` | meet in the middle, second opinion |
| `terms.tsv`, `b188196.txt` | the base 4 terms |
| `multibase/` | other bases, minimality certificate, OEIS submission pack |
| `results/` | recorded search output |
| `notes.md` | decisions, measured tradeoffs, dead ends |

[method.md](method.md) explains how it works, [proof.md](proof.md) why it is
complete, and [notes.md](notes.md) what was tried, what it cost, and what did
not work.

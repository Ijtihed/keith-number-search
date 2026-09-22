# Notes

What was tried, what worked, what did not. Numbers are measured, not estimated.

## Why there are two solvers

Meet in the middle needs a sorted table of `b^(k/2)` entries. In base 4 that is
2 GB at 28 digits and 8 GB at 29. That is where the first search stopped, and it
is not a tuning problem: the table is the algorithm.

Branch and bound has no table. Shifting every weight up by the most negative one
makes them all non-negative and turns the right side into `shift * digitsum`.
Fixing the digit sum then pins how much any remaining suffix can contribute, and
the bound is exact for that digit sum, so pruning only drops branches that
provably cannot reach the target. Memory is `O(k^2)`.

That is the whole reason base 3 reached 67 digits. A table there would need
`3^34`, about `1.7e16` entries.

## Cost

Growth per extra digit, written as `b^e`:

```
base   kmax   growth   b^e
  3     67     1.578   e=0.415
  4     41     1.961   e=0.486
  5     32     2.329   e=0.525
  7     24     3.224   e=0.602
  6     26     2.977   e=0.609
  8     23     3.684   e=0.627
  9     22     4.154   e=0.648
```

The exponent tracks how far each base got, not the base itself. Bases pushed far
sit at or below 1/2; bases stuck near 20 digits have not settled yet. So the cost
is about `b^(k/2)`, the same as meet in the middle, but without the memory.

## Where lattice reduction wins

Lichtblau reports about `2x` per digit for his lattice and ILP method, and got
base 10 to 29 digits in 2006. This method grows at about `b^(1/2)` per digit. So
it wins when `sqrt(b) < 2`, that is below base 4, ties at base 4, and loses above:

```
base 3    1.58x / digit    beats 2x
base 4    1.96x / digit    ties
base 5    2.33x / digit    loses
base 10   ~4.2x / digit    loses badly   (17 digits here vs his 29)
```

That is why base 10 was only a control. It also explains the gap this work fills:
the heavy machinery went to base 10, and the small bases got nothing, so they sat
near `10^11` for fifteen years.

Caveat: that is our measurements against his prose description, different
machines and decades apart. Nobody has run both on the same instance.

## Parity filter

The parity of a Keith sequence repeats with period `k+1`, following
`(d1..dk, S)`. A hit at position `m` therefore forces one particular digit to
match the parity of the number itself.

Worth about `2x` in odd bases, nothing in even ones:

```
base 3   k=30      136,141  ->      70,678   1.93x
base 7   k=16  208,524,005  -> 103,058,244   2.02x
base 9   k=14  504,549,801  -> 252,736,149   2.00x
base 5   k=20   43,344,361  ->  40,386,451   1.07x
base 4   k=26   20,463,759  ->  21,990,992   none
base 6   k=18  163,368,761  -> 163,371,391   none
```

In an odd base the digit sum is already fixed by the outer loop, so the condition
lands on a single digit. In an even base it relates two digits, and splitting the
loop to make it unary doubles the loop, cancelling the gain exactly. Base 5 is in
between because its heaviest equations put the constrained digit last in the
visit order, where the filter only touches the final level.

## Modular filter

Track which residues mod 64 the remaining digits can still reach, given the digit
sum, as a bitmask; prune when the needed residue is not among them. A second
modulus of 63 is coprime, so the two together act like modulus 4032.

```
base 3   k=30   3.2x        base 7   k=16    7.0x
base 4   k=26   3.2x        base 9   k=14   18.9x
base 6   k=18   3.2x        base 10  k=13    3.2x
```

Biggest where the method is otherwise weakest. But it is a constant factor, not a
better algorithm. Measured across widths the speedup is flat and the exponent
does not move:

```
base 9   k=10..15   17.2x -> 17.6x    e 0.634 -> 0.632
base 4   k=20..27    3.08x ->  3.11x  e 0.449 -> 0.448
```

Still worth having. Applied to the searches it bought one to three extra digits
per base and 29 further Keith numbers; base 3 and base 5 gained seven each,
base 7 only one.

## Dead end: 2-adic structure

For `t <= k` the coefficients have an exact closed form,

```
a_{k+1+t} = 2^t * S - sum_{j<=t} 2^(t-j) * d_j
```

so every one is a difference of two powers of two. If that sparsity held where
hits occur, the equation could plausibly be solved 2-adically, digit by digit
from the low end.

It does not hold. Hits happen at `t` around `1.8k`, well past the closed form,
and out there the coefficients look like generic integers:

```
 k    t    mean popcount   mean bit length
16   28        14.3            27.9
21   37        18.8            37.0
28   50        26.6            50.0
34   61        32.6            61.0
```

Weight is about half the length. Abandoned on this evidence.

## The barrier

`b^(k/2)` is the subset-sum barrier, not a limitation of this code. Getting past
it means representation techniques of the Howgrave-Graham and Joux kind, which
buy time by spending exponential memory, and memory is the only thing this method
has over lattice reduction. No asymptotic improvement is claimed.

## Base 2 is excluded

Every power of two is a binary Keith number, at hit index exactly `2k+1`. The
digits are `1` then zeros, so the digit sum is 1, and the sliding form doubles
cleanly while the window still sits on the zeros. 40 of the 69 published terms of
A162724 are powers of two, so most of that sequence is this one family.

Nothing similar exists elsewhere: the doubling only reaches `2^j`, and `b^j` is
larger for `b > 2`. So A162724 is provably infinite while the other bases look
sparse, and base 2 does not belong in a density comparison.

## Bugs found

From Aabir Fauzan's audit of the base 4 work:

- `minimality.json` recorded the hash of an older `exhaustive.cpp`. Now checked
  in CI, so it cannot go stale again.
- `verify.py` and `check_results.py` validated with `assert`, so `python -O`
  turned every check off and a junk `terms.tsv` still printed PASS.
- `check_results.py` accepted result tables with zeroed bounds or a wrong digit
  length. It now rebuilds the equation list itself.
- `exhaustive.cpp` did not terminate for `LOWER < 4` and read non-decimal
  arguments as zero.

Found later, while adding the filters:

- Reordering the digits after building the weight array desynchronised it from
  the depth-indexed bounds, so valid branches were pruned. It looked like a 2x
  speedup until the terms were counted. Caught only because filtered and
  unfiltered runs are required to agree, which is why that check exists.
- CI built `multibase.cpp -o multibase`, colliding with the `multibase/`
  directory. Passed on Windows, where the binary gets an `.exe` suffix, and
  failed on Linux.

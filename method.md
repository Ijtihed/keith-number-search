# Method

Take a number with base `b` digits `d1,...,dk`. Every later term of its Keith
recurrence is a fixed linear combination of those digits. So for each possible
hit position `m`, asking whether the number equals that term is one exact
equation:

```text
(place values - recurrence coefficients) dot digits = 0
```

Only a few positions need checking. Before the first, even the largest digits
give a value too small. After the last, even the smallest leading digit gives one
too large, and the coefficients of the generated terms never shrink, so no later
position can work either. In practice this leaves 6 to 12 equations per digit
length.

Two programs solve them.

`exhaustive.cpp` splits the digits in two, enumerates each side, sorts one side,
and finds matching sums by binary search. It checks every legal digit string.
The sorted table holds `b^(k/2)` entries: 2 GB at 28 base-4 digits, 8 GB at 29.
That is the ceiling.

`search.cpp` avoids the table. Adding the most negative weight to every weight
makes them all non-negative, which turns the right-hand side into
`shift * digitsum`. Fixing the digit sum then pins down how much any remaining
suffix can contribute, so branch and bound cuts most of the tree. Memory is
negligible and it reaches 39 base-4 digits, or 64 in base 3.

`multibase.cpp` is the same solver for any base, with two optional filters:
`--parity` uses the parity theorem, `--mod` and `--mod2` track which residues
modulo 64 and 63 the rest of the digits can still reach. Both are worth a
constant factor and neither changes the answer.

All arithmetic is 128-bit integer. Floating point is used only for timing.

## Prior work

The reduction to bounded linear Diophantine equations is old. Keith-number
searches were done this way decades ago. Ken Sherriff published an
equation-splitting method in 1994. Daniel Lichtblau published a lattice and
integer-programming approach in 2006 and used it to find all base-10 Keith
numbers up to 29 digits, which is still far beyond what the method here reaches
in base 10.

This method beats the lattice approach below base 4 and loses above it; see
[notes.md](notes.md) for the measured crossover.

What is here is the base 3 to 9 computation and its output, not the invention of
any of this.

- [Mike Keith, Keith Numbers](https://www.cadaeic.net/keithnum.htm)
- [MathWorld](https://mathworld.wolfram.com/KeithNumber.html)
- [Lichtblau, Making Change and Finding Repfigits](https://doi.org/10.1007/11832225_16)

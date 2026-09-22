# Why nothing was missed

For base 4 the search covers one unbroken interval:

```text
24453922693  through  4^41 - 1
```

For each digit length both programs work out every recurrence position that could
land in that block. A position is skipped when even the largest digits fall short
of it, and the loop stops when even the smallest leading digit overshoots. That
stopping rule is valid because the coefficients of the generated terms never
shrink. It applies from `m = k+1` on; the starting basis vectors are not monotone,
so the induction has to begin there.

At each position the condition is one exact linear equation in the digits. Both
programs try every legal digit string, and every match is checked again by
running the recurrence directly. A reported term cannot be an artefact of the
equation.

The output is exactly the terms in `terms.tsv`, in order. So:

- every listed value is a base 4 Keith number;
- nothing was skipped between `a(33)` and any listed value;
- they are `a(34)` through `a(69)`;
- any further term is greater than `4^41 - 1`.

## What was actually run

Membership was checked three ways: by the solver, by `verify.py` with Python
integers, and by the `IsKeith[n,b]` function on the OEIS entry.

Up to `4^28 - 1` both programs produced the result independently. They share no
search code. Recorded output from both is in `results`.

Past `4^28` the meet in the middle runs out of memory, so only `search.cpp`
reaches there. A separate branch-and-bound implementation, written by Aabir
Fauzan while auditing this repo, independently reproduced everything up to
`4^28 - 1` and agreed on every term and on the equation list. Its digit cap was
raised locally to check `4^29` through `4^34 - 1`, which also agreed. Above
`4^34 - 1` only the solvers here have run, and the guarantee rests on the bounds
and on the filters agreeing with each other.

`multibase/minimality.py` does the same for the other bases. It rebuilds every
equation list without touching the solver, checks the digit-length blocks tile
the range with no gap, and re-runs the recurrence on every term: 2115 equations
across seven bases.

One limitation worth stating. Past the reach of the meet in the middle there is
no unpruned enumeration. The guarantee there rests on the branch-and-bound
bounds being exact, and on independent implementations, compilers and filters
agreeing wherever they can both run.

That audit also turned up the defects fixed here: a stale source hash, checkers
that used `assert` and so did nothing under `python -O`, checkers that accepted
corrupted result tables, and an input range that did not terminate.

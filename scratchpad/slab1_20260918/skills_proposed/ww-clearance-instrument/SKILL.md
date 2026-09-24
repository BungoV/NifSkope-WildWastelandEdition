---
name: ww-clearance-instrument
description: Get GROUND TRUTH about what stands over a piece of terrain in a NifSkope WW lodgen lane, without trusting the map you are testing. Covers finding the instrument inside the product before building one (--dump-land, --dump-object-ao), the ESM height layout that trips everyone ([row=y][col=x], SW origin, height/8, 128-unit spacing = the object lattice pitch), joining the two files into a per-square CLEARANCE number, and the rule that a constant copied from a caster carries its units. Use before claiming a rectangle of far terrain is "under a deck", "beside a wall" or "open ground", and before writing a new probe.
---

# WW: the clearance instrument — what stands over this square, from files that are not the map under test

Lane SLAB1 (2026-09-18) had to answer "is this rectangle under a floating deck
or beside a pier that reaches the water" for six rectangles, and every answer
had to come from somewhere other than the darkening map it was about to
measure, or the argument is circular
(`docs/LODGEN_TERRAIN_VT.md` 2.5h states that requirement for `--dump-object-ao`
in so many words).

## 1. Look for the instrument INSIDE the product first

Before writing a probe, grep the CLI for a dump that already exists. SLAB1
nearly rebuilt a terrain reader and then found `--dump-land` in `nifcli.cpp`,
shipped, tested, and one command away:

```bash
grep -n '"--dump' src/nifcli.cpp
```

A dump already in the product costs one bake and no build; a new probe costs a
build, and a build costs a `tasklist` check, an exe run and a rung copy. The
rule is not thrift, it is that **the shipped reader is the one the shipped
writer agrees with.**

## 2. `--dump-land`: the layout, and the three things that trip people

`nifcli.cpp` (the writer is a few hundred lines further in) emits:

```
int32 mnx, mny, cw, ch
cw*ch   bytes     present flags, one per cell
cw*ch*33*33 qint16  heights, GAME units divided by 8
```

* **`EsmLand::heights[33][33]` is `[row = y][col = x]`**, not `[x][y]`. Getting
  this backwards produces a plausible-looking field that is the terrain
  transposed, and a transposed Commonwealth still has hills.
* **The origin is SOUTH-WEST**, while every `.lodt` georef in this repo has
  **row 0 = NORTH**. One of the two has to be flipped, always; write the flip
  once, in a named function, and assert a known cell's height against the
  game's own value before using it.
* **The spacing is 128 world units**, which is the same number as the object
  height field's `CELL`. That is a gift, not a coincidence to rely on
  silently: the two grids line up square for square, so state the assumption
  and check it (`assert land_spacing == lattice['cell']`).

Multiply by 8 to get world units. It is 76 MB for the Commonwealth; read it
once and cache.

## 3. `--dump-object-ao`: the lattice, and its v2 min plane

`OBJH`, then `int32 gx0 gy0 gw gh`, `float cell`, then `gw*gh` float32 of the
MAX object Z, rows south to north, west to east, empty = `-1e30f`. Since lane
SLAB1 a SECOND `gw*gh` plane of MIN Z follows, empty = `+1e30f`. **Tell the
versions apart by LENGTH** — `24 + n*4` against `24 + n*8` — never by a version
word, because there is not one. It is a debug file and it is allowed to change
shape; a reader that assumes v2 on a v1 file reads the max plane as the min
plane and every square becomes a ceiling.

## 4. The join: clearance

```
clearance(square) = minZ(square) - terrain_under(square)
```

and `terrain_under` is a CHOICE that has to be stated:

* the mean of the square's four land nodes — what a census counts;
* the LOWEST of the four — what a conservative classifier uses, because then
  "this square is a wall" is true for every texel inside it.

Print the distribution, not a single number: SLAB1's chunk was 38.1 % walls
(clearance <= 0), 23.7 % low ceilings, 38.3 % high ceilings, and the two
rectangles that behaved unexpectedly were both explained by their own row of
that table rather than by argument.

## 5. A constant copied from a caster carries its UNITS

Root `MISTAKES.md`, 2026-09-18 05:0x: a vertex-AO cast was copied from the
`.BTO` path, which works in MINIATURE units (world × 1/dim), into a path that
works in WORLD units. The caster's fixed 2-unit ray offset was then 4× too
small and a deck shaded its own slab. **Every fixed constant inside a caster —
offset, reach, bias, epsilon — is a fraction of the geometry only in the units
it was tuned in.** When you copy a cast, copy its units, and say in the comment
which ones the copy is in.

The same rule applies to this instrument: `--dump-land` is in GAME units over 8,
the lattice is in WORLD units, and a rectangle quoted to bungo is in WORLD
units. Convert once, at the reader's edge.

## 6. Cross-validate the shipped code with the instrument, not with itself

Once you have the lattice and the heights you can re-implement the law under
test in twenty lines of Python and predict each rectangle's move before opening
the output. SLAB1 predicted the sign and size of six rectangles to better than
0.9 %, including one that moved the "wrong" way — which is what turned an
anomaly into an explanation with a number (the 432-unit crossover) instead of a
suspicion.

If the re-implementation and the exe disagree by more than the output format's
quantisation, **the re-implementation is wrong first**: it sums in a different
order, at a different precision (see `ww-population-refuter` §5 and the
`0.290780187 against 0.290780127` entry in root `MISTAKES.md`).

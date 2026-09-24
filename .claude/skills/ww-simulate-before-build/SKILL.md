---
name: ww-simulate-before-build
description: Before spending a build on a new knob in the NifSkope Wild Wasteland generator, apply the knob's own arithmetic offline to sheets that are ALREADY baked and read the whole pre-registered gate table off the simulation — so a rule that cannot meet its gates is refused with numbers instead of coded, compiled and measured. Use when a lane has one build to spend, when the game is up and the build cannot be spent at all, or whenever a candidate rule is a closed-form function of two bakes you already have on disk.
---

# Simulate the knob before you spend the build

A lane gets one build. A lane whose build cannot happen at all — bungo is in
the game, and the standing rule is that the game being up ends the lane BUILD
PENDING — still owes an answer. Both cases have the same remedy: most of the
generator's per-texel passes are closed-form functions of planes that are
already sitting in a scratchpad `out/` directory, so the candidate rule can be
run in Python on those planes and the whole gate table read off the result.

Lane ROADS3 (2026-09-12) did this for `--road-opacity` and it changed the
lane's conclusion. The simulation found, before any C++ existed, that on chunk
(-8,8) **no** opacity could satisfy the brightness gate — the composite can only
land the road between our ground (102.12) and our paint (106.68) and vanilla's
road is at 94.59, 7.53 levels outside that interval — and that on (-20,20) four
gates wanted four incompatible values (0.83, 0.326, 0.25 and 0.75). Coding it
first would have spent the build to learn the same arithmetic.

## When this is legitimate

All four must hold, and each must be CHECKED, not assumed:

1. **The pass is a closed-form composite** you can write down from the source,
   with the file and line quoted. Roads: `colour = ground + ( paint - ground )
   * a` at `src/lodgen.cpp:7944` and again at `:9261`.
2. **Both inputs are on disk as bakes.** The pass ON and the pass OFF, from the
   same exe, same region, same flags otherwise — e.g. `--roads` and
   `--no-roads`. Their difference IS the thing the knob scales.
3. **Every stage AFTER the pass is inert or branched over on these inputs**, and
   you show it. ROADS3: the grass tint never fires because the cover plane is
   empty on those chunks, and `--grade` is 1.0 with its multiply branched over,
   so the sheet's RGB on a road texel IS the road plane's RGB.
4. **A load-bearing control says the sheet really carries the pass.** ROADS3
   used `--road-detail 0` against `--road-detail 1`: 23,270 of 25,249 mask
   texels differ, which cannot happen if the sheet were not carrying the paint.

## What it cannot show, and must say it cannot

* **Quantisation and compression.** The simulation works in float; the baked
  sheet goes through 8-bit rounding and BC1. Expect agreement to about half a
  level, never to the byte. A byte-identity gate (a switch at its off value
  reproducing the rung) can NEVER be closed by simulation — that one is owed to
  the build, always.
* **Anything that changes the pass's own inputs.** Scaling a composite's alpha
  is simulable; changing which meshes are gathered, which triangle wins the
  z test, or how coverage itself is computed is not, because the plane on disk
  was made under the old rule.
* **Cross-stage side effects.** If the knob also feeds a later stage (roads
  suppress the ground cover), simulate only the stage whose inputs you have and
  name the other one as unsimulated.

## The procedure

1. Write the composite down from the source, with file and line, into the
   script's docstring. If you cannot write it down, stop.
2. Bake the ON and OFF variants once, on the RUNG exe, into the lane's own
   `out/` — never near his installed `Data\Terrain`.
3. Build the mask from the two bakes (where they differ is where the pass
   touched), and a signed distance to its edge for the profile gates.
4. Apply the candidate rule over a sweep of its parameter, and for EVERY value
   print every gate in the pre-registered table on the same row — level, rise
   over the surround, local SD, profile step, hue axes. One row a candidate,
   one column a gate.
5. Read the table for **mutual exclusivity** before reading it for a winner. If
   gate A wants 0.83 and gate B wants 0.33, the lane's answer is a refusal with
   numbers and a table for bungo, not a default.
6. Ship the knob anyway if it is honest to ship it — at an off value that is
   byte-identical to the rung by construction — and let the measurement decide
   the default later. Shipping the knob at off costs nothing and makes the
   question answerable next time.

## Labelling

Any panel, plot or number that came from the simulation says **SIMULATED** on
its face, beside the reason the build was not spent. A picture that mixes baked
and simulated panels labels each column. When the build is later spent, the
pictures are re-taken from real bakes and the report says which ones changed.

## How accurate the simulation actually is (measured, ROADS3, 2026-09-12)

Not a guess. ROADS3 priced `--road-opacity` offline, then baked the same
setting on the built exe and compared the two texel by texel on two chunks:

* on the **aggregates the gate table is read on** -- road mean luminance, rise
  over the surround -- the simulation agreed to **0.286** and **0.221** of a
  level;
* **per texel it did not agree at all**: mean absolute difference **1.583**
  levels, 99th percentile 7.341, worst **15.279** (1.527 / 5.745 / 13.802 on
  the second chunk).

The gap is 8-bit quantisation plus BC1 block compression, which the bake goes
through and the simulation does not. Read that as the licence this technique
carries: it licenses **which settings are worth baking**, and it does not
license a picture, a gate row, or a per-texel claim of any kind. An earlier
draft of ROADS3's own contract amendment said the simulation was "right to
about half a level" before anyone had measured it -- 3x optimistic on the
average and 30x on the tail. A tolerance is a measurement or it is not written
down.

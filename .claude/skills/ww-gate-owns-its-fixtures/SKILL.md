---
name: ww-gate-owns-its-fixtures
description: Repair or write a gate whose FIXTURES are files the tool under test is allowed to rewrite -- a cache directory, a scratch bake, an output folder reused as an input. Covers the failure where several "different" fixtures silently become one file, the cheapest refuter (hash them), rebuilding the fixtures inside the gate instead of leaving them on disk, the pairwise-distinctness rows and the metric that goes with them, and the companion floor for any row that asks two things to be THE SAME. Use whenever a gate reads an input it did not create in the same run, and whenever a gate row reports a perfect number (IoU 1.000, delta 0.00, 0.00 per cent) instead of a weak one.
---

# A gate must own the fixtures it is judged on

## The failure this skill exists for

`tests/spells/native_lighting.sh` had four terrain arms -- own normals, flat,
tilted east, tilted west -- each pointed at its own sheet-cache directory by an
environment variable. Three of those caches were artefacts written once by lane
NATIVEVIEW2 on 2026-09-12 and left on disk.

`src/lodtsheets.cpp` then gained a correct rule on 2026-09-18: reuse a cached
tile only when it is NEWER than the `.lodt` container it came out of. The
container had been re-baked on 2026-09-16 16:44. So the next gate run found all
four caches stale and refilled every one of them from the same container, ten
seconds apart:

    Commonwealth.VT.2.0.4.n.DDS   sha1 ad7f8084e3fb3653   in ALL FOUR caches
    t_own/flat/tilt/tiltw_obl.png sha1 b39e407cc0a7        ALL FOUR renders

Four fixtures became one photograph four times. Three gate rows went red --
darkest-fifth IoU 1.000 against a bar of 0.800, own-minus-flat blockSD 0.00
against a floor of 3.50, luma ordering on 0.00 per cent of pixels against a bar
of 99.00 -- and a fourth row went GREEN for the wrong reason, because it asked
two frames to be identical and every frame was identical.

Nothing was wrong with the code under test. The gate had been depending on files
the program was entitled to overwrite.

## The smell

Read these as "the fixtures collapsed" until proved otherwise:

* a similarity row at exactly 1.000, a difference at exactly 0.00, a percentage
  at exactly 0.00 or 100.00 -- these are not weak signals, they are no signal;
* several red rows that all compare arm A with arm B, while every row that reads
  one arm alone stays green;
* a row that asks for SAMENESS sitting green next to them.

## The refuter, and it costs one command

Hash the fixtures, then hash the outputs. Before any hypothesis about the
shader, the camera, the arithmetic:

    for d in <cache1> <cache2> <cache3> <cache4>; do sha1sum "$d/<one tile>"; done
    sha1sum <out>/*_obl.png

Distinct hashes mean the collapse is not the cause and the diagnosis moves on.
Identical hashes end the investigation in one line.

## The repair: build them in the gate, in the right order

1. **Run the arm that refreshes the shared source FIRST.** The `own` arm is what
   makes the program repopulate the cache from the live container; only after it
   has run is that cache a usable source for the synthetic ones.
2. **Generate every synthetic fixture from that source, every run**, in a script
   the gate calls (`tests/spells/<gate>_fixtures.py`). Their mtimes are then
   newer than the container by construction, so the freshness rule that ate them
   now protects them.
3. **Assert the source was actually refreshed** before copying it: walk it and
   fail if any tile is older than the container. Otherwise a stale source is
   copied into three stale twins and the gate is green on nothing.
4. **Change only the one thing under test, and prove it.** The fixture builder
   rewrites the normal sheets and copies the colour sheets through. The gate
   then carries BOTH rows: the normal sheets are pairwise distinct, and the
   colour sheets are identical. The second is the control that says the builder
   moved one variable.
5. **Read the synthetic constant BACK** through whatever quantisation the format
   applies and print the decoded value. `(191,238,128)` asked of a 5/6/5 block
   comes back `(189,239,132)`; the arithmetic downstream must use the value that
   is actually in the file. When the read-back reproduces the constants the
   checker has hardcoded, that is a free independent confirmation.

## The rows that go with it

Four rows, and they come FIRST in the log because nothing below them is worth
reading if they are red:

* **inputs distinct** -- N distinct sha1 of N fixtures, with the collapsed count
  quoted in the row text so a reader sees what it is guarding against;
* **inputs otherwise identical** -- the control of point 4;
* **outputs distinct** -- N distinct sha1 of N renders;
* **the weakest pair, as a number** -- a hash says "not identical", which one
  changed bit satisfies. Report mean |difference| for every pair and put the
  floor on the WEAKEST one. Pre-register that floor from the gate's own algebra,
  not from the measurement: here the closest pair the physics predicts is
  N.L 0.7258 against 0.4434, so a floor of 2 luma levels is far below any real
  ordering and far above encoder noise.

## Any row that asks for SAMENESS needs its own floor

A row of the form "these two must be byte-identical" passes trivially under a
fixture collapse. Give it a companion on the same view whose inputs provably
DIFFER, and require that pair not to be identical:

    gate (d): at the TOP view east and west tilt are the same picture     <- the claim
    gate (d) (floor): at the TOP view flat and east tilt are NOT          <- the floor

Without the second, the first is green in exactly the world where the gate is
broken. (This is `ww-test-harness-add` rule 5 -- a floor that cannot fire -- in
the specific shape that fixture reuse produces.)

## What is NOT the repair

Do not "fix" the program so it stops refreshing the cache, and do not date-stamp
the fixtures forward. The freshness rule is correct and the cache is the
program's to manage. The gate is what must stop treating a managed directory as
a fixture.

Do not delete the collapsed row. A gate that has been red for a reason nobody
has named gets read as noise within a day; name the cause with the hashes and
repair the arming.

## Provenance

Lane GATEFIX1, 2026-09-19, repairing `tests/spells/native_lighting.sh` (lane
NATIVEVIEW2's gate, 2026-09-16) after lane HARNESSWIN2 found and hashed the
collapse the same day. Builder: `tests/spells/native_lighting_fixtures.py`.

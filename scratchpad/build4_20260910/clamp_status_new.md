**STATUS: BUILT AND MEASURED** (lane BUILD4, 2026-09-10). `qmake` rc 0 then
`make -j2` rc 0, `Nothing to be done` -- lane WATER2's link of **01:01:04**
already carried this code, and the object-level check says so rather than
inferring it (`lodgen.o` 00:44:00 against `src/lodgen.cpp` 00:40:11, and every
object including a header a pending lane touched is newer than that header).
Staleness sweep over all 15 changed files under `src/ res/ tools/ tests/`: 0
stale. `res/style.qss` and `release/style.qss` in step.

**The harnesses.**

| harness | result |
|---|---|
| `lodgen_terrain_vt.sh` | **35 checks, 0 failures, RESULT PASS** (32 -> 35 as predicted) |
| `lodgen_terrain.sh` | **26 checks, 0 failures, PASS** |
| `lodgen_identity.sh` | **8 ok, RESULT PASS**, baseline unmoved |

V9a is byte-identical with the tint OFF and, newly tightened to a `cmp`, with
the tint ON; V9b's `_msn` sheets are byte-identical on all four chunks; the
FLOOR reports **8 of 8** cover/no-cover sheet pairs differing. V9c landed on its
pre-registration to three significant figures: **E/W 2.75** (bar <= 3.20,
clamped 4.07) with interior control 1.803, **N/S 2.87** (bar <= 3.30, clamped
3.78) with interior 1.603, **edge step 1.961** (bar <= 2.60, clamped 3.310).
`lodgen_terrain.sh`'s pyramid statistics did not move: assembled and direct both
`UP=G D0=76 D1=32 D2=51 D3=32`, vanilla's own sheet `99/67/67/67` as the control.

**The edge-band re-baseline**, both AFTER bakes rc 0, 24 of 24 `.DDS` written,
absolute paths throughout.

| sheet | band | differing, 4 chunks | max distance | beyond the band |
|---|---|---|---|---|
| colour, cover | <= 4 | 127 | 3 | **0** |
| `_msn`, cover | <= 4 | 25,537 | 3 on the y=24 chunks, **7** on the y=28 chunks | **0 / 1,014 / 1,405** |
| `_data`, cover | <= 64 | 65,872 | 47 on three chunks, **79** on `-20.28` | **0 / 16** |
| colour, cover-free | byte-identical | **0 on all four chunks** | - | **0** |

The new baseline, the twelve `sha256[:16]` of the cover AFTER sheets, written
here beside the numbers that justify it and never alone:
`-24.24` **1f39d6aa0c64e83c** / `_msn` **594e49b57eadbb0b** / `_data`
**7f8af632f8e07c96**; `-20.24` **5478915e2145d15f** / **5b5e55e3279c0fc7** /
**7b58975182a55c6a**; `-24.28` **ce8a81d375d2cd97** / **dffe13752c2321b1** /
**72b268e4b00da1bb**; `-20.28` **dbbdeb8d39227021** / **3dc2c5b40b135e27** /
**b2ec30e781bb8dfb**.

**TWO BANDS MISSED, AND THE CAUSE IS IN BETHESDA'S DATA, NOT IN THE RING.**
994 of 1,014 and 1,405 of 1,405 beyond-band `_msn` texels sit on the **north**
border, at distances 4,5,6,7 and nowhere further; every passing border reaches
exactly 3. `--dump-land` over cells x=-24..-17 says the master disagrees across
exactly ONE shared vertex row here -- **y=31 | y=32, by 1..9 VHGT units** -- while
y=23|24, y=27|28, y=32|33 and both east seams read **0**. So
`lodgenTerrainFillRing`'s documented south-to-north order lets the y=32 cell
overwrite the row it shares with y=31, which is the chunk's OWN boundary grid
row; everywhere else the two cells agree and only points outside the chunk can
move. The reach then follows: a texel at distance `t` reads heights at `t +- 4`
texels through a bilinear tap, so grid index -1 is read out to `t = 3` and grid
index 0 out to `t = 7`. **The pre-registered band of 4 counted the central
difference's 128-unit step but not the bilinear tap's own 128-unit footprint.**
The 16 `_data` texels are exactly one 4x4 BC block and are the wetness flow
accumulation -- a global operator over the chunk grid whose north boundary row
moved, already named as owed item 2 of the lane's report.

**Neither miss was fixed and neither band was re-pinned** (a resuming lane
measures a failure and stops). The band is the lane's to re-derive or bungo's to
accept. NOT COMMITTED; bungo's open window needs a restart.

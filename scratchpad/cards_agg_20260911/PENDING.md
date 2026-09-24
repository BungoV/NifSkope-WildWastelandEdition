# Lane CARDS-AGG -- resume file: **NOT PENDING, THE LANE FINISHED**

Superseded 2026-09-11 16:43. The build ran (15:49:39) and one counted relink
followed (16:20:02); every gate is green or explained; `DONE` is in and
`BUILDING` is gone. Read `scratchpad/lane_cards_agg_report.md` and
`scratchpad/cards_agg_20260911/HANDOFF_BLOCK.md` instead. What follows is the
file as it stood before the build, kept because it is the record of what the
lane knew at that moment.

---

# Lane CARDS-AGG -- resume file (written 2026-09-11 15:47 from `date`, before the build; last touched 15:55)

## Where the lane is

The CENSUS is finished and is section 1 of `scratchpad/lane_cards_agg_report.md`
(timestamped 15:11..15:25, before any code -- gate A1). The CODE is written and
every changed translation unit passes `-fsyntax-only` with the real flags. The
BUILD HAS NOT RUN YET.

Exe at launch: `release/NifSkope.exe` 2026-09-11 14:48:52, 21,261,312 B, md5
`b9b8f55472514b1e1c2bb7a5de780737`. Rollback rung taken ONCE:
`release/NifSkope.before_cards_agg.exe`, same md5.

## What changed on disk (nothing committed)

| file | what |
|---|---|
| `src/lodgenaggregate.h` / `.cpp` | NEW. The aggregate compositor: one card set per forested cell, 8 horizon azimuths, orthographic composite of the cell's own trees' card sheets at the repetition breaker's rotation and mirror; the `kind: "aggregate"` `.lodm` writer and the sheet path stem. |
| `src/lodifile.h` / `.cpp` | `.lodi` **v4**: `LodiAggregate` (48 B) + a u32 covered-instance blob, header words at 0xB0..0xD3, writer refusals, reader validation, `lodiDescribe` rows. Version 4 is written ONLY when an aggregate exists; otherwise version 3, byte-identical. |
| `src/nativeemit.h` / `.cpp` | `lodgenNativeSetAggregate()`, the tree gather inside the instance loop, the aggregate build before `lodiWrite`, the `native-aggregate:` census line with the count-identity verdict. |
| `src/lodgen.h` / `.cpp` | `lodgenAggregateCards()` (reads the card library through the tree's ONE sidecar reader) and `lodgenAggregateWrite()` (dilation + DDS + `.lodm`). |
| `src/nifcli.cpp` | `--aggregate` / `--no-aggregate` / `--aggregate-min` / `--aggregate-tile` / `--aggregate-views`, the arming, the sheet write, the usage text. |
| `NifSkope.pro` | the two new sources (LF-only, CR count 0 verified). |

## The first three actions on resume

1. **`qmake` MUST be re-run** -- two NEW sources and a new header included by
   `lodgen.h`, `nativeemit.h` and `nifcli.cpp`; the frozen dependency list does
   not name them. Then `make`, and read **make's own exit code**, never grep's
   (`nifskope-ww-build-verify`).
2. Check `Fallout4.exe` is down and that `release/NifSkope.exe` is not held; if
   bungo's window holds it, RENAME the running copy aside
   (`NifSkope_inuse_<pid>.exe`), never kill it.
3. Put `scratchpad/cards_agg_20260911/BUILDING` up before the link and replace
   it with `DONE` after the gates.

## The gates still owed (pre-registered in report section 0)

* A2 count identity -- the census line already prints
  `count identity photographed N == file covered M == AGREE|DISAGREE`; the gate
  reads it and re-derives both numbers from the bytes.
* A3 the calibrated picture gate, floor (a WRONG cell's aggregate) and ceiling
  (the individual render against itself), plus the one-tree control: an
  aggregate of a SINGLE tree must reproduce that tree's own card frame, which
  is what catches a mirrored view basis.
* A4 height moves; A5 `--aggregate` off byte-identical to the rung;
  A6 the standalone gate for the v4 layout and the new `.lodm` kind.
* The baseline chain: `lodgen_native.sh` 18/0, `lodgen_panel_run.sh` 125/0,
  `lod_generation.sh` 116/0, `lodgen_terrain_vt.sh` 41/1 (pre-existing red).

## The card library the composite needs

`scratchpad/cardwidth_20260910/cards` -- 20 sets, `projection ortho` AND
`coverage 16 128 160`, i.e. the current vintage. It covers **19 of the
Sanctuary region's 23 tree bases**; the other four are refused in words by
design and their trees stay per-tree. The older libraries
(`cardfinal_20260909`, `cardpad_20260909`, `images_20260909`) are the
PERSPECTIVE vintage and the module refuses them by name -- do not point
`--impostors` at them for this feature.

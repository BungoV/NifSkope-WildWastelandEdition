# Lane NATIVE1c -- the object library from each base's NEAR model, trees never become stumps, cards darkened by their surroundings, and the four header words

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: read `release/NifSkope.exe`'s mtime and size
  yourself and write them in the report's first line (the queue runs NATIVEVIEW2 -> VT1 -> GENSMALL1 -> NATIVE1c -> BTOFREE1,
  one lane at a time, so the exe you find carries the lanes before you). Rung ONCE before your first build:
  `release/NifSkope.before_native1c.exe` (copy of the exe on disk at that moment; never delete any `release/NifSkope.before_*.exe`).
  Markers `scratchpad/native1c_20260916/BUILDING` (touch FIRST) / `DONE` (first word `library`). Report
  `scratchpad/native1c_20260916/lane_native1c_report.md`, incremental; `PENDING.md` past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the exe
  aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did not
  create. `-no-gui` bakes need no GUI slot; a GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time. Every
  path in argv and every WW_* path ABSOLUTE `E:/...`. Never bungo's installed Data/Terrain, never the whole Commonwealth: the
  fixture region is Sanctuary / chunk (-20,24) as the earlier native lanes used it (`scratchpad/nativeview1_20260912/shots.sh`,
  `tests/spells/lodgen_native.sh`).
- You are the ONLY lane in the tree. No mutex needed; leave none behind.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the 2026-09-16 lead line and the 2026-09-12 lines under it);
  `MISTAKES.md` (root, top entries) and `docs/MISTAKES.md`'s lodgen section; `docs/LODGEN_NATIVE_LODO_LODI.md` IN FULL
  (the `.lodo`/`.lodi` law; §11 deviations, §12 the free room); `docs/LODGEN_CENSUS.md` §6 (what the runtime needs from
  the file); `docs/FO4CS_IMPROVED_LOD_PLAN.md` §5 and §6 (what the generator owes, and bungo's rulings).
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen` (whole), `nifskope-ww-build-verify`, `ww-module-off-is-identical`,
  `ww-test-harness-add`, `ww-anchored-hookup`, `ww-census-contract`, `ww-contract-provenance` (for every doc line you change),
  `ww-spec-gate-audit` (before reproducing any number a plan row states).
- Also read: `docs/LODGEN_IMPOSTOR_SPEC.md` (cards and their AO channels), `WW_CHANGES.md` entries NATIVE1a, NATIVE1b, CARDS-AGG;
  `scratchpad/native1b_*/` pictures (`ladder.png`, the stump); the HANDOFF lines "OWED TO NATIVE1c" (two, 2026-09-11 15:3x and
  16:1x). Skills add: `ww-silhouette-compare` (the level gate), `ww-control-calibration`, `nifskope-ww-render-shot`,
  `nifskope-ww-vanilla-compare` (the picture), `ww-downsample-gate`.

## bungo's words, verbatim
- 2026-09-11 10:3x: "extend the cluster LOD inward" (the ruling §3.5.4 serves).
- 2026-09-11 16:1x, over native1b's `ladder.png`: "Hm, that tree LOD becomes a stump there".
- 2026-09-11 15:3x: "is vertex AO baked into impostors too on top of the texture AO they hold?"
- 2026-09-16 11:1x: "Also do the parked" -- the director read this as the ruling on plan §6 (a): BUILD THE LIBRARY FROM THE
  NEAR MODEL. If the director's reading is wrong the lane is stopped by bungo, not by you; proceed on it and keep the old
  behaviour as the exact way back (`--library mnam`, default `near`).

## The work
1. **Level 0 from the base's near `MODL`** (NATIVE §3.5.4: no format change; the library's level 0 is whatever the emitter
   puts there). The base's `MNAM` LOD slot becomes one rung down the ladder (its own level, not simplified from the near
   mesh -- Bethesda's LOD mesh is authored, keep it as a level), and the ladder continues from there. Way back `--library mnam`
   = today's bytes exactly (`lodgen_native_baseline --check` on that arm: 25 files 0 differ). Report the library size ratio
   (bytes, triangles, near vs LOD) and the bake time, both ways, on the region.
2. **Foliage refusal.** The ladder REFUSES alpha-tested foliage clusters (leaf cards do not simplify: the crown becomes
   fragments, then nothing). Trunk / opaque clusters may ladder. A tree's far representation is the card, by ruling. Census:
   `ladder-refused: foliage N` written AND moving (a region without trees reads 0).
3. **The silhouette gate.** Every level of every mesh keeps >= a stated fraction of level 0's silhouette from the horizon
   views (`ww-silhouette-compare`, with `ww-control-calibration`'s floor/ceiling pair: a random-vertex-drop twin as the floor,
   the unsimplified mesh as the ceiling) or the level is refused and the mesh stops laddering there. A building may shrink;
   a tree may never become a stump. Pin the fraction as a switch with its default stated and the reason.
4. **Per-instance AO on cards.** Cards carry the model's vertex colour and a self-AO x texture AO, but no PLACEMENT AO (ground
   contact, neighbours), which the chunk meshes have per vertex (colour B, ray-cast). Add a per-instance AO byte -- the
   `.lodi` record has no free byte at stride 24 (NATIVE §4.1: a set reserved bit is a refusal; bits 6-15 of flags are the only
   room) -> the honest home is the COLD record or a new per-chunk u8 table; state the choice, bump the version and REFUSE the
   old one by name the way v3 refuses v2, ray-cast at bake against the chunk + heightfield exactly as the chunk vertices get
   theirs. Census: written AND moves (a tree under a bridge reads darker than one in a field; both numbers).
5. **The four header words** (plan §5 rows 1, 2, 3, 5; census §6.3 items 1, 2, 3, 5): four u32 per-MNAM-slot instance totals
   in the `.lodi` header (0xD4..0xFF has 44 bytes), a per-base full-detail triangle count in `LodoBase.crossPx16[4]` (written
   as zeros today; re-using it is a format decision -- state it in §11 deviations and bump), a card count u32 in the `.lodo`
   header (0xCE..0xFF), a watertight bit in the `.lodo` mesh row's free flags. Each written AND moving under
   `lodgen_native_fields.py`; each read back by `lodgen_native_decode.py`.
6. **Gates**: `lodgen_native.sh`; `lodgen_native_baseline --check` on the `--library mnam` arm (0 differ); the new
   `lodgen_ladder.sh`: (a) no foliage cluster laddered (refuter: the rung ladders N of them), (b) the silhouette fraction
   holds on every kept level and the floor twin FAILS it, (c) the near-model library's level-1 median deviation is now
   selectable inside the Commonwealth at 1 px (the plan's number: the first step selected somewhere below 52,100 units --
   report the new distance), (d) the AO byte moves, (e) the four header words move; `lodgen_native_decode.py` on every
   fixture; `native_open.sh` (the viewer opens the new version); `lodgen_defaults.sh` 28/0.
7. **Pictures** (`images/`, labels burned in, sizes read back with PIL): (i) the re-shot `ladder.png`: the same tree, rung vs
   new, per level -- the stump gone; (ii) one building's ladder from the near model beside its MNAM ladder; (iii) a card under a
   bridge and one in a field, AO byte shown as a number. Send nothing yourself.
8. **Docs**: `docs/LODGEN_NATIVE_LODO_LODI.md` (§3.5 the source of level 0, §4.1 the AO home, §11 the deviations, §12 the
   room used, version words, `ww-contract-provenance`), `docs/LODGEN_CENSUS.md` §6.3 items closed, `docs/FO4CS_IMPROVED_LOD_PLAN.md`
   §5 rows 1/2/3/5 and §6 (a) marked as ruled with the date.
9. **Report sections**: `## 0. Exe at launch`, `## 1. The library` (sizes, times, the selectable distance), `## 2. Foliage and
   the silhouette gate`, `## 3. Card AO`, `## 4. Header words`, `## 5. Gates`, `## 6. Build and chain`, `## 7. Pictures`,
   `## 8. Owed / red / bungo's calls` (the pixel tolerance §6 (b) is his; the residency budget §6 (f) is his), `## 9. Mistakes`,
   `## 10. Skill review`.

## Documents in `scratchpad/native1c_20260916/`
`WW_CHANGES_ENTRY.md` (starts `## 2026-09-16 — <title>`), `HANDOFF_BLOCK.md` (10-20 lines, plain language, a blank line after
its lead line), `MISTAKES_ENTRIES.md` (and root `MISTAKES.md` at the TOP the moment recognised), `CHANGED_FILES.txt` (A/M +
path, CR/LF byte counts before and after: `b.count(b'\r')` in Python, never grep), the report with the numbered sections
below, the LAST always `## Skill review` (skills loaded, skills that should have existed, skills written -- write them the same
session at `<repo>/.claude/skills/<name>/SKILL.md` and list the file).

## Rules (all lanes)
No defaults change (bungo's four rulings of 2026-09-12 and every other default stay; `--road-detail` 1 in every bake). Every
behaviour a user can see has an exact way back pinned by a gate (`ww-module-off-is-identical`). Any field that ships is WRITTEN
and MOVES under a test (the three rules of 2026-09-04 21:33). Nothing stated as a cause without a measurement. Plain
language; numbers beside floors; no "final/true" claims; incremental report writes; if context runs short, PENDING.md with exact
resume steps and a DONE-so-far list. The build chain is `nifskope-ww-build-verify` (game check as its own command, rename aside,
`make -j2` gated on make's rc, exe newer than EVERY changed file via `git status --porcelain -- src res tools tests`, stale-object
check for every header touched, `make -n` zero compile lines); every harness count next to the exe timestamp; skipped harnesses
named with the reason. Tell the director in `HANDOFF_BLOCK.md` that bungo's open window needs a restart.

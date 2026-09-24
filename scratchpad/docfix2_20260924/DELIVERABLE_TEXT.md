# DOCFIX2 deliverable (2026-09-24 01:46)

Docs and the lodgen skill only. No src, tests, res, build, launch or commit.
Files edited (both LF-only before and after, measured with Python byte counts, 0 CR):
* `docs/LODGEN_TERRAIN_VT.md` (3046 -> 3124 lines). It already carried other lanes' uncommitted
  edits; commit by explicit path only after reading the diff.
* `.claude/skills/nifskope-ww-lodgen/SKILL.md` (672 -> 705 lines; the folder is untracked in git).

## Job 1: VTNORMAL1 doc text applied (checked against src first)

| section | done | source checked |
|---|---|---|
| §2.1 stale sentence | replaced; added "only the finest level is baked from the paint" and a `--vt-density` paragraph (CLI 32, panel 16) | lodgen.cpp:11093-11120 (ladder starts at finest), 12867-12881 (only levels[0] baked); nifcli.cpp:8804-8824; lodgenmanager.cpp:1276-1291 |
| new §2.2b normal source | added, after §2.2a | lodgen.cpp:12394-12476 (sheet reader, vector box, row 0 north), 12478-12514 (border reads neighbour), 12693 (heights plane), 13173-13181 (census) |
| §3.1 row 0xA0 `mipSkip` | amended | io/lodvfile.cpp:161, 211, 600-608; lodgen.cpp:12665-12668 |
| §3.3 | added the half-sheet sizes (computed: BC1 9,248 / BC3 18,496 / R16 36,992) | io/lodvfile.cpp:221-244 |
| §3.4 rule 13 | mipSkip rules added | io/lodvfile.cpp:600-608, 655-659 |
| §4 index keys | `halfAux` / `mipSkip` / `texels`, `normalSource` added | lodgen.cpp:12985-13013 |
| §5 rows | `--vt-density`, `--vt-half-aux` added; `--msn-cache` amended | nifcli.cpp:8088-8089, 3030-3034; lodgen.cpp:7211-7301, 7493-7600 |
| §7a.3 | appended | lodgen.cpp:4591-4616 (luminance 0.299/0.587/0.114) |

### Corrections to the lane text (the source won)
1. "Only roles 2, 4, 5 and 6 may set `mipSkip`": the READER refuses it only on the colour sheet (and
   past `sheetCount`); it does not restrict role 7. The writer sets it on every non-colour sheet.
   Written that way.
2. The flip proof: the lane said r "drops from 0.94 to 0.12"; the source comment
   (lodgen.cpp:12397-12399) says every r drops from 0.66-0.97 to 0.13. The doc says "to about 0.13".
3. `none` is panel-only (lodgenmanager.cpp:2598-2601). On the command line `--msn-cache none` is a
   folder named none; leaving the switch out is off. The row says so.
4. `auto` also accepts sheets loose in the folder itself, matches any world (`*.4.*_msn.dds`), and
   checks only the first sheet's width. The row says so.
5. `--vt-density 0` is treated as unset, not refused: the row says "any other non-zero value".
6. The heights normal's second staging plane exists only when the `.btr` sheets are assembled from
   the pyramid (lodgen.cpp:12693). The §2.2b text says so.

## Job 2: the 5 items owed after VTNORMAL1 (re-checked against the new source)

| # | item | status |
|---|---|---|
| 1 | §4 old `Data\Terrain\` index path | fixed: `Data\FO4CSLOD\<EDID>\<EDID>.VT.lodm` (lodgen.cpp:12568, 13100; lodgenlayout.cpp:19-27). The same stale path was also in §5's `--vt DIR` row; fixed there too |
| 2 | §5 sampler defaults shown as off | fixed: warp 341 / 1024 / 1, mip bias -0.22, hex 256, guide `flatwarp:1.0`, plus the four-switch way back (lodgen.cpp:6414-6417, 6609-6610, 6843). The `--land-guide` row also said an unknown rule leaves `off`; it leaves the default (nifcli.cpp:7800-7806) |
| 3 | §3.4 rule 13 says 1..6 sheets | fixed: 1..10, roles up to 7, the horizon-shape rules, role-7 format 28 (io/lodvfile.cpp:96-107, 564-659) |
| 4 | §2.3 says the AO march reaches 2,048 | fixed: it samples out to 1,458; 2,048 is the loop bound (lodgen.cpp:9054, 9282, 10829, 12070) |
| 5 | a §5 row cites "§3.6" | fixed: both citations in the `--vt-height` row now say §3.3 |

Extra, found while checking: §5 `--road-ground-paint` said default 1.0; it is 0 since DEFAULTS1
(lodgen.h:1004-1010). Fixed.

## Job 3: skill review applied (`.claude/skills/nifskope-ww-lodgen/SKILL.md`)
* CLI section: a "Terrain pyramid switches" block after the defaults block -- `--vt-density`,
  `--vt-half-aux`, `--msn-cache DIR|auto` (feeds the pyramid, takes a mod root, `auto`, census words,
  panel empty = auto, a miss is silent).
* Editing traps: float contraction moves bytes (`std::fma`, fmaprobe.py); a BC1 ceiling uses OUR
  encoder (encceil.py).
* The gates: the `.lodb` is never byte-equal across two exes; `lodgen_perf.sh` leg (c) assumes a
  pre-2026-09-17 rung (waybk.sh). Placed under "The gates" rather than "Editing traps": both are
  about reading a gate, not about editing.

## WW_CHANGES text
DOCFIX2 (2026-09-24): `docs/LODGEN_TERRAIN_VT.md` now matches the source after VTNORMAL1.
* New §2.2b: the pyramid's normal comes from the normal-sheets folder when it is set, and the census
  and index say how many tiles took which.
* `--vt-density`, `--vt-half-aux` and the header's `mipSkip` byte are documented (§2.1, §3.1, §3.3,
  §3.4, §4, §5). `--msn-cache` takes a mod root and `auto`.
* Stale lines fixed: the index path (`FO4CSLOD\<ws>\`), the sampler and road-ground-paint defaults,
  rule 13's sheet count (1..10), the AO reach (1,458), a dead §3.6 cite.
* The lodgen skill has the new switches, two editing traps (float contraction, our BC1 ceiling) and
  two gate traps (`.lodb` across exes, `lodgen_perf` leg (c)).

## HANDOFF text
DOCFIX2 done, uncommitted: VTNORMAL1's TERRAIN_VT text applied with 6 corrections from the source,
the 5 owed DOCFIX1 items fixed, plus 2 extra stale defaults (`--road-ground-paint`, the `--vt DIR`
path). Lodgen skill updated. Files: `docs/LODGEN_TERRAIN_VT.md`, `.claude/skills/nifskope-ww-lodgen/SKILL.md`.

## MISTAKES text
2026-09-24 DOCFIX2 -- a default row that outlived its ruling. `docs/LODGEN_TERRAIN_VT.md` §5 still
showed `--road-ground-paint` 1.0 and the four sampler switches as off, twelve days after DEFAULTS1
moved them; the skill had the right values. Rule: a lane that moves a default greps the contract
page's CLI table for the switch in the same landing.

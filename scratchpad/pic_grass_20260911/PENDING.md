# Lane PIC-GRASS -- PENDING: the wait gate never opened

Written 2026-09-11 16:2x. Nothing was baked, no picture exists yet, and no
number in this folder has been measured. Everything else is staged.

## Why it is pending

The brief's hard rule: wait until `scratchpad/cards_agg_20260911/DONE` exists
and no `scratchpad/*/BUILDING` exists, polling every 60 s, **at most 60 polls**
-- lane CARDS-AGG holds the one allowed NifSkope instance and the build slot.

| time | state |
|---|---|
| 15:1x (poll 1) | no `DONE`, no `BUILDING` anywhere -- CARDS-AGG had not started its build |
| ~15:5x (poll ~41-50) | `scratchpad/cards_agg_20260911/BUILDING` appeared, with `PENDING.md` beside it |
| 16:20:02 | `release/NifSkope.exe` relinked, 21,419,520 B (was 21,261,312 B at 14:48:52) -- CARDS-AGG's link landed |
| 16:22:15 (poll 60) | `BUILDING` still up, `DONE` still absent, `tasklist` rc=1 |

Sixty polls used. The exe had just been relinked, so CARDS-AGG was into its
gate runs -- which launch NifSkope. Baking then would have been a second
NifSkope process against an exe another lane is still gating. Refused.

## What is on disk and ready

| file | what |
|---|---|
| `bake.sh` | the two headless bakes, sequential, each preceded by the `tasklist` rc=1 gate; region `-20 20 -17 23 --dim 4`, into `out/default` and `out/tint0`, `--data-root E:/Tools/Fallout 4/DataUnpacked/Data`, never his installed `Data\Terrain` |
| `make_picture.py` | the four-panel picture + `images/numbers.json`; reads vanilla from `E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth` through `tests/spells/lodgen_terrain_model.py`'s `Dds`, as ROADS1 and TERRAIN-R did; the error metric is ROADS1's own (`np.abs(X-V).max(2)` then mean) so the numbers are comparable with `lane_roads1_report.md` |
| `run.sh` | gate -> bakes -> picture, one command, and it REFUSES by itself if the markers are not clear |

Vanilla's sheet was decoded as a read-only check (no process launched):
`Commonwealth.4.-20.20.DDS`, DXT5, 512x512, mean RGB (91.6, 83.4, 72.1),
alpha constant 255, `dwReserved1` 0 -- i.e. Bethesda's sheet carries no cover
stamp, which is expected and is why the cover mask is taken from OUR bake's
`_data.DDS` alpha (`docs/LODGEN_TERRAIN_VT.md` 1.4).

## The resume, one line

```bash
bash /e/Projects/NifskopeWildWastelandEdition/scratchpad/pic_grass_20260911/run.sh
```

Then write `NOTES.md` from `images/numbers.json` (the two commands, the exe
time/size, the numbers, two sentences per panel BEFORE any claim), and open
`images/cmp_grass_tint.png` and read it back (`ww-texel-picture` 5).

**One thing the resumer must decide, not assume:** the exe that will run the
bakes is now CARDS-AGG's 16:20:02 build, not the 14:48:52 one this lane
started against. That is fine for this picture -- CARDS-AGG touched the
aggregate card path, not the cover or tint path -- but the exe's time and size
go in `NOTES.md` as read at bake time, never as typed here.

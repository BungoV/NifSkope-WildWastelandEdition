# PIC-GRASS -- the grass tint in the far colour sheet, on the CORRECTED tiling

Run by lane RESUME3 at **19:23:29-19:23:36** on 2026-09-11, after the tiling
relink, so both of our panels carry the engine's own landscape repeat
(341.3333 world units) and not the 6x-too-large 2,048 the earlier bakes used.
Picture `images/cmp_grass_tint.png` (2148 x 1294), numbers
`images/numbers.json`.

## Provenance

| | |
|---|---|
| exe | `release/NifSkope.exe` **2026-09-11 19:08:42, 21,435,904 B** |
| region | chunk (-20,20), cells -20..-17 x 20..23, `--dim 4` |
| out-dirs | `out/default/`, `out/tint0/` -- this lane's own; his installed `Data\Terrain` was never written |
| vanilla | `E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/Commonwealth.4.-20.20.DDS` |
| process gate | `tasklist` rc=1 before each of the two bakes; one NifSkope at a time |

The two commands, exactly as `bake.sh` issued them:

```
release/NifSkope.exe -no-gui lodgen <Fallout4.esm> --worldspace 3C \
  --terrain-region -20 20 -17 23 --dim 4 \
  --out-dir out/default/obj --tex-dir out/default/tex \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" --cover

release/NifSkope.exe -no-gui lodgen <Fallout4.esm> --worldspace 3C \
  --terrain-region -20 20 -17 23 --dim 4 \
  --out-dir out/tint0/obj --tex-dir out/tint0/tex \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" --cover --grass-tint 0
```

Both `rc=0`, three texture files each.

## The cover plane the measurement stands on

From our own bake's `Commonwealth.4.-20.20_data.DDS` alpha, DXT5, stamped
`0x56435757` (`WWCV`): **137,399 of 262,144 texels carry cover > 0**, max 73,
mean 22.84 over the covered texels. The bake's own cover census agrees:
`coverMax=73 paintedPts=18496 tintStrength=0.350 coverFull=96.0`,
`ltexNoGnam 8/105`, `grasNoTint 2/82`, `pxUnresolvableLtex=0`.

## The numbers

| quantity | value |
|---|---|
| mean colour error vs vanilla, whole tile, **tint on (0.35, shipped)** | **20.97** of 255 |
| mean colour error vs vanilla, whole tile, **tint off** | **21.92** of 255 |
| mean colour error vs vanilla, **over cover>0 texels only**, tint on | **19.31** |
| mean colour error vs vanilla, **over cover>0 texels only**, tint off | **21.12** |
| the tint alone, whole tile | mean **3.34**, max **27.42** |
| the tint alone, over cover>0 | mean **6.31**, p95 **16.45** |
| the tint's largest reach OUTSIDE cover>0 | **22.94** |
| zoom window (chosen by the cover plane, not by eye) | x 280..408, y 64..192, 14,914 covered texels in it |

## Two sentences per panel, before any claim

**Panel 1, VANILLA.** Bethesda's shipped sheet: a pale, low-contrast ground with
the loop road drawn as a faint light band and no hard edges anywhere. It is the
reference both of our panels are scored against, and it scores 0.00 against
itself by construction.

**Panel 2, OURS with the shipped tint 0.35.** The same ground and the same road,
noticeably darker overall than vanilla, with our road raster drawn far more
crisply than vanilla draws it (kerbs and sleeper marks are individually
visible). The mottled green-brown patches over the wooded half are the cover
plane's tint mixed into the albedo.

**Panel 3, OURS with `--grass-tint 0`.** Indistinguishable from panel 2 at a
glance: the same ground, the same road, the same overall level. The difference
is in the wooded half, where the mottling is slightly flatter and browner.

**Panel 4, the tint alone, x4.** Black over the road, the water and the bare
ground, and a coloured speckle exactly where the cover plane is -- so the tint
is applied where the cover says and nowhere else, to a mean of 6.31 of 255 over
those texels.

## What the numbers say, and what they do not

* **The tint helps, and the amount is small.** It takes the whole-tile error
  from 21.92 to 20.97 (-0.95 of 255) and the error over covered texels from
  21.12 to 19.31 (-1.81). It is the right sign and it is not the grading.
* **The remaining ~20 of 255 is NOT the tint.** With the tint off the error is
  21.92, so at most 1.81 of the ~21 belongs to it. The rest is the uniform
  x0.82-0.83 grading ROADS1 measured, which is still open.
* **`--grass-tint 0` is not byte-identical outside the cover plane**
  (`identicalOutsideCover: false`, the two sheets differ by up to 22.94 there).
  **Named as a candidate, not a conclusion**: the sheet is BC-compressed in 4x4
  blocks, and a block that straddles the cover boundary is re-quantised as a
  whole when its covered texels change, which would produce exactly this. The
  discriminator is the same pair of bakes written UNCOMPRESSED; nobody has run
  it. Until then this is an unproven explanation of a measured 22.94.
* The four `.bgsm` road materials the log reports as "not found in archives"
  are the same four SPLAT1 named; they are inside both of our panels equally, so
  they cannot move the tint-on / tint-off comparison.

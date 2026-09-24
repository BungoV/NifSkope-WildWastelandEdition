# IMPOSTOR16 -- text for the overseer to splice (lane did not touch HANDOFF.md, WW_CHANGES.md, MISTAKES.md)

## HANDOFF entry

IMPOSTOR16 (2026-09-23): TreeMapleInstitute06Green (000531b3) baked as a
hemi-octahedral 16x16 grid at 128 px frames (a 2048 sheet) and at 64 px frames
(a 1024 sheet) on exe c172ba9d, with NO code change and NO build. The rows are
bake ms (warm), _d DDS bytes, and lodgen ms:

| set | bake ms (warm) | _d DDS bytes | lodgen ms |
|---|---|---|---|
| N4/512 (current) | 9,066 | 5,222,528 | 7,155 |
| N16/2k | 14,845 | 5,505,152 | 13,335 |
| N16/1k | 12,236 | 1,310,848 | 2,425 |

Other sheet bytes: N16/2k _n and _gsaos are 5,505,152 each, _g is 2,752,640.

The measurements come from a 1-degree sweep at elevation 0 (360 views): card
coverage against the mesh.

| set | IoU mean (min) | hole share mean (max) | worst per-step popping, share of mesh area | card step XOR / mesh step XOR (worst, mean) |
|---|---|---|---|---|
| N4/512 | 0.789 (0.706) | 0.081 (0.134) | 0.133 | 0.92, 0.76 |
| N16/2k | 0.839 (0.814) | 0.069 (0.098) | 0.092 | 0.63, 0.47 |
| N16/1k | 0.780 (0.742) | 0.137 (0.189) | 0.066 | 0.46, 0.33 |

Reading:
- N16/2k beats N4/512 on every coverage number, at the same sheet bytes.
- N16/1k is a quarter of the bytes. It has the least popping, but double the holes.
- The N16/2k card is soft up close, because its frames are 128 px. Look at
  gifs/still_az030.png.

Full-material look test: channel 20 in the run folder only. The shipped shaders
and the exe are untouched.
- Brightness, card over model:
  - AO on: 0.880 mean, 0.74-1.00. This misses the 0.9-1.1 bar.
  - AO off: 0.969 mean.
  - Cause: the model draws no AO at all. Vanilla trees carry none in the vertex
    colour or the albedo, so the card's AO is the only AO.
- AO is not applied twice. Card colour sheet over model base x vertex colour =
  0.998.
- Light placement: blurred-brightness correlation card vs model r = 0.78,
  against a floor of 0.19.
- The pre-registered top-2% highlight-centroid measure could not show
  agreement. Its signal is only 0.04 of tree height.

Resume: `scratchpad/impostor16_20260923/progress.md`. The scripts are orb.sh,
metrics.py, matmetrics.py and gifs.py.

## WW_CHANGES entry

None. No source, shader or exe change shipped. The channel-20 full-material
shader exists only as a patched copy in the lane's run_mat/ folder, made by
mat_shader.py.

If bungo wants gloss and specular on cards for real, that is a new change for a
later lane. It would port the mat_shader.py patch into res/shaders/impostor_oct.frag.
The patch covers:
- GGX + Smith + Schlick from _gsaos R/G
- ambient specular
- the BGSM backlight and soft-light terms, gated by _gsaos A

The .lodm would need to carry backlightPower and rolloff; today they are
constants from the maple BGSM.

## MISTAKES entry

2026-09-23 IMPOSTOR16: metrics.py converted N4 pixel counts to the N16 scale
with the ratio inverted:
- It used (hw16/hw4)^2. The right factor is (hw4/hw16)^2: the N4 camera's
  smaller half-width zooms IN, so its tree covers more pixels.
- Caught because the "mesh area mean" printed 234,295 px for N4 against
  205,540 for N16. The mesh is the same object, so those must match.
- Shares and IoU were scale-free and unaffected.
- Rule: when a script rescales, print one quantity that must come out equal
  across the rescale, and read it.

## Skill updates (constitution 1a), for nifskope-ww-lodgen "Impostor cards: the frame law"

- The "4, 6 or 8 FRAMES PER SIDE" bullet is stale. The bake hook takes
  WW_IMPOSTOR_OCT 2..16, and WW_IMPOSTOR_TILE 32..512 px per frame. An N16 bake
  (256 views) runs end to end on c172ba9d with no code change:
  - bake + lodgen compress + orbit preview all work
  - a 2048 sheet at TILE=128, 14.8 s warm
- Add: the loose-sheet registration can race the NIF opening and log "colour
  sheet did not bind" / "orbit counted 0". Retry the run with a new port; up to
  3 tries were enough.
- Add: WW_RENDER_SIZE=1000x1000 on the 1080-tall second monitor gives a
  1000x1000 viewport. 1100x1159 does not fit.
- Add: orbit-mode framing is 1.02 x max(set halfW, halfH), per set. Two sets'
  pictures are not at the same scale until one is rescaled by the ratio of
  their half extents about the frame centre (N4 345.305 vs N16 356.831 for the
  maple). The maple's trunk base runs off the frame bottom at elevation 0.
- Add, for GIFs under a size cap:
  - one global palette
  - pixels unchanged from the previous frame written as the transparent index
    (disposal 1)
  - read every frame back and compare
  - foliage is roughly 0.2 bytes per pixel-frame at 254 colours, so budget by
    frames x pixels

# N8 addendum (2026-09-23, director relaying bungo "Okay, now let's try 8x8 at 2k" + cut variants)

## HANDOFF addition

N8/2k: TreeMapleInstitute06Green baked at 8x8 frames of 240x256 px, a
1920x2048 sheet. Same exe c172ba9d, no code change. Outputs are in
scratchpad/impostor16_20260923/n8/.

Sizes and times:
- _d, _n and _gsaos are 5,222,528 bytes each; _g is 2,611,328.
- The bake is slow and uneven: 48 s at best, 184-373 s in the other runs. N16/2k
  under the same conditions took 12.6 s. The output was byte-identical run to
  run, and the cause is not diagnosed.

Results over the 1-degree sweep at elevation 0, with the default stipple cut:

| set | IoU mean | model missing (mean) | worst per-step pop, share of model |
|---|---|---|---|
| N4/512 | 0.789 | 8.1% | 13.3% |
| N8/2k | 0.839 | 5.2% | 11.7% |
| N16/2k | 0.839 | 6.9% | 9.2% |
| N16/1k | 0.780 | 13.7% | 6.6% |

Brightness, card over model:

| set | AO on | AO off |
|---|---|---|
| N4/512 | 0.914 | 0.971 |
| N8/2k | 0.897 | 0.971 |
| N16/2k | 0.880 | 0.969 |
| N16/1k | 0.858 | 0.959 |

The cut variants are exe env only, with no shader change:
- S = the shipped stipple
- A = WW_IMPOSTOR_CUT=mean
- F = WW_IMPOSTOR_CUT=strong

TEAR is the share of the model missing in patches at least 7 px across
(n8/chunks.py). It replaced the pre-registered TEAR1 torn share, because the
nearest-frame reference differs from every blended card by 29-37% at N8 az150.

Tear at el 20, the worst elevation:

| set | S worst | S mean | A worst | A mean | F worst | F mean | nearest single frame worst |
|---|---|---|---|---|---|---|---|
| N4/512 | 8.8% | 2.8% | 26.4% | 9.0% | not measured | not measured | not measured |
| N8/2k | 1.2% | 0.4% | 5.7% | 1.9% | 7.1% | 2.0% | 10.3% |
| N16/2k | 0.9% | 0.4% | 3.3% | 1.6% | 4.3% | 1.7% | 9.0% |

Tear at el 0, worst view:

| set | S | A | F |
|---|---|---|---|
| N8/2k | 0.9% | 4.3% | 3.7% |
| N16/2k | 0.7% | 1.5% | 1.2% |

Worst per-step pop, share of model area. The model's own step is 0.14-0.19.

| set, elevation | S | A | F |
|---|---|---|---|
| N8, el 0 | 0.117 | 0.122 | 0.320 |
| N8, el 20 | 0.132 | 0.122 | 0.384 |
| N16, el 0 | 0.092 | 0.069 | 0.209 |
| N16, el 20 | 0.095 | 0.075 | 0.280 |

Speckle, measured on the covered pixels. The model's own speckle is 0.042.

| set | S | A | F |
|---|---|---|---|
| N8 | 0.0146 | 0.0103 | 0.0099 |
| N16 | 0.0045 | 0.0028 | 0.0030 |

Ranking:
- On tear, A beats F at el 20 for both N8 and N16, and on the pre-registered
  torn share everywhere. F is marginally ahead at el 0.
- On noise, A and F are tied.
- F pops 2.6-3x as much as A.
- Among the crisp cuts, A is the one to use.
- Both crisp cuts still tear. At N16, A still loses 3.3% of the tree at its
  worst view, against 0.9% for the stipple. The denser grid shrinks the crisp
  tear 5-8x from N4, but does not remove it.

## WW_CHANGES addition

None. No source, shader or exe change.

## MISTAKES addition

2026-09-23 IMPOSTOR16 N8: the pre-registered TEAR1 torn share (reference =
nearest single frame, WW_IMPOSTOR_BLEND=0) did not measure the tear at N8 and N16.
- The nearest frame is a different picture from the blend's strongest frame:
  29-37% XOR at N8 az150.
- So the torn share charged every variant, the stipple included, for frame
  mismatch.
- It was replaced after reading by a mesh-referenced chunk measure, and the
  replacement is labelled as not pre-registered.
- Rule: before using a reference picture, measure how far it is from the thing
  it stands for.

## Skill note (nifskope-ww-lodgen, frame law)

- WW_IMPOSTOR_OCT=8 with WW_IMPOSTOR_TILE=256 bakes correctly, but took 48-373 s
  against 12.6 s for OCT=16 TILE=128 on the same machine.
- A background grep over / from another session made it worse, but it stayed
  slow on a quiet machine (184 s).
- Time a bake at least twice before quoting it.

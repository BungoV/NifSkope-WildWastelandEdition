# AO2 deliverable text (lane AO2, 2026-09-26, branch ao2-20260926 from incr2-20260926 @6382a09a)

## HANDOFF text

### AO2 -- native per-vertex AO + sky: open roofs no longer drawn black (built, baked, NOT installed, not flown)
- Commits: ee52efc0 (code: src/lodgenao.h, src/nativeemit.cpp, src/lodifile.h), 4b23daa9 + 22d83a48
  (docs/LODGEN_NATIVE_LODO_LODI.md s4.8, s4.10).
- Cause, measured on bungo's three circled spots in 08_boston_oblique_AO.png:
  (a) the Prudential roof was darkened by a far-ring stand-in: the native cast built one scene with every MNAM slot
  mixed, so a lower-ring lid 72 u above the roof took 136 of 136 rays;
  (b), (c) the cathedral wall and tower cap were darkened because the vertex corners sit inside the neighbouring towers
  and pinnacles (back faces), and the corner value spread across a face that is open in the middle.
  bungo's hypothesis holds for (b) and (c), not for (a).
- Fix:
  - One scene per chunk AND slot.
  - Each vertex is the area- and hat-weighted mean over k x k equal-area pieces of its triangles, with
    k = ceil(longest edge / 256), 1..4. The AO ray law (8 rays, 1 - 0.85 hits/8) is unchanged.
  - Sky (v7 stream) is horizon aware: 7 rings at the irradiance medians (7-79 deg), cos-weighted, 8 azimuths
    (56 rays), reach 10000 u, origin offset 2 u along the normal.
  - The .BTO colour B, the v5 placement AO, the .lodo selfAO and the 0x11 byte are untouched.
- Gates on the Boston region (-8 -12 3 -1), each red on the old code:
  - a: open roofs AO 237.5 / 13.4% below 201 -> 252.0 / 2.2%;
  - b: enclosed faces AO 73.2, 179 below open;
  - s1: sky-open 221 -> 252;
  - s3: low canyon 7 -> 48 below open;
  - c: whole map, every file except Commonwealth.lodi sha1-equal; inside the .lodi only vertexAo/vertexSky and the
    two CRCs move.
- Whole bake (not installed): scratchpad\ao2_20260926\whole\after\mod\FO4CSLOD\Commonwealth in the bake2 worktree.
  The overseer installs Commonwealth.lodi (+ the .lodb record) only. whole\before equals the installed files.
- Cost: the whole-map instances stage 1478 s -> 1810 s (+22%). The whole bake is 5448 s before and 5386 s after
  (noisy machine).
- Open:
  - The wall foot reads only 12 bytes darker than the wall top (was 33): the whole-triangle averaging flattens it.
    A least-squares vertex fit would keep it; bungo's call.
  - Sky is now physical for every facing, so walls read 99 -> 72 and soffits 74 -> 25; FO4CS consumers darken walls more.
  - Face step 256 leaves a mean 6 byte per-vertex sampling error against a dense reference (step 128: 3.6, +85% cast).
  - tests/spells/lodi_v7.sh G3 (stream vs the 0x11 byte) was written for the old law; it was not re-run and is
    expected to move.
- What would prove this wrong: bungo's eye on the after picture and the three crops in scratchpad\ao2_20260926\pics, then a flight.

## WW_CHANGES text

### Native LOD objects: ambient shading no longer blackens open roofs (AO2, 2026-09-26)
- The per-vertex ambient occlusion baked into FO4CSLOD .lodi files now uses each LOD level's own geometry only.
  A coarse stand-in for a far ring no longer shades a roof it floats above.
- Each vertex is now shaded from samples spread across its faces, not from the corner alone. A corner tucked inside
  a neighbouring building no longer blackens a wall that is open in the middle.
- The per-vertex sky visibility stream now sees the sky down to the horizon (7 elevation rings, 8 directions,
  weighted by how much light each band of sky casts on a flat roof, reach 10000 units, as FO4CS Skylighting's default).
  Street canyons read darker than open roofs; walls and undersides read by the share of sky they can see.
- Only the .lodi changes; every other LOD file bakes byte-for-byte as before. Whole-map bake cost +22% on that stage.
- Tuning knobs for testing only (environment): WW_AO_FACE_STEP, WW_AO_FACE_MAX, WW_SKY_REACH, WW_AO_PROBE.

## MISTAKES text

### Heredoc backslash trap, three times in one lane (AO2, 2026-09-26)
- A bash heredoc (or a printf format) holding a literal backslash was written through the tool and lost or changed
  the backslash:
  - a C++ probe insertion;
  - the bake_region printf "\n";
  - gatec.py's '\\' became '\', a SyntaxError.
- Rule: never put a backslash literal in generated code. Use os.sep, chr(92) or a raw file written with the Write
  tool, then run it.

### A brief's coordinates were taken on trust (AO2, 2026-09-26)
- The brief named world coordinates for the "right white tower", and they pointed at another building. Time went
  into probing the wrong placement.
- Rule: match a circled spot by projecting candidate placements into the picture's camera and checking the pixel,
  before measuring.

### Comment written from memory, not from the constants (AO2, 2026-09-26)
- A lodifile.h comment said the sky rings span "5 to 87 deg". The constants say 7.1 to 79.5. It was caught before
  the commit and corrected.
- Rule: derive numbers in comments from the code constants in the same turn.

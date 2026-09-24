## 2026-09-10 - FO4 .hkx animation reader and spline decompressor (lane HKX1) -- **BUILD PENDING** for the NifSkope link only: the reader, its standalone driver and every gate are built and run (`release/hkxanim_dump.exe` 05:10:22); `src/hkxanim.cpp` passed `g++ -fsyntax-only` with the real `Makefile.Release` flags (rc=0); `qmake` + `make` owed (two NEW files in the .pro)

bungo, 2026-09-10 ~05:0x, verbatim: *"in animation workspace, add an option to
load a hkx file with animation, then they get added to the animations list,
and if there's rigged geometry with nodes / bone names that match, they play"*.
HKX1 = the reader + decompressor with its gates; HKX2 playback/mapping and
HKX3 the UI rows follow.

`docs/HKX_ANIMATION_FORMAT.md` (NEW, the contract: every offset tagged REFL /
DISASM / XML / CENSUS), `src/hkxanim.{h,cpp}` (NEW: `hkxAnimLoad()`, route A
= HKXPACK XML, route B = the packfile; `HkxAnimFile { skeletons, clips }`,
`HkxAnimClip.frames[frame][track]` of translation / Quat / scale, the binding's
`trackToBone`, annotations, root motion, blendHint, diagnostics), `NifSkope.pro`
(+2 lines), `tests/hkxanim_dump.cpp` + `tests/hkxanim_shim.cpp` (the standalone
driver, Qt6Core + Qt6Gui), `tests/spells/hkxanim_decode.py` (the independent
Python decoder, both routes), `tests/spells/hkxanim_gates.py` /
`hkxanim_synthetic.py` / `hkxanim_mutate.py` (the gates),
`.claude/skills/ww-hkx-animation/SKILL.md` (NEW), `MISTAKES.md` (5 entries),
`scratchpad/hkx1_20260910/`, `scratchpad/lane_hkx1_report.md`.

**The format, from the exe.** The `hkClass` reflection arrays of the 1.10.155
exe name every serialised member and its offset (`hkaSplineCompressedAnimation::Members`
at RVA 0x2e46140: numFrames +0x38, numBlocks +0x3c, maxFramesPerBlock +0x40,
maskAndQuantizationSize +0x44, blockDuration +0x48, frameDuration +0x50,
blockOffsets +0x58, floatBlockOffsets +0x68, data +0x98; `hkaSkeleton`: name
+0x10, parentIndices +0x18, bones +0x28, referencePose +0x38;
`hkaAnimationBinding`: originalSkeletonName +0x10, transformTrackToBoneIndices
+0x20, blendHint +0x50). The block layout, which reflection does not describe,
is read off the engine's own decoder (`samplePartialTracks` 0x1ec4140,
`readNURBSCurve<1>` 0x1ec5e50, `readKnots` 0x1ec55e0,
`hkaSignedQuaternion::unpackSignedQuaternion40` 0x1fbfa00 / `48` 0x1fbfc30,
`hkaDefaultAnimatedReferenceFrame::getReferenceFrame` 0x1f9ea20): 4-byte masks
per track, per-axis static/spline vector channels with (min,max) ranges and
16-bit control points, packed quaternion splines (THREECOMP40 = 3x12 bits +
missing index + sign, scale (sqrt2/2)/2047; THREECOMP48 = 3x15 bits, scale
(sqrt2/2)/16383), u8 knots in local frames, de Boor of degree 1..3, blocks of
256 frames overlapping by one, root motion as (xyz, yaw about up) per frame.
The container's section headers start at `0x40 + u16@0x3e` (0x50 in every
animation file) -- the collision walker's hardcoded 0x40 would refuse them.

**Census** (`census.py`, all 15,320 `.hkx` of `Fallout4 - Animations.ba2`,
6 s): 13,514 spline clips; THREECOMP40 on 1,173,390 tracks, THREECOMP48 on
118,436, nothing else; 16-bit scalars everywhere; blocks up to 25; 856
`hkaLosslessCompressedAnimation` files (the 1st-person set) refused by name;
bindings permuted in 192; blendHint ADDITIVE in 230.

**Gates** (pre-registered): (a) C++ vs Python on 5 clips x 2 routes, 64,379
rows: translation 3.8e-6, rotation 1e-5 deg, scale exact -- PASS; (d) a
hand-built 90-degree clip packed by HKXPACK: 90.0000 deg at frame 9, worst
0.0214 deg -- PASS 29/29; (e) 20 single-byte corruptions refused by name on
both decoders -- PASS 20/20 (it found and fixed two holes first); (f) frame
count, duration, walk-end == float offset -- PASS; (g) the block-boundary frame
from both blocks: 2.7e-4 -- PASS. (b) and (c) FAIL as pre-registered and the
fixtures are why: skeleton.nif lacks the 17 `Weapon*` bones and spells
Head/Spine1/Spine2/Weapon in capitals (rotations agree to 2.7e-4, translations
to 1e-3 on 77 of 78 shared bones, `Weapon` 1.48e-3); the furniture "Tpose"
idle is NOT the bind pose (15 of 94 bones match; no bind-pose clip exists in
the archive). Totals 134/3, 29/0, 20/0.

**For HKX2:** names case-insensitively, partial matches are the norm, the hkx
quaternion maps to the NiNode rotation directly (no transpose), ADDITIVE
clips are deltas, root motion separate from track 0.

MISTAKES: the exe launched once via `nifskope-cli` after an rc=1 check (still
against the brief); the census copied the 0x40 section-header constant instead
of reading the header; an acos-based angle metric with no resolution below
0.03 deg; two decoder crash/blind spots the mutation gate found; a heredoc
backslash halving.

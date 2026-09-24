import sys

P = r"E:\Projects\NifskopeWildWastelandEdition\WW_CHANGES.md"
b = open(P, "rb").read()
cr0, lf0, n0 = b.count(b"\r"), b.count(b"\n"), len(b)

anchor = (
    b"**STATUS: NOT BUILT.** Lane HOOKCAM's build gate was red on both halves -- lane\n"
    b"CARDFINAL's `DONE` marker absent and a `NifSkope.exe` alive (pid 16364, another\n"
    b"lane's card bake) -- so nothing here has been compiled or run. Every number\n"
    b"above was measured on the exe that was already on disk. Resume:\n"
    b"`scratchpad/hookcam_20260909/PENDING.md`; report\n"
    b"`scratchpad/lane_hookcam_report.md`.\n"
)
assert b.count(anchor) == 1, "anchor count %d" % b.count(anchor)

new = (
    b"**STATUS: BUILT AND MEASURED** (lane BUILD3, 2026-09-10). "
    b"`release/NifSkope.exe`\n"
    b"2026-09-10 00:13:19, qmake + `make -j2`, zero errors. It carries this camera\n"
    b"work, lane CARDFINAL's bake changes and lane WATER2's `src/lodtfile.h` /\n"
    b"`.cpp` in one link; `src/nifskope_ui.cpp` now holds BOTH HOOKCAM's hook edits\n"
    b"and CARDFINAL's bake edits, where the 23:41:00 exe held only CARDFINAL's.\n"
    b"\n"
    b"`tests/spells/render_shot.sh`: **82 checks, 1 failure**, and section 7 -- the\n"
    b"camera -- is **27 of 27 green**, every pre-registered number hit on the\n"
    b"1507x421 viewport the clamp gives:\n"
    b"\n"
    b"| check | predicted | measured |\n"
    b"|---|---|---|\n"
    b"| control, no camera switch | 230.98 px | 231.0 |\n"
    b"| ortho half-width 1024, eye 500/1000/2000, views Front and Right | 376.75 px, all six | 376.91, all six |\n"
    b"| ortho half-width 2048 / 4096 | 188.38 / 94.19 | 188.82 / 94.50 |\n"
    b"| perspective fov 60, eye 500 / 1000 / 2000 | 765.06 / 250.91 / 107.04 | 765.01 / 251.0 / 107.0 |\n"
    b"| look-at moved 400 units | 294.34 px | 294.36 |\n"
    b"| `WW_RENDER_ORTHO=-5` | refused by name | `arm=...refused-WW_RENDER_ORTHO-not-positive...`, `persp=1` |\n"
    b"| two runs of one pinned camera | identical | `ea644af68f880bcb` twice |\n"
    b"| the same pin on a generated `.lodl` | holds | `upp` 53.086 at half-width 40000, 106.171 at 80000 |\n"
    b"\n"
    b"The census agrees with the arithmetic to six figures (`upp=1.358991`\n"
    b"computed and reported) and reports the viewport the PNG actually has.\n"
    b"\n"
    b"**The one failure is not the camera.** Section 6's floor \"the pixel sampler\n"
    b"CAN see a window's pixels\" measured the visible control at a luminance range\n"
    b"of 169.847 against a bar of 295.737. That bar is three times the desktop\n"
    b"noise floor taken in section 0, and section 0 caught a transient: 98.579 at\n"
    b"00:14:24, where the same region with the same sampler at 00:17 measured\n"
    b"**0.111**. Every fixed-bar check in the run passed and every hidden run\n"
    b"measured 0.111-0.233. Reported as a number, not re-pinned and not fixed.\n"
    b"\n"
    b"Also green on this exe: `lodgen_octahedral` 85/0, `lodgen_impostor_cards`\n"
    b"12/0, `lod_generation` 97/0, `lodl_open` 23/0, `btd_terrain` 13/0,\n"
    b"`lodgen_identity` 8/0.\n"
    b"\n"
    b"**THE TRANSITION RENDER IS UNMET, AND THE REASON IS THE SCENE, NOT THE PIN.**\n"
    b"`scratchpad/hookcam_20260909/transition.py` ran in full on lane CARDFINAL's\n"
    b"fresh library (`scratchpad/cardfinal_20260909/cards_perframe`, 20 sidecars).\n"
    b"All three chunks baked, all eighteen frames were shot, and the pin held\n"
    b"exactly in every one -- the census `upp` matches\n"
    b"`2*tan(30 deg)*eye/421` to four figures at all six distances. But **0 of 6\n"
    b"card rows and 0 of 6 control rows pass**, and on one row the control beats\n"
    b"the card (`00038599` mid: control 12.50 px off, card 565.00 px), which is\n"
    b"the signature of a measurement that is not measuring the offset.\n"
    b"\n"
    b"The cause is measured and it is not tunable. The test frames one instance\n"
    b"inside a fully placed Sanctuary chunk, and the most isolated instance of each\n"
    b"tree still has a neighbour 644-892 units away while the ring distances the\n"
    b"test must use are 767-1874 units, so the neighbour sits 25.5-40.0 degrees off\n"
    b"axis. Fitting the subject at those distances needs a vertical field of view of\n"
    b"58.8-61.3 degrees; excluding the neighbour from the 1507x421 frame allows at\n"
    b"most 15.1-26.4. **There is no field of view that does both, on any of the\n"
    b"three trees.** The flood fill therefore takes a neighbouring card or the\n"
    b"ground, which the three pictures show plainly\n"
    b"(`scratchpad/hookcam_20260909/transition_<base>.png`). A one-texel transition\n"
    b"measurement needs a scene with ONE ref in it; that is a design change and it\n"
    b"was not made here.\n"
)

open(P, "wb").write(b.replace(anchor, new))
b2 = open(P, "rb").read()
print("CR %d -> %d (must be equal)" % (cr0, b2.count(b"\r")))
print("LF %d -> %d, bytes %d -> %d" % (lf0, b2.count(b"\n"), n0, len(b2)))
assert b2.count(b"\r") == cr0 == 19020
assert b"STATUS: NOT BUILT" not in b2
print("ok")

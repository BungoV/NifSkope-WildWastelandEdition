#!/usr/bin/env python3
"""Lane BUILD8: splice into WW_CHANGES.md, in binary, CR count asserted.

Two blocks go in directly after the H1 title, newest first:

  1. THE ROUND TRIP (lane BUILD8) -- the entry for what bungo asked for,
     "now export and import of gltf", measured end to end through the built exe.
  2. Lane HKX4b's own entry (scratchpad/hkx4_20260910/WW_CHANGES_ENTRY.md), read
     from that file rather than retyped, with ONE paragraph replaced: its
     "Status: BUILD PENDING" becomes the measured status
     (skill nifskope-ww-resume-pending, section 7.1).

WW_CHANGES.md is MIXED (19,020 CR against 25,183 LF as of 2026-09-09); the new
text is LF-only like the other 2026-09 entries, so the CR count must not move.
"""
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CHANGES = os.path.join(REPO, "WW_CHANGES.md")
ENTRY = os.path.join(REPO, "scratchpad", "hkx4_20260910", "WW_CHANGES_ENTRY.md")
TITLE = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"
MARK = b"lane BUILD8"

HKX4_STATUS_OLD = """**Status: BUILD PENDING.** `release/NifSkope.exe` (03:57:46) contains none of
this. Everything above was measured through the standalone
`release/gltfexport_dump.exe` (13:36:20, newer than every source it links);
the menu entry, the `gltf` CLI command and the three `NifSkope.pro` lines are
eleven inserts in `scratchpad/hkx4_20260910/hookup.py`, **not applied** --
`--check` reports 11 of 11 anchors matching exactly once. Syntax with the real
`Makefile.Release` flags is `SYNTAX-RC=0` on every new source and on the CLI
function inside the hook-up script. Resume:
`scratchpad/hkx4_20260910/PENDING.md`; what other lanes own:
`CHANGE_NEEDED.md` beside it. Skill `ww-interchange-readback` written.
"""
# the entry file uses an em dash in this paragraph; match on the ASCII head only
HKX4_STATUS_HEAD = "**Status: BUILD PENDING.**"

HKX4_STATUS_NEW = """**Status: BUILT AND GATED (lane BUILD8, 2026-09-10).**
`scratchpad/hkx4_20260910/hookup.py --apply` wrote its eleven inserts
(`NifSkope.pro` 18,859 -> 18,982 B, `src/lib/importex/importex.cpp` 7,044 ->
7,551, `src/nifcli.cpp` 263,848 -> 267,524; CR 0 unchanged on all three), then
`qmake` + `make -j2`, both RC=0. `release/NifSkope.exe` is 14:37:53,
19,382,272 bytes and newer than all 72 changed files under
`src/ res/ tools/ tests/`; `Makefile.Release` names `gltfexport.h` for
`gltfexport.o`, `gltfexportnif.o`, `gltfanim.o` and `nifcli.o`.
`NifSkope -no-gui --help` lists `gltf`, and the CLI reproduces the standalone
driver's numbers exactly -- **139 nodes, 9 shapes (9 skinned), 6,443 vertices,
11,375 triangles, 78 of 95 tracks matched** -- with `gltf_check.py` 6,411 / 0
and `gltf_readback.py` 3,234 / 2 (the pre-registered `LLeg_Toe1` pair) on the
CLI's own output. `bash tests/spells/gltf_gates.sh` on the rebuilt driver:
**0 gate(s) not as registered**. Skill `ww-interchange-readback` written.
"""

BUILD8 = """## 2026-09-10 -- glTF export and import, round-tripped 1:1 through the built exe (lane BUILD8)

bungo's order, verbatim, after confirming lane BUILD7's animation frames
(*"they look good"*): **"now export and import of gltf"**.

**What was built.** Lane HKX4b's export hook-up (11 inserts, four files) and the
import hook-up lane HKX5b never wrote. HKX5b had put `src/gltfimport.{h,cpp}`
and `src/hkxwrite.{h,cpp}` into `NifSkope.pro`, so they linked -- but nothing
called them: no menu item, no CLI command, no route of any kind. Three CLI
commands now exist and are in `NifSkope -no-gui --help`:

```
gltf        <file.nif>  -o OUT.gltf [--clip C.hkx [--bones S.hkx]] [--root-motion]
gltf-import <in.gltf>   -o OUT.hkx  [--bones S.hkx] [--fps N] [--source-rate]
                                    [--root-motion --root-node Root] [--route a|b] [--tsv T]
hkx-tsv     <in.hkx>    -o OUT.tsv
```

**THE ROUND TRIP, bone for bone and frame for frame.** `.hkx -> .gltf -> .hkx`,
every step through `release/NifSkope.exe`, compared against an INDEPENDENT
Python decode of the original clip (`tests/spells/hkxanim_decode.py`) and an
independent decode of the written one
(`scratchpad/hkx5_20260910/interleaved_decode.py`). Bars pre-registered by lane
HKX5b: **1e-4 units, 0.01 degrees**; the angle is the corrected
`4*asin(|q1-+q2|/2)`.

| leg | rows | max abs dT (units) | max angle (deg) |
|---|---|---|---|
| `JogForward` the imported clip vs the original decode | 1,794 | **8.0e-06** | **3.41e-05** |
| `JogForward` written, decoded again | 1,794 | 8.0e-06 | 3.41e-05 |
| `JogForward` + `--root-motion --root-node Root` | 1,794 | 8.0e-06 | 3.41e-05 |
| Mixamo 60 fps `--source-rate`, imported clip | 7,254 | **4.4e-05** | **5.21e-06** |
| Mixamo written, decoded again | 7,254 | 4.4e-05 | 5.21e-06 |
| the writer alone (imported clip vs its own decode) | 1,794 / 7,254 | 0.0 | 0.0 |
| the exe's reader vs the independent decoder, on the written file | 1,794 / 7,254 | 0.0 | 0.0 |

Scale is exact on every row. Root motion survives at **1.5e-05 units, 0.0
degrees of yaw** over 165.354 units of travel. The 391 and 1,581 rows that do
not survive are exactly the 17 `Weapon*` tracks the exporter names one by one --
they have no node in the body NIF. The mesh side is unchanged by the trip: 9
shapes, 6,443 vertices, 11,375 triangles, every index identical to the NIF's.

**THE PICTURE.** The five frames of lane BUILD7's sheet, rendered twice from one
pinned orthographic camera -- once from the original clip and once from the clip
that went `hkx -> glTF -> hkx`:

| tile | Mixamo: pixels differing of 1,352,217 | worst channel step | jog |
|---|---|---|---|
| bind (no clip either side) | **0** | 0 | **0** |
| frame 0 | 11 | 1 | 0 |
| 1/4 | 1 | 1 | 12 (step 1) |
| 1/2 | **0** | 0 | 33 (step 4) |
| 3/4 | 8 | 1 | 170 (step 16) |
| last | 22 | 1 | 52 (step 2) |

**The noise floor is zero**: the same clip rendered twice is byte-identical on
all six tiles, so every count above is the round trip's float error showing up
as sub-pixel antialiasing on a silhouette, and nothing else. Worst case anywhere
is 170 pixels of 1,352,217 -- **0.013% of the frame** -- at 16 of 255 levels.
Sheets: `scratchpad/build8_20260910/frames_{mixamo,jog}_{original,roundtrip}.png`;
the amplified difference pictures are `images/diff_*.png` beside them.

**THE APPLICATION COULD NOT OPEN THE FILE IT HAD JUST WRITTEN.** Lane HKX1's
reader dispatches on the class name and refused every
`hkaInterleavedUncompressedAnimation` -- which is the only class lane HKX5's
writer emits -- so `hkx-tsv` refused, and so did the render hook, which is the
only route to a picture. `src/hkxanim.cpp` gained the reading arm: the
`hkArray<hkQsTransform>` at +0x38, 48 bytes an element, **frame-major**, with
the frame count derived as `transforms.size / numberOfTransformTracks` because
the object states it nowhere else (`docs/HKX_WRITE_FORMAT.md` section 3.1, from
the engine's own `transformTrack`, rva `0x01fa1ac0`). Additive: every other
class is refused with exactly the sentence it was refused with before, and the
spline path is byte-for-byte the code it was. Its floor: one payload float moved
by 1.0 makes the exe's reader report 1.0 **in that row and no other**.

**Two things the two contract pages disagreed about**, found by running them
against each other and now written into both
(`docs/GLTF_INTERCHANGE.md`, `docs/GLTF_IMPORT.md`):

1. *Which node carries the root motion.* The export composes the travel onto the
   ROOT BONE's node (`Root`); the import's default is the single scene root,
   which in a real export is the NIF's own root `NiNode` (`skeleton.nif`) and
   drives no bone. The import refused by name until `--root-node Root` existed.
2. *The frame rate.* The export writes the clip's own rate; the import defaults
   to 30 fps for everything, so a default round trip of the 60 fps fixture comes
   back at 30. `--source-rate` keeps the grid, and the 93 frames return one for
   one.

**Blender 4.5, the third-party leg** (its own importer and exporter,
`--factory-startup`): our `.gltf` in, Blender's `.gltf` out, ours in again.
Blender keeps our node names, reports the armature `COM` with 110 bones, and
puts a non-zero roll on **107 of them** -- a Blender concept glTF has no field
for. At its FACTORY 24 fps scene rate the clip comes back re-timed: 22 frames
instead of 23 and **0.29 units / 4.83 degrees** off. With the scene rate set to
the clip's own 30 fps it comes back at 23 frames and **1.006e-04 units /
4.68e-04 degrees** -- inside the angle bar by 21x and **0.6% OVER** the 1e-4
translation bar. Both figures are Blender's resampling, not ours; the number to
tell a user is: set the scene frame rate before exporting.

**Gates.** `gltf_gates.sh` **0 not as registered**; `hkxwrite_gates.py`
**23/23**; `hkxanim_gates.py` **134 / 3** (the same three pre-registered fixture
failures, on a freshly rebuilt `hkxanim_dump.exe`); `hkxanim_mutate.py` 20 / 0;
`hkxanim_synthetic.py` 29 / 0; `hkxanim_play.sh` 27 / 0; `render_shot.sh`
82 / 0.

**Not measured.** Fallout 4 has still never loaded a file written by
`src/hkxwrite.cpp` -- the two flight files in `scratchpad/hkx5_20260910/flight/`
are unflown. The menu entry `File > Export > .glTF (skeleton, skin, animation)`
was not exercised by hand; there is no menu route to the IMPORT at all, only the
CLI. Nothing is committed.

"""


def main(argv):
    apply_it = "--apply" in argv
    raw = open(CHANGES, "rb").read()
    entry = open(ENTRY, "rb").read()
    before = (len(raw), raw.count(b"\r"), raw.count(b"\n"))
    print("WW_CHANGES.md %d bytes, CR %d, LF %d" % before)
    print("entry         %d bytes, CR %d" % (len(entry), entry.count(b"\r")))
    ok = True
    if raw.count(TITLE) != 1:
        print("REFUSED: the H1 title does not appear exactly once")
        ok = False
    if raw.count(MARK):
        print("REFUSED: %r is already in WW_CHANGES.md" % MARK.decode())
        ok = False
    # the HKX4 entry's status paragraph, matched on its ASCII head
    i = entry.find(HKX4_STATUS_HEAD.encode("utf-8"))
    if i < 0 or entry.count(HKX4_STATUS_HEAD.encode("utf-8")) != 1:
        print("REFUSED: the entry's status paragraph head is not there exactly once")
        ok = False
    print("status paragraph head at byte %d, to end of file (%d bytes)" % (i, len(entry) - i))
    if not apply_it:
        print("\n--check only: nothing written.")
        return 0 if ok else 1
    if not ok:
        return 1
    new_entry = entry[:i] + HKX4_STATUS_NEW.encode("utf-8")
    out = raw.replace(TITLE, TITLE + BUILD8.encode("utf-8") + new_entry + b"\n")
    open(CHANGES, "wb").write(out)
    nb, cr, lf = len(out), out.count(b"\r"), out.count(b"\n")
    print("wrote WW_CHANGES.md %d bytes (%+d), CR %d (was %d), LF %d (was %d)"
          % (nb, nb - before[0], cr, before[1], lf, before[2]))
    assert cr == before[1], "the CR count MOVED -- the spliced text is not LF-only"
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

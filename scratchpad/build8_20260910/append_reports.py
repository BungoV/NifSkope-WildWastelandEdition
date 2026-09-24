#!/usr/bin/env python3
"""Lane BUILD8: append a `## Build (BUILD8)` section to the HKX4 and HKX5 lane
reports. Append-only, verified byte for byte; both files are LF-only.
"""
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
MARK = b"## Build (BUILD8)"

COMMON = """

## Build (BUILD8)

Built by lane BUILD8, 2026-09-10, on `main`, **nothing committed**.
bungo's order, verbatim: *"now export and import of gltf"*.

### The exe, and the mtimes in ONE table

| artefact | mtime | bytes |
|---|---|---|
| `release/NifSkope.exe` | 2026-09-10 **14:37:53** | 19,382,272 |
| `release/style.qss` | 14:37:53 | 11,097 (compares equal to `res/style.qss`) |
| `release/hkxanim_dump.exe` | 14:51:01 | 532,675 |
| `release/hkxwrite_dump.exe` | 14:46:32 | 802,556 |
| `release/gltfexport_dump.exe` | 14:46:46 | 802,959 |
| `src/hkxanim.cpp` (BUILD8's interleaved read arm) | 14:42 | 43,891 |
| `src/nifcli.cpp` (the three CLI commands) | 14:36 | 275,399 |
| `scratchpad/build8_20260910/frames_mixamo_original.png` | 14:5x | 1,370x1,420 |
| `scratchpad/build8_20260910/frames_mixamo_roundtrip.png` | 14:5x | 1,370x1,420 |

`qmake` + `make -j2`, both RC=0. The exe is newer than all **72** changed files
under `src/ res/ tools/ tests/ NifSkope.pro`. `Makefile.Release`, read back per
object by name, gives `gltfexport.h` to `gltfexport.o`, `gltfexportnif.o`,
`gltfanim.o` and `nifcli.o`; `gltfimport.h` and `hkxwrite.h` to their own
objects and to `nifcli.o`.

**The first link FAILED** because bungo opened `release/NifSkope.exe` at
14:26:18, mid-lane. The running copy was renamed aside (never killed) as
`release/NifSkope_inuse_44632.exe` and the link re-run. It is in `MISTAKES.md`:
a process check that only echoes its answer is not a gate.

### The CLI, and what had to be written to have one

`scratchpad/hkx4_20260910/hookup.py --apply` -- 11 of 11 anchors, exactly once
each, CR 0 unchanged on all three files. Then TWO hook-ups this lane wrote,
because **lane HKX5b's importer and writer had no route at all**: they were in
`NifSkope.pro` and linked, and nothing called them.

```
gltf        <file.nif>  -o OUT.gltf [--clip C.hkx [--bones S.hkx]] [--root-motion]
gltf-import <in.gltf>   -o OUT.hkx  [--bones S.hkx] [--fps N] [--source-rate]
                                    [--root-motion --root-node Root] [--route a|b] [--tsv T]
hkx-tsv     <in.hkx>    -o OUT.tsv
```

All three are in `NifSkope -no-gui --help`. Scripts:
`scratchpad/build8_20260910/hookup_import.py`, `hookup_rootnode.py`.

### THE ROUND TRIP -- every leg through `release/NifSkope.exe`

Bars pre-registered by lane HKX5b: **1e-4 units, 0.01 degrees**, per bone per
frame, the angle being the corrected `4*asin(|q1-+q2|/2)`. The original is
decoded by `tests/spells/hkxanim_decode.py` and the written file by
`scratchpad/hkx5_20260910/interleaved_decode.py` -- two independent Python
decoders, neither of them our C++.

| leg | rows | max abs dT (units) | max angle (deg) |
|---|---|---|---|
| `JogForward`: the IMPORTED clip vs the original decode | 1,794 | **8.0e-06** | **3.41e-05** |
| `JogForward`: written, decoded again | 1,794 | 8.0e-06 | 3.41e-05 |
| `JogForward` `--root-motion --root-node Root` | 1,794 | 8.0e-06 | 3.41e-05 (root motion 1.5e-05 / 0.0 deg over 165.354 units) |
| Mixamo 60 fps `--source-rate`: the imported clip | 7,254 | **4.4e-05** | **5.21e-06** |
| Mixamo: written, decoded again | 7,254 | 4.4e-05 | 5.21e-06 |
| the WRITER alone (imported clip vs its own decode) | 1,794 / 7,254 | 0.0 | 0.0 |
| the exe's own reader vs the independent decoder, on the written file | 1,794 / 7,254 | 0.0 | 0.0 |
| the exe's own reader vs the oracle, on the ORIGINAL spline clip | 2,185 / 8,835 | 3.6e-06 / 1.5e-05 | 4.70e-06 / 4.83e-06 |

Scale exact on every row. The 391 / 1,581 rows that do not survive are the 17
`Weapon*` tracks with no node in the body NIF, each named by the exporter.
Mesh and skin through the exporter equal the NIF's: 9 shapes, 6,443 vertices,
11,375 triangles, every index identical.

### THE PICTURE

The five frames of lane BUILD7's sheet plus the bind pose, rendered twice from
ONE pinned orthographic camera (`WW_RENDER_VIEW=5 CENTER=0,0,62 ORTHO=80`,
`upp=0.150235`, read back from `release/ww_camera_pin.log` at every grab) --
once from the original clip, once from the clip that went `hkx -> glTF -> hkx`.

| tile | Mixamo: differing pixels of 1,352,217 | worst step | jog |
|---|---|---|---|
| bind (no clip on either side) | **0** | 0 | **0** |
| frame 0 | 11 | 1 | 0 |
| 1/4 | 1 | 1 | 12 (step 1) |
| 1/2 | **0** | 0 | 33 (step 4) |
| 3/4 | 8 | 1 | 170 (step 16) |
| last | 22 | 1 | 52 (step 2) |

**The noise floor is ZERO**: the same clip rendered twice is byte-identical on
all six tiles, so every count above is the round trip's float error surfacing as
sub-pixel antialiasing on a silhouette. Worst case anywhere: 170 pixels of
1,352,217 -- 0.013% -- at 16 of 255 levels.

Sheets: `scratchpad/build8_20260910/frames_{mixamo,jog}_{original,roundtrip}.png`;
amplified difference pictures `images/diff_*.png`.

### Blender 4.5, the third-party leg

`--background --factory-startup`, our `.gltf` in, Blender's own `.gltf` out,
ours in again (`scratchpad/build8_20260910/blender_roundtrip.py`).

* Blender KEEPS our node names, including the synthetic `NifSkope_Y_up`, so our
  importer's first arm serves its file too.
* armature `COM`, 110 bones, **107 of them with a non-zero roll** -- a Blender
  concept glTF has no field for.
* scene unit METRIC, scale 1.0, metres. Nothing changed there.
* **the frame rate is what it changes.** At the factory 24 fps the clip comes
  back re-timed: 18 keys on a 1/24 grid, our importer resamples to 22 frames,
  and the error is **0.29 units / 4.83 degrees**. With the scene rate set to the
  clip's own 30 fps: 23 frames and **1.006e-04 units / 4.68e-04 degrees** --
  21x inside the angle bar and **0.6% OVER** the translation bar. Both are
  Blender's resampling, not ours.

### Gates

| gate | result |
|---|---|
| `bash tests/spells/gltf_gates.sh` | **0 gate(s) not as registered** (G2 0/0/0, G2f 12/12 refused, G3 2/2/0 pre-registered, G3f 7/7 red, G4 Blender 9/9, 9/9, 1/1) |
| `python tests/spells/hkxwrite_gates.py` | **23/23** |
| `python tests/spells/hkxanim_gates.py` | **134 checks / 3 failures**, all three pre-registered (the 17 `Weapon*` bones, the 0.00148 `weapon` pose translation, the furniture T-pose clip) -- on `hkxanim_dump.exe` REBUILT at 14:51:01 |
| `python tests/spells/hkxanim_mutate.py` | 20 / 0 |
| `python tests/spells/hkxanim_synthetic.py` | 29 / 0 |
| `bash tests/spells/hkxanim_play.sh` | 27 / 0 |
| `bash tests/spells/render_shot.sh` | 82 / 0 |
| FLOOR, the new interleaved read arm | one payload float moved by 1.0: the exe's reader reports **1.000e+00 at (frame 1, track 1) and 0 everywhere else** |
| FLOOR, the picture | the same clip rendered twice: 0 differing pixels on all six tiles |

### THE CROSS-LANE CHANGE, named

`src/hkxanim.cpp` is lane HKX2b's file and this lane changed it: lane HKX1's
reader dispatches on the class name and refused every
`hkaInterleavedUncompressedAnimation` -- the only class `src/hkxwrite.cpp` can
write -- so **NifSkope could not open the file its own glTF import had just
written**, and the picture proof (which goes through the render hook, which goes
through this reader) was impossible. The arm reads the `hkArray<hkQsTransform>`
at +0x38, 48 bytes an element, frame-major, with the frame count derived as
`transforms.size / numberOfTransformTracks` (`docs/HKX_WRITE_FORMAT.md` 3.1,
from the engine's own `transformTrack`, rva `0x01fa1ac0`). Additive: every other
class is refused with the identical sentence and the spline path is unchanged.
The pre-change file is kept at
`scratchpad/build8_20260910/hkxanim.cpp.pre-build8`. It is in `MISTAKES.md`.

### The two contract pages, read side by side at last

Lane HKX5b's owed item is discharged. Every source both pages cite was re-hashed
first and every one is unchanged (`src/gltfexport.h` `77e2b2f7e2010ca5`,
`src/gltfexport.cpp` `cbd5ce5e0e569a34`, `src/gltfimport.h` `0c0da7182b3e965e`,
`src/gltfimport.cpp` `a2cec5a5f738ef57`). They AGREE on the unit, the up-axis
node and its quaternion, the quaternion component order, dimensionless scale,
LINEAR being exact for FO4's degree-1 splines, and the root-motion formula.
They DISAGREED on two things, now written into both:

1. **which node carries the root motion** -- the export composes it onto the
   ROOT BONE's node (`Root`), the import defaulted to the single scene root
   (`skeleton.nif`), which drives no bone. The import refused by name until
   `--root-node` existed;
2. **the frame rate** -- the export writes the clip's own, the import defaults
   to 30 for everything, so a default round trip of the 60 fps fixture returns
   47 frames instead of 93.

### What is NOT measured

* **Fallout 4 has still never loaded a file written by `src/hkxwrite.cpp`.** The
  two flight files in `scratchpad/hkx5_20260910/flight/` are unchanged and
  unflown.
* The menu entry `File > Export > .glTF (skeleton, skin, animation)` was not
  exercised by hand (PENDING.md's P6): the exe was kept free of a GUI session.
  **There is no menu route to the IMPORT at all** -- CLI only.
* `tests/spells/hkxanim_decode.py`, `tests/spells/hkxanim_gates.py` and
  `docs/HKX_ANIMATION_FORMAT.md` section 4.6 still carry the `2*asin` angle
  metric, so lane HKX1's published angle figures are still halves. Untouched
  again, for the same reason BUILD7 gave: changing the metric moves the gate
  numbers this build had to reproduce.
* Nothing is committed.
"""


def main(argv):
    apply_it = "--apply" in argv
    ok = True
    for rel in ("scratchpad/lane_hkx4_report.md", "scratchpad/lane_hkx5_report.md"):
        p = os.path.join(REPO, rel)
        raw = open(p, "rb").read()
        print("%-34s %6d bytes, CR %d, marker %d"
              % (rel, len(raw), raw.count(b"\r"), raw.count(MARK)))
        if raw.count(MARK):
            ok = False
    t = COMMON.encode("utf-8")
    if t.count(b"\r"):
        print("REFUSED: the appended text is not LF-only")
        return 1
    if not apply_it:
        print("--check only: would append %d bytes to each." % len(t))
        return 0 if ok else 1
    if not ok:
        print("REFUSED: already appended.")
        return 1
    for rel in ("scratchpad/lane_hkx4_report.md", "scratchpad/lane_hkx5_report.md"):
        p = os.path.join(REPO, rel)
        raw = open(p, "rb").read()
        open(p, "wb").write(raw + t)
        back = open(p, "rb").read()
        assert back[:len(raw)] == raw, "not append-only: " + rel
        print("appended to %-34s %d -> %d bytes, CR %d" % (rel, len(raw), len(back), back.count(b"\r")))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

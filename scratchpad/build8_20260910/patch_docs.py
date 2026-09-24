#!/usr/bin/env python3
"""Lane BUILD8: reconcile docs/GLTF_INTERCHANGE.md (export) with
docs/GLTF_IMPORT.md (import), and retire the "not built" status of both.

WHAT WAS HELD AGAINST WHAT.  The two pages were read side by side for the first
time (lane HKX5b's owed item, its section 7).  Every source both pages cite was
re-hashed first and every one is unchanged, so neither page is stale about its
own code:

  src/gltfexport.h   77e2b2f7e2010ca5  5,434    src/gltfimport.h   0c0da7182b3e965e  7,289
  src/gltfexport.cpp cbd5ce5e0e569a34  29,592   src/gltfimport.cpp a2cec5a5f738ef57  43,805

THEY AGREE on: the unit (0.9144/64 m, the same constant in both files); the
up-axis node (name `NifSkope_Y_up`, quaternion -90 deg about X, and the
import's "consume it" arm is the exact inverse of the export's "hang everything
under it"); the quaternion component order at the glTF boundary; scale left
dimensionless; LINEAR being exact for FO4's degree-1 splines; and the
root-motion formula.

THEY DISAGREED on TWO things, both measured today, both now written into both
pages:

  1. WHICH NODE CARRIES THE ROOT MOTION.  The export composes the travel onto
     the ROOT BONE's node (`Root`).  The import's default is "the single scene
     root once the up-axis node is consumed", which in a real export is the
     NIF's own root NiNode `skeleton.nif` -- a different node, and one that
     reaches no bone.  `gltf-import --root-motion` refused by name until
     `--root-node Root` was given.  Neither page said so.
  2. THE FRAME RATE.  The export writes the clip's own rate (30 vanilla, 60 for
     the Mixamo fixture); the import defaults to 30 for everything, so a
     default round trip of a 60 fps clip resamples it.  Both pages stated their
     own rule and neither warned that the two defaults do not compose.

Every anchor here is ASCII-only ON PURPOSE: both pages use en/em dashes and a
middle dot, and an anchor that carries one is a re-encoding accident waiting to
happen (paid for once in this very script, 2026-09-10).

--check by default; both files are LF-only and the CR count is asserted after.
"""
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
EX = "docs/GLTF_INTERCHANGE.md"
IM = "docs/GLTF_IMPORT.md"
MARK = b"lane BUILD8"

EX_STATUS_OLD = """**Status.** The writer, the model-side builder and the standalone driver are
written, syntax-clean with the real `Makefile.Release` flags, and gated
end to end through the standalone `release/gltfexport_dump.exe`. **NifSkope
itself has not been built with them**: the menu entry, the CLI command and the
three `NifSkope.pro` lines live in `scratchpad/hkx4_20260910/hookup.py` and are
NOT applied.
"""
EX_STATUS_NEW = """**Status: BUILT AND MEASURED IN THE APPLICATION, 2026-09-10 14:37:53, lane
BUILD8.** `scratchpad/hkx4_20260910/hookup.py` is applied: `src/gltfexport.cpp`,
`src/gltfexportnif.cpp` and `src/lib/importex/gltfanim.cpp` are in
`NifSkope.pro`, the menu entry
`File > Export > .glTF (skeleton, skin, animation)` exists, and
`NifSkope -no-gui gltf` is in the CLI's own help. The CLI reproduces the
standalone driver's numbers exactly -- 139 nodes, 9 shapes (9 skinned), 6,443
vertices, 11,375 triangles, 78 of 95 tracks matched -- and the gates run on the
CLI's output unchanged: `tests/spells/gltf_check.py` 6,411 checks / 0 failures,
`tests/spells/gltf_readback.py` 3,234 checks / 2 pre-registered fixture
failures (the `LLeg_Toe1` pair of section 6).

**The round trip through this page and `docs/GLTF_IMPORT.md` is closed and
measured** (lane BUILD8): `.hkx -> .gltf -> .hkx`, compared bone for bone and
frame for frame against an independent decode of the original, worst
**8.0e-06 NIF units / 3.41e-05 degrees** on the vanilla `JogForward` (1,794
bone-frames) and **4.4e-05 units / 5.21e-06 degrees** on the 60 fps Mixamo clip
(7,254 bone-frames), against bars of 1e-4 units and 0.01 degrees. The 391 and
1,581 rows that do not survive are the 17 `Weapon*` tracks section 8 already
names, times the frame count.
"""

# insert AFTER this ASCII-only line (the last line of the root-motion "on" bullet)
EX_RM_ANCHOR = "  the clip's own `up` vector. No other node is touched.\n"
EX_RM_TEXT = """
**WHICH node that is, and why it matters on the way back** (lane BUILD8,
2026-09-10). The travel goes onto the node of the clip's ROOT BONE -- `Root` on
the player skeleton -- and NOT onto the glTF's scene root, which is the NIF's
own root `NiNode` (`skeleton.nif` on the fixture) and drives no bone at all.
The importer's default is the scene root, so a re-import must be told the node
by name:
`NifSkope -no-gui gltf-import ... --root-motion --root-node Root`.
Without it the import refuses, in words -- *"root motion was asked for from
'skeleton.nif', which has no track (it reached no bone)"* -- which is correct
behaviour on both sides, but neither page said it until it was measured. With
it, the 165.354 units of `JogForward`'s travel come back to **1.5e-05 units and
0.0 degrees of yaw**.
"""

# insert AFTER this ASCII-only line (the last line of the Times bullet)
EX_RATE_ANCHOR = "  `extras.frames` / `extras.frameDuration` / `extras.framesPerSecond`.\n"
EX_RATE_TEXT = """  **The two defaults do not compose** (lane BUILD8): this exporter writes
  whatever rate the clip has, and `docs/GLTF_IMPORT.md`'s importer defaults to
  30 fps for everything, so a DEFAULT round trip of the 60 fps fixture comes
  back at 30 -- 47 frames instead of 93. Pass `--source-rate` (or `--fps 60`)
  on the import to keep the grid; with it the 93 frames return one for one.
"""

IM_STATUS_OLD = """**Status: MEASURED and GATED, 2026-09-10, lane HKX5.** `src/gltfimport.{h,cpp}`
implements exactly this page. It is the inverse of the EXPORT contract,
"""
IM_STATUS_NEW = """**Status: BUILT AND MEASURED IN THE APPLICATION, 2026-09-10 14:37:53, lane
BUILD8.** `src/gltfimport.{h,cpp}` implements exactly this page, is linked into
`release/NifSkope.exe`, and is reachable as
`NifSkope -no-gui gltf-import <in.gltf> -o OUT.hkx`.

**THE TWO PAGES HAVE NOW BEEN READ SIDE BY SIDE** (lane HKX5b's owed item,
discharged by lane BUILD8, 2026-09-10). Every source both pages cite was
re-hashed first and every one is unchanged. They AGREE on the unit, the up-axis
node and its quaternion, the quaternion component order, scale being
dimensionless, LINEAR being exact for FO4's degree-1 splines, and the
root-motion formula. They DISAGREED on two things, both now written into both
pages: **which node carries the root motion** (section 5) and **the frame rate
the two defaults produce** (section 4).

**The measured round trip, end to end through the built exe:** `.hkx -> .gltf ->
.hkx`, compared against an independent decode of the original, bone for bone and
frame for frame -- worst **8.0e-06 units / 3.41e-05 degrees** on the vanilla
`JogForward` (1,794 bone-frames) and **4.4e-05 units / 5.21e-06 degrees** on the
60 fps Mixamo clip (7,254 bone-frames), against bars of 1e-4 and 0.01 degrees.
Through **Blender 4.5** and back (its own importer and exporter, factory
settings) the same clip lands at **1.006e-04 units / 4.68e-04 degrees** with
Blender's scene rate set to the clip's 30 fps, and at **0.29 units / 4.83
degrees** at Blender's factory 24 fps -- that difference is Blender re-timing
the action, not this page.

It is the inverse of the EXPORT contract,
"""

IM_RM_ANCHOR = "frame `f`, against the transform at `rootMotionReferenceFrame` (default 0):\n"
IM_RM_TEXT = """
> **NAME THE NODE when the file came from our own exporter** (lane BUILD8,
> 2026-09-10). `src/gltfexport.cpp` composes the travel onto the ROOT BONE's
> node, `Root`; the single scene root of such a file is the NIF's own root
> `NiNode` (`skeleton.nif`), which drives no bone. So the default arm refuses,
> correctly and by name -- *"root motion was asked for from 'skeleton.nif',
> which has no track (it reached no bone)"* -- and the round trip needs
> `--root-node Root`. With it, 165.354 units of travel return to **1.5e-05
> units, 0.0 degrees of yaw**.
"""

IM_RATE_ANCHOR = "* `preserveSourceRate` keeps the source's own rate instead, but **only when\n"
IM_RATE_TEXT = """* **the default is 30 fps whatever the file says**, while
  `docs/GLTF_INTERCHANGE.md`'s exporter writes the clip's OWN rate, so a
  default round trip of a 60 fps clip comes back at 30 -- 47 frames instead of
  93 (lane BUILD8). `--source-rate` on the CLI keeps the grid, and with it the
  93 frames return one for one;
"""

# (file, mode, anchor, text): "replace" swaps, "after"/"before" insert
EDITS = [
    (EX, "replace", EX_STATUS_OLD, EX_STATUS_NEW),
    (EX, "after", EX_RM_ANCHOR, EX_RM_TEXT),
    (EX, "after", EX_RATE_ANCHOR, EX_RATE_TEXT),
    (IM, "replace", IM_STATUS_OLD, IM_STATUS_NEW),
    (IM, "after", IM_RM_ANCHOR, IM_RM_TEXT),
    (IM, "before", IM_RATE_ANCHOR, IM_RATE_TEXT),
]


def main(argv):
    apply_it = "--apply" in argv
    files = {}
    for rel, _m, _a, _t in EDITS:
        if rel not in files:
            files[rel] = open(os.path.join(REPO, rel), "rb").read()
    before = {r: (len(b), b.count(b"\r"), b.count(b"\n")) for r, b in files.items()}
    ok = True
    for rel, mode, anchor, _t in EDITS:
        n = files[rel].count(anchor.encode("utf-8"))
        print("%-26s %-8s anchor x%d  %s" % (rel, mode, n, repr(anchor.split("\n")[0][:46])))
        if n != 1:
            ok = False
    marks = {r: b.count(MARK) for r, b in files.items()}
    print("\n%r already in: %s" % (MARK.decode(), marks))
    for rel, (nb, cr, lf) in sorted(before.items()):
        print("  %-26s %7d bytes, CR %d, LF %d" % (rel, nb, cr, lf))
    if any(marks.values()):
        ok = False
        print("REFUSED: the marker is already in one of the pages.")
    if not apply_it:
        print("\n--check only: nothing written.")
        return 0 if ok else 1
    if not ok:
        return 1
    for rel, mode, anchor, text in EDITS:
        b = files[rel]
        a, t = anchor.encode("utf-8"), text.encode("utf-8")
        assert b.count(a) == 1, (rel, anchor[:40])
        if mode == "replace":
            files[rel] = b.replace(a, t)
        elif mode == "after":
            files[rel] = b.replace(a, a + t)
        else:
            files[rel] = b.replace(a, t + a)
    for rel, b in files.items():
        open(os.path.join(REPO, rel), "wb").write(b)
        nb, cr, lf = len(b), b.count(b"\r"), b.count(b"\n")
        was = before[rel]
        print("wrote %-26s %7d bytes (%+d), CR %d (was %d), LF %d (was %d)"
              % (rel, nb, nb - was[0], cr, was[1], lf, was[2]))
        assert cr == was[1], "CR moved in " + rel
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

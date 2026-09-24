#!/usr/bin/env python
"""Lane BUILD11: rewrite lane HKXEDIT2's WW_CHANGES.md entry from BUILD PENDING to
the measured result. Title line and the "NOT measured" sentence only; the entry's
body is the lane's own text and is not touched. WW_CHANGES.md is MIXED -- binary
splice, CR asserted at 19,020 before and after.
"""
import os, sys

REPO = r"E:\Projects\NifskopeWildWastelandEdition"
P = os.path.join(REPO, "WW_CHANGES.md")
CR_EXPECTED = 19020

OLD_TITLE = ("## 2026-09-10 -- lane HKXEDIT2: the animation workspace, and the "
             ".hkx clip fully editable (BUILD PENDING)\n")
NEW_TITLE = ("## 2026-09-10 -- lane HKXEDIT2: the animation workspace, and the "
             ".hkx clip fully editable (built and gated, lane BUILD11)\n")

OLD_NOT = """**NOT measured**: the dock itself, gates (a)-(j) in the app (`WW_ANIMWS_TEST`,
`tests/spells/animws.sh`, written and syntax-checked, never run), the gizmo
"""
NEW_NOT = """**MEASURED, lane BUILD11 on `release/NifSkope.exe` 17:08:39**: the dock's own
gate ran for the first time -- `tests/spells/animws.sh` **57 checks, 0 failures,
1 skip, PASS**. Green in that run: 78 bone rows with 93 keys each and the ruler
at 60 fps, the readout `frame 46 / 92 - 0.767 s`, a 30-degree key at frame 46
reading back 0 degrees off from the DOCUMENT and 0 off from the viewport NODE
with every other bone and frame byte-identical, row->viewport and
viewport->row selection (block 10 both ways, COM row 2), trim 10..50 -> 41
frames with frame 0 equal to old frame 10 exactly, retime 60->30 -> 47 frames
with 0 coincident frames differing, root-motion bake COM travel 487.643 -> 0 and
unbake byte-identical in both the track and the extracted motion, Save ->
6 objects / 95 tracks x 93 frames / 429,728 bytes re-read bit for bit, and Undo
of the retime back to 93 frames at 60. Pictures
`scratchpad/hkxedit2_20260910/dock_frame46.png` (34,589 B) and
`viewport_gizmo.png` (47,217 B). The standalone document gate was re-run here
too: **72 checks, 0 failures**, HKXPACK seeing `FootLeft` 2 / 1.

**THE ONE SKIP, and it is not a pass.** Gate (i) (a NIF `NiControllerSequence`
still plays and its rows still write the block) prints a named SKIP on the
default fixture, `10mmPistol.nif`, which has no `NiControllerSequence`. Given
one that does -- `Meshes/Effects/TeleportInFXLight.nif` -- the harness DIES
inside gate (i) and writes no summary line at all. The NIF is not the problem:
it opens and renders on its own in this exe. The lane pre-registered this branch
as a likely first-run defect ("opens a second file through `NifSkope::openFile`
and waits 2.5 s"). Gate (i) is therefore still unproven, in either direction.

**Still not measured**: the gizmo
"""


def main():
    apply = "--apply" in sys.argv
    b = open(P, "rb").read()
    cr0 = b.count(b"\r")
    print("WW_CHANGES.md bytes=%d CR=%d" % (len(b), cr0))
    assert cr0 == CR_EXPECTED
    for name, s in (("title", OLD_TITLE), ("not-measured", OLD_NOT)):
        n = b.count(s.encode("utf-8"))
        print("  %-14s count=%d (need 1)" % (name, n))
        assert n == 1, name
    nb = b.replace(OLD_TITLE.encode(), NEW_TITLE.encode(), 1)
    nb = nb.replace(OLD_NOT.encode(), NEW_NOT.encode(), 1)
    assert nb.count(b"\r") == cr0, "CR moved"
    print("would grow by %d bytes; CR stays %d" % (len(nb) - len(b), cr0))
    if not apply:
        print("--check only")
        return 0
    open(P, "wb").write(nb)
    c = open(P, "rb").read()
    print("wrote: bytes %d -> %d  CR %d" % (len(b), len(c), c.count(b"\r")))
    assert c.count(b"\r") == CR_EXPECTED
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python
"""Append lane SKELFIX's MISTAKES.md section, append-only, LF, refusing.

Another lane may be appending to this file in the same minute, so: read, check
the section is not already there, append at the END only, and assert that the
old bytes are still a PREFIX of the new file.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
PATH = os.path.join(ROOT, "MISTAKES.md")
KEY = "## 2026-09-10, lane SKELFIX:"

TEXT = """

## 2026-09-10, lane SKELFIX: the brief's own rule would have cut the skeleton in half

The brief said to draw a bone body only "when BOTH ends are bones the Skeleton
Manager classes as bones (its Bones filter)". Applied literally to the fixture,
that rule draws **60 of the 129** segments, and with the brief's second clause
(the child has a track, or its whole parent chain does) **40**. The reason is
measured, not guessed: FO4 body meshes are weighted to the `*_skin` helper
bones, so `Pelvis`, `COM`, `LLeg_Thigh`, `LLeg_Calf`, `LArm_UpperArm`,
`LArm_ForeArm1..3` and the rest of the animation chain are **not in the Bones
filter at all** -- 33 deforming bones have a parent the dock calls "not a bone".
The spine, both legs and both forearms would have come apart.

The second clause is worse than useless here: an UNTRACKED node cannot lag
behind a tracked parent -- with no track it keeps its bind local and inherits
its parent's world transform, so it rides along by construction. The nodes that
stay behind are TRACKED ones the clip parks at the origin (`Root`, `Camera`,
`CamTarget`, the eight `AnimObject*`), which the clause keeps.

What shipped instead: the Bones filter CLOSED UPWARDS through the parent chain
and CUT at the deepest node that still has every bone beneath it. 110 of 129
segments, longest 31.9 units against a 33.6 limit, every endpoint on the
character. The 19 nodes it drops are exactly the camera, weapon, anim-object,
bumper, eye-dummy and above-root nodes.

The rule this leaves: **a pre-registered rule is a hypothesis about the data
until the data is read.** The gates in the brief were outcome gates (no segment
longer than X, nothing outside the character) and they survived; the MECHANISM
in the brief did not, and the lane's job was to say so with the number rather
than to implement a rule that passes the gate by drawing less skeleton.

## 2026-09-10, lane SKELFIX: a picture was shipped as proof that no count could check

BUILD9's gate table read `17 checks, 0 failures, PASS` on the very frame whose
picture carries the defect. Every check was sound; none of them measured the
LENGTH of what was drawn or WHERE it went, so a body 300 units long fanning to
the world origin passed a mask gate (it was inside the mask -- the mask is
rasterised from the segments the overlay reports) and a joint gate (every joint
was on its animated node, including the ones at the origin).

The rule: when the deliverable is a picture, one gate has to read the PICTURE.
`tests/spells/skeleton_overlay_mask.py` is that gate here -- the character's
bounding box is measured from the same frame with the overlay OFF, so the
overlay cannot enlarge the box it is judged against, and its floor is the
BUILD9 image itself (`scratchpad/skelfix_20260910/before_on_frame46.png`), kept
in the tree for exactly that reason.

## 2026-09-10, lane SKELFIX: WW_RENDER_SIZE did not reach the picture

`tests/spells/skeleton_overlay.sh` exports `WW_RENDER_SIZE=1000x1000` and every
one of the three delivered images is **1437x941**. Not this lane's code and not
diagnosed here -- recorded because two of BUILD9's numbers (the mask's 12.23%
of the frame, the 2.412% of pixels changed) are fractions of a frame whose size
was not the one the spell asked for, and because a later lane will otherwise
re-measure it from scratch. The candidates, unmeasured: the render hook clamps
to the window it was given, or `WW_RENDER_SIZE` is read before the window is
sized. The discriminator is one render at 400x400.
"""


def main(argv):
    blob = open(PATH, "rb").read()
    if KEY.encode() in blob:
        print("already present")
        return 0
    if blob.count(b"\r"):
        print("MISTAKES.md is no longer LF-only (CR %d): refusing" % blob.count(b"\r"))
        return 2
    out = blob + TEXT.encode("utf-8")
    if not out.startswith(blob):
        print("not append-only, refusing")
        return 2
    print("MISTAKES.md %d -> %d bytes, CR %d, three sections appended"
          % (len(blob), len(out), out.count(b"\r")))
    if "--apply" in argv:
        open(PATH, "wb").write(out)
        print("WRITTEN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

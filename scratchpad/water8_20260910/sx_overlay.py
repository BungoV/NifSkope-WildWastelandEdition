#!/usr/bin/env python3
"""Write hookup.py's and gate_patch.py's OUTPUT into a scratch overlay, so the
text they would insert can be syntax-checked without touching the shared tree.

The lane may not apply either script while lane UI4 holds the build slot and
the two gate files, but the text they would insert still has to be proved to
COMPILE -- otherwise the director's one build of the session dies on a typo in
a string this lane wrote. So: take both EDITS tables (never a retyped copy),
apply them to copies under scratchpad/water8_20260910/sx/, and syntax-check the
copies FROM THAT DIRECTORY -- a quoted `#include "nifskope.h"` is resolved
against the INCLUDER'S OWN directory before any -I, which is what makes a copy
in sx/ read sx/nifskope.h while src/nifskope_ui.cpp goes on reading src/'s. So
`-Iscratchpad/water8_20260910/sx` on the tree's own file changes nothing, and
was measured doing nothing (RC=0 with the unpatched clamp still naming
`LeftWater` against a header that no longer has it).

THE FLOOR THAT PROVES THE OVERLAY IS BEING READ, and it fires:

    cp src/nifskope_ui.cpp scratchpad/water8_20260910/sx/unpatched_ui.cpp
    bash sx_WATER8.sh scratchpad/water8_20260910/sx/unpatched_ui.cpp
    -> error: 'LeftWater' was not declared in this scope

i.e. the UNPATCHED source, sitting beside the PATCHED header, fails on exactly
the identifier the hook-up removes. The patched pair passing is therefore a
statement about the patched pair.

    python scratchpad/water8_20260910/sx_overlay.py
    bash sx_WATER8.sh scratchpad/water8_20260910/sx/nifskope_ui.cpp \\
                      scratchpad/water8_20260910/sx/wateruitest.cpp

It proves the inserted text compiles in place. It proves nothing about linking,
about moc, or about behaviour.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import hookup                                                   # noqa: E402
import gate_patch                                               # noqa: E402

OUT = os.path.join(HERE, "sx")


def apply_edits(blobs, edits):
    for entry in edits:
        path, mode, anchor, text = entry[0], entry[1], entry[2], entry[3]
        if path not in blobs:
            with open(os.path.join(ROOT, path), "rb") as f:
                blobs[path] = f.read()
        buf = blobs[path]
        if mode == "splice":
            start = anchor[0].encode("utf-8")
            end = anchor[1].encode("utf-8")
            if buf.count(start) != 1 or buf.count(end) != 1:
                print("REFUSED: %s's splice markers do not match once" % path)
                return False
            s, e = buf.index(start), buf.index(end)
            blobs[path] = buf[:s] + text.encode("utf-8") + buf[e:]
            continue
        a = anchor.encode("utf-8")
        t = text.encode("utf-8")
        if buf.count(a) != 1:
            print("REFUSED: %s's anchor does not match once (%d)" % (path, buf.count(a)))
            return False
        blobs[path] = buf.replace(a, t if mode == "replace" else a + t, 1)
    return True


def main():
    if not os.path.isdir(OUT):
        os.makedirs(OUT)
    blobs = {}
    if not apply_edits(blobs, hookup.EDITS):
        return 1
    if not apply_edits(blobs, gate_patch.EDITS):
        return 1
    for path, blob in sorted(blobs.items()):
        dst = os.path.join(OUT, os.path.basename(path))
        with open(dst, "wb") as f:
            f.write(blob)
        print("wrote %-52s %8d bytes  CR %d" % (dst, len(blob), blob.count(b"\r")))
    # the two things the pass would silently skip if they were missing
    for need in ("nifskope.h", "nifskope_ui.cpp", "wateruitest.cpp"):
        if not os.path.exists(os.path.join(OUT, need)):
            print("REFUSED: the overlay has no %s, so the pass would read the tree's" % need)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

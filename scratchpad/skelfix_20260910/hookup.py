#!/usr/bin/env python
"""Lane SKELFIX: the ONE edit in a file this lane does not own.

`src/nifskope_ui.cpp` belongs to whichever lane is writing into it (it was
FILESTAB and HKX3 this session), so the tooltip edit is written here as a
refusing script instead of applied. It does two things:

  1. the static tooltip of Overlays > Show Skeleton states the RULE -- that a
     bone body joins two armature nodes and everything else gets a marker;
  2. the moment the entry is ticked, the tooltip gains the measured sentence
     for the file that is actually open (`GLView::skeletonOverlayRule()`),
     which names the armature's root and how many nodes are marker-only.

Nothing in this lane needs the edit to COMPILE: `skeletonOverlayRule()` is a
public accessor that exists whether or not anybody calls it, and the harness
gate (i) reads it directly.

  hookup.py            --check (the default): counts anchors, writes nothing
  hookup.py --apply    writes, only if every anchor still matches exactly once

APPLIED-OR-NOT is decided by grepping for the marker string `lane SKELFIX`,
never by the anchor still matching -- it matches either way.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
MARK = "lane SKELFIX"

OLD_TIP = """		aShowSkeleton->setToolTip( tr( "Draw the rig's bones over the model and through it, "
			"coloured the way the Skeleton Manager colours them. Tick Show Nodes as well for bone names." ) );
		connect( aShowSkeleton, &QAction::toggled, this, [this]( bool on ) {
			if ( ogl ) {
				ogl->setSkeletonOverlay( on );
				ogl->update();
			}
		} );
"""

NEW_TIP = """		const QString skelTipBase = tr( "Draw the rig's bones over the model and through it, "
			"coloured the way the Skeleton Manager colours them. Tick Show Nodes as well for bone names.\\n\\n"
			"A bone body joins two ARMATURE nodes only -- the nodes a skin lists, plus the nodes "
			"between them. Camera, anim-object, weapon and attach nodes are still listed and still "
			"get a joint marker, but nothing is drawn to them: they sit where the file puts them "
			"while an animated character moves away, and joining the two drew segments across the "
			"whole scene (lane SKELFIX)." );
		aShowSkeleton->setToolTip( skelTipBase );
		connect( aShowSkeleton, &QAction::toggled, this, [this, aShowSkeleton, skelTipBase]( bool on ) {
			if ( ogl ) {
				ogl->setSkeletonOverlay( on );
				// The measured sentence for the file that is open NOW: which
				// node the armature is rooted at, and how many nodes are
				// marker-only. Built by setSkeletonOverlay(), so it is there
				// by the time this line runs (lane SKELFIX).
				const QString rule = on ? ogl->skeletonOverlayRule() : QString();
				aShowSkeleton->setToolTip( rule.isEmpty() ? skelTipBase
					: ( skelTipBase + QStringLiteral( "\\n\\n" ) + rule ) );
				ogl->update();
			}
		} );
"""

EDITS = [("src/nifskope_ui.cpp", "replace", OLD_TIP, NEW_TIP)]


def main(argv):
    apply = "--apply" in argv
    rc = 0
    plan = []
    for rel, how, anchor, text in EDITS:
        path = os.path.join(ROOT, rel)
        blob = open(path, "rb").read()
        cr = blob.count(b"\r")
        if MARK.encode() in blob:
            print("%-24s ALREADY APPLIED (%r present)" % (rel, MARK))
            continue
        a = anchor.encode("utf-8")
        hits = blob.count(a)
        print("%-24s anchor x%d  (CR %d, LF %d)" % (rel, hits, cr, blob.count(b"\n")))
        if hits != 1:
            print("    REFUSED: an anchor must match exactly once")
            rc = 2
            continue
        out = blob.replace(a, text.encode("utf-8")) if how == "replace" \
            else blob.replace(a, a + text.encode("utf-8"))
        print("    would grow by %d bytes; CR %d -> %d (must be equal); marker x%d"
              % (len(out) - len(blob), cr, out.count(b"\r"), out.count(MARK.encode())))
        if out.count(b"\r") != cr:
            print("    REFUSED: the line-ending count moved")
            rc = 2
            continue
        plan.append((path, out))
    if apply and rc == 0:
        for path, out in plan:
            open(path, "wb").write(out)
            print("WROTE %s" % path)
    elif apply:
        print("NOTHING WRITTEN: an edit refused")
    return rc


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

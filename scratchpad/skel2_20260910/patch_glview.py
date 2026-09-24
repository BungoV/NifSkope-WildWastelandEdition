#!/usr/bin/env python
"""Lane SKEL2 -- the binary splice into src/glview.cpp.

src/glview.cpp is MIXED and mostly CRLF (CR 23,423 / LF 23,494 on the rung), so
a text editor that writes LF would quietly add LF-only lines to a CRLF file.
CONSTITUTION rule 8: match neighbours, never normalise, splice mixed files in
binary, and count the line endings with Python bytes -- never with grep.

Every new line this script writes carries CRLF, and the CR count is asserted to
move by exactly the number of lines inserted minus the number removed.

  python patch_glview.py --check      # writes nothing, prints every count
  python patch_glview.py --apply

Snippets live beside this file in snip/ as LF text and are converted on the way
in, so the C++ can be read and edited as ordinary files.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TARGET = os.path.join(ROOT, "src", "glview.cpp")
SNIP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "snip")


def crlf(text):
    return text.replace("\r\n", "\n").replace("\n", "\r\n")


def snippet(name):
    with open(os.path.join(SNIP, name), "r", encoding="utf-8", newline="") as fh:
        return crlf(fh.read())


def func_span(buf, start_text, sig):
    """Byte span of a whole function, from start_text through its closing brace.

    The end is the first line that is exactly `}` at column 0 after the
    signature. Function bodies here are tab-indented, so a `}` at column 0
    inside one cannot happen; a lambda ends `};` and does not match either.
    Both line endings are tried because the file is mixed.
    """
    if buf.count(start_text) != 1:
        raise SystemExit("start text matches %d times, not 1: %r"
                         % (buf.count(start_text), start_text[:70]))
    if buf.count(sig) != 1:
        raise SystemExit("signature matches %d times, not 1: %r"
                         % (buf.count(sig), sig))
    i = buf.index(start_text)
    j = buf.index(sig, i)
    cands = []
    for end in (b"\r\n}\r\n", b"\n}\n"):
        k = buf.find(end, j)
        if k >= 0:
            cands.append((k, len(end)))
    if not cands:
        raise SystemExit("no closing brace found after %r" % sig)
    k, n = min(cands)
    return i, k + n


REPLACE_FUNCS = [
    # (start marker, signature, snippet file)
    (b"/*! Blender's octahedral bone, as a wireframe from head to tail.",
     b"void GLView::drawOctahedralBone( const Vector3 & head, const Vector3 & tail )",
     "A_octa.cpp"),
    (b"void GLView::drawPoseSkeleton()",
     b"void GLView::drawPoseSkeleton()",
     "C_pose.cpp"),
    (b"void GLView::refreshSkeletonOverlay()",
     b"void GLView::refreshSkeletonOverlay()",
     "D_refresh.cpp"),
    (b"void GLView::drawSkeletonOverlay()",
     b"void GLView::drawSkeletonOverlay()",
     "E_draw.cpp"),
    (b"/*! Bone names, behind the Overlays menu's EXISTING \"Show Nodes\" entry.",
     b"void GLView::paintSkeletonOverlayNames( QPainter & painter )",
     "F_names.cpp"),
]

# --- the small, anchored edits ------------------------------------------------
HOVER_ANCHOR = b"""	// Pose Mode hover highlight: light the bone under the cursor
	if ( poseMode && !gizmoMode ) {
		int h = poseBoneAt( getQMouseEventPosition( event ) );
		if ( h != poseHoverBone ) {
			poseHoverBone = h;
			update();
		}
	}
"""

HOVER_ADD = b"""	/* The Overlays armature lights the bone under the cursor too, and names it
	 * (lane SKEL2). Its OWN member, not poseHoverBone: that one also drives the
	 * weight-influence overlay and Pose Mode's labels, which belong to Pose
	 * Mode and would start running with the overlay merely ticked.
	 */
	if ( skeletonOverlay && !poseMode && !gizmoMode )
		setSkeletonOverlayHover( skeletonOverlayBoneAt( getQMouseEventPosition( event ) ) );
"""

PICK_ANCHOR = b"""			} else if ( !( event->modifiers() & ( Qt::ShiftModifier | Qt::ControlModifier ) ) ) {
				objectSelectClick( -1, false );   // click empty = clear selection
			}
			update();
			return;
		}
"""

PICK_ADD = b"""
		/* THE OVERLAYS ARMATURE IS CLICKABLE (lane SKEL2).
		 *
		 * bungo, 2026-09-11: the bone view is to mirror the Skeleton Manager,
		 * and "click a bone in the viewport -> its row is current and visible in
		 * the manager" is half of what mirroring means. `clicked()` is what the
		 * application's own selection listens to, and the dock's
		 * currentNifIndexChanged handler is what makes the row current, so this
		 * adds no second selection path.
		 *
		 * It does NOT return when nothing is within the pick radius: a click on
		 * the mesh then still selects the mesh, so ticking the overlay costs a
		 * user nothing they had before.
		 */
		if ( skeletonOverlay && !poseMode && !editMode && !riggingWeightPaintMode
			 && !vertexPaintMode && !segmentPaintMode
			 && event->button() == selectMouseButton() && !isColorPicker ) {
			const int bone = skeletonOverlayBoneAt( evtPos );
			if ( bone >= 0 ) {
				const bool extend = event->modifiers() & ( Qt::ShiftModifier | Qt::ControlModifier );
				objectSelectClick( bone, extend );
				scene->currentBlock = model->getBlockIndex( bone );
				scene->currentIndex = scene->currentBlock;
				emit poseBonePicked( bone );
				emit clicked( model->getBlockIndex( bone ) );
				update();
				return;
			}
		}
"""

FRAME_ANCHOR = b"""	if ( !have ) {
		center();
		return;
	}
	Vector3 c = ( lo + hi ) * 0.5f;
"""

FRAME_NEW = b"""	/* A BONE IS NOT A SHAPE (lane SKEL2).
	 *
	 * objSelection can hold bone NODES -- a click in Pose Mode, or on the
	 * Overlays armature, puts one there -- and scene->shapes has nothing with
	 * that id, so Frame Selected fell through to center() and framed the whole
	 * model. Double-clicking a row in the Skeleton Manager is supposed to frame
	 * THAT BONE, so the same selection has to answer for nodes as well.
	 */
	if ( !have && !editMode && !objSelection.isEmpty() && scene ) {
		const float cap = ( skeletonOverlay ? skelOverlaySize : poseBoneSize ) * 2.0f;
		const QVector<int> & drawnSet = skelOverlayArmList.isEmpty() ? poseBones : skelOverlayArmList;
		for ( int b : objSelection ) {
			Node * bn = scene->findNode( model, model->getBlockIndex( b ) );
			if ( !bn )
				continue;
			grow( bn->worldTrans().translation );
			grow( boneTailIn( b, drawnSet, cap ) );
		}
		if ( have ) {
			// A single bone is a point or a very short segment; pad it so the
			// camera does not end up inside the rig.
			const Vector3 l2 = lo, h2 = hi;
			const Vector3 pad( cap, cap, cap );
			grow( l2 - pad );
			grow( h2 + pad );
		}
	}
	if ( !have ) {
		center();
		return;
	}
	Vector3 c = ( lo + hi ) * 0.5f;
"""

OFF_ANCHOR = b"""	} else {
		skelOverlayBones.clear();
		skelOverlayClass.clear();
		skelOverlayDrawnAt.clear();
		skelOverlayDrawnSegs.clear();
		skelOverlayCensus = SkeletonOverlayCensus();
	}
"""

OFF_NEW = b"""	} else {
		skelOverlayBones.clear();
		skelOverlayClass.clear();
		skelOverlayDrawnAt.clear();
		skelOverlayDrawnSegs.clear();
		skelOverlayCensus = SkeletonOverlayCensus();
		skelOverlayHover = -1;			// lane SKEL2
		skelOverlayFiltered = 0;
	}
"""

ANCHORED = [
    ("hover in mouseMoveEvent", HOVER_ANCHOR, HOVER_ANCHOR + HOVER_ADD),
    ("overlay pick in mousePressEvent", PICK_ANCHOR, PICK_ANCHOR + PICK_ADD),
    ("frameSelected over bone nodes", FRAME_ANCHOR, FRAME_NEW),
    ("setSkeletonOverlay off-state reset", OFF_ANCHOR, OFF_NEW),
]


def main(argv):
    apply = "--apply" in argv
    with open(TARGET, "rb") as fh:
        buf = fh.read()
    cr0, lf0, n0 = buf.count(b"\r"), buf.count(b"\n"), len(buf)
    print("before: %d bytes, CR %d, LF %d" % (n0, cr0, lf0))

    for start, sig, name in REPLACE_FUNCS:
        i, j = func_span(buf, start, sig)
        new = snippet(name)
        print("  replace %-28s %6d bytes -> %6d bytes (%s)"
              % (sig.decode().split("::")[1].split("(")[0], j - i, len(new), name))
        buf = buf[:i] + new.encode("utf-8") + buf[j:]

    for label, anchor, new in ANCHORED:
        anchor_crlf = anchor.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
        new_crlf = new.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
        n = buf.count(anchor_crlf)
        print("  anchor  %-40s x%d" % (label, n))
        if n != 1:
            raise SystemExit("anchor %r matched %d times, not 1" % (label, n))
        buf = buf.replace(anchor_crlf, new_crlf, 1)

    cr1, lf1, n1 = buf.count(b"\r"), buf.count(b"\n"), len(buf)
    print("after:  %d bytes, CR %d, LF %d   (dCR %+d, dLF %+d)"
          % (n1, cr1, lf1, cr1 - cr0, lf1 - lf0))
    if cr1 - cr0 != lf1 - lf0:
        raise SystemExit("CR and LF moved by different amounts: the splice is not CRLF-clean")
    print("marker 'lane SKEL2' x%d" % buf.count(b"lane SKEL2"))

    if apply:
        with open(TARGET, "wb") as fh:
            fh.write(buf)
        print("APPLIED")
    else:
        print("CHECK ONLY -- nothing written")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

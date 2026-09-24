#!/usr/bin/env python
"""Lane SKELFIX: the overlay draws bodies for the ARMATURE only.

Binary splice, because src/glview.cpp is CRLF and src/glview.h is LF
(CONSTITUTION rule 8: line endings measured with Python byte counts, mixed
files spliced in binary, never normalised). Every anchor must match EXACTLY
ONCE or nothing is written.

  patch.py --check    say what would change, write nothing
  patch.py --apply    do it
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
MARK = b"lane SKELFIX"


def crlf(s):
    return s.replace("\n", "\r\n").encode("utf-8")


def lf(s):
    return s.encode("utf-8")


# ---------------------------------------------------------------- src/glview.cpp
CPP_A_OLD = """	std::sort( skelOverlayBones.begin(), skelOverlayBones.end() );

	const float s = characteristicBoneSize( skelOverlayBones );
"""

CPP_A_NEW = """	std::sort( skelOverlayBones.begin(), skelOverlayBones.end() );

	/* THE ARMATURE -- which pairs may be joined by a BODY (lane SKELFIX).
	 *
	 * The first cut of this overlay drew a body between every parent and child
	 * in the list, and at frame 46 of a running clip that produced segments
	 * 300.5 units long fanning out of the character: the clip carries the
	 * travel on the COM track (487 units), while `Root`, `Camera`, `CamTarget`,
	 * `CamTargetParent`, `Camera Control` and the eight `AnimObject*` nodes
	 * stay at the world origin, and each of them was being joined to a relative
	 * that had moved. `CharacterBumper` and `EyeLeftDummy001` did the same from
	 * the file's own root. Measured, all of them, in
	 * scratchpad/skelfix_20260910/segments_frame46.tsv.
	 *
	 * THE RULE, and it is a rule about the FILE, never a list of names: a body
	 * is drawn only between two ARMATURE nodes, where the armature is
	 *
	 *   every node a skin lists (the Skeleton Manager's Bones filter -- its
	 *   Deforming and Unused classes), CLOSED UPWARDS through the parent chain,
	 *   and then CUT at the deepest node that still has every one of those
	 *   bones at or beneath it.
	 *
	 * The upward closure is what keeps the rig intact: FO4 body meshes weight
	 * the `*_skin` helper bones, so `Pelvis`, `LLeg_Thigh`, `LLeg_Calf`,
	 * `LArm_UpperArm`, `COM` and the whole limb chain are NOT in the Bones
	 * filter at all -- taking the filter alone would have cut the skeleton into
	 * 60 disconnected pieces of the 129 it draws (40 with a track test on top).
	 * The cut at the common root is what removes `Root` and the file root above
	 * it, and with them the last long segment (`Root` -> `COM`, 300.5 units).
	 * Camera, weapon, anim-object, bumper and eye-dummy nodes are outside the
	 * armature because no skin bone is beneath them, not because of their names.
	 *
	 * A NODE IS NEVER HIDDEN by this: pass 3 still draws a joint marker for
	 * every row in the list, so the overlay's census still equals the Skeleton
	 * Manager's, which is the harness's gate (a).
	 *
	 * The clip is not consulted. An UNTRACKED node cannot lag behind a tracked
	 * parent -- with no track it keeps its bind local and its world transform is
	 * its parent's, so it rides along by construction; and every tracked node in
	 * the armature is placed by the clip itself. Measured on the same frame: a
	 * "child has a track, or its whole parent chain does" test changes nothing
	 * that is drawn and removes 48 legitimate `*_skin` bodies.
	 */
	skelOverlayArm.clear();
	skelOverlayRule.clear();
	{
		const QSet<int> shown( skelOverlayBones.begin(), skelOverlayBones.end() );
		QVector<int> boneRows;
		for ( int b : skelOverlayBones ) {
			if ( skelOverlayClass.value( b, int( SkelNotABone ) ) != int( SkelNotABone ) )
				boneRows.append( b );
		}

		if ( boneRows.isEmpty() ) {
			/* FALLBACK, named in words (CONSTITUTION rule 10). A file with no
			 * skin at all -- an exported skeleton.nif -- gives the closure
			 * nothing to close over, and refusing every body there would leave
			 * a cloud of dots. Every node keeps its body, and the summary SAYS
			 * that is the arm that served.
			 */
			skelOverlayArm = shown;
			skelOverlayRule = tr( "No skin in this file, so there is no bone class to "
				"close over: every one of the %1 nodes is drawn as a bone." ).arg( shown.count() );
		} else {
			// how many of the bones sit at or beneath each block
			QHash<int, int> below;
			for ( int b : boneRows ) {
				int x = b;
				for ( int guard = 0; x >= 0 && guard <= skelOverlayBones.count() + 2; guard++ ) {
					below[x] = below.value( x, 0 ) + 1;
					x = model->getParent( x );
				}
			}
			// the DEEPEST block that still has all of them beneath it
			int common = -1, commonDepth = -1;
			for ( auto it = below.constBegin(); it != below.constEnd(); ++it ) {
				if ( it.value() != boneRows.count() )
					continue;
				int d = 0, x = model->getParent( it.key() );
				for ( ; x >= 0 && d <= skelOverlayBones.count() + 2; d++ )
					x = model->getParent( x );
				if ( d > commonDepth ) {
					commonDepth = d;
					common = it.key();
				}
			}
			for ( auto it = below.constBegin(); it != below.constEnd(); ++it ) {
				if ( !shown.contains( it.key() ) )
					continue;
				bool inside = false;
				int x = it.key();
				for ( int guard = 0; x >= 0 && guard <= skelOverlayBones.count() + 2; guard++ ) {
					if ( x == common ) {
						inside = true;
						break;
					}
					x = model->getParent( x );
				}
				if ( inside )
					skelOverlayArm.insert( it.key() );
			}
			const QString rootName = ( common >= 0 )
				? model->get<QString>( model->getBlockIndex( common ), "Name" ) : QString();
			skelOverlayRule = tr( "Bones are drawn between armature nodes only: the %1 node(s) "
				"a skin lists, plus every node between them, rooted at %2. The other %3 node(s) "
				"-- camera, anim-object, weapon, attach and root nodes, which no skin bone sits "
				"beneath -- get a joint marker and no bone." )
				.arg( boneRows.count() )
				.arg( rootName.isEmpty() ? tr( "the file root" ) : rootName )
				.arg( skelOverlayBones.count() - skelOverlayArm.count() );
		}
		skelOverlayArmList.clear();
		skelOverlayArmList.reserve( skelOverlayArm.count() );
		for ( int b : skelOverlayBones ) {
			if ( skelOverlayArm.contains( b ) )
				skelOverlayArmList.append( b );
		}
	}

	const float s = characteristicBoneSize( skelOverlayBones );
"""

CPP_B_OLD = """	// Which bones have a drawn child: a bone with none gets a stub instead of a
	// body, so no bone in the list is invisible.
	QSet<int> hasDrawnChild;
	for ( int b : skelOverlayBones ) {
		const int p = model->getParent( b );
		if ( p >= 0 && head.contains( p ) )
			hasDrawnChild.insert( p );
	}
"""

CPP_B_NEW = """	// Which ARMATURE bones have a drawn child: a bone with none gets a stub
	// instead of a body. The test is the same one pass 1 applies, so a bone
	// whose only children are outside the armature still gets its stub
	// (lane SKELFIX).
	QSet<int> hasDrawnChild;
	for ( int b : skelOverlayBones ) {
		const int p = model->getParent( b );
		if ( p >= 0 && head.contains( p ) && skelOverlayArm.contains( b )
			 && skelOverlayArm.contains( p ) )
			hasDrawnChild.insert( p );
	}
"""

CPP_C_OLD = """	for ( int b : skelOverlayBones ) {
		const int p = model->getParent( b );
		if ( p < 0 || !head.contains( p ) || !head.contains( b ) )
			continue;
		scene->setGLColor( colClass[ qBound( 0, skelOverlayClass.value( b, 2 ), 2 ) ] );
"""

CPP_C_NEW = """	for ( int b : skelOverlayBones ) {
		const int p = model->getParent( b );
		if ( p < 0 || !head.contains( p ) || !head.contains( b ) )
			continue;
		if ( !skelOverlayArm.contains( b ) || !skelOverlayArm.contains( p ) ) {
			c.skipped++;			// lane SKELFIX: not an armature pair
			continue;
		}
		scene->setGLColor( colClass[ qBound( 0, skelOverlayClass.value( b, 2 ), 2 ) ] );
"""

CPP_D_OLD = """	for ( int b : skelOverlayBones ) {
		if ( hasDrawnChild.contains( b ) || !head.contains( b ) )
			continue;
		const Vector3 tail = boneTailIn( b, skelOverlayBones, cap );
"""

CPP_D_NEW = """	for ( int b : skelOverlayBones ) {
		if ( hasDrawnChild.contains( b ) || !head.contains( b ) )
			continue;
		if ( !skelOverlayArm.contains( b ) ) {
			c.skipped++;			// lane SKELFIX: a joint marker, and nothing else
			continue;
		}
		// The stub points at the mean of the bone's DRAWN children, so a bone
		// whose only child is outside the armature does not aim at it.
		const Vector3 tail = boneTailIn( b, skelOverlayArmList, cap );
"""

# ---------------------------------------------------------------- src/glview.h
H_A_OLD = """		int segments = 0;     //!< parent -> child bone bodies drawn
		int stubs = 0;        //!< bones with no drawn child, drawn down their own axis
"""

H_A_NEW = """		int segments = 0;     //!< parent -> child bone bodies drawn
		int stubs = 0;        //!< bones with no drawn child, drawn down their own axis
		int skipped = 0;      //!< bodies the ARMATURE rule refused (lane SKELFIX):
		                      //!< a pair with an end outside the armature in pass 1,
		                      //!< plus a non-armature bone's stub in pass 2. Those
		                      //!< nodes still get their joint marker in pass 3, so
		                      //!< `nodes` is unaffected by this count.
"""

H_B_OLD = """	//! SkelOverlayClass for a block, or -1 when the overlay does not draw it.
	int skeletonOverlayClassOf( int block ) const { return skelOverlayClass.value( block, -1 ); }
"""

H_B_NEW = """	//! SkelOverlayClass for a block, or -1 when the overlay does not draw it.
	int skeletonOverlayClassOf( int block ) const { return skelOverlayClass.value( block, -1 ); }
	/*! Is this block one of the nodes a BODY may be drawn to (lane SKELFIX)?
	 *
	 *  The armature: every node a skin lists, closed upwards through the parent
	 *  chain, cut at the deepest node that has all of them beneath it. A node
	 *  outside it is still drawn -- as a joint marker -- but never joined to
	 *  anything, which is what stops a camera or anim-object node parked at the
	 *  world origin from being tied to a character that has moved.
	 */
	bool skeletonOverlayInArmature( int block ) const { return skelOverlayArm.contains( block ); }
	//! The rule the overlay is applying, in words, INCLUDING which arm served
	//! when there is no skin to close over. The Overlays entry's tooltip.
	QString skeletonOverlayRule() const { return skelOverlayRule; }
"""

H_C_OLD = """	QHash<int, int> skelOverlayClass;      //!< block -> SkelOverlayClass
"""

H_C_NEW = """	QHash<int, int> skelOverlayClass;      //!< block -> SkelOverlayClass
	QSet<int> skelOverlayArm;              //!< lane SKELFIX: blocks a body may join
	QVector<int> skelOverlayArmList;       //!< the same set, in draw order, for boneTailIn()
	QString skelOverlayRule;               //!< the rule in words (tooltip / summary)
"""

EDITS = [
    ("src/glview.cpp", crlf, [(CPP_A_OLD, CPP_A_NEW), (CPP_B_OLD, CPP_B_NEW),
                              (CPP_C_OLD, CPP_C_NEW), (CPP_D_OLD, CPP_D_NEW)]),
    ("src/glview.h", lf, [(H_A_OLD, H_A_NEW), (H_B_OLD, H_B_NEW), (H_C_OLD, H_C_NEW)]),
]


def main(argv):
    apply = "--apply" in argv
    rc = 0
    for rel, enc, pairs in EDITS:
        path = os.path.join(ROOT, rel)
        blob = open(path, "rb").read()
        cr0, lf0 = blob.count(b"\r"), blob.count(b"\n")
        if MARK in blob:
            print("%-18s ALREADY APPLIED (%s present)" % (rel, MARK.decode()))
            continue
        out = blob
        for old, new in pairs:
            o, n = enc(old), enc(new)
            hits = out.count(o)
            if hits != 1:
                print("%-18s ANCHOR MATCHES %d TIMES, refusing: %r"
                      % (rel, hits, old.splitlines()[0][:60]))
                rc = 2
                break
            out = out.replace(o, n)
        else:
            cr1, lf1 = out.count(b"\r"), out.count(b"\n")
            print("%-18s %+d bytes, CR %d -> %d, LF %d -> %d, dCR %+d dLF %+d"
                  % (rel, len(out) - len(blob), cr0, cr1, lf0, lf1, cr1 - cr0, lf1 - lf0))
            if apply:
                open(path, "wb").write(out)
                print("%-18s WRITTEN" % rel)
    return rc


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

#!/usr/bin/env python
"""Lane SKEL2 -- the hook-up into src/nifskope_ui.cpp, as a REFUSING script.

`src/nifskope_ui.cpp` is 32,000 lines and is the file every lane has to touch;
this lane's whole footprint in it is two edits, and they are written here rather
than applied by hand so the resume can prove what went in (ww-anchored-hookup).

  E1  Overlays > Bone Display -- Blender's Armature > Viewport Display >
      Display As, as three exclusive rows plus X-ray and Names. bungo's
      END-menu rule: rows only, no descriptions.
  E2  the headless render hook learns the armature's switches, so a picture of
      a display mode or of a Skeleton Manager chip is reproducible instead of
      depending on what the user last ticked.

The file is LF-only (CR 0) and stays LF-only; the CR count is asserted.

  python hookup.py            # --check, writes nothing
  python hookup.py --apply
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TARGET = os.path.join(ROOT, "src", "nifskope_ui.cpp")

E1_ANCHOR = """		m->addAction( aShowSkeleton );
"""

E1_TEXT = """
		/* Overlays > Bone Display (lane SKEL2, bungo 2026-09-11).
		 *
		 * His words: "what about the bone shape? Shouldn't it be something like
		 * in Blender?", over a screenshot of Blender 4.5.3's default armature
		 * bone -- and then "Just keep the color of the bones blue".
		 *
		 * This is Blender's Armature > Viewport Display > Display As, with its
		 * own X-ray and Names beside it. ROWS ONLY, no descriptions: his rule
		 * for the END menu, which is the rule for every menu in the fork.
		 *
		 * B-Bone and Envelope are the two Blender modes that are NOT here, and
		 * the reason is the zero-authoring rule rather than effort: a B-Bone
		 * needs a per-bone segment count and two handle bones, an Envelope
		 * needs a head radius, a tail radius and a distance, and a NIF NiNode
		 * carries none of the five. Inventing them would be hand-authored data
		 * coupled to one rig.
		 */
		{
			QMenu * bm = m->addMenu( tr( "Bone Display" ) );
			bm->setToolTipsVisible( true );
			QSettings bs;
			const int startDisplay = qBound( 0,
				bs.value( QStringLiteral( "GLView/ArmatureDisplay" ), 0 ).toInt(), 2 );
			const bool startXray = bs.value( QStringLiteral( "GLView/ArmatureXray" ), true ).toBool();
			const bool startNames = bs.value( QStringLiteral( "GLView/ArmatureNames" ), false ).toBool();
			auto * grp = new QActionGroup( bm );
			grp->setExclusive( true );
			const QStringList shapeNames = { tr( "Octahedral" ), tr( "Stick" ), tr( "Wire" ) };
			for ( int i = 0; i < shapeNames.size(); i++ ) {
				QAction * a = bm->addAction( shapeNames.at( i ) );
				a->setObjectName( QStringLiteral( "BoneDisplay%1" ).arg( i ) );
				a->setCheckable( true );
				a->setActionGroup( grp );
				a->setChecked( i == startDisplay );
				connect( a, &QAction::triggered, this, [this, i]() {
					QSettings().setValue( QStringLiteral( "GLView/ArmatureDisplay" ), i );
					if ( ogl )
						ogl->setArmatureDisplay( i );
				} );
			}
			bm->addSeparator();
			QAction * aBoneXray = bm->addAction( tr( "X-ray" ) );
			aBoneXray->setObjectName( QStringLiteral( "BoneXray" ) );
			aBoneXray->setCheckable( true );
			aBoneXray->setChecked( startXray );
			connect( aBoneXray, &QAction::toggled, this, [this]( bool on ) {
				QSettings().setValue( QStringLiteral( "GLView/ArmatureXray" ), on );
				if ( ogl )
					ogl->setArmatureXray( on );
			} );
			QAction * aBoneNames = bm->addAction( tr( "Names" ) );
			aBoneNames->setObjectName( QStringLiteral( "BoneNames" ) );
			aBoneNames->setCheckable( true );
			aBoneNames->setChecked( startNames );
			connect( aBoneNames, &QAction::toggled, this, [this]( bool on ) {
				QSettings().setValue( QStringLiteral( "GLView/ArmatureNames" ), on ? 1 : 0 );
				if ( ogl )
					ogl->setArmatureNames( on ? 1 : 0 );
			} );
			if ( ogl ) {
				ogl->setArmatureDisplay( startDisplay );
				ogl->setArmatureXray( startXray );
				ogl->setArmatureNames( startNames ? 1 : 0 );
			}
		}
"""

E2_ANCHOR = """					if ( qEnvironmentVariableIntValue( "WW_SKELETON_OVERLAY" ) != 0 )
						skope->ogl->setSkeletonOverlay( true );
"""

E2_TEXT = """
					/* lane SKEL2: the armature's own switches, on the same
					 * grounds -- a picture of a display mode, an X-ray state or
					 * a Skeleton Manager chip has to be reproducible, and a
					 * QSettings value the user last ticked is not.
					 * WW_SKELOVERLAY_DUMP (read in GLView::drawSkeletonOverlay)
					 * writes every drawn node's screen position for THIS frame.
					 */
					if ( qEnvironmentVariableIsSet( "WW_SKELETON_DISPLAY" ) )
						skope->ogl->setArmatureDisplay(
							qEnvironmentVariableIntValue( "WW_SKELETON_DISPLAY" ) );
					if ( qEnvironmentVariableIsSet( "WW_SKELETON_XRAY" ) )
						skope->ogl->setArmatureXray(
							qEnvironmentVariableIntValue( "WW_SKELETON_XRAY" ) != 0 );
					if ( qEnvironmentVariableIsSet( "WW_SKELETON_NAMES" ) )
						skope->ogl->setArmatureNames(
							qEnvironmentVariableIntValue( "WW_SKELETON_NAMES" ) );
					if ( qEnvironmentVariableIsSet( "WW_SKELETON_CHIP" )
						 || qEnvironmentVariableIsSet( "WW_SKELETON_SEARCH" ) )
						skope->ogl->setSkeletonOverlayFilter(
							qEnvironmentVariableIntValue( "WW_SKELETON_CHIP" ),
							qEnvironmentVariable( "WW_SKELETON_SEARCH" ) );
"""

EDITS = [
    ("E1 Overlays > Bone Display", E1_ANCHOR, E1_TEXT),
    ("E2 render hook armature switches", E2_ANCHOR, E2_TEXT),
]

MARKER = "lane SKEL2"


def main(argv):
    apply = "--apply" in argv
    with open(TARGET, "rb") as fh:
        buf = fh.read()
    cr0, lf0, n0 = buf.count(b"\r"), buf.count(b"\n"), len(buf)
    print("before: %d bytes, CR %d, LF %d, marker x%d"
          % (n0, cr0, lf0, buf.count(MARKER.encode())))

    for label, anchor, text in EDITS:
        a = anchor.encode("utf-8")
        n = buf.count(a)
        print("  %-36s anchor x%d, +%d bytes" % (label, n, len(text.encode("utf-8"))))
        if n != 1:
            raise SystemExit("anchor for %s matched %d times, not 1" % (label, n))
        buf = buf.replace(a, a + text.encode("utf-8"), 1)

    cr1, lf1, n1 = buf.count(b"\r"), buf.count(b"\n"), len(buf)
    print("after:  %d bytes, CR %d, LF %d, marker x%d"
          % (n1, cr1, lf1, buf.count(MARKER.encode())))
    if cr1 != cr0:
        raise SystemExit("CR count moved %d -> %d: the file is LF-only and must stay so"
                         % (cr0, cr1))

    if apply:
        with open(TARGET, "wb") as fh:
            fh.write(buf)
        print("APPLIED")
    else:
        print("CHECK ONLY -- nothing written")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

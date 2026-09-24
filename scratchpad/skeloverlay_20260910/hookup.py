#!/usr/bin/env python
"""Lane SKELOVERLAY -- the hook-up, as a REFUSING script.

The lane owns src/glview.{h,cpp} and the new src/skeloverlaytest.cpp. It does
NOT own NifSkope.pro (lane BUILD8 is building), nor src/nifskope_ui.cpp (lanes
FILESTAB and HKX3 are writing into it). Everything those two files need is in
the table below and is applied by whoever holds the build, not by this lane
while another lane's edits are in flight (ww-anchored-hookup).

Four edits, three of them in nifskope_ui.cpp:

  P1  NifSkope.pro          src/skeloverlaytest.cpp joins the build
  U1  src/nifskope_ui.cpp   the Overlays menu entry "Show Skeleton"
  U2  src/nifskope_ui.cpp   it persists like the other Overlays toggles
  U3  src/nifskope_ui.cpp   WW_SKELETON_OVERLAY=1 arms it for a headless render
  U4  src/nifskope_ui.cpp   the harness's single line

Every inserted block carries the marker string "lane SKELOVERLAY", so a resume
decides "applied or not" by grepping for that -- never from the anchor still
matching, which it does either way (ww-anchored-hookup section 4).

    python scratchpad/skeloverlay_20260910/hookup.py            # --check
    python scratchpad/skeloverlay_20260910/hookup.py --apply
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MARKER = b'lane SKELOVERLAY'

# ---------------------------------------------------------------- the table
# (name, relative path, mode, anchor, text)   mode: "after" | "replace"
EDITS = []

EDITS.append((
    'P1', 'NifSkope.pro', 'after',
    '\tsrc/hkxplaybacktest.cpp \\\n',
    '\tsrc/skeloverlaytest.cpp \\\n',        # lane SKELOVERLAY
))

EDITS.append((
    'U1', 'src/nifskope_ui.cpp', 'after',
    '''		const QList<QAction *> ds = { ui->aShowCollision, ui->aShowAxes, ui->aShowNodes, ui->aDoSkinning,
			ui->aShowConstraints, ui->aShowMarkers, ui->aShowHidden };
		for ( QAction * a : ds )
			m->addAction( a );
''',
    '''
		/* Show Skeleton (lane SKELOVERLAY). bungo 2026-09-10, verbatim:
		 * "Add to the overlays: View skeleton, shows you the bones, basically
		 * the same view as in the skeleton manager".
		 *
		 * Beside Show Nodes and Do Skinning because it is the same kind of
		 * thing: what the viewport draws on top of the model. Its bone list and
		 * its three colours come from skeletonAnalyse(), the Skeleton Manager
		 * dock's own analysis, so the two views cannot disagree -- and the bone
		 * NAMES ride on Show Nodes rather than a row of their own.
		 */
		QAction * aShowSkeleton = new QAction( tr( "Show Skeleton" ), this );
		aShowSkeleton->setCheckable( true );
		aShowSkeleton->setChecked( false );
		aShowSkeleton->setToolTip( tr( "Draw the rig's bones over the model and through it, "
			"coloured the way the Skeleton Manager colours them. Tick Show Nodes as well for bone names." ) );
		connect( aShowSkeleton, &QAction::toggled, this, [this]( bool on ) {
			if ( ogl ) {
				ogl->setSkeletonOverlay( on );
				ogl->update();
			}
		} );
		m->addAction( aShowSkeleton );
''',
))

EDITS.append((
    'U2', 'src/nifskope_ui.cpp', 'after',
    '''			persist( aOrbitSel,     QStringLiteral( "GLView/Display/OrbitSelection" ) );
''',
    '''			persist( aShowSkeleton, QStringLiteral( "GLView/Display/ShowSkeleton" ) );	// lane SKELOVERLAY
''',
))

EDITS.append((
    'U3', 'src/nifskope_ui.cpp', 'after',
    '''						if ( hkxErr.isEmpty() && !hkxAdded.isEmpty() )
							skope->ogl->setSceneSequence( hkxAdded.first() );
					}
				}
''',
    '''
					/* WW_SKELETON_OVERLAY=1: draw Overlays > Show Skeleton in this
					 * capture (lane SKELOVERLAY). It is a viewport toggle, not a
					 * Scene::option, so WW_RENDER_CLEAN cannot reach it and a
					 * picture of the armature would otherwise depend on whatever
					 * the user last ticked -- which is not reproducible.
					 */
					if ( qEnvironmentVariableIntValue( "WW_SKELETON_OVERLAY" ) != 0 )
						skope->ogl->setSkeletonOverlay( true );

''',
))

EDITS.append((
    'U4', 'src/nifskope_ui.cpp', 'after',
    '''	// TEST HARNESS (WW_HKXANIM_TEST=1): lane HKX2's playback and mapping gates.
	// The whole harness is src/hkxplaybacktest.cpp; this is its only line here.
	wwHkxAnimHarness( skope );
''',
    '''
	// TEST HARNESS (WW_SKELOVERLAY_TEST=1): lane SKELOVERLAY's gates for
	// Overlays > Show Skeleton. The whole harness is src/skeloverlaytest.cpp;
	// this is its only line here.
	{
		extern void wwSkelOverlayHarness( NifSkope * );
		wwSkelOverlayHarness( skope );
	}
''',
))


def main():
    apply = '--apply' in sys.argv
    files = {}
    problems = []
    predicted = {}

    for name, rel, mode, anchor, text in EDITS:
        path = os.path.join(ROOT, rel)
        if rel not in files:
            files[rel] = open(path, 'rb').read()
        data = files[rel]
        # The line ending is IN the anchor: match the file's own convention.
        cr = data.count(b'\r')
        lf = data.count(b'\n')
        crlf_file = cr > lf * 0.5
        a = anchor.encode('utf-8')
        t = text.encode('utf-8')
        if crlf_file:
            a = a.replace(b'\n', b'\r\n')
            t = t.replace(b'\n', b'\r\n')
        n = data.count(a)
        already = MARKER in data and data.count(t) == 1
        print('%-3s %-24s anchor x%d   text already present: %s   (file CR %d / LF %d)'
              % (name, rel, n, 'yes' if already else 'no', cr, lf))
        if already:
            problems.append('%s: ALREADY APPLIED' % name)
            continue
        if n != 1:
            problems.append('%s: anchor matches %d times in %s, need exactly 1' % (name, n, rel))
            continue
        if mode == 'after':
            new = data.replace(a, a + t, 1)
        elif mode == 'replace':
            new = data.replace(a, t, 1)
        else:
            problems.append('%s: unknown mode %s' % (name, mode))
            continue
        # CR and LF must move together, or a bare LF just entered a CRLF file.
        dcr = new.count(b'\r') - cr
        dlf = new.count(b'\n') - lf
        if crlf_file and dcr != dlf:
            problems.append('%s: dCR %+d but dLF %+d -- a bare LF got in' % (name, dcr, dlf))
            continue
        if not crlf_file and dcr != 0:
            problems.append('%s: dCR %+d in an LF-only file' % (name, dcr))
            continue
        predicted[rel] = predicted.get(rel, 0) + len(t)
        files[rel] = new

    print()
    for rel, grew in predicted.items():
        print('%-24s would grow by %d bytes' % (rel, grew))

    if problems:
        print('\nREFUSED, nothing written:')
        for p in problems:
            print('  ' + p)
        return 2

    if not apply:
        print('\n--check: %d edits match, nothing written' % len(EDITS))
        return 0

    for rel, data in files.items():
        open(os.path.join(ROOT, rel), 'wb').write(data)
        print('written %s (%d bytes)' % (rel, len(data)))
    return 0


sys.exit(main())

#!/usr/bin/env python3
"""Lane HKX3's hook-up, as a REFUSING script (skill ww-anchored-hookup).

Lane HKX3 owns src/ui/widgets/timeline.{cpp,h} and its own new files. Lane
BUILD8 is building NifSkope.pro / src/nifskope.cpp / src/nifskope_ui.cpp /
src/gltfexport.*, and lane FILESTAB is preparing edits to the browser dock, so
none of those may be edited by this lane. These are the nine lines they need.

The .pro edit also DEFINES WW_HKXANIM_UI, which is what src/ui/widgets/
timeline.{cpp,h} guard every clip-shaped line with: with this script unapplied
the timeline dock compiles, links and behaves exactly as it did before, and the
new files are simply not in the build. That is the "compiles with AND without
the hook-up" rule, and both halves are proven by
scratchpad/hkx3_20260910/syntax.sh (with) and syntax_nohook.sh (without).

  python scratchpad/hkx3_20260910/hookup.py            # --check, writes nothing
  python scratchpad/hkx3_20260910/hookup.py --apply

--check prints, for every anchor, how many times it occurs (it must be 1) and
the file's CR count. It never prints "ok": an anchor is the line the text goes
AFTER, so it still matches after the edit, and a --check that says "ok" on an
already-applied file is exactly the trap BUILD5b fell into on 2026-09-10.
Decide "applied or not" from the marker string below, which every inserted
block carries, or from the byte delta this script predicts.

MARKER: "lane HKX3"
"""

import os
import sys

ROOT = os.path.abspath( os.path.join( os.path.dirname( __file__ ), "..", ".." ) )

# ( path, "after" | "replace", anchor, text )
EDITS = [

	# ---------------------------------------------------------------- .pro ---
	( "NifSkope.pro", "after",
	  "\tsrc/hkxplayback.h \\\n",
	  "\tsrc/hkxanimui.h \\\n" ),

	( "NifSkope.pro", "after",
	  "\tsrc/hkxplaybacktest.cpp \\\n",
	  "\tsrc/hkxanimui.cpp \\\n"
	  "\tsrc/hkxanimuitest.cpp \\\n" ),

	# The switch. Everything lane HKX3 added to the timeline dock is behind it.
	( "NifSkope.pro", "after",
	  "DEFINES += WW_EDITION_VERSION=\\\\\\\"$${WW_VER}\\\\\\\"\n",
	  "\n"
	  "# Loaded Havok animation clips in the Animation Manager dock (lane HKX3).\n"
	  "# src/ui/widgets/timeline.{cpp,h} guard every clip-shaped line with this,\n"
	  "# so the dock builds and behaves as before when it is not defined.\n"
	  "DEFINES += WW_HKXANIM_UI\n" ),

	# ------------------------------------------------------- nifskope_ui.cpp ---
	( "src/nifskope_ui.cpp", "after",
	  "#include \"hkxplayback.h\"\n",
	  "#include \"hkxanimui.h\"		// lane HKX3\n" ),

	( "src/nifskope_ui.cpp", "after",
	  "\twwHkxAnimHarness( skope );\n",
	  "\t// WW_HKXANIM_UI_TEST: lane HKX3's gates, src/hkxanimuitest.cpp.\n"
	  "\twwHkxAnimUiHarness( skope );\n" ),

	( "src/nifskope_ui.cpp", "after",
	  "\ttimeline->setNif( nif );\n",
	  "\t// The dock's second list source: the .hkx clips loaded into this\n"
	  "\t// view's Scene, which have no block to be a NiControllerSequence row\n"
	  "\t// (lane HKX3).\n"
	  "\ttimeline->setGLView( ogl );\n" ),

	# The render toolbar's Load button becomes a caller of the shared loader,
	# so the button, the dock and a drop cannot drift apart.
	( "src/nifskope_ui.cpp", "replace",
	  "			QStringList added;\n"
	  "			const QString err = sc->hkx->load( f, &added );\n"
	  "			if ( !err.isEmpty() ) {\n"
	  "				hkxNote->setText( err );\n"
	  "				return;\n"
	  "			}\n"
	  "			if ( !added.isEmpty() )\n"
	  "				ogl->setSceneSequence( added.first() );\n"
	  "			hkxNote->setText( sc->hkx->summary() );\n",
	  "			// ONE loader for the button, the Animation Manager dock and a\n"
	  "			// dropped file (lane HKX3). It loads, activates, MEASURES\n"
	  "			// whether the clip actually bound, and returns the sentence.\n"
	  "			hkxNote->setText(\n"
	  "				WwHkxAnimHub::instance()->loadFiles( ogl, { f }, true ) );\n" ),

	( "src/nifskope_ui.cpp", "replace",
	  "			sc->hkx->setRootMotion( on );\n",
	  "			// through the hub so the dock's own Root motion button follows\n"
	  "			// this one (lane HKX3)\n"
	  "			WwHkxAnimHub::instance()->setRootMotion( ogl, on );\n" ),

	# THE DROP. bungo: "So either win exporer pick or drag and drop".
	( "src/nifskope_ui.cpp", "after",
	  "		QWidget * eventWidget = qobject_cast<QWidget *>( o );\n"
	  "		const bool belongsHere = o == this || o == ogl || o == graphicsView\n"
	  "			|| ( eventWidget && isAncestorOf( eventWidget ) );\n",
	  "\n"
	  "		/* A DROPPED .hkx IS AN ANIMATION, NOT A DOCUMENT (lane HKX3).\n"
	  "		 *\n"
	  "		 * bungo, 2026-09-10: \"So either win exporer pick or drag and drop\".\n"
	  "		 * .hkx is deliberately NOT one of NifSkope::fileExtensions() -- it is\n"
	  "		 * not a thing this program opens as a file -- so the loop above never\n"
	  "		 * collects it and the drop was ignored with no feedback at all. It\n"
	  "		 * goes to the same loader the two Load buttons use, onto the model\n"
	  "		 * that is already open, and the loader's sentence (the summary, or\n"
	  "		 * the refusal when nothing is open to animate) goes to the status bar\n"
	  "		 * as well as to the Animation Manager's own line.\n"
	  "		 */\n"
	  "		QStringList hkxUrls;\n"
	  "		if ( drop->mimeData() && drop->mimeData()->hasUrls() ) {\n"
	  "			for ( const QUrl & url : drop->mimeData()->urls() ) {\n"
	  "				if ( url.isLocalFile() )\n"
	  "					hkxUrls.append( url.toLocalFile() );\n"
	  "			}\n"
	  "		}\n"
	  "		const QStringList hkxDropped = WwHkxAnimHub::animationFilesIn( hkxUrls );\n"
	  "		if ( belongsHere && !hkxDropped.isEmpty() ) {\n"
	  "			drop->setDropAction( Qt::CopyAction );\n"
	  "			drop->accept();\n"
	  "			if ( e->type() == QEvent::Drop ) {\n"
	  "				// Queued for the same reason the .nif route is: never run a\n"
	  "				// dialog or a scene rebuild while Qt is unwinding the\n"
	  "				// platform drag.\n"
	  "				QTimer::singleShot( 0, this, [this, hkxDropped]() {\n"
	  "					const QString said = WwHkxAnimHub::instance()->loadFiles(\n"
	  "						ogl, hkxDropped, true );\n"
	  "					if ( statusBar() )\n"
	  "						statusBar()->showMessage( said, 15000 );\n"
	  "				} );\n"
	  "			}\n"
	  "			return true;\n"
	  "		}\n" ),
]


def main():
	apply = "--apply" in sys.argv[1:]
	ok = True
	byfile = {}

	for path, mode, anchor, text in EDITS:
		full = os.path.join( ROOT, path )
		if full not in byfile:
			with open( full, "rb" ) as f:
				byfile[full] = f.read()
		blob = byfile[full]
		a = anchor.encode( "utf-8" )
		n = blob.count( a )
		print( "%-24s %-7s anchor x%d  %r" % ( path, mode, n, anchor.splitlines()[0][:52] ) )
		if n != 1:
			ok = False
			continue
		if apply:
			t = text.encode( "utf-8" )
			byfile[full] = blob.replace( a, ( a + t ) if mode == "after" else t, 1 )

	for full, blob in byfile.items():
		with open( full, "rb" ) as f:
			orig = f.read()
		print( "%-40s CR %d -> %d   bytes %d -> %d"
			   % ( os.path.relpath( full, ROOT ), orig.count( b"\r" ), blob.count( b"\r" ),
				   len( orig ), len( blob ) ) )
		# Every file this lane touches is LF-only; a CR appearing here means a
		# heredoc or an editor rewrote a line ending, and the commit would move
		# lines nobody edited.
		if blob.count( b"\r" ) != orig.count( b"\r" ):
			print( "  REFUSED: CR count moved" )
			ok = False

	if not ok:
		print( "REFUSED: an anchor does not match exactly once (or a CR moved). Nothing written." )
		return 1
	if not apply:
		print( "check only, nothing written. Marker to grep for after --apply: 'lane HKX3'" )
		return 0

	for full, blob in byfile.items():
		with open( full, "wb" ) as f:
			f.write( blob )
	print( "applied %d edits over %d files" % ( len( EDITS ), len( byfile ) ) )
	return 0


if __name__ == "__main__":
	sys.exit( main() )

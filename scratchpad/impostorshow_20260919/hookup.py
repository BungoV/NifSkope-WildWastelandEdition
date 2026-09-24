#!/usr/bin/env python
# ---------------------------------------------------------------------------
# hookup.py -- lane IMPOSTORSHOW's anchored hook-up. 2026-09-19.
#
# HORIZONOUT's lesson, and why this file exists at all: a scripted edit that
# matches loosely lands somewhere it was not meant to, and in a tree holding
# ~235 uncommitted files from two other lanes that is not recoverable by
# reading a diff. So every edit here:
#
#   * names an EXACT anchor string, and REFUSES the whole run if it does not
#     occur EXACTLY ONCE;
#   * refuses if its own inserted text is already present (so a second run is
#     a no-op, not a doubled hunk);
#   * writes bytes back with the file's OWN line endings preserved -- this
#     tree is mixed CRLF/LF and a heredoc's endings are not the file's;
#   * prints every file it touched and the byte delta.
#
# Nothing is reverted, reformatted or tidied. Four files, four hunks, each the
# smallest that can work.
#
#   python scratchpad/impostorshow_20260919/hookup.py --check   (say what it would do)
#   python scratchpad/impostorshow_20260919/hookup.py           (do it)
# ---------------------------------------------------------------------------

import io, os, sys

ROOT = os.path.abspath( os.path.join( os.path.dirname( __file__ ), "..", ".." ) )
CHECK = "--check" in sys.argv

# (file, anchor, replacement)  -- anchor must occur EXACTLY ONCE.
EDITS = []

# --- 1. NifSkope.pro : the four new sources and three new headers -----------
EDITS.append( (
	"NifSkope.pro",
	"\tsrc/gl/renderer.h \\\n",
	"\tsrc/gl/renderer.h \\\n"
	"\tsrc/gl/impostordraw.h \\\n"
	"\tsrc/impostoroct.h \\\n"
	"\tsrc/impostorcard.h \\\n"
	"\tsrc/impostorpreviewtest.h \\\n"
) )
EDITS.append( (
	"NifSkope.pro",
	"\tsrc/gl/renderer.cpp \\\n",
	"\tsrc/gl/renderer.cpp \\\n"
	"\tsrc/gl/impostordraw.cpp \\\n"
	"\tsrc/impostoroct.cpp \\\n"
	"\tsrc/impostorcard.cpp \\\n"
	"\tsrc/impostorpreviewtest.cpp \\\n"
) )

# --- 2. src/glview.cpp : the pass call, in paintGL --------------------------
# The card is drawn BEFORE the scene, not after. It is opaque, alpha-TESTED and
# it writes depth (see res/shaders/impostor_oct.frag), so the depth test alone
# decides what covers what and the order is free -- and drawing first means the
# scene's own transparent pass still sorts against the card's depth, which is
# the whole point of the spec's pixel depth offset.
EDITS.append( (
	"src/glview.cpp",
	'#include "gl/renderer.h"\n',
	'#include "gl/renderer.h"\n'
	'#include "impostorpreviewtest.h"\n'
) )
EDITS.append( (
	"src/glview.cpp",
	"\t\tscene->draw();\n"
	"\t\tfor ( Scene * ws : std::as_const( workspaceDrawScenes ) )\n"
	"\t\t\tws->draw();\n",
	"\t\t// lane IMPOSTORSHOW 2026-09-19: the octahedral impostor card, when one\n"
	"\t\t// is armed. Inert in every ordinary session -- see impostorpreviewtest.h.\n"
	"\t\twwImpostorPreviewDraw( scene );\n"
	"\t\tif ( !wwImpostorPreviewSuppressScene() ) {\n"
	"\t\t\tscene->draw();\n"
	"\t\t\tfor ( Scene * ws : std::as_const( workspaceDrawScenes ) )\n"
	"\t\t\t\tws->draw();\n"
	"\t\t}\n"
) )

# --- 3. src/nifskope_ui.cpp : arm the harness at startup --------------------
# Beside the other WW_* harnesses, inside NifSkope::createWindow, which is
# where `skope`, `skope->ogl` and `skope->viewportHeader` are all in scope and
# accessible (they are private members; the harness is handed them, it does not
# reach for them).
EDITS.append( (
	"src/nifskope_ui.cpp",
	'#include "glview.h"\n',
	'#include "glview.h"\n'
	'#include "impostorpreviewtest.h"\n'
) )
EDITS.append( (
	"src/nifskope_ui.cpp",
	'\tif ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_RENDER_SHOT" ) ) {\n',
	"\t// WW_IMPOSTOR_PREVIEW (lane IMPOSTORSHOW, 2026-09-19): the octahedral\n"
	"\t// impostor preview and its harness. Returns false and costs one\n"
	"\t// environment read when the variable is unset, which is every ordinary\n"
	"\t// session. It forces its own window size and prints the size it GOT --\n"
	"\t// the native_open.sh lesson, where a maximized persisted geometry\n"
	"\t// floored WW_RENDER_SIZE and the harness measured the machine.\n"
	"\twwImpostorPreviewStart( skope, skope->ogl, skope->viewportHeader );\n"
	"\n"
	'\tif ( !fname.isEmpty() && qEnvironmentVariableIsSet( "WW_RENDER_SHOT" ) ) {\n'
) )

def read_bytes( rel ):
	with open( os.path.join( ROOT, rel ), "rb" ) as f:
		return f.read()

def write_bytes( rel, data ):
	with open( os.path.join( ROOT, rel ), "wb" ) as f:
		f.write( data )

def main():
	failures = []
	planned = []
	for rel, anchor, repl in EDITS:
		path = os.path.join( ROOT, rel )
		if not os.path.exists( path ):
			failures.append( "%s: missing" % rel )
			continue
		data = read_bytes( rel )
		# MIXED LINE ENDINGS, PER REGION, NOT PER FILE. src/glview.cpp is
		# majority CRLF and the block this hooks into is LF -- a whole-file
		# heuristic reads that as CRLF and then matches nothing, which is
		# exactly the "anchor occurs 0 times" this script refuses on. So the
		# anchor is tried in BOTH spellings and the one that is actually there
		# decides the replacement's endings too. Matching in both at once would
		# be a bug of its own, so more than one total match also refuses.
		lf_a, lf_r = anchor.encode( "utf-8" ), repl.encode( "utf-8" )
		crlf_a = anchor.replace( "\n", "\r\n" ).encode( "utf-8" )
		crlf_r = repl.replace( "\n", "\r\n" ).encode( "utf-8" )
		n_lf, n_crlf = data.count( lf_a ), data.count( crlf_a )
		if n_lf and n_crlf:
			failures.append( "%s: anchor matches in BOTH LF (%d) and CRLF (%d) form" % ( rel, n_lf, n_crlf ) )
			continue
		crlf = n_crlf > 0
		ab, rb = ( crlf_a, crlf_r ) if crlf else ( lf_a, lf_r )
		if rb in data:
			planned.append( "%s: already hooked up, skipping" % rel )
			continue
		n = data.count( ab )
		if n != 1:
			failures.append( "%s: anchor occurs %d times, needs exactly 1: %r" % ( rel, n, anchor ) )
			continue
		planned.append( "%s: +%d bytes (%s)" % ( rel, len( rb ) - len( ab ), "CRLF" if crlf else "LF" ) )
		if not CHECK:
			write_bytes( rel, data.replace( ab, rb ) )

	for line in planned:
		print( "PLAN " if CHECK else "DONE ", line )
	for line in failures:
		print( "REFUSED", line )
	if failures:
		print( "\nNOTHING FURTHER WAS WRITTEN FOR THE REFUSED FILES."
				" Fix the anchor, do not loosen it." )
		return 1
	return 0

if __name__ == "__main__":
	sys.exit( main() )

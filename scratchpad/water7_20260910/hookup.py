#!/usr/bin/env python
"""Lane WATER7 / UI2 -- the hook-up, as a REFUSING script.

Everything this lane wrote lives in files it owns.  These six edits are what
FOUR FILES IT DOES NOT OWN need so the new code joins the build and the new tab
can actually be reached:

    E1  src/nifskope.h        the LeftColumnMode enum gains LeftWater = 3
    E2  src/nifskope_ui.cpp   setLeftColumnMode stops clamping 3 back to 0
    E3  src/nifskope_ui.cpp   wwBarRowButtonQss is defined beside the other
                              skin helpers
    E4  src/nifskope_ui.cpp   wwAlignBarRow takes `styleChildren` and applies
                              that sheet to each bar it aligns
    E5  src/nifskope_ui.cpp   the bar row gains the MENU BAR, behind the
                              UI/CompactTopBars key that is the way back
    E6  src/nifskope_ui.cpp   the WW_WATERUI_TEST harness is called
    E7  NifSkope.pro          src/wateruitest.cpp joins SOURCES

RULES THIS SCRIPT KEEPS (skill ww-anchored-hookup):
  * every ANCHOR is exact, carries the file's own line ending, and must match
    EXACTLY ONCE.  The count is printed for every one; the script never prints
    "ok" for an anchor, because an anchor that text was inserted AFTER still
    matches once and would read green on an already-applied file.  Decide
    "applied or not" from the MARKER strings (--check prints them) or from the
    file size against the predicted delta.
  * `--check` is the default and writes NOTHING.
  * `--apply` refuses unless every anchor still matches once, and then writes
    every file at once.
  * CR counts are asserted unchanged: all four files are LF-only (measured
    2026-09-10 with Python byte counts, CR 0 on every one), and none of the
    inserted text carries a CR.
  * ORDER: E7 (the .pro) must be applied WITH the rest, because
    src/wateruitest.cpp calls wwBarRowButtonQss, which E3 defines.  The script
    applies all seven or none, so the order cannot go wrong.

AFTER APPLYING:
    qmake NifSkope.pro     (SOURCES changed -- E7)
    make -j2
  No DEFINES or CXXFLAGS line is touched by any of these edits, so the
  changed-flag staleness trap of lane BUILD9 does not apply here.  A new
  translation unit and four edits inside one existing one are all `make` needs
  mtimes for.
"""

import os
import sys

ROOT = os.path.dirname( os.path.dirname( os.path.dirname( os.path.abspath( __file__ ) ) ) )

MARKER = "lane WATER7"

# ---------------------------------------------------------------- E1 --------

E1_ANCHOR = (
	"\tenum LeftColumnMode { LeftBlocks = 0, LeftNifs = 1, LeftHeader = 2 };\n"
)

E1_TEXT = (
	"\t/* LeftWater = 3 is the water page (lane WATER7).\n"
	"\t *\n"
	"\t * bungo, 2026-09-10: the water tool is a TAB of the LOD Generation\n"
	"\t * workspace, in this same strip -- Header | Blocks | Files | Water.\n"
	"\t * The mode number IS the stack page index (setLeftColumnMode does\n"
	"\t * leftColumnStack->setCurrentIndex( int( mode ) )), and the page is\n"
	"\t * added by src/watermarkpanel.cpp, which asserts it landed at 3 and\n"
	"\t * refuses in words if it did not. */\n"
	"\tenum LeftColumnMode { LeftBlocks = 0, LeftNifs = 1, LeftHeader = 2, LeftWater = 3 };\n"
)

# ---------------------------------------------------------------- E2 --------

E2_ANCHOR = (
	"\tif ( mode < LeftBlocks || mode > LeftHeader )\n"
	"\t\tmode = LeftBlocks;\n"
)

E2_TEXT = (
	"\t/* lane WATER7: LeftWater, not LeftHeader. Clamping 3 back to 0 here is\n"
	"\t * what would make the Water tab bounce to Blocks the moment it is\n"
	"\t * clicked -- gate T4 of tests/spells/water_ui.sh is named after exactly\n"
	"\t * that failure, so a resume that skipped this edit reads it by name. */\n"
	"\tif ( mode < LeftBlocks || mode > LeftWater )\n"
	"\t\tmode = LeftBlocks;\n"
)

# ---------------------------------------------------------------- E3 --------

E3_ANCHOR = (
	"static int wwBarRow = 0;\n"
	"\n"
	"int wwBarRowHeight()\n"
	"{\n"
	"\treturn wwBarRow;\n"
	"}\n"
)

E3_TEXT = (
	"\n"
	"/* ...AND THE BUTTONS IN THE ROW TAKE THE ROW'S HEIGHT AND PADDING TOO\n"
	" * (lane WATER7; bungo, 2026-09-10, on a screenshot of the aligned strip:\n"
	" * \"compact these vertically like this, the top bar and the buttons\").\n"
	" *\n"
	" * wwAlignBarRow made the BARS agree with one another. What is inside them\n"
	" * was still set by whatever res/style.qss says for the widget class --\n"
	" * QMenuBar::item at 4px 8px, QToolBar QToolButton at 2px 4px -- neither of\n"
	" * which knows the row height, so the buttons sat in the middle of a band\n"
	" * they did not fill. This is the one place that arithmetic is done.\n"
	" *\n"
	" * The vertical padding is HALF the slack the row has over a button's text\n"
	" * line, floored at 2, so the number follows the row rather than being typed\n"
	" * beside it; the horizontal paddings keep the sheet's own values, because\n"
	" * the complaint was vertical. min-height is the row minus twice the padding\n"
	" * and the 2 px of border and margin the sheet already spends.\n"
	" *\n"
	" * wwAlignBarRow appends the result to each BAR, not to the application, so\n"
	" * a rule here reaches that bar's own children and nothing else in the\n"
	" * window. */\n"
	"bool wwCompactTopBars()\n"
	"{\n"
	"\t/* THE WAY BACK, exact at its off value (CONSTITUTION 7). ONE reader, so\n"
	"\t * the sheet below and the call site that decides whether the menu bar\n"
	"\t * joins the row can never disagree about it. */\n"
	"\treturn QSettings().value( QStringLiteral( \"UI/CompactTopBars\" ), true ).toBool();\n"
	"}\n"
	"\n"
	"QString wwBarRowButtonQss( int rowHeight )\n"
	"{\n"
	"\tif ( rowHeight <= 0 || !wwCompactTopBars() )\n"
	"\t\treturn QString();\n"
	"\tconst int inner = qMax( 12, rowHeight - 4 );\n"
	"\tconst int pad = qMax( 2, ( rowHeight - 18 ) / 2 );\n"
	"\treturn QStringLiteral(\n"
	"\t\t\"QMenuBar::item { min-height: %1px; padding: %2px 8px; }\"\n"
	"\t\t\"QToolButton { min-height: %1px; padding: %2px 4px; }\" )\n"
	"\t\t.arg( inner ).arg( pad );\n"
	"}\n"
)

# ---------------------------------------------------------------- E4 --------

E4_ANCHOR = (
	"void wwAlignBarRow( const QList<QWidget *> & bars )\n"
	"{\n"
	"\tint h = 0;\n"
	"\tfor ( QWidget * w : bars ) {\n"
	"\t\tif ( w )\n"
	"\t\t\th = qMax( h, qMax( w->sizeHint().height(), w->minimumSizeHint().height() ) );\n"
	"\t}\n"
	"\tif ( h <= 0 )\n"
	"\t\treturn;\n"
	"\twwBarRow = h;\n"
	"\tfor ( QWidget * w : bars ) {\n"
	"\t\tif ( !w )\n"
	"\t\t\tcontinue;\n"
	"\t\t// the property is what a sheet or a gate can find the family by\n"
	"\t\tw->setProperty( \"wwBarRow\", true );\n"
	"\t\tw->setMinimumHeight( h );\n"
	"\t\tw->setMaximumHeight( h );\n"
	"\t}\n"
	"}\n"
)

E4_TEXT = (
	"void wwAlignBarRow( const QList<QWidget *> & bars )\n"
	"{\n"
	"\tint h = 0;\n"
	"\tfor ( QWidget * w : bars ) {\n"
	"\t\tif ( w )\n"
	"\t\t\th = qMax( h, qMax( w->sizeHint().height(), w->minimumSizeHint().height() ) );\n"
	"\t}\n"
	"\tif ( h <= 0 )\n"
	"\t\treturn;\n"
	"\twwBarRow = h;\n"
	"\t// lane WATER7: computed ONCE, outside the loop, so every bar in the row\n"
	"\t// is given the same string and no second number can appear at a call site\n"
	"\t// (empty when UI/CompactTopBars is off -- that is the way back, and it\n"
	"\t// is exact: nothing is appended and no bar's own sheet is touched)\n"
	"\tconst QString childSheet = wwBarRowButtonQss( h );\n"
	"\tfor ( QWidget * w : bars ) {\n"
	"\t\tif ( !w )\n"
	"\t\t\tcontinue;\n"
	"\t\t// the property is what a sheet or a gate can find the family by\n"
	"\t\tw->setProperty( \"wwBarRow\", true );\n"
	"\t\tw->setMinimumHeight( h );\n"
	"\t\tw->setMaximumHeight( h );\n"
	"\t\t/* APPENDED, never assigned: a bar may already carry a per-widget sheet\n"
	"\t\t * (the viewport header does), and replacing it would take that look\n"
	"\t\t * away while fixing the height. */\n"
	"\t\tif ( !childSheet.isEmpty() )\n"
	"\t\t\tw->setStyleSheet( w->styleSheet() + childSheet );\n"
	"\t}\n"
	"}\n"
)

# ---------------------------------------------------------------- E5 --------

E5_ANCHOR = (
	"\twwAlignBarRow( { ui->tFile, ui->tLOD, ui->tView, viewportHeader, leftColumnSelector } );\n"
)

E5_TEXT = (
	"\t/* THE MENU BAR IS PART OF THE ROW (lane WATER7).\n"
	"\t *\n"
	"\t * bungo, 2026-09-10, on a screenshot of the aligned strip beside the\n"
	"\t * Object Mode row: \"compact these vertically like this, the top bar and\n"
	"\t * the buttons\". BUILD9 aligned the toolbars, the viewport header and the\n"
	"\t * dock strip with one another and left the menu row out of it entirely,\n"
	"\t * so the top of the window was still two rows of different heights.\n"
	"\t *\n"
	"\t * THE WAY BACK, exact at its off value (CONSTITUTION 7): with\n"
	"\t * UI/CompactTopBars set false the menu bar is not in the row and no bar's\n"
	"\t * children are restyled, which is the 2026-09-10 BUILD9 behaviour, and\n"
	"\t * the gate pins that the off value changes nothing. wwCompactTopBars()\n"
	"\t * is the ONE reader of that key -- wwBarRowButtonQss asks it too. */\n"
	"\tQList<QWidget *> wwBarRowBars =\n"
	"\t\t{ ui->tFile, ui->tLOD, ui->tView, viewportHeader, leftColumnSelector };\n"
	"\tif ( wwCompactTopBars() )\n"
	"\t\twwBarRowBars.prepend( ui->menubar );\n"
	"\twwAlignBarRow( wwBarRowBars );\n"
)

# ---------------------------------------------------------------- E6 --------

E6_ANCHOR = (
	"\t{\n"
	"\t\textern void wwUiAlignHarness( NifSkope * );\n"
	"\t\twwUiAlignHarness( skope );\n"
	"\t}\n"
)

E6_TEXT = (
	"\t{\n"
	"\t\t// lane WATER7: the Water tab and the compact bar row\n"
	"\t\textern void wwWaterUiHarness( NifSkope * );\n"
	"\t\twwWaterUiHarness( skope );\n"
	"\t}\n"
)

# ---------------------------------------------------------------- E7 --------

# TAB-indented, and it ends in a backslash: NifSkope.pro's SOURCES list uses a
# real tab, and a one-off count of this anchor typed into a Bash heredoc would
# HALVE the backslash and report 0 on a file that is perfectly fine
# (ww-anchored-hookup, section 3a). This file is written with the Write tool
# for that reason, and the repr is printed below before the count.
E7_ANCHOR = "\tsrc/uialigntest.cpp \\\n"

E7_TEXT = "\tsrc/wateruitest.cpp \\\n"

EDITS = [
	( "src/nifskope.h", "replace", E1_ANCHOR, E1_TEXT ),
	( "src/nifskope_ui.cpp", "replace", E2_ANCHOR, E2_TEXT ),
	( "src/nifskope_ui.cpp", "after", E3_ANCHOR, E3_TEXT ),
	( "src/nifskope_ui.cpp", "replace", E4_ANCHOR, E4_TEXT ),
	( "src/nifskope_ui.cpp", "replace", E5_ANCHOR, E5_TEXT ),
	( "src/nifskope_ui.cpp", "after", E6_ANCHOR, E6_TEXT ),
	( "NifSkope.pro", "after", E7_ANCHOR, E7_TEXT ),
]

# Markers a resume reads the APPLIED state from, never the anchor.
#
# The expected COUNT is DERIVED from the table above, never typed: lane BUILD9
# paid for a resume whose hand-written marker counts said 1 and 7 where the
# truth was 1 and 6, and a wrong prediction in a resume is worse than none.
MARKER_NAMES = [
	( "src/nifskope.h", "LeftWater = 3" ),
	( "src/nifskope_ui.cpp", "mode > LeftWater" ),
	( "src/nifskope_ui.cpp", "wwBarRowButtonQss" ),
	( "src/nifskope_ui.cpp", "UI/CompactTopBars" ),
	( "src/nifskope_ui.cpp", "wwWaterUiHarness" ),
	( "NifSkope.pro", "src/wateruitest.cpp" ),
]


def marker_expectations():
	out = []
	for path, marker in MARKER_NAMES:
		want = 0
		for p, mode, anchor, text in EDITS:
			if p != path:
				continue
			want += text.count( marker )
			if mode == "after":
				want += anchor.count( marker )   # the anchor survives an insert
		out.append( ( path, marker, want ) )
	return out


def main():
	apply = "--apply" in sys.argv
	print( "lane WATER7 hook-up -- %s" % ( "APPLY" if apply else "check only (writes nothing)" ) )
	print( "root: %s" % ROOT )

	files = {}
	ok = True
	for path, mode, anchor, text in EDITS:
		full = os.path.join( ROOT, path )
		if full not in files:
			with open( full, "rb" ) as f:
				files[full] = f.read()
		blob = files[full]
		a = anchor.encode( "utf-8" )
		n = blob.count( a )
		print( "  %-24s %-7s anchor %3d bytes  matches %d%s"
			% ( path, mode, len( a ), n,
				( "   repr " + repr( anchor[:48] ) ) if "\\" in anchor else "" ) )
		if n != 1:
			ok = False

	print( "  --- markers (this is what says APPLIED, never the anchor) ---" )
	for path, marker, want in marker_expectations():
		full = os.path.join( ROOT, path )
		if full not in files:
			with open( full, "rb" ) as f:
				files[full] = f.read()
		have = files[full].count( marker.encode( "utf-8" ) )
		print( "  %-24s %-24s %d (applied = %d)" % ( path, marker, have, want ) )

	print( "  --- line endings ---" )
	for full in sorted( files ):
		blob = files[full]
		print( "  %-40s bytes %8d  CR %d  LF %d"
			% ( os.path.relpath( full, ROOT ), len( blob ),
				blob.count( b"\r" ), blob.count( b"\n" ) ) )

	if not ok:
		print( "REFUSED: not every anchor matches exactly once" )
		return 1
	if not apply:
		print( "every anchor matches exactly once; nothing written (pass --apply)" )
		return 0

	out = dict( files )
	for path, mode, anchor, text in EDITS:
		full = os.path.join( ROOT, path )
		a = anchor.encode( "utf-8" )
		t = text.encode( "utf-8" )
		if a.count( b"\r" ) or t.count( b"\r" ):
			print( "REFUSED: %s carries a CR and these files are LF-only" % path )
			return 1
		blob = out[full]
		if blob.count( a ) != 1:
			print( "REFUSED: %s's anchor stopped matching once mid-run" % path )
			return 1
		out[full] = blob.replace( a, t if mode == "replace" else a + t, 1 )

	for full, blob in out.items():
		before = files[full]
		if blob.count( b"\r" ) != before.count( b"\r" ):
			print( "REFUSED: %s changed its CR count" % full )
			return 1
		with open( full, "wb" ) as f:
			f.write( blob )
		print( "  wrote %-40s %d -> %d bytes"
			% ( os.path.relpath( full, ROOT ), len( before ), len( blob ) ) )
	print( "applied" )
	return 0


if __name__ == "__main__":
	sys.exit( main() )

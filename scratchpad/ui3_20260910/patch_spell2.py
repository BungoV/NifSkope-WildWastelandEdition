#!/usr/bin/env python
"""UI3, round 2: the menu-arrow gate joins water_ui.sh's R4 list and header."""
import io
import os

ROOT = os.path.abspath( os.path.join( os.path.dirname( __file__ ), "..", ".." ) )
P = os.path.join( ROOT, "tests/spells/water_ui.sh" )

SUBS = [
( "#       horizontal, and the bar's own box taken away -- which is what puts a\n"
  "#       button at y 0 instead of y 4",

  "#       horizontal, and the bar's own box taken away -- which is what puts a\n"
  "#       button at y 0 instead of y 4.  It also pins the menu ARROW to the\n"
  "#       middle of the button: a QToolButton's default indicator sits in the\n"
  "#       bottom-right corner, invisible while the button is only as tall as\n"
  "#       its text and obvious the moment it is the row's height (UI3's first\n"
  "#       build put the viewport header's arrows a row below their glyphs)." ),

( "\t\"(R4) the skin states the height\" \"(R4) ...and nothing horizontal\" \\\n",
  "\t\"(R4) the skin states the height\" \"(R4) ...and nothing horizontal\" \\\n"
  "\t\"(R4) ...and the menu arrow is centred\" \\\n" ),

( "# check 1 -- 35 with both pictures, 32 with neither.  30 is two below the\n"
  "# smaller of those, so losing a check still goes red either way.",

  "# check 1.  Measured on the 18:21:46 exe: 36 with both pictures, so 33 with\n"
  "# neither, and the menu-arrow gate of the second round makes it 37 / 34.\n"
  "# 30 is three below the smaller of those, so losing a check still goes red\n"
  "# either way." ),
]


def main():
    with io.open( P, "rb" ) as f:
        raw = f.read()
    cr = raw.count( b"\r" )
    s = raw.decode( "utf-8" )
    for a, b in SUBS:
        n = s.count( a )
        print( "anchor %-40s matches %d" % ( repr( a[:38] ), n ) )
        assert n == 1, "anchor must match once"
        s = s.replace( a, b )
    out = s.encode( "utf-8" )
    assert out.count( b"\r" ) == cr, "CR count moved"
    with io.open( P, "wb" ) as f:
        f.write( out )
    print( "written: %d -> %d bytes, CR %d unchanged" % ( len( raw ), len( out ), cr ) )


if __name__ == "__main__":
    main()

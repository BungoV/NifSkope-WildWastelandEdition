#!/usr/bin/env python
"""UI3: tighten tests/spells/water_ui.sh -- R3 to 1 px, R4 to the new sheet,
the check-count floor re-derived. Written with the Write tool, never a
heredoc (ww-anchored-hookup 3a: a heredoc halves the backslashes this file's
line continuations are made of)."""
import io
import os

ROOT = os.path.abspath( os.path.join( os.path.dirname( __file__ ), "..", ".." ) )
P = os.path.join( ROOT, "tests/spells/water_ui.sh" )

SUBS = [
( "#   R3  every button in those bars takes the row's height and agrees with its\n"
  "#       neighbours within 1 px  (floor: at least 4 buttons found)\n"
  "#   R4  the padding is stated ONCE, by wwBarRowButtonQss, from the row height",

  "#   R3  EVERY tool button in tFile / tLOD / tView is the row's own height\n"
  "#       within 1 px, and agrees with its neighbours within 1 px.  BUILD12\n"
  "#       shipped 39 px buttons in a 35 px row and this gate passed them,\n"
  "#       because it allowed 8 px where this header already promised 1.  It is\n"
  "#       1 px, and every button is printed by name, not just the extremes.\n"
  "#       (floors: at least 4 buttons found; the calibration measured a real\n"
  "#        style overhead rather than falling back; and THE SAME TEST GOES RED,\n"
  "#        live in the same run, when BUILD12's own arithmetic is appended over\n"
  "#        the shipped sheet -- then green again when it is taken away, which is\n"
  "#        also what makes the picture below the shipped state)\n"
  "#   R4  the row's box is stated ONCE by the skin, from the row's own number:\n"
  "#       one min-height and one pair of vertical paddings per selector, nothing\n"
  "#       horizontal, and the bar's own box taken away -- which is what puts a\n"
  "#       button at y 0 instead of y 4" ),

( "\t\"(R2 floor)\" \"(R3) every button in the row\" \"(R4) the skin emits one padding\" \\\n"
  "\t\"(R4 floor)\" \"(R5) the search row\"; do",

  "\t\"(R2 floor)\" \"(R3) every button in the row\" \"(R3) ...and the calibration\" \\\n"
  "\t\"(R3 floor) the SAME test goes red\" \"(R3 floor) ...and taking it away\" \\\n"
  "\t\"(R4) the skin states the height\" \"(R4) ...and nothing horizontal\" \\\n"
  "\t\"(R4) the bar's own box is taken away\" \"(R4) ...and the bars actually carry it\" \\\n"
  "\t\"(R4 floor)\" \"(R5) the search row\"; do" ),

( "# 20 named gates above plus the floors and the two picture checks; 24 is one\n"
  "# below what a green run with both SHOT variables set produces, so losing a\n"
  "# check still goes red.  Arithmetic, not a wish: T-group 15, R-group 13, the\n"
  "# document check 1 -- 29 with the pictures, 26 without them.",

  "# The named gates above plus the floors and the two picture checks.\n"
  "# Arithmetic, not a wish, re-counted by lane UI3: T-group 15, R-group 19 (1\n"
  "# row-height floor, 2 for R1, 2 for R2, 6 for R3 including BOTH halves of its\n"
  "# live floor, 5 for R4, 1 for R5, 2 for the top-strip grab), the document\n"
  "# check 1 -- 35 with both pictures, 32 with neither.  30 is two below the\n"
  "# smaller of those, so losing a check still goes red either way." ),

( "echo \"checks run: ${COUNT:-none} (floor 24)\"",
  "echo \"checks run: ${COUNT:-none} (floor 30)\"" ),

( "\t*) [ \"$COUNT\" -ge 24 ] || { echo \"FAIL: only $COUNT checks ran, floor is 24\"; fails=$((fails+1)); } ;;",
  "\t*) [ \"$COUNT\" -ge 30 ] || { echo \"FAIL: only $COUNT checks ran, floor is 30\"; fails=$((fails+1)); } ;;" ),
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

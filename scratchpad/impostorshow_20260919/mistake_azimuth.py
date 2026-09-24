#!/usr/bin/env python
# Splice one entry into MISTAKES.md, CRLF preserved, refusing on anything
# unexpected. Byte counts only -- grep lies about line endings.

import sys

PATH = "MISTAKES.md"
ANCHOR = b"Newest at the top.\r\n\r\n"

ENTRY = (
	"## 2026-09-19 -- the octahedral bake photographed every frame from the "
	"opposite side for months, and every gate passed\r\n"
	"\r\n"
	"The impostor bake's camera was `rotZ = 90 - azimuth` where the derivation "
	"gives\r\n"
	"`270 - azimuth`. With `Matrix::fromEuler(rx, 0, rz)` the camera SITS at\r\n"
	"`row2 = (sinX sinZ, sinX cosZ, cosX)`; `rx = -90 + elev` already fixes the\r\n"
	"elevation, so wanting `row2 = d` requires `sinZ = -cos(azim)` and\r\n"
	"`cosZ = -sin(azim)` -- which `270 - azim` gives exactly and `90 - azim`\r\n"
	"negates in x and y. Frame (i,j) therefore held the view from the FAR SIDE of\r\n"
	"the object, with a correct elevation. bungo saw it in a diagram and ruled\r\n"
	"\"Okay, fix the 180 issue\"; the repair is one line and it invalidates every\r\n"
	"impostor set ever baked.\r\n"
	"\r\n"
	"- **Why no gate caught it.** Everything ever baked was a TREE, and a tree is\r\n"
	"  near-symmetric about its own axis: the sheet from azimuth 45 and the sheet\r\n"
	"  from azimuth 225 look alike to every check that measures coverage, extents,\r\n"
	"  mip depth, gutters or file shape. The bake gates were thorough about the\r\n"
	"  things a symmetric subject can express and silent about orientation,\r\n"
	"  because orientation is the one property that subject cannot express. **A\r\n"
	"  gate's SUBJECT is part of the gate.** When the quantity under test is a\r\n"
	"  direction, a symmetric fixture is not a cheap fixture, it is no fixture.\r\n"
	"- **What replaces it.** `impostor_draw.sh` steps 7 and 8: the card's\r\n"
	"  silhouette against the mesh from the frame's spec direction AND from the\r\n"
	"  opposite one, on an asymmetric subject, with the old formula forced as the\r\n"
	"  red control. Step 8 passing as well as step 7 is reported as a FAILURE\r\n"
	"  naming the cause -- \"the subject is too symmetric to carry the argument\" --\r\n"
	"  rather than as two green rows about nothing.\r\n"
	"- **The one-liner was right, and was still proved.** The repair arrived as\r\n"
	"  `rz = 270 - azim` with \"prove it, do not trust the one-liner\" attached. It\r\n"
	"  was derived from `Matrix::fromEuler` before it was typed. A correct\r\n"
	"  instruction taken on trust is indistinguishable, afterwards, from a wrong\r\n"
	"  one taken on trust.\r\n"
	"- **Absence had to be given a meaning.** The `.lodm` gained a `conv` token\r\n"
	"  (`spec1`). Old sets carry none, so ABSENCE means \"baked before the repair\",\r\n"
	"  not \"unknown\" -- the same reading the `projection` and `coverage` keys\r\n"
	"  already use in this format. A new key whose absence means \"unknown\" would\r\n"
	"  have left every existing set undiagnosable.\r\n"
	"\r\n"
).encode( "utf-8" )


def main():
	data = open( PATH, "rb" ).read()
	crlf, lf = data.count( b"\r\n" ), data.count( b"\n" ) - data.count( b"\r\n" )
	if lf:
		sys.stderr.write( "REFUSED: %d LF-only lines in %s; this splice assumes CRLF\n" % ( lf, PATH ) )
		return 2
	if data.count( ANCHOR ) != 1:
		sys.stderr.write( "REFUSED: the anchor occurs %d times\n" % data.count( ANCHOR ) )
		return 2
	if ENTRY[:40] in data:
		sys.stderr.write( "already spliced; nothing to do\n" )
		return 0
	out = data.replace( ANCHOR, ANCHOR + ENTRY, 1 )
	open( PATH, "wb" ).write( out )
	after = open( PATH, "rb" ).read()
	acrlf, alf = after.count( b"\r\n" ), after.count( b"\n" ) - after.count( b"\r\n" )
	print( "spliced %d bytes; CRLF %d -> %d, LF-only %d -> %d"
			% ( len( ENTRY ), crlf, acrlf, lf, alf ) )
	return 0 if alf == 0 else 1


if __name__ == "__main__":
	sys.exit( main() )

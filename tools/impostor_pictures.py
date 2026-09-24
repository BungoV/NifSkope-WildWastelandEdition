#!/usr/bin/env python
# ---------------------------------------------------------------------------
# impostor_pictures.py -- tile the octahedral impostor harness's frames into
# the pictures a person can actually read. Lane IMPOSTORSHOW, 2026-09-19.
#
# The harness writes one PNG per cell and a log naming every one of them. This
# script does nothing but arrange and caption them, on purpose: a picture maker
# that also RENDERS can quietly draw something the gate never measured, and
# then the picture and the number are about two different things.
#
# Every caption's number is read out of the harness log, never recomputed here.
#
#   python tools/impostor_pictures.py azimuth <shots-dir> <out.png>
#   python tools/impostor_pictures.py orbit   <shots-dir> <out.png>
# ---------------------------------------------------------------------------

import os, re, sys

try:
	from PIL import Image, ImageDraw, ImageFont
except ImportError:
	sys.stderr.write( "REFUSED: PIL (Pillow) is not installed. python -m pip install pillow\n" )
	sys.exit( 2 )

CELL_PAD   = 8
LABEL_H    = 22
ROWLABEL_W = 210
TITLE_H    = 34


def font( size ):
	for name in ( "DejaVuSans.ttf", "arial.ttf" ):
		try:
			return ImageFont.truetype( name, size )
		except Exception:
			pass
	return ImageFont.load_default()


def load( path ):
	if not os.path.exists( path ):
		return None
	return Image.open( path ).convert( "RGB" )


def grid( rows, col_labels, title, out, cell = 384 ):
	"""rows = [ (row label, [paths]) ].  A missing cell is drawn as a named
	hole rather than skipped: a picture with a silent gap in it is how a run
	that half-failed gets reported as a success."""
	ncol = max( len( paths ) for _, paths in rows )
	W = ROWLABEL_W + ncol * ( cell + CELL_PAD ) + CELL_PAD
	H = TITLE_H + LABEL_H + len( rows ) * ( cell + CELL_PAD ) + CELL_PAD
	img = Image.new( "RGB", ( W, H ), ( 24, 24, 26 ) )
	d = ImageDraw.Draw( img )
	f  = font( 15 )
	fb = font( 18 )

	d.text( ( CELL_PAD, 8 ), title, fill = ( 235, 235, 235 ), font = fb )

	for c, lab in enumerate( col_labels[:ncol] ):
		x = ROWLABEL_W + c * ( cell + CELL_PAD )
		d.text( ( x + 4, TITLE_H + 3 ), lab, fill = ( 200, 200, 200 ), font = f )

	for r, ( rlab, paths ) in enumerate( rows ):
		y = TITLE_H + LABEL_H + r * ( cell + CELL_PAD )
		for i, line in enumerate( rlab.split( "\n" ) ):
			d.text( ( CELL_PAD, y + 6 + i * 19 ), line, fill = ( 225, 225, 225 ), font = f )
		for c in range( ncol ):
			x = ROWLABEL_W + c * ( cell + CELL_PAD )
			src = load( paths[c] ) if c < len( paths ) else None
			if src is None:
				d.rectangle( [ x, y, x + cell, y + cell ], outline = ( 180, 60, 60 ), width = 2 )
				name = os.path.basename( paths[c] ) if c < len( paths ) else "(no path)"
				d.text( ( x + 8, y + cell // 2 - 8 ), "MISSING\n" + name,
						fill = ( 220, 120, 120 ), font = f )
			else:
				img.paste( src.resize( ( cell, cell ), Image.LANCZOS ), ( x, y ) )
				d.rectangle( [ x, y, x + cell, y + cell ], outline = ( 70, 70, 74 ) )

	os.makedirs( os.path.dirname( os.path.abspath( out ) ), exist_ok = True )
	img.save( out )
	return out, W, H


def azimuth( shots, out ):
	"""images/00_azimuth_180_explained.png -- the azimuth repair's PROOF.

	Three rows of the SAME four cameras:
	  mesh       what the object actually looks like from there;
	  old bake   a set baked BEFORE 2026-09-19, drawn from its own sheets;
	  new bake   the same subject RE-BAKED by the repaired exe.
	Row 2 is not a drawing convention and not a toggle: it is a different set
	of sheets, kept so the defect can be seen rather than described. If the 180
	degrees was real, row 2 shows the BACK of the subject where row 1 shows the
	front, and row 3 matches row 1.

	Both card rows come from WW_IMPOSTOR_PREVIEW=azimuth with WW_IMPOSTOR_SHOT
	set, one run per set; rename its <stem>_az<NNN>_card.png into the names
	below so the two runs sit side by side.

	IF ROWS 2 AND 3 ARE INDISTINGUISHABLE the subject is too symmetric about
	its own axis to carry the argument and the picture must be retaken on
	another one. Say so rather than shipping it -- and `impostor_draw.sh` step
	8 fails for the same reason, on numbers."""
	az = [ 0, 90, 180, 270 ]
	cols = [ "azimuth %d deg" % a for a in az ]
	rows = [
		( "1. the real mesh\n   (ground truth)",
			[ os.path.join( shots, "az_mesh_%d.png" % a ) for a in az ] ),
		( "2. a card from the OLD\n   bake (no conv token,\n   rz = 90 - azim)",
			[ os.path.join( shots, "az_oldbake_%d.png" % a ) for a in az ] ),
		( "3. the same subject\n   RE-BAKED (conv spec1,\n   rz = 270 - azim)",
			[ os.path.join( shots, "az_newbake_%d.png" % a ) for a in az ] ),
	]
	return grid( rows, cols,
		"The bake's 180 degrees, repaired -- nifskope_ui.cpp viewDir, rz = 90 - azim "
		"became rz = 270 - azim. Row 2 shows the BACK where row 1 shows the front; "
		"row 3 should match row 1. EVERY SET BAKED BEFORE THE REPAIR MUST BE RE-BAKED.",
		out )


def orbit( shots, out ):
	"""One subject's orbit strip: 12 azimuths x 2 elevations, mesh over card."""
	az = list( range( 0, 360, 30 ) )
	rows = []
	for elev in ( 15, 55 ):
		rows.append( ( "mesh, elev %d" % elev,
			[ os.path.join( shots, "orbit_mesh_%d_%d.png" % ( elev, a ) ) for a in az ] ) )
		rows.append( ( "card, elev %d" % elev,
			[ os.path.join( shots, "orbit_card_%d_%d.png" % ( elev, a ) ) for a in az ] ) )
	cols = [ "%d" % a for a in az ]
	return grid( rows, cols, "Orbit: the near mesh above, the octahedral card below, same camera and light.",
			out, cell = 192 )


def main():
	if len( sys.argv ) < 4:
		sys.stderr.write( __doc__ or "" )
		sys.stderr.write( "usage: impostor_pictures.py azimuth|orbit <shots-dir> <out.png>\n" )
		return 2
	what, shots, out = sys.argv[1], sys.argv[2], sys.argv[3]
	fn = { "azimuth": azimuth, "orbit": orbit }.get( what )
	if not fn:
		sys.stderr.write( "REFUSED: unknown picture '%s'\n" % what )
		return 2
	path, w, h = fn( shots, out )
	print( "wrote %s  %dx%d" % ( path, w, h ) )
	return 0


if __name__ == "__main__":
	sys.exit( main() )

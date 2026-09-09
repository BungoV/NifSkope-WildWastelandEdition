#!/usr/bin/env python
"""Pixel diff two renders taken from the SAME pinned camera.

Reports, over the pixels where EITHER image has terrain (background is the
renderer's flat clear colour, so a pixel is "terrain" when it differs from the
image's own dominant border colour):

    covered      how many pixels are terrain in either image
    identical    how many of those are byte-identical
    maxdelta     the largest per-channel absolute difference
    meandelta    the mean per-channel absolute difference over covered pixels
    over8        how many covered pixels differ by more than 8 in any channel

Usage: python pixdiff.py A.png B.png [out_diff.png]
"""
import sys
import numpy as np
from PIL import Image


def load( p ):
	im = Image.open( p ).convert( "RGB" )
	return np.asarray( im ).astype( np.int16 ), im.size


def background( a ):
	# the clear colour is whatever the 1px border is mostly made of
	border = np.concatenate( [ a[0, :, :], a[-1, :, :], a[:, 0, :], a[:, -1, :] ] )
	cols, counts = np.unique( border.reshape( -1, 3 ), axis=0, return_counts=True )
	return cols[ counts.argmax() ]


def main():
	a, sa = load( sys.argv[1] )
	b, sb = load( sys.argv[2] )
	if sa != sb:
		print( "SIZE MISMATCH %s vs %s" % ( sa, sb ) )
		return 2
	bg = background( a )
	mask = ( np.abs( a - bg ).max( axis=2 ) > 6 ) | ( np.abs( b - bg ).max( axis=2 ) > 6 )
	d = np.abs( a - b )
	covered = int( mask.sum() )
	if covered == 0:
		print( "NO TERRAIN PIXELS — background %s" % ( bg, ) )
		return 3
	dm = d[ mask ]
	ident = int( ( dm.max( axis=1 ) == 0 ).sum() )
	print( "size %dx%d  background %s" % ( sa[0], sa[1], tuple( int( x ) for x in bg ) ) )
	print( "covered %d  identical %d (%.3f%%)  maxdelta %d  meandelta %.4f  over8 %d (%.4f%%)"
		% ( covered, ident, 100.0 * ident / covered, int( dm.max() ), float( dm.mean() ),
			int( ( dm.max( axis=1 ) > 8 ).sum() ), 100.0 * ( dm.max( axis=1 ) > 8 ).sum() / covered ) )
	if len( sys.argv ) > 3:
		amp = np.clip( d.astype( np.int32 ) * 8, 0, 255 ).astype( np.uint8 )
		Image.fromarray( amp ).save( sys.argv[3] )
		print( "wrote %s (differences amplified x8)" % sys.argv[3] )
	return 0


if __name__ == "__main__":
	sys.exit( main() )

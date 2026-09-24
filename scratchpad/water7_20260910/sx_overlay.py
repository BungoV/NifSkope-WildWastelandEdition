#!/usr/bin/env python
"""Write the hook-up's OUTPUT into a scratch overlay so it can be syntax-checked
without touching the shared tree.

The lane may not apply hookup.py until the build slot is free, but the text it
would insert still has to be proved to COMPILE -- otherwise the director's one
build of the session dies on a typo in a string this lane wrote. So: take
hookup.py's own EDITS table (never a retyped copy of it), apply it to copies of
the four files under scratchpad/water7_20260910/sx/, and syntax-check the copy
with that directory FIRST on the include path, so `#include "nifskope.h"`
resolves to the patched header and not to the one still on disk.

    python scratchpad/water7_20260910/sx_overlay.py
    bash sx_WATER7.sh -Iscratchpad/water7_20260910/sx \\
        scratchpad/water7_20260910/sx/nifskope_ui.cpp

It proves the inserted text compiles in place. It proves nothing about linking
or about behaviour, and the real build still has to run.
"""

import os
import sys

HERE = os.path.dirname( os.path.abspath( __file__ ) )
ROOT = os.path.dirname( os.path.dirname( HERE ) )
sys.path.insert( 0, HERE )

from hookup import EDITS                                       # noqa: E402

OUT = os.path.join( HERE, "sx" )


def main():
	if not os.path.isdir( OUT ):
		os.makedirs( OUT )
	blobs = {}
	for path, mode, anchor, text in EDITS:
		if path not in blobs:
			with open( os.path.join( ROOT, path ), "rb" ) as f:
				blobs[path] = f.read()
		a = anchor.encode( "utf-8" )
		t = text.encode( "utf-8" )
		if blobs[path].count( a ) != 1:
			print( "REFUSED: %s's anchor does not match once" % path )
			return 1
		blobs[path] = blobs[path].replace( a, t if mode == "replace" else a + t, 1 )
	for path, blob in blobs.items():
		dst = os.path.join( OUT, os.path.basename( path ) )
		with open( dst, "wb" ) as f:
			f.write( blob )
		print( "wrote %s  %d bytes  CR %d" % ( dst, len( blob ), blob.count( b"\r" ) ) )
	return 0


if __name__ == "__main__":
	sys.exit( main() )

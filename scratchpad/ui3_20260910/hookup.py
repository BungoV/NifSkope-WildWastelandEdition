#!/usr/bin/env python
"""UI3 hook-up: the ONE edit lane UI3 makes to src/nifskope_ui.cpp.

The skin's bar-row functions live in that file, not in a wwskin.cpp -- there
is no wwskin.cpp -- so the fix bungo asked for ("compact these vertically
like this, the top bar and the buttons") cannot be made anywhere else. Lane
UI3 owns src/wwskin.h, res/style.qss, src/wateruitest.cpp and
tests/spells/water_ui.sh directly, and reaches nifskope_ui.cpp only here.

ONE edit, "replace", over one contiguous region: the comment block, the
`wwCompactTopBars` / `wwBarRowButtonQss` / `wwAlignBarRow` definitions.
Nothing outside that region is authorised, and the script refuses rather than
guessing.

    python scratchpad/ui3_20260910/hookup.py            # --check, writes nothing
    python scratchpad/ui3_20260910/hookup.py --apply
"""

import io
import os
import sys

ROOT = os.path.abspath( os.path.join( os.path.dirname( __file__ ), "..", ".." ) )
HERE = os.path.dirname( os.path.abspath( __file__ ) )

TARGET = "src/nifskope_ui.cpp"
OLD = os.path.join( HERE, "anchor_old.txt" )   # the region as it stands, byte for byte
NEW = os.path.join( HERE, "anchor_new.txt" )   # what replaces it

# The marker the replacement carries, so a resume decides "applied or not"
# from the FILE and never from the anchor still matching (ww-anchored-hookup 4).
MARKER = "wwBarRowBoxQss"


def read( path ):
    with io.open( path, "rb" ) as f:
        return f.read()


def main():
    apply_it = "--apply" in sys.argv[1:]
    revert = "--revert" in sys.argv[1:]

    target = os.path.join( ROOT, TARGET )
    src = read( target )
    old = read( OLD )
    new = read( NEW )
    if revert:
        # put the region back exactly as the lane found it, so a second round
        # of this edit is still ONE edit from the pristine text and the anchor
        # in this table keeps meaning what it says
        old, new = new, old
        print( "REVERT: the table is read backwards for this run" )

    cr_before = src.count( b"\r" )
    lf_before = src.count( b"\n" )
    n_old = src.count( old )
    n_marker = src.count( MARKER.encode( "ascii" ) )

    print( "target        : %s (%d bytes, CR %d, LF %d)"
           % ( TARGET, len( src ), cr_before, lf_before ) )
    print( "anchor        : %d bytes, matches %d time(s)" % ( len( old ), n_old ) )
    print( "replacement   : %d bytes, CR %d" % ( len( new ), new.count( b"\r" ) ) )
    print( "marker %-7s: %d occurrence(s) in the target already" % ( MARKER, n_marker ) )

    if new.count( b"\r" ) != old.count( b"\r" ):
        print( "REFUSED: the replacement's line endings do not match the region's "
               "(CR %d vs %d)" % ( new.count( b"\r" ), old.count( b"\r" ) ) )
        return 2

    if n_old != 1:
        if n_marker > 0 and n_old == 0:
            print( "REFUSED: the anchor is gone and the marker is present -- "
                   "this edit is ALREADY APPLIED, nothing to do" )
            return 0
        print( "REFUSED: the anchor must match exactly once, it matched %d" % n_old )
        return 2

    if not apply_it:
        print( "\n--check: 1 of 1 anchors matches once, CR %d unchanged. "
               "Nothing was written." % cr_before )
        return 0

    out = src.replace( old, new )
    n_after = out.count( MARKER.encode( "ascii" ) )
    expect_marker = ( n_marker - old.count( MARKER.encode( "ascii" ) )
                      + new.count( MARKER.encode( "ascii" ) ) )
    assert n_after == expect_marker, "marker did not land"
    print( "marker         : %d -> %d occurrence(s)" % ( n_marker, n_after ) )
    cr_after = out.count( b"\r" )
    expect_cr = cr_before - old.count( b"\r" ) + new.count( b"\r" )
    if cr_after != expect_cr:
        print( "REFUSED: CR count would move %d -> %d, expected %d"
               % ( cr_before, cr_after, expect_cr ) )
        return 2

    with io.open( target, "wb" ) as f:
        f.write( out )
    print( "\n--apply: %s written, %d -> %d bytes, CR %d -> %d"
           % ( TARGET, len( src ), len( out ), cr_before, cr_after ) )
    return 0


if __name__ == "__main__":
    sys.exit( main() )

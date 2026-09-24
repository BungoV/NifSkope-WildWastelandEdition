#!/usr/bin/env python
"""UI3: append this lane's MISTAKES entries to MISTAKES.md, append-only.
The file is LF-only (CR 0) and so is the text; both are asserted, and the
original tail is asserted unchanged byte for byte."""
import io
import os

ROOT = os.path.abspath( os.path.join( os.path.dirname( __file__ ), "..", ".." ) )
DST = os.path.join( ROOT, "MISTAKES.md" )
SRC = os.path.join( ROOT, "scratchpad/ui3_20260910/MISTAKES_ENTRIES.md" )
MARK = "lane UI3 -- a Bash heredoc"


def main():
    with io.open( DST, "rb" ) as f:
        dst = f.read()
    with io.open( SRC, "rb" ) as f:
        src = f.read()

    assert dst.count( b"\r" ) == 0, "MISTAKES.md is LF-only and is not any more"
    assert src.count( b"\r" ) == 0, "the entry text carries a CR"
    if dst.count( MARK.encode( "utf-8" ) ):
        print( "already appended (marker found); nothing written" )
        return

    # drop the HTML note at the top of the entry file -- it is instructions to
    # the lane, not changelog text
    body = src.split( b"-->\n", 1 )[-1].lstrip( b"\n" )
    assert body.startswith( b"## 2026-09-10" ), "the entry text does not start at a heading"

    tail = b"" if dst.endswith( b"\n\n" ) else ( b"\n" if dst.endswith( b"\n" ) else b"\n\n" )
    out = dst + tail + body
    assert out.startswith( dst ), "the append is not append-only"
    assert out.count( b"\r" ) == 0
    with io.open( DST, "wb" ) as f:
        f.write( out )
    print( "MISTAKES.md %d -> %d bytes, CR 0, %d entries appended"
           % ( len( dst ), len( out ), body.count( b"\n## " ) + 1 ) )


if __name__ == "__main__":
    main()

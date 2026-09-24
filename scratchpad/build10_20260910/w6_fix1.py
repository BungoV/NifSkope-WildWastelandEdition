#!/usr/bin/env python3
"""Lane WATER6 fix 1: std::max( int, qsizetype ) does not deduce."""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OLD = ("\t\tramp.append( float( 1.0 + 2.0 * double( i ) "
       "/ double( std::max( 1, axis.pts.size() - 1 ) ) ) );\n")
NEW = ("\t\tramp.append( float( 1.0 + 2.0 * double( i ) "
       "/ double( qMax<qsizetype>( 1, axis.pts.size() - 1 ) ) ) );\n")

for p in (os.path.join(ROOT, "src", "watermark.cpp"),
          os.path.join(os.path.dirname(os.path.abspath(__file__)), "w6_gates.cpp.txt")):
    b = open(p, "rb").read()
    cr = b.count(b"\r")
    n = b.count(OLD.encode("utf-8"))
    print("%-30s count=%d CR=%d" % (os.path.basename(p), n, cr))
    assert n == 1, p
    b = b.replace(OLD.encode("utf-8"), NEW.encode("utf-8"))
    assert b.count(b"\r") == cr
    open(p, "wb").write(b)
    print("  wrote %d bytes" % len(b))

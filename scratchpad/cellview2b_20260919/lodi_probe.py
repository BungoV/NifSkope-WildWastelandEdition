#!/usr/bin/env python3
"""Print magic/version/chunk-bounds/groupCount of .lodi candidates.

Layout is taken from src/lodifile.h's own stated file offsets: the header's
fixed prefix starts after the magic, and every v2+ field carries its offset in
a comment (loadOrderHash at 0x90, offGroup at 0x100, groupCount at 0x108).
So the chunk bounds are located by SEARCH, not by assumption: we read the
prefix up to worldspaceEdid, which is a length-prefixed string.
"""
import struct, sys, glob, os

def probe(p):
    b = open(p, 'rb').read(0x140)
    magic, version, flags, hcrc = struct.unpack_from('<IIII', b, 0)
    # groupCount at 0x108, groupStride at 0x10C (v7+)
    gc = gs = None
    if len(b) >= 0x110:
        gc, gs = struct.unpack_from('<IH', b, 0x108)
    print(f"{os.path.getsize(p):>10} magic={magic:08x} version={version} "
          f"groupCount={gc} groupStride={gs}  {p}")

for pat in sys.argv[1:]:
    for p in sorted(glob.glob(pat, recursive=True)):
        try:
            probe(p)
        except Exception as e:
            print(f"           ERROR {e}  {p}")

#!/usr/bin/env python
"""Minimal NIF 20.2.0.7 (BSVER 130) header reader.

Enough to answer "what blocks and what NODE NAMES does this file carry" without
launching NifSkope.  Reads: version line, user/bs version, block count, block
type strings, per-block type index, block sizes, and the string table.  Then,
for NiNode blocks only, resolves the Name string index (the first uint32 of a
NiObjectNET's body) so the bone-name set can be compared without a full parse.

Usage:  nifhdr.py FILE [--names] [--blocks]
"""
import struct
import sys


def read(path):
    b = open(path, "rb").read()
    p = b.index(b"\n") + 1                     # header string
    hdr = b[:p].decode("latin-1").strip()
    ver, = struct.unpack_from("<I", b, p); p += 4
    endian = b[p]; p += 1
    user, = struct.unpack_from("<I", b, p); p += 4
    nblocks, = struct.unpack_from("<I", b, p); p += 4
    bs, = struct.unpack_from("<I", b, p); p += 4
    # Export info: Author / Process Script / Export Script / (BS 130) Max Filepath.
    # These are ExportStrings -- a BYTE length, not a uint32, and the length
    # counts the trailing NUL.  (Block-type names below ARE uint32-prefixed.)
    for _ in range(4):
        n = b[p]; p += 1 + n
    ntypes, = struct.unpack_from("<H", b, p); p += 2
    types = []
    for _ in range(ntypes):
        n, = struct.unpack_from("<I", b, p); p += 4
        types.append(b[p:p + n].decode("latin-1")); p += n
    tidx = list(struct.unpack_from("<%dH" % nblocks, b, p)); p += 2 * nblocks
    sizes = list(struct.unpack_from("<%dI" % nblocks, b, p)); p += 4 * nblocks
    nstr, = struct.unpack_from("<I", b, p); p += 4
    maxlen, = struct.unpack_from("<I", b, p); p += 4
    strs = []
    for _ in range(nstr):
        n, = struct.unpack_from("<I", b, p); p += 4
        strs.append(b[p:p + n].decode("latin-1")); p += n
    ngroups, = struct.unpack_from("<I", b, p); p += 4 + 4 * ngroups
    body = p
    return dict(header=hdr, version=ver, user=user, bs=bs, nblocks=nblocks,
                types=types, tidx=tidx, sizes=sizes, strings=strs,
                bodystart=body, blob=b)


def block_offsets(h):
    off, out = h["bodystart"], []
    for s in h["sizes"]:
        out.append(off)
        off += s
    return out


def node_names(h):
    """Name string of every block whose type derives from NiObjectNET (first u32)."""
    offs = block_offsets(h)
    out = []
    for i, o in enumerate(offs):
        t = h["types"][h["tidx"][i]]
        if t in ("NiNode", "BSFadeNode", "BSLeafAnimNode", "BSTreeNode",
                 "BSOrderedNode", "NiBillboardNode", "BSSubIndexTriShape",
                 "BSTriShape", "BSDynamicTriShape", "BSMeshLODTriShape"):
            si, = struct.unpack_from("<i", h["blob"], o)
            nm = h["strings"][si] if 0 <= si < len(h["strings"]) else "<%d>" % si
            out.append((i, t, nm))
    return out


def main(argv):
    h = read(argv[1])
    print("%s  version %s user %d bs %d  blocks %d" %
          (h["header"], ".".join(str((h["version"] >> s) & 255) for s in (24, 16, 8, 0)),
           h["user"], h["bs"], h["nblocks"]))
    tally = {}
    for i in h["tidx"]:
        tally[h["types"][i]] = tally.get(h["types"][i], 0) + 1
    for t, n in sorted(tally.items()):
        print("   %-34s x%d" % (t, n))
    if "--names" in argv:
        for i, t, nm in node_names(h):
            print("  #%-4d %-22s %s" % (i, t, nm))
    if "--strings" in argv:
        for i, s in enumerate(h["strings"]):
            print("  $%-4d %s" % (i, s))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

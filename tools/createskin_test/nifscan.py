"""Scan NIFs: per BSTriShape-family block report name, verts, tris, skin link."""
import struct, sys, os, glob
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                '..', 'rigging_prototype'))
import io, contextlib
import nifparse

SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape')

def scan(path):
    buf = io.StringIO()
    try:
        with contextlib.redirect_stdout(buf):
            data, hdr, strings, blocks = nifparse.parse(path)
    except Exception as e:
        print(f"{os.path.basename(path)}: parse failed: {e}")
        return
    head = buf.getvalue().strip()
    lines = []
    for i, tname, start, size in blocks:
        if tname not in SHAPES:
            continue
        o = start
        def U32():
            nonlocal o
            v = struct.unpack_from('<I', data, o)[0]; o += 4; return v
        def U16():
            nonlocal o
            v = struct.unpack_from('<H', data, o)[0]; o += 2; return v
        nameidx = U32()
        nm = strings[nameidx] if 0 <= nameidx < len(strings) else '?'
        ne = U32(); o += 4*ne
        o += 8            # controller, flags
        o += 12 + 36 + 4  # translation, rotation, scale
        o += 4            # collision
        o += 16           # bounding sphere
        skin = struct.unpack_from('<i', data, o)[0]; o += 4
        o += 8            # shader, alpha
        vdesc = struct.unpack_from('<Q', data, o)[0]; o += 8
        ntri = U32(); nv = U16(); dsize = U32()
        stride = (vdesc & 0xF) * 4
        skinned = 'SKINNED' if skin >= 0 else 'unskinned'
        lines.append(f"    [{i}] {tname} '{nm}' nv={nv} nt={ntri} stride={stride} skin={skin} {skinned}")
    print(head)
    for l in lines:
        print(l)

for pat in sys.argv[1:]:
    for p in sorted(glob.glob(pat)):
        scan(p)

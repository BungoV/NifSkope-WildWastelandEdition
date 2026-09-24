"""Add a `moveid` mode to b_esmedit.py.

WHY. The `move` mode takes the FIRST uncompressed REFR in a cell, and gate B3's
first run proved that is the wrong ref to take: both refs arms came back with a
floor of exactly ONE file -- `Commonwealth.lodb` itself. The moved reference was
not drawn in LOD (most are not: an empty MNAM slot drops a ref at every ring),
so the only thing the edit moved was the input digest, which is the thing under
test. An arm whose only witness is the artefact being tested is not a witness.

`moveid` moves a named form id, and the caller picks that id out of the base
bake's own `.BTO.manifest.txt` -- i.e. out of the list of references the bake
actually drew. Then the floor is a real `.BTO`.

b_esmedit.py is a CRLF scratchpad script (230 CR / 230 LF), unlike the tree's
LF-only sources, so this patch normalises to match and puts the CRLFs back.
"""
import sys

CR = chr(13)
LF = chr(10)

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/b_esmedit.py'

ANCHOR = """    elif mode == 'move':
        refrs = [f for f in hits if f[1] == 'REFR' and not (f[4] & COMPRESSED)]"""

NEWMODE = """    elif mode == 'moveid':
        # Move ONE NAMED reference -- the caller picked it out of the base
        # bake's manifest, so it is a ref the bake demonstrably draws. See the
        # module docstring for why `move` was not good enough.
        target = int(argv[6], 16)
        refrs = [f for f in hits if f[1] == 'REFR' and not (f[4] & COMPRESSED)]
        done = False
        for _c, _t, rs, sz, _fl, _st in refrs:
            form = struct.unpack_from('<I', buf, rs + 12)[0]
            if form != target:
                continue
            for sig, off, ln in subrecords(buf, rs + HDR, sz):
                if sig == b'DATA' and ln >= 24:
                    x = struct.unpack_from('<f', buf, off)[0]
                    struct.pack_into('<f', buf, off, x + amount)
                    print('REFR 0x%08X DATA x: %.3f -> %.3f (cell %d,%d)'
                          % (form, x, x + amount, cx, cy))
                    done = True
                    break
            if done:
                break
        if not done:
            print('REFUSED: 0x%08X is not an uncompressed REFR of cell (%d,%d)'
                  % (target, cx, cy))
            return 2

    elif mode == 'move':
        refrs = [f for f in hits if f[1] == 'REFR' and not (f[4] & COMPRESSED)]"""

DOCOLD = "    python b_esmedit.py move   <esm> <out> <cx> <cy> <dx>"
DOCNEW = ("    python b_esmedit.py move   <esm> <out> <cx> <cy> <dx>" + LF +
          "    python b_esmedit.py moveid <esm> <out> <cx> <cy> <dx> <formid-hex>")


def main():
    b = open(P, 'rb').read()
    crlf = b.count((CR + LF).encode())
    s = b.decode('utf-8').replace(CR + LF, LF)

    n = s.count(ANCHOR)
    assert n == 1, 'move anchor matched %d times' % n
    s = s.replace(ANCHOR, NEWMODE)

    n = s.count(DOCOLD)
    assert n == 1, 'doc anchor matched %d times' % n
    s = s.replace(DOCOLD, DOCNEW)

    compile(s, P, 'exec')
    if crlf:
        s = s.replace(LF, CR + LF)
    out = s.encode('utf-8')
    open(P, 'wb').write(out)
    print('b_esmedit.py %d -> %d bytes, compiles, CR %d LF %d'
          % (len(b), len(out), out.count(b'\x0d'), out.count(b'\x0a')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

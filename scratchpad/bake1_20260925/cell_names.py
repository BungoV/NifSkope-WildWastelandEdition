"""Commonwealth exterior cells whose EDID or FULL matches a word (read only). usage: cell_names.py <esm> word [word ...]"""
import sys, struct, re
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
import fo4esm
buf = fo4esm.load(sys.argv[1])
words = [w.lower() for w in sys.argv[2:]]
tes4 = fo4esm.read_record_header(buf, 0)
hits = []
def on_record(off, sig, dsize, flags, formid, tail, stack):
    if sig != b'CELL':
        return
    if not any(n.gtype == 1 and struct.unpack('<I', n.label)[0] == 0x3C for n in stack):
        return
    p = fo4esm.record_payload(buf, off, dsize, flags)
    edid = full = ''; grid = None
    for s in fo4esm.subrecords(p):
        if s[0] == b'EDID':
            edid = s[1].split(b'\0')[0].decode('latin1')
        elif s[0] == b'XCLC':
            grid = struct.unpack('<ii', s[1][:8])
    if grid and any(w in edid.lower() for w in words):
        hits.append((edid, grid))
fo4esm.walk(buf, 24 + tes4[1], len(buf), [], on_record)
for e, g in sorted(hits):
    print(g[0], g[1], e)

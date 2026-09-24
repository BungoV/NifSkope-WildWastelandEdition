"""G2 known answer, independent of the exe: find TestWorldspace.esp's WRLD records in the raw
file (raw form ID + EDID), predict the load-order form ID by xEdit's rule (top byte >= master
count -> the plugin's own index in the --print-source listing), and compare with the exe's
--list-worldspaces line. usage: g2_check.py <print-source txt> <list-worldspaces txt>"""
import struct, sys, re
src = [l.rstrip('\r\n') for l in open(sys.argv[1], encoding='utf-8')]
plugins = {int(m.group(1)): m.group(2) for m in (re.match(r'plugin (\d+): (.*)$', l) for l in src) if m}
idx = [k for k, v in plugins.items() if v.endswith('/TestWorldspace.esp')][0]
p = plugins[idx]; b = open(p, 'rb').read()
tes4 = struct.unpack_from('<I', b, 4)[0]
mc = 0; pos = 24
while pos + 6 <= 24 + tes4:
    tag, ln = b[pos:pos + 4], struct.unpack_from('<H', b, pos + 4)[0]
    mc += tag == b'MAST'; pos += 6 + ln
listed = {}
for l in open(sys.argv[2], encoding='utf-8', errors='replace'):
    m = re.match(r'\s+([0-9a-f]{8})\s+(\S+)', l.rstrip('\r\n'))
    if m: listed[m.group(2)] = int(m.group(1), 16)
pos = 24 + tes4; fails = 0; n = 0
while pos + 24 <= len(b):
    typ = b[pos:pos + 4]; size, fl, fid = struct.unpack_from('<III', b, pos + 4)
    if typ == b'GRUP': pos += 24; continue
    if typ == b'WRLD':
        assert not fl & 0x40000
        d = b[pos + 24:pos + 24 + size]; q = 0; edid = None
        while q + 6 <= len(d):
            t, ln = d[q:q + 4], struct.unpack_from('<H', d, q + 4)[0]
            if t == b'EDID': edid = d[q + 6:q + 6 + ln].rstrip(b'\0').decode('latin1')
            q += 6 + ln
        top = fid >> 24
        pred = ((idx if top >= mc else top) << 24) | (fid & 0xFFFFFF)
        got = listed.get(edid)
        n += 1
        ok = got == pred
        fails += not ok
        print('%s WRLD raw %08X EDID %s, masters %d, plugin index %d -> predicted %08X, exe %s' % ('ok  ' if ok else 'FAIL', fid, edid, mc, idx, pred, '%08X' % got if got is not None else 'absent'))
    pos += 24 + size
print('G2 %d WRLD, %d failures' % (n, fails)); sys.exit(1 if fails or not n else 0)

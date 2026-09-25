"""BAKE2: placed refs a worldspace's .lodi cannot hold. The .lodi stores an instance's scale as u16/8192 and the
writer REFUSES the whole file on a scale above 65535/8192 = 7.99988 (src/lodifile.cpp). This lists every REFR in the
worldspace (last override wins, load-order ids) whose XSCL is above that line, with its base and whether the base
(a STAT) carries an MNAM LOD model at all (a base with no LOD model never reaches the .lodi).
usage: overscale.py <wrld load-order id hex> <plugin> [<plugin> ...]   (all plugins, in load order)"""
import sys, struct, os
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
import fo4esm

WS = int(sys.argv[1], 16)
plugins = sys.argv[2:]
LIMIT = 65535.0 / 8192.0
gindex = {}; n_full = 0; bufs = []
for p in plugins:
    b = fo4esm.load(p); bufs.append(b)
    flags = struct.unpack_from('<I', b, 8)[0]
    if not (p.lower().endswith('.esl') or flags & 0x200):
        gindex[os.path.basename(p).lower()] = n_full; n_full += 1
refs = {}; stat_lod = {}
for path, buf in zip(plugins, bufs):
    me = os.path.basename(path).lower()
    if me not in gindex:
        continue
    tes4 = fo4esm.read_record_header(buf, 0)
    masters = [s[1].split(b'\0')[0].decode('latin1').lower() for s in fo4esm.subrecords(bytes(buf[24:24 + tes4[1]])) if s[0] == b'MAST']
    nm = len(masters)
    def g(fid):
        i = fid >> 24
        owner = masters[i] if i < nm else me
        return (gindex[owner] << 24) | (fid & 0xFFFFFF) if owner in gindex else None
    def on_record(off, sig, dsize, flags, formid, tail, stack):
        if sig == b'STAT':
            p = fo4esm.record_payload(buf, off, dsize, flags)
            stat_lod[g(formid)] = any(s[0] == b'MNAM' for s in fo4esm.subrecords(p))
        elif sig == b'REFR':
            if not any(n.gtype == 1 and g(struct.unpack('<I', n.label)[0]) == WS for n in stack):
                return
            p = fo4esm.record_payload(buf, off, dsize, flags)
            base = None; sc = 1.0
            for s in fo4esm.subrecords(p):
                if s[0] == b'NAME': base = g(struct.unpack('<I', s[1][:4])[0])
                elif s[0] == b'XSCL': sc = struct.unpack('<f', s[1][:4])[0]
            refs[g(formid)] = (base, sc, me)
    fo4esm.walk(buf, 24 + tes4[1], len(buf), [], on_record)
over = sorted((r, b, s, f) for r, (b, s, f) in refs.items() if s > LIMIT)
withlod = [o for o in over if stat_lod.get(o[1])]
print('ws %08x: refs %d; scale > %.5f: %d; of those with a STAT base carrying MNAM: %d' % (WS, len(refs), LIMIT, len(over), len(withlod)))
mx = max(refs.items(), key=lambda kv: kv[1][1]) if refs else None
if mx: print('ws %08x: max XSCL %.4f on ref %08x (base %08x, %s, lod %s)' % (WS, mx[1][1], mx[0], mx[1][0] or 0, mx[1][2], stat_lod.get(mx[1][0])))
for r, b, s, f in over[:30]:
    print('   ref %08x base %08x scale %.6f (%s) lod %s' % (r, b or 0, s, f, stat_lod.get(b)))

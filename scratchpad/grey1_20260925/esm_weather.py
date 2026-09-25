"""GREY1 candidate 3: read vanilla Fallout4.esm (read only) -- the Commonwealth climate's weathers, each one's
midday (Day slot) Sunlight + Ambient (NAM0), DALC 6 axes, and its Day imagespace (IMSP -> IMGS HNAM/CNAM/TNAM).
Layouts: xEdit wbDefinitionsFO4.pas (IMGS l.7393) and src/esmweather.cpp (NAM0 19x8 at form >= 119; DALC axes
X+ X- Y+ Y- Z+ Z- RGBA then specular, fresnel)."""
import struct, zlib, sys, json
ESM = r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'
b = open(ESM, 'rb').read()
recs = {}          # formid -> (type, flags, formver, data)
def walk(o, end, want):
    while o < end:
        t = b[o:o+4]
        if t == b'GRUP':
            sz = struct.unpack_from('<I', b, o+4)[0]
            lab = b[o+8:o+12]
            gt = struct.unpack_from('<i', b, o+12)[0]
            if gt != 0 or lab in want:
                walk(o+24, o+sz, want)
            o += sz
        else:
            sz, fl, fid = struct.unpack_from('<III', b, o+4)
            fv = struct.unpack_from('<H', b, o+20)[0]
            if t in want:
                d = b[o+24:o+24+sz]
                if fl & 0x40000:
                    d = zlib.decompress(d[4:])
                recs[fid] = (t.decode(), fl, fv, d)
            o += 24 + sz
hdr = struct.unpack_from('<I', b, 4)[0]
walk(24 + hdr, len(b), {b'WTHR', b'IMGS', b'CLMT', b'WRLD'})
# optional override plugins (argv[2:]), loaded in order after the ESM; their records whose FormID top byte is 0
# (Fallout4.esm as master 0) replace the ESM's -- enough for a weather mod that overrides WTHR/IMGS/CLMT
for _p in sys.argv[2:]:
    b = open(_p, 'rb').read()
    hdr = struct.unpack_from('<I', b, 4)[0]
    walk(24 + hdr, len(b), {b'WTHR', b'IMGS', b'CLMT'})
    print('overrides loaded from', _p)
def fields(d):
    o = 0; out = []; big = None
    while o < len(d):
        t = d[o:o+4].decode('latin-1'); n = struct.unpack_from('<H', d, o+4)[0]; o += 6
        if t == 'XXXX':
            big = struct.unpack_from('<I', d, o)[0]; o += n; continue
        if big is not None: n = big; big = None
        out.append((t, d[o:o+n])); o += n
    return out
def edid(fid):
    for t, v in fields(recs[fid][3]):
        if t == 'EDID': return v.rstrip(b'\0').decode('latin-1')
    return '?'
W = recs[0x3C]; cw = dict(fields(W[3]))
clmt = struct.unpack_from('<I', cw['CNAM'])[0]
print('WRLD 0x3C', edid(0x3C), 'climate', hex(clmt), edid(clmt))
C = dict(fields(recs[clmt][3]))
wl = C['WLST']; n = len(wl) // 12
wlist = [struct.unpack_from('<IiI', wl, 12*i) for i in range(n)]
DAY = 1
def lin(c): return (c/255.0) ** 2.2
out = []
for wf, ch, glob in wlist:
    fv = recs[wf][2]; F = fields(recs[wf][3])
    nam0 = [v for t, v in F if t == 'NAM0'][0]
    tods = 8 if fv >= 111 else 4
    def row(r): return list(nam0[(r*tods+DAY)*4:(r*tods+DAY)*4+3])
    dalc = [v for t, v in F if t == 'DALC']
    dd = dalc[DAY] if len(dalc) > DAY else None
    axes = [list(dd[4*k:4*k+3]) for k in range(6)] if dd else None
    imsp = [v for t, v in F if t == 'IMSP']
    ig = struct.unpack_from('<I', imsp[0], 4*DAY)[0] if imsp and len(imsp[0]) >= 8 else 0
    igd = {}
    if ig in recs:
        G = dict(fields(recs[ig][3]))
        if 'HNAM' in G: igd['HNAM'] = struct.unpack_from('<9f', G['HNAM'])
        if 'CNAM' in G: igd['CNAM'] = struct.unpack_from('<3f', G['CNAM'])
        if 'TNAM' in G: igd['TNAM'] = struct.unpack_from('<4f', G['TNAM'])
        igd['edid'] = edid(ig)
    out.append(dict(fid=hex(wf), edid=edid(wf), chance=ch, fv=fv, sunlight=row(4), ambient=row(3), dalc=axes, imgs=igd))
json.dump(out, open(sys.argv[1], 'w'), indent=1)
for w in out:
    print('%-28s ch %3d sun %-15s amb %-15s dalcZ-(up) %s imgs %s CNAM %s TNAM %s' % (w['edid'], w['chance'], w['sunlight'], w['ambient'],
          w['dalc'][5] if w['dalc'] else None, w['imgs'].get('edid'), w['imgs'].get('CNAM'), w['imgs'].get('TNAM')))

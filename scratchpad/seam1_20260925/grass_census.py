"""GRASSCOL: is the ground-cover tint the grass bungo sees? Read only (plugins, mods, the shipped VT).
Walks LTEX -> GNAM -> GRAS over the BAKE1 plugin list (last override wins), resolves every GRAS model and its
texture through the SAME stack rule the bake uses (lodgenStackSearchPaths: loose dirs newest-first, then archives
newest-first, first hit wins) and prints, per GRAS:
  plugin that supplied it, MODL, NIF tex0, NIF .bgsm, the .bgsm's diffuse, the file each resolves to,
  the bake's tint (smallest mip rgb / a, clamped, exactly lodgenGrassTintResolve),
  the mip-0 mean unweighted, alpha-weighted, and alpha-tested (a >= 0.5), for the tex0 file AND the bgsm file.
usage: grass_census.py <out.txt>"""
import sys, os, struct, zlib, glob, collections, pickle
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/mountains_20260907')
sys.path.insert(0, 'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools')
import fo4esm, nifwind
B1 = 'E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/'
PLUGINS = open(B1 + 'esm_list.txt').read().strip().split(',')
STACK = [l.strip() for l in open(B1 + 'resources.txt') if l.strip()]
CELLS = [(x, y) for x in range(-20, -15) for y in range(20, 25)] + [(-24, -8)]
WS = 0x3C
out = open(sys.argv[1], 'w')
def P(*a):
    s = ' '.join(str(x) for x in a); print(s); out.write(s + '\n')

# ------------------------------------------------------------------ plugins
def gkey(fid, masters, me):
    i = fid >> 24
    return ((masters[i] if i < len(masters) else me).lower(), fid & 0xFFFFFF)

LTEX = {}; GRAS = {}; LAND = {}
for path in PLUGINS:
    name = path.split('/')[-1]
    buf = fo4esm.load(path)
    tes4 = fo4esm.read_record_header(buf, 0)
    masters = [s[1].rstrip(b'\0').decode('latin-1') for s in fo4esm.subrecords(bytes(buf[24:24 + tes4[1]])) if s[0] == b'MAST']
    heavy = name.lower().startswith(('dlc', 'cc'))
    grid = {}; lands = []
    def on_record(off, sig, dsize, flags, formid, tail, stack):
        if sig == b'LTEX':
            p = fo4esm.record_payload(buf, off, dsize, flags)
            sub = list(fo4esm.subrecords(p))
            LTEX[gkey(formid, masters, name)] = dict(src=name, edid=next((s[1][:-1].decode('latin-1') for s in sub if s[0] == b'EDID'), ''),
                gnam=[gkey(struct.unpack('<I', s[1][:4])[0], masters, name) for s in sub if s[0] == b'GNAM'])
        elif sig == b'GRAS':
            p = fo4esm.record_payload(buf, off, dsize, flags)
            sub = dict((s[0], s[1]) for s in fo4esm.subrecords(p))
            GRAS[gkey(formid, masters, name)] = dict(src=name, edid=sub.get(b'EDID', b'?\0')[:-1].decode('latin-1'),
                modl=sub.get(b'MODL', b'\0')[:-1].decode('latin-1'))
        elif sig == b'CELL':
            if not any(n.gtype == 1 and struct.unpack('<I', n.label)[0] == WS for n in stack): return
            for s in fo4esm.subrecords(fo4esm.record_payload(buf, off, dsize, flags)):
                if s[0] == b'XCLC': grid[formid] = struct.unpack('<ii', s[1][:8])
        elif sig == b'LAND':
            cg = [n for n in stack if n.gtype in (6, 8, 9)]
            if not cg: return
            base, layer = [], []
            for s in fo4esm.subrecords(fo4esm.record_payload(buf, off, dsize, flags)):
                if s[0] == b'BTXT': base.append(gkey(struct.unpack('<I', s[1][:4])[0], masters, name))
                elif s[0] == b'ATXT': layer.append(gkey(struct.unpack('<I', s[1][:4])[0], masters, name))
            lands.append((struct.unpack('<I', cg[-1].label)[0], base, layer))
    for g in fo4esm.top_level_groups(buf):
        if g.label in (b'LTEX', b'GRAS') or (g.label == b'WRLD' and not heavy):
            fo4esm.walk(buf, g.offset + 24, g.offset + g.gsize, [g], on_record)
    for cell, base, layer in lands:
        if cell in grid and grid[cell] in CELLS: LAND[grid[cell]] = (base, layer, name)

# ------------------------------------------------------------------ the stack, the bake's rule
DATADIRS = None
def build_index():
    loose, arcs = [], []
    for e in reversed(STACK):
        if os.path.isdir(e): loose.append(e)
    for e in reversed(STACK):
        if os.path.isdir(e):
            arcs += sorted(glob.glob(e + '/*.ba2'), key=lambda a: os.path.basename(a).lower())
    return loose, arcs

class BA2:
    def __init__(self, path):
        self.path = path; f = open(path, 'rb'); self.f = f
        magic, ver, kind, n, nto = struct.unpack('<4sII IQ', f.read(24))
        if ver in (2, 3): f.read(8 if ver == 2 else 12)
        self.kind = kind.to_bytes(4, 'little'); self.recs = {}
        if self.kind == b'GNRL':
            recs = [struct.unpack('<IIIIQIII', f.read(36)) for _ in range(n)]
        else:
            recs = []
            for _ in range(n):
                h = struct.unpack('<I4sIBBHHHBBBB', f.read(24))
                ch = [struct.unpack('<QIIHHI', f.read(24)) for _ in range(h[4])]
                recs.append((h, ch))
        f.seek(nto)
        for r in recs:
            k = struct.unpack('<H', f.read(2))[0]
            self.recs[f.read(k).decode('latin-1').replace('/', '\\').lower()] = r
    def get(self, name):
        r = self.recs[name]; f = self.f
        if self.kind == b'GNRL':
            f.seek(r[4]); return zlib.decompress(f.read(r[5])) if r[5] else f.read(r[6])
        h, ch = r; data = b''
        for off, packed, full, m0, m1, _ in ch:
            f.seek(off); data += zlib.decompress(f.read(packed)) if packed else f.read(full)
        # (w, h, mips, dxgi, data) -- the texture, no DDS header
        return ('DX10RAW', h[7], h[6], h[8], h[9], data)

loose, arcpaths = build_index()
arcs = {}
def arc(p):
    if p not in arcs: arcs[p] = BA2(p)
    return arcs[p]

def resolve(rel):
    """first hit under the bake's stack rule -> (label, bytes or DX10RAW tuple)"""
    rel = rel.replace('/', '\\').lower()
    for e in loose:
        fp = e + '/' + rel.replace('\\', '/')
        if os.path.isfile(fp): return ('loose ' + e.split('/')[-1], open(fp, 'rb').read())
    for a in arcpaths:
        A = arc(a)
        if rel in A.recs: return ('ba2 ' + os.path.basename(a), A.get(rel))
    return (None, None)

# ------------------------------------------------------------------ textures
BPB = {71: 8, 72: 8, 77: 16, 78: 16, 98: 16, 99: 16, 70: 8, 76: 16, 80: 8, 83: 16}
def dds_parse(x):
    if isinstance(x, tuple):
        _, w, h, mips, fmt, data = x; return w, h, mips, fmt, data
    assert x[:4] == b'DDS '
    h, w = struct.unpack_from('<II', x, 12); mips = max(1, struct.unpack_from('<I', x, 28)[0])
    four = x[84:88]; off = 128
    fmt = {b'DXT1': 71, b'DXT5': 77, b'DXT3': 74, b'ATI2': 83, b'BC5U': 83}.get(four)
    if four == b'DX10': fmt = struct.unpack_from('<I', x, 128)[0]; off = 148
    if fmt is None and struct.unpack_from('<I', x, 80)[0] & 0x40:
        fmt = 87   # uncompressed BGRA8 (assumed 32 bit)
    return w, h, mips, fmt, x[off:]

def _565(c):
    c = c.astype(np.int32)
    return np.stack([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], -1)

def bc_decode(p, w, h, fmt):
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4); nb = bw * bh
    if fmt in (98, 99):
        from PIL import Image; import io
        hdr = b'DDS ' + struct.pack('<7I', 124, 0x1007 | 0x80000, h, w, len(p), 0, 1) + b'\0' * 44
        hdr += struct.pack('<2I4s5I', 32, 4, b'DX10', 0, 0, 0, 0, 0) + struct.pack('<5I', 0x1000, 0, 0, 0, 0)
        hdr += struct.pack('<5I', fmt, 3, 0, 1, 0)
        return np.array(Image.open(io.BytesIO(hdr + p[:nb * 16])).convert('RGBA'))
    if fmt == 87:
        a = np.frombuffer(p, np.uint8, w * h * 4).reshape(h, w, 4); return a[..., [2, 1, 0, 3]]
    bc3 = fmt in (77, 78); bb = 16 if bc3 else 8
    raw = np.frombuffer(p, np.uint8, nb * bb).reshape(nb, bb)
    col = raw[:, 8:16] if bc3 else raw
    c0 = col[:, 0:2].copy().view('<u2').ravel(); c1 = col[:, 2:4].copy().view('<u2').ravel()
    bits = col[:, 4:8].copy().view('<u4').ravel(); e0, e1 = _565(c0), _565(c1)
    four = (c0 > c1) | bc3
    p2 = np.where(four[:, None], (2 * e0 + e1) // 3, (e0 + e1) // 2); p3 = np.where(four[:, None], (e0 + 2 * e1) // 3, 0)
    pal = np.stack([e0, e1, p2, p3], 1); idx = (bits[:, None] >> (2 * np.arange(16))) & 3
    rgb = pal[np.arange(nb)[:, None], idx]
    if bc3:
        a0 = raw[:, 0].astype(np.int32); a1 = raw[:, 1].astype(np.int32)
        ab = np.concatenate([raw[:, 2:8], np.zeros((nb, 2), np.uint8)], 1).copy().view('<u8').ravel()
        ai = ((ab[:, None] >> (3 * np.arange(16)).astype(np.uint64)) & 7).astype(np.int32); k = np.arange(8)[None]
        pal8 = np.where(k == 0, a0[:, None], np.where(k == 1, a1[:, None], ((8 - k) * a0[:, None] + (k - 1) * a1[:, None]) // 7))
        pal6 = np.where(k == 0, a0[:, None], np.where(k == 1, a1[:, None], np.where(k == 6, 0, np.where(k == 7, 255,
               ((6 - k) * a0[:, None] + (k - 1) * a1[:, None]) // 5))))
        a = np.where((a0 > a1)[:, None], pal8, pal6)[np.arange(nb)[:, None], ai]
    else:
        a = np.where((~four)[:, None] & (idx == 3), 0, 255)
    px = np.concatenate([rgb, a[..., None]], -1).astype(np.uint8)
    img = px.reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return img[:h, :w]

def mips_of(w, h, mips, fmt, data):
    bpb = BPB.get(fmt, 0); o = 0; out = []
    for m in range(mips):
        mw, mh = max(1, w >> m), max(1, h >> m)
        sz = (mw * mh * 4) if fmt == 87 else max(1, (mw + 3) // 4) * max(1, (mh + 3) // 4) * bpb
        if o + sz > len(data): break
        out.append((mw, mh, o)); o += sz
    return out

def srgb2lin(c): c = np.asarray(c, np.float64); return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)

TEXCACHE = {}
def tex_stats(rel):
    rel = rel.replace('/', '\\').lower()
    if not rel.startswith('textures\\'): rel = 'textures\\' + rel
    if rel in TEXCACHE: return TEXCACHE[rel]
    lab, x = resolve(rel)
    if x is None: TEXCACHE[rel] = None; return None
    w, h, mips, fmt, data = dds_parse(x)
    if fmt not in BPB and fmt != 87: TEXCACHE[rel] = dict(src=lab, fmt=fmt); return TEXCACHE[rel]
    ml = mips_of(w, h, mips, fmt, data)
    img = bc_decode(data[ml[0][2]:], w, h, fmt).astype(np.float64) / 255
    sw, sh, so = ml[-1]; small = bc_decode(data[so:], sw, sh, fmt).astype(np.float64) / 255
    sm = small.reshape(-1, 4).mean(0)
    srgb = fmt in (72, 78, 99)
    if srgb: sm = np.r_[srgb2lin(sm[:3]), sm[3]]
    bake = np.clip(sm[:3] / sm[3], 0, 1) if sm[3] >= 0.05 else None
    rgb, a = img[..., :3].reshape(-1, 3), img[..., 3].ravel()
    r = dict(src=lab, fmt=fmt, w=w, h=h, mips=mips, mipsRead=len(ml), smallest=(sw, sh), smallMean=sm, bake=bake,
             unweighted=rgb.mean(0), aweighted=(rgb * a[:, None]).sum(0) / max(a.sum(), 1e-9),
             atested=rgb[a >= 0.5].mean(0) if (a >= 0.5).any() else None, meanA=a.mean(), covA=(a >= 0.5).mean())
    TEXCACHE[rel] = r; return r

def bgsm_diffuse(b):
    r = type('R', (), {})(); o = [4]
    def u8(): v = b[o[0]]; o[0] += 1; return v
    def u32(): v, = struct.unpack_from('<I', b, o[0]); o[0] += 4; return v
    def f(n=1): o[0] += 4 * n
    def st(): n = u32(); s = b[o[0]:o[0] + n].rstrip(b'\0').decode('latin-1'); o[0] += n; return s
    v = u32(); u32(); f(4); f(); u8(); u32(); u32(); u8()
    for _ in range(11): u8()
    f(); u8()
    if v < 10: f()
    u8()
    if v >= 6: u8()
    return st()

def nif_info(model):
    rel = model.replace('/', '\\').lower()
    if not rel.startswith('meshes\\'): rel = 'meshes\\' + rel
    lab, x = resolve(rel)
    if x is None: return dict(src=None)
    n = nifwind.Nif(x)
    for k, (t, o, sz) in enumerate(n.blocks):
        if t not in ('BSTriShape', 'BSMeshLODTriShape', 'BSSubIndexTriShape'): continue
        sh = n.shape(k)
        if not (0 <= sh['shader'] < len(n.blocks)): continue
        st, so, ss = n.blocks[sh['shader']]
        if st != 'BSLightingShaderProperty': continue
        o2 = so + 4; mat, o2 = n.objnet(o2); o2 += 8 + 16
        ts = struct.unpack_from('<i', n.b, o2)[0]; tex0 = ''
        if 0 <= ts < len(n.blocks):
            _, to, _ = n.blocks[ts]; cnt = struct.unpack_from('<I', n.b, to)[0]
            if cnt: L = struct.unpack_from('<I', n.b, to + 4)[0]; tex0 = n.b[to + 8:to + 8 + L].decode('latin-1')
        if not mat.lower().endswith('.bgsm'): mat = ''
        return dict(src=lab, tex0=tex0, mat=mat, alpha=sh['alpha'] >= 0)
    return dict(src=lab, tex0='', mat='')

def fmtc(c): return 'None' if c is None else '(%3d %3d %3d)' % tuple(int(round(v * 255)) for v in c[:3])

# ------------------------------------------------------------------ report
P('# plugins walked', len(PLUGINS), '| LTEX', len(LTEX), '| GRAS', len(GRAS), '| cells with LAND', len(LAND))
use = collections.Counter(); cellgras = {}
for c in CELLS:
    if c not in LAND: P('cell', c, 'no LAND'); continue
    base, layer, lsrc = LAND[c]; gs = collections.Counter()
    for t in base + layer:
        lt = LTEX.get(t)
        if lt:
            for g in lt['gnam']: gs[g] += 1
    cellgras[c] = gs; use.update(gs)
    P('cell %4d %3d LAND from %-22s textures %2d -> GRAS %s' % (c[0], c[1], lsrc, len(base + layer),
      ' '.join('%s:%d' % (GRAS.get(g, {}).get('edid', '%s/%06X' % g), n) for g, n in gs.most_common())))
P('')
P('# per GRAS (count = texture references across the cells above)')
rows = []
for g, n in use.most_common():
    gr = GRAS.get(g)
    if not gr: P('GRAS %s/%06X MISSING' % g); continue
    ni = nif_info(gr['modl'])
    t0 = tex_stats(ni['tex0']) if ni.get('tex0') else None
    md = None; bg = None
    if ni.get('mat'):
        mp = ni['mat'].replace('/', '\\').lower(); mp = mp[mp.find('materials\\'):] if 'materials\\' in mp else 'materials\\' + mp
        ml, mb = resolve(mp)
        if mb: md = bgsm_diffuse(mb); bg = tex_stats(md); bg = dict(bg or {}, matsrc=ml)
    P('GRAS %-28s n=%3d from %-26s MODL %s' % (gr['edid'], n, gr['src'], gr['modl']))
    P('   nif  <- %s | tex0 %s | bgsm %s -> diffuse %s (bgsm from %s)' % (ni.get('src'), ni.get('tex0'), ni.get('mat'), md, bg and bg.get('matsrc')))
    for lab, s in (('tex0', t0), ('bgsm', bg)):
        if not s or 'unweighted' not in s: P('   %s: %s' % (lab, s and {k: s[k] for k in ('src', 'fmt') if k in s})); continue
        P('   %s <- %-34s fmt %d %dx%d mips %d (smallest %dx%d) meanA %.2f cov %.2f' % (lab, s['src'], s['fmt'], s['w'], s['h'], s['mips'], s['smallest'][0], s['smallest'][1], s['meanA'], s['covA']))
        P('        smallest-mip rgb %s a %.2f | BAKE tint rgb/a %s | mip0 unweighted %s alpha-weighted %s a>=.5 %s' % (
            fmtc(s['smallMean']), s['smallMean'][3], fmtc(s['bake']), fmtc(s['unweighted']), fmtc(s['aweighted']), fmtc(s['atested'])))
    rows.append((gr['edid'], n, t0, bg))
pickle.dump(dict(rows=rows, cellgras={k: dict(v) for k, v in cellgras.items()}, GRAS=GRAS), open(HERE + '/grass_census.pkl', 'wb'))

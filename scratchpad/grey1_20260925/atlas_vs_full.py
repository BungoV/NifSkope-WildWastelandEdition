"""GREY1 candidate 1 (read only): surface-mean colour of each placed BUILDING LOD model (the atlas texels its UVs
actually sample, triangle-area weighted) vs the full-detail model it stands in for (every shape's BGSM diffuse, same
sampling, x its vertex colour where SLSF2 Vertex_Colors is set). Files resolved through bungo's MO2 stack (BAKE1
resources.txt), loose beats archive, highest priority first -- TINT1 census.py's resolver, extended to materials and
textures. Texture archives (DX10 BA2) are resolved by NAME through the stack and decoded from the unpacked vanilla
copy only when the winner is a vanilla archive; a mod override is decoded from its loose file, or counted and refused
if it sits inside a mod DX10 archive.
usage: atlas_vs_full.py <out.tsv> [maxModels]"""
import sys, os, struct, collections, re
import numpy as np
from PIL import Image
T = r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools'
sys.path.insert(0, T)
sys.path.insert(0, r'E:/Projects/NifskopeWWE-tint1/tests/spells')
import ba2lib, nifwind
import lodgen_native_decode as dec
BS = chr(92)
D = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth'
RES = [l.strip() for l in open(r'E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/resources.txt') if l.strip()]
VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data'
L = dec.read_lodo(D + '/Commonwealth.lodo')
Ti = dec.read_lodi(D + '/Commonwealth.lodi')
names = [L['string_at'](m['modelStringOffset']) for m in L['meshes']]


def dx10_names(path):
    f = open(path, 'rb')
    magic, version, kind, n, nto = struct.unpack('<4sII I Q', f.read(24))
    if kind.to_bytes(4, 'little') != b'DX10':
        return None
    f.seek(nto)
    out = set()
    for _ in range(n):
        k = struct.unpack('<H', f.read(2))[0]
        out.add(f.read(k).decode('latin-1').replace('/', BS).lower())
    return out


arcs, texarcs = [], []
for r in reversed(RES):
    try:
        fs = sorted(x for x in os.listdir(r) if x.lower().endswith('.ba2'))
    except OSError:
        continue
    for x in fs:
        a = ba2lib.load(r + '/' + x)
        if a is not None:
            arcs.append((r + '/' + x, a))
        else:
            t = dx10_names(r + '/' + x)
            if t is not None:
                texarcs.append((r + '/' + x, t))


def norm(p, top):
    n = p.replace('/', BS).lower().lstrip(BS)
    k = n.find(top + BS)
    return n[k:] if k >= 0 else top + BS + n


def getfile(p, top):
    n = norm(p, top)
    for r in reversed(RES):
        q = r + '/' + n.replace(BS, '/')
        if os.path.isfile(q):
            return open(q, 'rb').read(), 'loose:' + os.path.basename(r)
    for an, a in arcs:
        if n in a['recs']:
            return ba2lib.get(a, n), os.path.basename(an)
    return None, None


texcache = {}


def gettex(p):
    """-> (float32 HxWx4 sRGB 0..1, box-reduced to ~256, source) or (None, why)"""
    n = norm(p, 'textures')
    if n in texcache:
        return texcache[n]
    res = (None, 'missing')
    for r in reversed(RES):
        q = r + '/' + n.replace(BS, '/')
        if os.path.isfile(q):
            res = (q, 'loose:' + os.path.basename(r))
            break
    if res[0] is None:
        for an, t in texarcs:
            if n in t:
                bn = os.path.basename(an)
                if bn.lower().startswith('fallout4 - ') or bn.lower().startswith('dlc'):
                    q = VAN + '/' + n.replace(BS, '/')
                    res = (q, bn) if os.path.isfile(q) else (None, 'vanilla-not-unpacked:' + bn)
                else:
                    res = (None, 'modarchive:' + bn)
                break
    if res[0] is not None:
        try:
            im = Image.open(res[0])
            im.load()
            im = im.convert('RGBA')
            w, h = im.size
            s = max(1, min(w, h) // 256)
            if s > 1:
                im = im.reduce(s)
            res = (np.asarray(im, np.float32) / 255.0, res[1])
        except Exception as e:
            res = (None, 'decode:' + str(e)[:40])
    texcache[n] = res
    return res


class R:
    def __init__(s, b):
        s.b = b
        s.o = 0

    def u8(s):
        v = s.b[s.o]
        s.o += 1
        return v

    def u32(s):
        v, = struct.unpack_from('<I', s.b, s.o)
        s.o += 4
        return v

    def f(s, n=1):
        v = struct.unpack_from('<%df' % n, s.b, s.o)
        s.o += 4 * n
        return v

    def st(s):
        n = s.u32()
        v = s.b[s.o:s.o + n].rstrip(b'\0').decode('latin-1')
        s.o += n
        return v


matcache = {}


def bgsm(name):
    """src/io/materialfile.cpp Material::readFile + ShaderMaterial::readFile replayed, up to the smoothness."""
    if name in matcache:
        return matcache[name]
    data, where = getfile(name, 'materials')
    out = None
    if data is not None and data[:4] == b'BGSM':
        r = R(data)
        r.o = 4
        v = r.u32()
        r.u32()
        r.f(4)
        r.f()
        r.u8(); r.u32(); r.u32()
        ref = r.u8()
        at = r.u8()
        for _ in range(9):
            r.u8()          # zwrite ztest ssr wssr decal twosided decalNoFade nonOccluder refraction
        r.u8()              # refraction falloff
        r.f()               # refraction power
        env = r.u8()
        if v < 10:
            r.f()
        g2p = r.u8()
        if v >= 6:
            r.u8()
        tex = [r.st() for _ in range(10 if v >= 17 else 9)]
        r.u8()
        if v >= 8:
            r.u8(); r.u8(); r.u8(); r.f(3); r.f(2)
        else:
            r.u8(); r.f(2); r.u8(); r.f()
        specOn = r.u8()
        specCol = r.f(3)
        specMult, smooth = r.f(2)
        # ... on to fGrayscaleToPaletteScale (materialfile.cpp order)
        r.f(); r.f(3)
        if v < 10:
            r.f()
        r.f(2)
        if v > 2:
            r.u8()
        if v >= 9:
            r.u8(); r.f()
        r.st()
        r.u8()
        emit = r.u8()
        if emit:
            r.f(3)
        r.f(); r.u8(); r.u8()
        if v >= 12:
            r.f()
        if v >= 13:
            r.u8(); r.f(3)
        if v < 8:
            r.u8()
        for _ in range(6):
            r.u8()
        if v < 7:
            r.u8(); r.u8()
        r.u8(); r.f(3)
        bools = [r.u8() for _ in range(4)]
        if v < 3:
            r.f(5)
        g2pScale = r.f()[0]
        ok = all(x in (0, 1) for x in bools) and 0.0 <= g2pScale <= 1.5
        out = dict(tex=tex, g2p=g2p, g2pScale=g2pScale, parseOk=ok, alphaTest=at, ref=ref, env=env, spec=specOn,
                   specCol=specCol, specMult=specMult, smooth=smooth, src=where, v=v)
    elif data is not None:
        out = dict(tex=[], g2p=0, alphaTest=0, ref=0, spec=0, src=where, bgem=True)
    matcache[name] = out
    return out


def shape_uv_tris(N, k):
    t, o, size = N.blocks[k]
    sh = N.shape(k)
    b = N.b
    if sh['pos'] is None:
        return None
    name, oo = N.avobject(o)
    oo += 16 + 12
    desc = struct.unpack_from('<Q', b, oo)[0]; oo += 8
    ntri = struct.unpack_from('<I', b, oo)[0]; oo += 4
    nv = struct.unpack_from('<H', b, oo)[0]; oo += 2
    oo += 4
    stride = (desc & 0xF) * 4
    raw = np.frombuffer(b, np.uint8, nv * stride, oo).reshape(nv, stride)
    uo = ((desc >> 8) & 0xF) * 4
    uv = raw[:, uo:uo + 4].copy().view(np.float16).reshape(nv, 2).astype(np.float32)
    tri = np.frombuffer(b, np.uint16, ntri * 3, oo + nv * stride).reshape(ntri, 3).astype(np.int64)
    return sh, uv, tri


rng = np.random.default_rng(1)


def s2l(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def l2s(c):
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * np.power(np.maximum(c, 0), 1 / 2.4) - 0.055)


def measure(data, K=6, useVC=True, keep=None):
    N = nifwind.Nif(data)
    acc = dict(area=0.0, lin=np.zeros(3), S=0.0, vcArea=0.0, g2pArea=0.0, noTexArea=0.0, bgemArea=0.0,
               mats=collections.Counter(), why=collections.Counter())
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES:
            continue
        got = shape_uv_tris(N, k)
        if got is None:
            continue
        sh, uv, tri = got
        if not (0 <= sh['shader'] < len(N.blocks)):
            continue
        f1, f2, mat = N.shader_flags(sh['shader'])
        P = sh['pos'][tri]
        area = 0.5 * np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1)
        A = float(area.sum())
        if A <= 0:
            continue
        M = bgsm(mat) if mat else None
        if M is None or M.get('bgem') or not M['tex'] or not M['tex'][0]:
            key = 'bgemArea' if (M and M.get('bgem')) else 'noTexArea'
            acc[key] += A
            acc['why']['nomat' if M is None else 'bgem' if M.get('bgem') else 'notex'] += 1
            continue
        img, why = gettex(M['tex'][0])
        if img is None:
            acc['noTexArea'] += A
            acc['why'][why] += 1
            continue
        acc['mats'][(mat.lower(), why)] += A
        if M['g2p']:
            acc['g2pArea'] += A
        n = len(tri)
        u = rng.random((n, K))
        v = rng.random((n, K))
        flip = u + v > 1
        u[flip] = 1 - u[flip]
        v[flip] = 1 - v[flip]
        w0 = 1 - u - v
        UV = uv[tri]
        su = w0 * UV[:, 0, 0, None] + u * UV[:, 1, 0, None] + v * UV[:, 2, 0, None]
        sv = w0 * UV[:, 0, 1, None] + u * UV[:, 1, 1, None] + v * UV[:, 2, 1, None]
        H, W = img.shape[:2]
        x = np.floor(np.mod(su, 1.0) * W).astype(int) % W
        y = np.floor(np.mod(sv, 1.0) * H).astype(int) % H
        c = img[y, x]
        rgb = c[..., :3]
        wgt = np.repeat(area[:, None] / K, K, axis=1)
        if M['alphaTest']:
            wgt = wgt * (c[..., 3] * 255 >= max(M['ref'], 1))
        vc = useVC and sh['cols'] is not None and bool((f2 or 0) & 0x20)
        if vc:
            C = sh['cols'][:, :3].astype(np.float32) / 255.0
            CV = C[tri]
            vcol = w0[..., None] * CV[:, 0, None, :] + u[..., None] * CV[:, 1, None, :] + v[..., None] * CV[:, 2, None, :]
            rgb = rgb * vcol
            acc['vcArea'] += A
        if keep is not None:
            keep.append((rgb.reshape(-1, 3), wgt.reshape(-1)))
        lin = s2l(rgb)
        mx = rgb.max(-1)
        mn = rgb.min(-1)
        S = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0)
        acc['area'] += float(wgt.sum())
        acc['lin'] += (lin * wgt[..., None]).reshape(-1, 3).sum(0)
        acc['S'] += float((S * wgt).sum())
    return acc


def summ(acc):
    if acc['area'] <= 0:
        return None
    lin = acc['lin'] / acc['area']
    s = l2s(lin)
    Smean = (s.max() - s.min()) / max(s.max(), 1e-6)
    Y = float(0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2])
    return dict(lin=lin, srgb=s, Y=Y, S_of_mean=float(Smean), mean_S=acc['S'] / acc['area'])


def fullname(lodname):
    n = lodname.replace('/', BS)
    n = re.sub(r'(?i)^(meshes\\)?lod\\', '', n)
    return re.sub(r'(?i)_?lod(_?\d+)?(\.nif)$', r'\2', n)


def is_building(n):
    l = n.lower().replace('/', BS)
    return l.startswith('lod' + BS + 'architecture' + BS) or l.startswith('lod' + BS + 'buildings' + BS)


if __name__ == '__main__':
    import pickle
    MODL = pickle.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'stat_modl.pkl'), 'rb'))
    place = collections.Counter(r['baseId'] for r in Ti['instances'])
    # unit = one (LOD slot-0 mesh, full-detail MODL) pair, weighted by the placements of every base that has it;
    # the full model is the base record's own MODL in Fallout4.esm (load index 00 only; others counted, not guessed)
    per = collections.Counter()
    skipped = collections.Counter()
    for b, c in place.items():
        B = L['bases'][b]
        ms = [B['rep%d' % k] for k in range(4) if B['rep%d' % k] != 0xFFFF]
        if not ms or not is_building(names[ms[0]]):
            continue
        fid = B['formId']
        if (fid >> 24) != 0 or fid not in MODL:
            skipped['notFallout4esm' if (fid >> 24) else 'noMODL'] += c
            continue
        per[(ms[0], MODL[fid])] += c
    print('building placements not paired:', dict(skipped))
    bld = sorted(((c, k) for k, c in per.items()), reverse=True)
    maxM = int(sys.argv[2]) if len(sys.argv) > 2 else 10 ** 9
    print('building LOD models placed (slot 0): %d, placements %d' % (len(bld), sum(c for c, m in bld)))
    out = open(sys.argv[1], 'w')
    out.write('placements\tlod\tfull\tlodY\tfullY\tfullY_noVC\tlodS_of_mean\tfullS_of_mean\tfullS_of_mean_noVC\tlod_meanS\t'
              'full_meanS\tfull_meanS_noVC\tlod_srgb\tfull_srgb\tfull_vcShare\tfull_g2pShare\tlod_mats\tfull_texsrc\n')
    tot = collections.Counter()
    W = collections.defaultdict(float)
    for c, (m, fn) in bld[:maxM]:
        lod, lsrc = getfile(names[m], 'meshes')
        if lod is None:
            tot['lodMissing'] += c
            continue
        la = measure(lod)
        ls = summ(la)
        fd, fsrc = getfile(fn, 'meshes')
        if fd is None:
            tot['fullMissingPlacements'] += c
            tot['fullMissingModels'] += 1
            continue
        fa = measure(fd)
        fs = summ(fa)
        fa0 = measure(fd, useVC=False)
        fs0 = summ(fa0)
        if ls is None or fs is None:
            tot['unmeasured'] += c
            continue
        tot['pairs'] += 1
        tot['pairPlacements'] += c
        for key, val in (('lodY', ls['Y']), ('fullY', fs['Y']), ('fullY0', fs0['Y']), ('lodS', ls['S_of_mean']),
                         ('fullS', fs['S_of_mean']), ('fullS0', fs0['S_of_mean']), ('lodmS', ls['mean_S']),
                         ('fullmS', fs['mean_S']), ('fullmS0', fs0['mean_S'])):
            W[key] += c * val
        W['w'] += c
        fmt = lambda a: '%.3f,%.3f,%.3f' % tuple(a)
        lm = ';'.join(os.path.basename(k[0]) for k, _ in la['mats'].most_common(3))
        ftx = ';'.join(sorted(set(k[1] for k in fa['mats'])))
        out.write('%d\t%s\t%s\t%.4f\t%.4f\t%.4f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%s\t%s\t%.2f\t%.2f\t%s\t%s\n' % (
            c, names[m], fn, ls['Y'], fs['Y'], fs0['Y'], ls['S_of_mean'], fs['S_of_mean'], fs0['S_of_mean'],
            ls['mean_S'], fs['mean_S'], fs0['mean_S'], fmt(ls['srgb']), fmt(fs['srgb']),
            fa['vcArea'] / max(fa['area'], 1e-9), fa['g2pArea'] / max(fa['area'], 1e-9), lm, ftx))
        out.flush()
    print(dict(tot))
    w = W['w']
    if w:
        print('placement-weighted over %d placements:' % w)
        print('  luminance Y (linear):           lod %.4f | full %.4f (no VC %.4f)' % (W['lodY'] / w, W['fullY'] / w, W['fullY0'] / w))
        print('  saturation of the mean colour:  lod %.3f | full %.3f (no VC %.3f)' % (W['lodS'] / w, W['fullS'] / w, W['fullS0'] / w))
        print('  mean per-texel saturation:      lod %.3f | full %.3f (no VC %.3f)' % (W['lodmS'] / w, W['fullmS'] / w, W['fullmS0'] / w))
    print('texture sources seen:', collections.Counter(v[1] for v in texcache.values()).most_common(8))

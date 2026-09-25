"""Boston look, measurement only (coordinator 2026-09-25): what vanilla's own object-LOD chunks carry that could
tint them -- vertex colour stream, Vertex_Colors bit, shader flags -- over the downtown box (dim-4 chunks x -8..0,
y -12..-4). Read in place from the unpacked vanilla data; prints one census, no pictures."""
import sys, glob, collections, os, struct
import numpy as np
sys.path.insert(0, r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools')
import nifwind
D = 'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth/Objects/'
c = collections.Counter(); cols = []; f1s = collections.Counter(); f2s = collections.Counter(); tex = collections.Counter(); emis = collections.Counter()
for x in (-8, -4, 0):
    for y in (-12, -8, -4):
        p = D + 'Commonwealth.4.%d.%d.BTO' % (x, y)
        if not os.path.exists(p):
            c['missing'] += 1; continue
        N = nifwind.Nif(open(p, 'rb').read())
        c['chunks'] += 1
        for k, (t, o, sz) in enumerate(N.blocks):
            if t in nifwind.SHAPES:
                sh = N.shape(k); c['shapes'] += 1; c['verts'] += sh['nv']
                f1, f2, m = N.shader_flags(sh['shader']) if 0 <= sh['shader'] < len(N.blocks) else (0, 0, '')
                f1s['0x%08X' % (f1 or 0)] += 1; f2s['0x%08X' % (f2 or 0)] += 1; tex[m] += 1
                if sh['cols'] is not None:
                    c['withStream'] += 1; cols.append(np.asarray(sh['cols'], np.float32))
                if (f2 or 0) & 0x20: c['VCbit'] += 1
                t2, o2, _ = N.blocks[sh['shader']]
                o2 += 4; _, o2 = N.objnet(o2); o2 += 8 + 16
                ts = struct.unpack_from('<i', N.b, o2)[0]; o2 += 4
                em = struct.unpack_from('<4f', N.b, o2)
                emis[tuple(round(v, 3) for v in em)] += 1
                if 0 <= ts < len(N.blocks):
                    tt, ot, _ = N.blocks[ts]; n = struct.unpack_from('<I', N.b, ot)[0]; ot += 4; ps = []
                    for _ in range(n):
                        L = struct.unpack_from('<I', N.b, ot)[0]; ps.append(N.b[ot + 4:ot + 4 + L].decode()); ot += 4 + L
                    tex[ps[0]] += 1
print(dict(c))
print('SLSF1', f1s.most_common(4)); print('SLSF2', f2s.most_common(4)); print('diffuse', tex.most_common(4)); print('emissive RGB + multiple', emis.most_common(4))
if cols:
    C = np.concatenate(cols); lum = C[:, :3] @ np.array([.2126, .7152, .0722])
    sat = (C[:, :3].max(1) - C[:, :3].min(1)) / np.maximum(C[:, :3].max(1), 1)
    print('stream RGBA mean %s sd %s | lum mean %.1f p5 %.1f p95 %.1f | sat mean %.3f | white share %.3f' % (
        C.mean(0).round(1), C.std(0).round(1), lum.mean(), np.percentile(lum, 5), np.percentile(lum, 95), sat.mean(),
        (C[:, :3] >= 254).all(1).mean()))

# the atlas those chunks sample, at the chunks' own vertex UVs (vertex-weighted), vs its whole-sheet stats
A = open('E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/Objects/Commonwealth.Objects.DDS', 'rb').read()
from PIL import Image
im = Image.open('E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/Objects/Commonwealth.Objects.DDS')
print('atlas', im.size, im.mode)
img = np.asarray(im.convert('RGB'), np.float32); hgt, wid = img.shape[:2]
uvs = []
for x in (-8, -4, 0):
    for y in (-12, -8, -4):
        N = nifwind.Nif(open(D + 'Commonwealth.4.%d.%d.BTO' % (x, y), 'rb').read())
        for k, (t, o, sz) in enumerate(N.blocks):
            if t in nifwind.SHAPES:
                sh = N.shape(k); desc = sh['desc']; stride = (desc & 0xF) * 4; uo = ((desc >> 4) & 0xF) * 4
                _, o2 = N.avobject(o); o2 += 16 + 12 + 8 + 4 + 2 + 4
                raw = np.frombuffer(N.b, np.uint8, sh['nv'] * stride, o2).reshape(sh['nv'], stride)
                uvs.append(raw[:, uo:uo + 4].copy().view(np.float16).reshape(-1, 2).astype(np.float32))
U = np.concatenate(uvs); U = U - np.floor(U)
px = img[np.clip((U[:, 1] * hgt).astype(int), 0, hgt - 1), np.clip((U[:, 0] * wid).astype(int), 0, wid - 1)]
W = np.array([.2126, .7152, .0722])
def st(P, tag):
    lum = P @ W; sat = (P.max(1) - P.min(1)) / np.maximum(P.max(1), 1)
    print('%-22s lum mean %.1f p5 %.1f p95 %.1f | sat mean %.3f | mean RGB %s' % (tag, lum.mean(), np.percentile(lum, 5),
          np.percentile(lum, 95), sat.mean(), P.mean(0).round(1)))
st(px, 'atlas @ Boston UVs')
st(img.reshape(-1, 3), 'atlas whole sheet')

"""Lane VKFAR2 (row G3): the job of tests/vkframe/farcards.cpp, written as files the C++ harness reads: the GL program
(the tree's impostor_instez.vert / impostor_cellinstez.frag with the loader's #include expansion and the WW_CELL_FOGLEAK
define the shipped SPIR-V twins), the fog's uniforms and its 5 textures (tests/vklight/frag_jobs.py's volsky job, as
farview_prep.py), the pass-wide card + interior cell uniforms, the global block (light 0, ambient, the tonemap scales),
and the card sheets as BC3 levels (the app's sheets are BC; numpy encodes them here). The cards themselves, their frame
tables, the cell's two lights and the shadow cube are made by the harness (seeded).

    python -I tests/vkframe/farcards_prep.py <out dir>
"""
import os, struct, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.abspath(os.path.join(HERE, '..', '..'))
FEAT = os.path.join(TREE, 'res', 'shaders', 'features')
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'vklight'))
OUT = os.path.abspath(sys.argv[1])
import shader_link_check as S  # noqa: E402
import frag_jobs as FJ  # noqa: E402

FMT_DT = {'rgba32f': np.float32, 'rgba16f': np.float32, 'r32f': np.float32, 'rg32f': np.float32, 'r8': np.uint8, 'rgba8': np.uint8}


class Tex:
    def __init__(self, kind, fmt, data, filt='linear', wrap='clamp', mips=0):
        a = np.asarray(data)
        if a.ndim == 2 + (kind != '2d'):
            a = a[..., None]
        if fmt == 'rgba16f':
            a = a.astype(np.float16).astype(np.float32)
        self.kind, self.fmt, self.filt, self.wrap, self.mips = kind, fmt, filt, wrap, mips
        self.data = np.ascontiguousarray(a.astype(FMT_DT[fmt]))
        if kind == '2d':
            self.h, self.w = a.shape[:2]; self.d = 1
        else:
            self.d, self.h, self.w = a.shape[:3]


FJ.Tex = Tex


def flat(v):
    return [float(x) for x in np.asarray(v, np.float64).reshape(-1)]


def write_src(path, src):
    with open(path, 'w', newline='\n', encoding='latin-1') as f:
        f.write(src)


# ---- BC3 (DXT5): alpha a0 255 / a1 0 (8-value mode), colour c0 > c1 (4-colour), nearest index per texel ----
A_PAL = np.array([255, 0, 219, 182, 146, 109, 73, 36], np.float64)


def bc3(img):
    """img: h x w x 4 floats 0..1 -> the level's BC3 bytes (blocks row by row, padded by edge copies)"""
    h, w = img.shape[:2]
    bh, bw = max(1, (h + 3) // 4), max(1, (w + 3) // 4)
    p = np.pad(img, ((0, bh * 4 - h), (0, bw * 4 - w), (0, 0)), mode='edge')
    blk = p.reshape(bh, 4, bw, 4, 4).transpose(0, 2, 1, 3, 4).reshape(bh, bw, 16, 4)
    out = np.zeros((bh, bw, 16), np.uint8)
    # alpha
    a = np.clip(blk[..., 3], 0.0, 1.0) * 255.0
    ai = np.abs(a[..., None] - A_PAL).argmin(-1).astype(np.uint64)
    bits = np.zeros((bh, bw), np.uint64)
    for i in range(16):
        bits |= ai[..., i] << np.uint64(3 * i)
    out[..., 0] = 255
    out[..., 1] = 0
    for k in range(6):
        out[..., 2 + k] = ((bits >> np.uint64(8 * k)) & np.uint64(255)).astype(np.uint8)
    # colour
    rgb = np.clip(blk[..., :3], 0.0, 1.0)
    lum = rgb @ np.array([0.3, 0.59, 0.11])
    hi = np.take_along_axis(rgb, lum.argmax(-1)[..., None, None].repeat(3, -1), -2)[..., 0, :]
    lo = np.take_along_axis(rgb, lum.argmin(-1)[..., None, None].repeat(3, -1), -2)[..., 0, :]

    def q565(c):
        r, g, b = np.round(c[..., 0] * 31), np.round(c[..., 1] * 63), np.round(c[..., 2] * 31)
        return (r.astype(np.int64) << 11) | (g.astype(np.int64) << 5) | b.astype(np.int64)

    def dq(v):
        return np.stack([((v >> 11) & 31) / 31.0, ((v >> 5) & 63) / 63.0, (v & 31) / 31.0], -1)
    v0, v1 = q565(hi), q565(lo)
    sw = v0 < v1
    v0, v1 = np.where(sw, v1, v0), np.where(sw, v0, v1)
    e0, e1 = dq(v0), dq(v1)
    pal = np.stack([e0, e1, (2 * e0 + e1) / 3, (e0 + 2 * e1) / 3], -2)  # bh bw 4 3
    d = ((rgb[..., :, None, :] - pal[..., None, :, :]) ** 2).sum(-1)
    ci = d.argmin(-1).astype(np.uint64)
    ci[v0 == v1] = 0
    cb = np.zeros((bh, bw), np.uint64)
    for i in range(16):
        cb |= ci[..., i] << np.uint64(2 * i)
    out[..., 8] = v0 & 255
    out[..., 9] = v0 >> 8
    out[..., 10] = v1 & 255
    out[..., 11] = v1 >> 8
    for k in range(4):
        out[..., 12 + k] = ((cb >> np.uint64(8 * k)) & np.uint64(255)).astype(np.uint8)
    return out.tobytes()


def levels(img):
    out = [img]
    while out[-1].shape[0] > 1 or out[-1].shape[1] > 1:
        p = out[-1]
        h, w = max(1, p.shape[0] // 2), max(1, p.shape[1] // 2)
        q = p[:h * 2, :w * 2].reshape(h, 2, w, 2, 4).mean((1, 3)) if p.shape[0] > 1 and p.shape[1] > 1 else p.reshape(h, w, 4)
        out.append(q)
    return out


def sheets(seed):
    """the four channels' pictures of a 256 x 256 sheet, 4 x 4 frames of 64 px: a covered blob per frame"""
    rng = np.random.default_rng(seed)
    col = np.zeros((256, 256, 4)); nrm = np.zeros((256, 256, 4)); msk = np.zeros((256, 256, 4))
    y, x = np.mgrid[0:64, 0:64] + 0.5
    for f in range(16):
        r0, c0 = (f // 4) * 64, (f % 4) * 64
        cx, cy = 32 + rng.uniform(-4, 4), 32 + rng.uniform(-3, 3)
        R = rng.uniform(17, 26)
        dx, dy = x - cx, y - cy
        rr = np.sqrt((dx / R) ** 2 + (dy / (R * rng.uniform(0.8, 1.15))) ** 2)
        holes = rng.uniform(0, 1, (64, 64))
        a = np.clip((1.0 - rr) * 5.0, 0.0, 1.0) * np.where(holes < 0.12, 0.35, 1.0)
        a[rr > 1.0] = 0.0
        col[r0:r0 + 64, c0:c0 + 64, 0] = 0.2 + 0.5 * x / 64
        col[r0:r0 + 64, c0:c0 + 64, 1] = 0.35 + 0.4 * y / 64
        col[r0:r0 + 64, c0:c0 + 64, 2] = 0.15 + 0.3 * f / 16
        col[r0:r0 + 64, c0:c0 + 64, 3] = a
        inside = np.clip(1.0 - rr * rr, 0.0, 1.0)
        nrm[r0:r0 + 64, c0:c0 + 64, 0] = 0.5 + 0.4 * np.clip(dx / R, -1, 1)
        nrm[r0:r0 + 64, c0:c0 + 64, 1] = 0.5 - 0.4 * np.clip(dy / R, -1, 1)
        nrm[r0:r0 + 64, c0:c0 + 64, 2] = 0.5 + 0.3 * inside * rng.uniform(-1, 1)
        nrm[r0:r0 + 64, c0:c0 + 64, 3] = 1.0 - y / 64
        msk[r0:r0 + 64, c0:c0 + 64, 0] = 0.5
        msk[r0:r0 + 64, c0:c0 + 64, 1] = 0.2
        msk[r0:r0 + 64, c0:c0 + 64, 2] = 0.35 + 0.65 * inside
        msk[r0:r0 + 64, c0:c0 + 64, 3] = 0.0
    return col, nrm, msk


j = FJ.volsky('farcards', 'air/cell_volsky_ps__WW_CELL_FOGLEAK.hlsl', ['WW_CELL_FOGLEAK'])
d = os.path.join(OUT, 'farcards')
os.makedirs(d, exist_ok=True)
FAR = os.path.join(FEAT, 'far-lod')
write_src(os.path.join(d, 'prog.vert.glsl'), S.inject(S.expand(os.path.join(FAR, 'impostor_instez.vert')), ['WW_CELL_FOGLEAK']))
write_src(os.path.join(d, 'prog.frag.glsl'), S.inject(S.expand(os.path.join(FAR, 'impostor_cellinstez.frag')), ['WW_CELL_FOGLEAK']))
U = dict(j['uniforms'])
U.update({
    # the card pass (impostordraw.cpp's pass-wide uniforms; the per-set ones and the view's are the harness's)
    'useHeightBlend': 1, 'depthSearchSteps': 4, 'useDepthOffset': 1, 'cutRule': 0, 'alphaThreshold': 0.5,
    'swayAmplitude': 0.3, 'emissiveScale': 1.0, 'cardMipCap': 8.0, 'debugChannel': 0, 'cardFadeOut': 0.0, 'cardFadeFrame': 0.0,
    'cardZcull': 0, 'cardZcullRed': 0, 'cardLightRed': 0, 'cardSunRed': 0, 'ussCloudOn': 0, 'useBakedAo': 1,
    # an interior cell: its 2 lights (one shadowed), the DALC ambient, no GI / cluster / imagespace
    'cellOn': 1, 'cellInterior': 1, 'cellLightCount': 2, 'cellClusterOn': 0, 'cellHasDalc': 1, 'cellExtDalc': 0,
    'cellDalc': [[0.05, 0.10, 0.02, 0.35], [0.05, 0.10, 0.02, 0.33], [0.04, 0.08, 0.03, 0.38]],
    'cellGiAmb': 0, 'cellGiOn': 0, 'cellHasDir': 0, 'cellSunScale': 1.0, 'cellShadowOn': 1, 'cellShadowTexel': 1.0 / 64.0,
    'cellShadowFadeOn': 0, 'cellIsOn': 0, 'cellIsLinear': 0, 'cellRed': 0, 'cellCtOn': 0, 'cellFarOn': 0, 'cellGoboOn': 0,
    'cellSsgiOn': 0, 'cellLightPhys': [0.0, 0.0, 0.0, 0.0],
})
with open(os.path.join(d, 'uni.txt'), 'w', newline='\n') as f:
    for k, v in U.items():
        f.write('%s %s\n' % (k, ' '.join(repr(x) for x in flat(v))))
T = j['textures']
with open(os.path.join(d, 'tex.txt'), 'w', newline='\n') as f:
    for k in ['atApTex', 'volAirTerr', 'volAirRainMap', 'volTex', 'volTile']:
        t = T[k]
        fn = 'tex_%s.bin' % k
        t.data.tofile(os.path.join(d, fn))
        f.write('%s %s %s %d %d %d %s %s %d %s\n' % (k, t.kind, t.fmt, t.w, t.h, t.d, t.filt, t.wrap, t.mips, fn))
g = bytearray(336)  # uniforms.glsl globalUniforms, std140
vm = np.asarray(U['viewMatrix'], np.float32).reshape(3, 3)
for c in range(3):
    struct.pack_into('<3f', g, 16 * c, *vm[c])
    struct.pack_into('<3f', g, 48 + 16 * c, *[1.0 if r == c else 0.0 for r in range(3)])
struct.pack_into('<16f', g, 96, *np.asarray(U['projectionMatrix'], np.float32))
L = np.array([0.3, 0.6, 0.7]); L /= np.linalg.norm(L)
struct.pack_into('<4f', g, 160, *L, 0.0)                 # lightSourcePosition[0], view space
struct.pack_into('<4f', g, 208, 0.8, 0.7, 0.6, 1.0)      # lightSourceDiffuse[0]
struct.pack_into('<4f', g, 256, 0.3, 0.3, 0.35, 1.0)     # lightSourceAmbient
struct.pack_into('<4f', g, 272, 1.0, 1.0, 1.0, 1.0)      # toneMapScale, brightnessScale, glowScale, glowScaleSRGB
struct.pack_into('<4i', g, 288, *[int(x) for x in U['viewportDimensions']])
open(os.path.join(d, 'global.bin'), 'wb').write(bytes(g))
# the sheets: name w h levels file (every level's BC3 blocks, level 0 first)
with open(os.path.join(d, 'sheets.txt'), 'w', newline='\n') as f:
    for s in range(2):
        col, nrm, msk = sheets(20261113 + s)
        for name, img in (('colour%d' % s, col), ('normal%d' % s, nrm), ('mask%d' % s, msk)):
            lv = levels(img)
            blob = b''.join(bc3(x) for x in lv)
            fn = 'sheet_%s.bin' % name
            open(os.path.join(d, fn), 'wb').write(blob)
            f.write('%s %d %d %d %s\n' % (name, img.shape[1], img.shape[0], len(lv), fn))
W, H = j['size']
open(os.path.join(d, 'job.txt'), 'w', newline='\n').write('%d %d\n' % (W, H))
print('farcards job: %dx%d, %d uniforms, 5 textures, 6 BC3 sheets' % (W, H, len(U)))

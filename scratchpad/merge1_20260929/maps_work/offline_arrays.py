# MERGE1 copy of MAPS1 offline_arrays.py: input paths come from env MAPS_CW (and MAPS_V2); nothing else changed.
"""offline_arrays.py -- MAPS1: the object-side texture arrays and impostor card sheets, decoded offline
(Pillow's BCn decoder, sharing no code with the viewer). SHEET SPACE, not top-down: these sheets are
not laid over the map, they are the textures the LOD meshes and the cards sample.

Channel roles (docs/LODGEN_IMPOSTOR_SPEC.md:64-69, docs/LODGEN_TEXTURE_ARRAYS.md:79-82):
  _d  BC3  RGB colour (sRGB), A coverage
  _n  BC3 (BC7 on a card)  R normal X, G normal Y (tangent space, Z = sqrt(1-x2-y2)), B height, A sway
  _gsaos BC3  R gloss, G specular, B AO, A subsurface mask
  _g  BC1  emissive colour
A MESH layer keeps height 128 and sway 0 on purpose (TEXTURE_ARRAYS.md:84) -- that is refuter A.
A CARD's normal at a silhouette edge must point OUT of the silhouette -- refuter B."""
import os, sys, glob, struct, json
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OBJ = os.environ['MAPS_CW'] + '/Objects'
PICS = os.path.join(HERE, 'pics')
BCN = {71: (1, 8), 72: (1, 8), 77: (3, 16), 78: (3, 16), 98: (7, 16), 99: (7, 16)}


def layers(path):
    """yield mip-0 RGBA arrays, one per array layer."""
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    mips = struct.unpack_from('<I', b, 28)[0]
    dxgi, _, _, arr = struct.unpack_from('<IIII', b, 128)
    n, bb = BCN[dxgi]
    stride = 0
    for m in range(mips):
        mw, mh = max(1, w >> m), max(1, h >> m)
        stride += max(1, (mw + 3) // 4) * max(1, (mh + 3) // 4) * bb
    m0 = ((w + 3) // 4) * ((h + 3) // 4) * bb
    assert 148 + stride * arr == len(b), (path, 148 + stride * arr, len(b))
    out = []
    for L in range(arr):
        o = 148 + L * stride
        im = Image.frombytes('RGBA', (w, h), b[o:o + m0], 'bcn', n)
        out.append(np.asarray(im))
    return out, dxgi


def glow(st, like):
    """MERGE1: lane TIDY1 writes no _g file for a set whose glow is black on every layer;
    such a set draws as all-black glow (dxgi reported as 0 = absent)."""
    p = os.path.join(OBJ, st + '_g.DDS')
    if os.path.exists(p):
        return layers(p)
    return [np.zeros_like(a) for a in like], 0


def sets(kind):
    s = sorted(set(os.path.basename(p).rsplit('_', 1)[0] for p in glob.glob(os.path.join(OBJ, 'Commonwealth.%s.*_d.DDS' % kind))))
    return s


res = {'arrays': {}, 'cards': {}}
# ---------------------------------------------------------------- census + refuters over EVERY set
edge = {'left': [], 'right': [], 'top': [], 'bottom': [], 'left_y': []}
for kind, key in (('LodgenArrays', 'arrays'), ('LodgenCards.legacy', 'cards')):
    for st in sets(kind):
        d, dd = layers(os.path.join(OBJ, st + '_d.DDS'))
        nn, nd = layers(os.path.join(OBJ, st + '_n.DDS'))
        g, gd = layers(os.path.join(OBJ, st + '_gsaos.DDS'))
        e, ed = glow(st, d)
        D = np.stack(d); N = np.stack(nn); G = np.stack(g); E = np.stack(e)
        cov = D[..., 3] > 127
        row = {'layers': len(d), 'dxgi_d_n_gsaos_g': [dd, nd, gd, ed], 'covered_texels': int(cov.sum()),
               'n_R_mean_cov': round(float(N[..., 0][cov].mean()), 2), 'n_G_mean_cov': round(float(N[..., 1][cov].mean()), 2),
               'n_B_values_cov': {int(k): int(v) for k, v in zip(*np.unique(N[..., 2][cov], return_counts=True))} if key == 'arrays' else
                                 [int(N[..., 2][cov].min()), int(N[..., 2][cov].max()), round(float(N[..., 2][cov].mean()), 1)],
               'n_A_range_cov': [int(N[..., 3][cov].min()), int(N[..., 3][cov].max()), round(float(N[..., 3][cov].mean()), 2)],
               'gsaos_mean_cov': [round(float(G[..., k][cov].mean()), 1) for k in range(4)],
               'gsaos_B_range_cov': [int(G[..., 2][cov].min()), int(G[..., 2][cov].max())],
               'g_max': int(E[..., :3].max()), 'g_nonblack_texels': int((E[..., :3].max(axis=-1) > 8).sum())}
        if key == 'arrays' and len(row['n_B_values_cov']) > 6:
            v = N[..., 2][cov]
            row['n_B_values_cov'] = {'min': int(v.min()), 'max': int(v.max()), 'share_128': round(float((v == 128).mean()), 4)}
        res[key][st.split('.', 1)[1]] = row
        if key == 'cards':
            # silhouette refuter: an edge texel's normal must point away from the silhouette
            x = (N[..., 0].astype(np.float64) - 127.5) / 127.5
            y = (N[..., 1].astype(np.float64) - 127.5) / 127.5
            l = cov.copy(); l[:, :, 1:] &= ~cov[:, :, :-1]; l[:, :, 0] = False
            r = cov.copy(); r[:, :, :-1] &= ~cov[:, :, 1:]; r[:, :, -1] = False
            t = cov.copy(); t[:, 1:, :] &= ~cov[:, :-1, :]; t[:, 0, :] = False
            bt = cov.copy(); bt[:, :-1, :] &= ~cov[:, 1:, :]; bt[:, -1, :] = False
            edge['left_y'].append(y[l])
            edge['left'].append(x[l]); edge['right'].append(x[r]); edge['top'].append(y[t]); edge['bottom'].append(y[bt])
        print(st, row['layers'], row['covered_texels'], flush=True)
ed = {k: np.concatenate(v) for k, v in edge.items()}
res['card_silhouette_refuter'] = {
    'left_edge_texels': int(ed['left'].size), 'left_mean_normalX': round(float(ed['left'].mean()), 3),
    'left_share_X_negative': round(float((ed['left'] < 0).mean()), 3),
    'right_edge_texels': int(ed['right'].size), 'right_mean_normalX': round(float(ed['right'].mean()), 3),
    'right_share_X_positive': round(float((ed['right'] > 0).mean()), 3),
    'top_edge_mean_normalY': round(float(ed['top'].mean()), 3), 'bottom_edge_mean_normalY': round(float(ed['bottom'].mean()), 3),
    'wrong_axis_left_mean_normalY': round(float(ed['left_y'].mean()), 3)}
json.dump(res, open(os.path.join(HERE, 'offline_arrays.json'), 'w'), indent=1)


# ---------------------------------------------------------------- mosaics (two sets), sheet space
def checker(h, w, sq=8):
    yy, xx = np.mgrid[0:h, 0:w]
    c = ((yy // sq + xx // sq) % 2).astype(np.uint8)
    return np.where(c[..., None] == 1, 150, 100).astype(np.uint8).repeat(3, axis=2)


def mosaic(ims, cols, cell):
    rows = (len(ims) + cols - 1) // cols
    page = Image.new('RGB', (cols * cell, rows * cell), (30, 30, 34))
    for i, a in enumerate(ims):
        im = Image.fromarray(a).resize((cell, cell), Image.BOX)
        page.paste(im, ((i % cols) * cell, (i // cols) * cell))
    return page


def views(st):
    d, _ = layers(os.path.join(OBJ, st + '_d.DDS'))
    nn, _ = layers(os.path.join(OBJ, st + '_n.DDS'))
    g, _ = layers(os.path.join(OBJ, st + '_gsaos.DDS'))
    e, _ = glow(st, d)
    out = {}
    colour, normal, height, sway, gloss, spec, ao, sss, emis = [], [], [], [], [], [], [], [], []
    for D, N, G, E in zip(d, nn, g, e):
        h, w = D.shape[:2]
        a = D[..., 3:4].astype(np.float32) / 255
        colour.append((D[..., :3] * a + checker(h, w) * (1 - a)).astype(np.uint8))
        x = (N[..., 0].astype(np.float32) - 127.5) / 127.5
        y = (N[..., 1].astype(np.float32) - 127.5) / 127.5
        z = np.sqrt(np.clip(1 - x * x - y * y, 0, 1))
        nm = np.stack([N[..., 0], N[..., 1], (z * 127.5 + 127.5).astype(np.uint8)], axis=2)
        m = D[..., 3] > 127
        normal.append(np.where(m[..., None], nm, 40).astype(np.uint8))
        for lst, ch in ((height, N[..., 2]), (sway, N[..., 3]), (gloss, G[..., 0]), (spec, G[..., 1]), (ao, G[..., 2]), (sss, G[..., 3])):
            lst.append(np.where(m, ch, 40).astype(np.uint8)[..., None].repeat(3, axis=2))
        emis.append(E[..., :3].copy())
    return {'colour_d': colour, 'normal_n_rebuiltZ': normal, 'height_nB': height, 'sway_nA': sway,
            'gloss_gsR': gloss, 'spec_gsG': spec, 'ao_gsB': ao, 'sss_gsA': sss, 'emissive_g': emis}


for st, tag, cols, cell in (('Commonwealth.LodgenArrays.512x512', 'A512', 7, 128),
                            ('Commonwealth.LodgenCards.legacy.2048x2048', 'C2048', 3, 300)):
    V = views(st)
    for name, ims in V.items():
        mosaic(ims, cols, cell).save(os.path.join(PICS, 'S_%s_%s.png' % (tag, name)))
print(json.dumps(res['card_silhouette_refuter'], indent=1))

"""DEFECT 2 -- the mechanism, its known-answer control, and the simulated repair.

THE MECHANISM, read out of the source and not guessed.
`lodgenEncodeBC1Block` (src/lodgen.cpp:4571) chooses a block's two endpoints as
the block's MIN- and MAX-LUMINANCE texels:

    l = 0.299 R + 0.587 G + 0.114 B

On the `_n` sheet R is normal X, G is normal Y and **B is the HEIGHT**, so the
endpoint choice weights the height at ELEVEN PER CENT and the normal at
eighty-nine. Two texels on opposite sides of a silhouette edge can differ by a
hundred levels of height and still have almost the same luminance, so neither
is chosen, the block's line never spans the height range, and every texel in
that 4x4 collapses toward one height. That is a 4x4-shaped artefact, which is
the shape IMPOSTORFIX3 photographed at the trunk.

THE CONTROL. This file re-implements that function exactly and re-encodes the
pre-compression PNG. If the model is right it reproduces the SHIPPED DDS colour
bytes. Until it does, nothing below counts.

THE REPAIR (no ruling needed, no format change): choose the endpoints on the
block's PRINCIPAL AXIS instead of on luminance -- the extreme texels along the
direction the block's own colours actually vary. On a height-cliff block that
axis IS the blue axis, so the line spans the height. Everything else about the
block -- 565 quantisation, the four-level palette, the nearest-palette index
search, the byte order -- is untouched.
"""
import os, sys, glob, json, struct
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inst4 import *

HERE = os.path.dirname(os.path.abspath(__file__))
TAGS = ('blast_n4', 'blast_n8', 'maple_n4', 'dead_n4', 'rock_n4')

# ---------------------------------------------------------------- DDS blocks
def dds_mip0_blocks(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<I', b, 12)[0], struct.unpack_from('<I', b, 16)[0]
    fcc = b[84:88]
    assert fcc == b'DXT5', fcc
    off = 128
    bw, bh = (w + 3) // 4, (h + 3) // 4
    raw = np.frombuffer(b[off:off + bw * bh * 16], np.uint8).reshape(bh, bw, 16)
    return raw, w, h, bw, bh

def to_blocks(img):
    """HxWx3 uint8 -> (bh,bw,16,3) in the encoder's y-major, x-minor order."""
    H, W, _ = img.shape
    bh, bw = H // 4, W // 4
    b = img[:bh * 4, :bw * 4].reshape(bh, 4, bw, 4, 3).transpose(0, 2, 1, 3, 4)
    return b.reshape(bh, bw, 16, 3)

def pack565(rgb):
    r = (rgb[..., 0].astype(np.uint32) >> 3) & 31
    g = (rgb[..., 1].astype(np.uint32) >> 2) & 63
    bl = (rgb[..., 2].astype(np.uint32) >> 3) & 31
    return ((r << 11) | (g << 5) | bl).astype(np.uint16)

def unpack565_enc(v):
    """The ENCODER's own expansion (src/lodgen.cpp:4646): value * 255/31 etc."""
    v = v.astype(np.float64)
    return np.stack([((v.astype(np.uint32) >> 11) & 31) * (255.0 / 31.0),
                     ((v.astype(np.uint32) >> 5) & 63) * (255.0 / 63.0),
                     (v.astype(np.uint32) & 31) * (255.0 / 31.0)], -1)

def unpack565_gpu(v):
    """What the GPU/decoder does: replicate the high bits."""
    v = v.astype(np.uint32)
    r = (v >> 11) & 31; g = (v >> 5) & 63; b = v & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.float64)

def encode_colour(blk, mode='lum'):
    """blk: (...,16,3) float. Returns (c0, c1, bits) exactly as
       lodgenEncodeBC1Block writes them with allowPunch = false."""
    sh = blk.shape[:-2]
    V = blk.reshape(-1, 16, 3)
    if mode == 'lum':
        l = 0.299 * V[..., 0] + 0.587 * V[..., 1] + 0.114 * V[..., 2]
        lo = l.argmin(1); hi = l.argmax(1)
    else:  # 'pca' -- the repair: extremes along the block's own principal axis
        mu = V.mean(1, keepdims=True); X = V - mu
        C = np.einsum('bij,bik->bjk', X, X)
        a = np.tile(np.array([0.299, 0.587, 0.114]), (V.shape[0], 1))
        for _ in range(24):
            a = np.einsum('bjk,bk->bj', C, a)
            n = np.linalg.norm(a, axis=1, keepdims=True); n[n == 0] = 1
            a = a / n
        t = np.einsum('bij,bj->bi', X, a)
        lo = t.argmin(1); hi = t.argmax(1)
    idx = np.arange(V.shape[0])
    c0 = pack565(V[idx, hi]); c1 = pack565(V[idx, lo])
    # 4-colour mode: c0 must be > c1 (BC3 colour blocks never punch through)
    sw = c0 < c1
    c0n = np.where(sw, c1, c0).astype(np.uint16); c1n = np.where(sw, c0, c1).astype(np.uint16)
    p0 = unpack565_enc(c0n); p1 = unpack565_enc(c1n)
    pal = np.stack([p0, p1, (2 * p0 + p1) / 3.0, (p0 + 2 * p1) / 3.0], 1)   # (B,4,3)
    d = ((V[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)               # (B,16,4)
    sel = d.argmin(-1)
    same = c0n == c1n
    sel = np.where(same[:, None], 0, sel)
    bits = np.zeros(V.shape[0], np.uint32)
    for i in range(15, -1, -1):
        bits = (bits << np.uint32(2)) | sel[:, i].astype(np.uint32)
    bits = np.where(same, 0, bits).astype(np.uint32)
    return c0n.reshape(sh), c1n.reshape(sh), bits.reshape(sh)

def decode_colour(c0, c1, bits):
    """(bh,bw) -> (bh,bw,16,3) as the GPU decodes a 4-colour BC1 block."""
    p0 = unpack565_gpu(c0); p1 = unpack565_gpu(c1)
    pal = np.stack([p0, p1, (2 * p0 + p1) / 3.0, (p0 + 2 * p1) / 3.0], -2)   # (...,4,3)
    sel = np.stack([(bits >> np.uint32(2 * i)) & np.uint32(3) for i in range(16)], -1)
    return np.take_along_axis(pal[..., None, :, :],
                              sel[..., None, None], -2)[..., 0, :]

def blocks_to_img(b, H, W):
    bh, bw = b.shape[:2]
    return b.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 3)

# ---------------------------------------------------------------------------
rows = []
sheets = {}
for tag in TAGS:
    cs = Sheets(tag, 'cards', root=R3, sub='fixture')
    pn = np.asarray(Image.open(glob.glob(cs.dir + '*_oct_normal.png')[0]).convert('RGBA'))
    raw, w, h, bw, bh = dds_mip0_blocks(cs.nrmPath)
    shipped_c0 = raw[..., 8].astype(np.uint16) | (raw[..., 9].astype(np.uint16) << 8)
    shipped_c1 = raw[..., 10].astype(np.uint16) | (raw[..., 11].astype(np.uint16) << 8)
    shipped_bits = (raw[..., 12].astype(np.uint32) | (raw[..., 13].astype(np.uint32) << 8)
                    | (raw[..., 14].astype(np.uint32) << 16) | (raw[..., 15].astype(np.uint32) << 24))

    B = to_blocks(pn[..., :3].astype(np.float64))
    c0, c1, bits = encode_colour(B, 'lum')

    # ---- KNOWN-ANSWER CONTROL: does the model reproduce the shipped bytes?
    okE = float((( c0 == shipped_c0) & (c1 == shipped_c1)).mean())
    okB = float((bits == shipped_bits).mean())

    # ---- the repair
    r0, r1, rbits = encode_colour(B, 'pca')
    dec_l = blocks_to_img(decode_colour(c0, c1, bits), h, w)
    dec_p = blocks_to_img(decode_colour(r0, r1, rbits), h, w)
    ref = pn[:dec_l.shape[0], :dec_l.shape[1], 2].astype(np.float64)
    eL = np.abs(dec_l[..., 2] - ref); eP = np.abs(dec_p[..., 2] - ref)
    # normal X/Y cost of the repair
    nL = np.abs(dec_l[..., :2] - pn[:dec_l.shape[0], :dec_l.shape[1], :2]).mean()
    nP = np.abs(dec_p[..., :2] - pn[:dec_l.shape[0], :dec_l.shape[1], :2]).mean()

    # the repaired sheet, in the instrument's 0..1 layout, alpha kept from the DDS
    rep = cs.nrm.copy()
    rep[:dec_p.shape[0], :dec_p.shape[1], :3] = dec_p / 255.0
    sheets[tag] = rep
    np.save(os.path.join(HERE, 'nrm_pca_%s.npy' % tag), rep)

    rows.append(dict(tag=tag, endpoints_match=okE, bits_match=okB,
                     lum_mean=float(eL.mean()), pca_mean=float(eP.mean()),
                     lum_p95=float(np.percentile(eL, 95)), pca_p95=float(np.percentile(eP, 95)),
                     lum_worst=float(eL.max()), pca_worst=float(eP.max()),
                     nxy_lum=float(nL), nxy_pca=float(nP)))
    print(tag, 'done', flush=True)

print()
print('KNOWN-ANSWER CONTROL: my model of lodgenEncodeBC1Block vs the SHIPPED `_n` bytes')
print('%-10s %18s %14s' % ('tag', 'endpoints identical', 'indices identical'))
for r in rows:
    print('%-10s %17.2f%% %13.2f%%' % (r['tag'], 100 * r['endpoints_match'], 100 * r['bits_match']))
print()
print('HEIGHT (`_n` blue) round-trip error in levels -- SIMULATED repair')
print('%-10s %8s %8s | %8s %8s | %8s %8s | %9s %9s' %
      ('tag', 'mean L', 'mean PCA', 'p95 L', 'p95 PCA', 'worst L', 'worst PCA', 'nXY L', 'nXY PCA'))
for r in rows:
    print('%-10s %8.2f %8.2f | %8.2f %8.2f | %8.0f %8.0f | %9.2f %9.2f' %
          (r['tag'], r['lum_mean'], r['pca_mean'], r['lum_p95'], r['pca_p95'],
           r['lum_worst'], r['pca_worst'], r['nxy_lum'], r['nxy_pca']))
json.dump(rows, open(os.path.join(HERE, 's2c_encoder.json'), 'w'), indent=1)

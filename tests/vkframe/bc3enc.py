"""Lane VKFAR2: the numpy BC3 (DXT5) encoder the vkframe preps share (farcards_prep.py's sheets, precipview_prep.py's
atlases, the app's textures are BC). Alpha a0 255 / a1 0 (8-value mode), colour c0 > c1 (4-colour), nearest index per
texel. levels() halves to 1 x 1 and keeps non-square pictures (a 1-row level halves along the other axis only).
"""
import numpy as np

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
    """every level down to 1 x 1 (GL's count: floor(log2(max(w, h))) + 1), a 2 x 2 box mean; once one side is 1 the
    other halves alone (a 1 x 4 level -> 1 x 2)"""
    out = [img]
    while out[-1].shape[0] > 1 or out[-1].shape[1] > 1:
        p = out[-1]
        h, w = max(1, p.shape[0] // 2), max(1, p.shape[1] // 2)
        if p.shape[0] > 1 and p.shape[1] > 1:
            q = p[:h * 2, :w * 2].reshape(h, 2, w, 2, 4).mean((1, 3))
        elif p.shape[0] > 1:
            q = p[:h * 2].reshape(h, 2, 1, 4).mean(1)
        else:
            q = p[:, :w * 2].reshape(1, w, 2, 4).mean(2)
        out.append(q)
    return out

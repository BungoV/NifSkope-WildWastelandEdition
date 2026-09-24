#!/usr/bin/env python3
"""A tiny wireframe-over-grey rasteriser.  numpy + PIL only, no NifSkope, no GL.

Every panel is framed on the mesh's own bounding sphere at a fixed angular
size, so two meshes with different world scales come out at the SAME on-screen
size and can be compared triangle for triangle.
"""
import numpy as np
from PIL import Image, ImageDraw, ImageFont

BG = 250
LINE = (20, 20, 25)


def look_at(eye, target, up=(0, 0, 1)):
    f = np.array(target, float) - np.array(eye, float)
    f /= np.linalg.norm(f)
    up = np.array(up, float)
    r = np.cross(f, up)
    if np.linalg.norm(r) < 1e-9:
        r = np.cross(f, [0, 1.0, 0])
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    return np.stack([r, u, -f])


def project(V, eye, target, fov_deg, W, H):
    R = look_at(eye, target)
    C = (np.asarray(V, float) - np.asarray(eye, float)) @ R.T
    z = -C[:, 2]
    t = np.tan(np.radians(fov_deg) * 0.5)
    with np.errstate(divide='ignore', invalid='ignore'):
        x = C[:, 0] / (z * t * (W / float(H)))
        y = C[:, 1] / (z * t)
    px = (x * 0.5 + 0.5) * W
    py = (0.5 - y * 0.5) * H
    return px, py, z


def render(V, T, W=900, H=900, azim=40.0, elev=28.0, fov=28.0, fill=True,
           wire=True, line_alpha=1.0, up=(0, 0, 1)):
    V = np.asarray(V, float)
    T = np.asarray(T, np.int64).reshape(-1, 3)
    c = 0.5 * (V.min(axis=0) + V.max(axis=0))
    rad = float(np.linalg.norm(V - c, axis=1).max())
    if rad <= 0:
        rad = 1.0
    d = rad / np.sin(np.radians(fov) * 0.5) * 1.06
    a, e = np.radians(azim), np.radians(elev)
    eye = c + d * np.array([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a), np.sin(e)])

    px, py, z = project(V, eye, c, fov, W, H)
    depth = np.full((H, W), np.inf)
    shade = np.full((H, W), float(BG))

    p0, p1, p2 = V[T[:, 0]], V[T[:, 1]], V[T[:, 2]]
    nrm = np.cross(p1 - p0, p2 - p0)
    nl = np.linalg.norm(nrm, axis=1)
    nl[nl == 0] = 1
    nrm = nrm / nl[:, None]
    ldir = np.array([0.42, 0.30, 0.86])
    ldir = ldir / np.linalg.norm(ldir)
    lam = np.abs(nrm @ ldir)
    grey = 118 + 105 * lam ** 0.8

    if fill:
        X = px[T]
        Y = py[T]
        Z = z[T]
        order = np.argsort(-Z.mean(axis=1))
        for k in order:
            x0, x1, x2 = X[k]
            y0, y1, y2 = Y[k]
            if not np.isfinite([x0, x1, x2, y0, y1, y2]).all():
                continue
            xa, xb = int(max(0, np.floor(min(x0, x1, x2)))), int(min(W - 1, np.ceil(max(x0, x1, x2))))
            ya, yb = int(max(0, np.floor(min(y0, y1, y2)))), int(min(H - 1, np.ceil(max(y0, y1, y2))))
            if xb < xa or yb < ya:
                continue
            xs = np.arange(xa, xb + 1) + 0.5
            ys = np.arange(ya, yb + 1) + 0.5
            gx, gy = np.meshgrid(xs, ys)
            den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
            if abs(den) < 1e-12:
                continue
            l0 = ((y1 - y2) * (gx - x2) + (x2 - x1) * (gy - y2)) / den
            l1 = ((y2 - y0) * (gx - x2) + (x0 - x2) * (gy - y2)) / den
            l2 = 1.0 - l0 - l1
            m = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
            if not m.any():
                continue
            zz = l0 * Z[k, 0] + l1 * Z[k, 1] + l2 * Z[k, 2]
            sub = depth[ya:yb + 1, xa:xb + 1]
            hit = m & (zz < sub)
            if hit.any():
                sub[hit] = zz[hit]
                shade[ya:yb + 1, xa:xb + 1][hit] = grey[k]

    img = np.repeat(shade[:, :, None], 3, axis=2).astype(np.uint8)

    if wire:
        E = np.unique(np.sort(np.vstack([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]]), axis=1), axis=0)
        A, B_ = E[:, 0], E[:, 1]
        good = np.isfinite(px[A]) & np.isfinite(px[B_]) & (z[A] > 0) & (z[B_] > 0)
        A, B_ = A[good], B_[good]
        steps = np.maximum(np.abs(px[A] - px[B_]), np.abs(py[A] - py[B_])).astype(int) + 1
        steps = np.minimum(steps, 4000)
        for a_, b_, ns in zip(A, B_, steps):
            t = np.linspace(0, 1, ns)
            xs = px[a_] + (px[b_] - px[a_]) * t
            ys = py[a_] + (py[b_] - py[a_]) * t
            zs = z[a_] + (z[b_] - z[a_]) * t
            xi = np.clip(xs.astype(int), 0, W - 1)
            yi = np.clip(ys.astype(int), 0, H - 1)
            ok = (xs >= 0) & (xs < W) & (ys >= 0) & (ys < H)
            ok &= zs <= depth[yi, xi] * 1.004 + 1e-4
            if ok.any():
                img[yi[ok], xi[ok]] = LINE
    return Image.fromarray(img)


def trim(im, margin=0.03):
    """Crop to the drawn content, then pad back to a square so that panels put
    side by side still show their meshes at a comparable on-screen size."""
    a = np.asarray(im.convert('L'))
    m = a < BG - 2
    if not m.any():
        return im
    ys, xs = np.where(m)
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    cx, cy = (x0 + x1) * 0.5, (y0 + y1) * 0.5
    half = 0.5 * max(x1 - x0, y1 - y0) * (1 + 2 * margin)
    box = (int(cx - half), int(cy - half), int(cx + half), int(cy + half))
    out = Image.new('RGB', (box[2] - box[0], box[3] - box[1]), (BG, BG, BG))
    src = im.crop((max(0, box[0]), max(0, box[1]),
                   min(im.width, box[2]), min(im.height, box[3])))
    out.paste(src, (max(0, -box[0]), max(0, -box[1])))
    return out


def _font(sz):
    for p in (r'C:\Windows\Fonts\segoeui.ttf', r'C:\Windows\Fonts\arial.ttf'):
        try:
            return ImageFont.truetype(p, sz)
        except Exception:
            pass
    return ImageFont.load_default()


def sheet(panels, out, title='', panel_w=760, panel_h=760, cap_h=104, cols=None):
    """panels = [(PIL.Image, caption lines list)]"""
    cols = cols or len(panels)
    rows = (len(panels) + cols - 1) // cols
    top = 58 if title else 12
    W = cols * panel_w + (cols + 1) * 14
    H = top + rows * (panel_h + cap_h) + 14
    sh = Image.new('RGB', (W, H), (255, 255, 255))
    d = ImageDraw.Draw(sh)
    if title:
        d.text((16, 14), title, fill=(15, 15, 20), font=_font(28))
    ft = _font(21)
    for i, (im, caption) in enumerate(panels):
        r, c = divmod(i, cols)
        x = 14 + c * (panel_w + 14)
        y = top + r * (panel_h + cap_h)
        sh.paste(im.resize((panel_w, panel_h), Image.LANCZOS), (x, y))
        d.rectangle([x, y, x + panel_w - 1, y + panel_h - 1], outline=(180, 180, 185))
        for j, line in enumerate(caption):
            d.text((x + 6, y + panel_h + 6 + j * 25), line, fill=(15, 15, 20), font=ft)
    sh.save(out)
    return out

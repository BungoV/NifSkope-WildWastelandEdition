"""Lane SHOWCASE1 -- shared picture helpers.

Every picture this lane ships is built through here so a caption number and the
number in the report come from ONE piece of arithmetic.
"""
import os
import struct

import numpy as np
from PIL import Image, ImageDraw, ImageFont

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912'
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'

_FONTS = ('C:/Windows/Fonts/consola.ttf', 'C:/Windows/Fonts/arial.ttf')


def font(size):
    for p in _FONTS:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


# ---------------------------------------------------------------- DDS reading
def dds_rgba(path, mip=0):
    """Top-mip RGBA as a uint8 HxWx4 array.

    PIL handles every block codec these sheets use EXCEPT the uncompressed
    DX10 B8G8R8A8 the `--msn-cache` writes (dxgi 87), which PIL refuses by
    name; that one is read here straight out of the file.
    """
    with open(path, 'rb') as fh:
        head = fh.read(148)
        if head[:4] != b'DDS ':
            raise ValueError('%s is not a DDS' % path)
        h = struct.unpack_from('<I', head, 12)[0]
        w = struct.unpack_from('<I', head, 16)[0]
        fourcc = head[84:88]
        if fourcc == b'DX10':
            dxgi = struct.unpack_from('<I', head, 128)[0]
            if dxgi in (71, 72, 74, 75, 77, 78):   # BC1 / BC2 / BC3, UNORM or sRGB
                fh.seek(148)
                stride = 8 if dxgi in (71, 72) else 16
                rgb = _bc_colour(fh.read(((w + 3) // 4) * ((h + 3) // 4) * stride),
                                 w, h, stride)
                return np.dstack([rgb, np.full((h, w), 255, np.uint8)])
            if dxgi not in (87, 88, 28, 29):     # BGRA8 / BGRX8 / RGBA8
                raise ValueError('%s: unhandled dxgi %d' % (path, dxgi))
            fh.seek(148)
            buf = fh.read(w * h * 4)
            a = np.frombuffer(buf, np.uint8).reshape(h, w, 4)
            if dxgi in (87, 88):                 # BGRA -> RGBA
                a = a[:, :, [2, 1, 0, 3]]
            return np.ascontiguousarray(a)
    im = Image.open(path)
    im.load()
    return np.asarray(im.convert('RGBA'))


def _bc_colour(buf, w, h, stride):
    """RGB of a BC1/BC2/BC3 surface, colour block only, vectorised.

    stride = 8 for BC1, 16 for BC2/BC3 (the colour block is the LAST 8 bytes of
    a 16-byte block).  Alpha is not decoded -- every panel that uses this shows
    colour.
    """
    bw, bh = (w + 3) // 4, (h + 3) // 4
    blocks = np.frombuffer(buf[:bw * bh * stride], np.uint8).reshape(bh, bw, stride)
    if stride == 16:
        blocks = blocks[:, :, 8:]
    c0 = blocks[:, :, 0].astype(np.uint16) | (blocks[:, :, 1].astype(np.uint16) << 8)
    c1 = blocks[:, :, 2].astype(np.uint16) | (blocks[:, :, 3].astype(np.uint16) << 8)
    bits = (blocks[:, :, 4].astype(np.uint32) | (blocks[:, :, 5].astype(np.uint32) << 8)
            | (blocks[:, :, 6].astype(np.uint32) << 16) | (blocks[:, :, 7].astype(np.uint32) << 24))

    def unpack(c):
        r = ((c >> 11) & 0x1F).astype(np.float32) * (255.0 / 31.0)
        g = ((c >> 5) & 0x3F).astype(np.float32) * (255.0 / 63.0)
        b = (c & 0x1F).astype(np.float32) * (255.0 / 31.0)
        return np.stack([r, g, b], -1)

    e0, e1 = unpack(c0), unpack(c1)
    pal = np.zeros(e0.shape[:2] + (4, 3), np.float32)
    pal[:, :, 0], pal[:, :, 1] = e0, e1
    gt = (c0 > c1)[:, :, None]
    pal[:, :, 2] = np.where(gt, (2 * e0 + e1) / 3.0, (e0 + e1) / 2.0)
    pal[:, :, 3] = np.where(gt, (e0 + 2 * e1) / 3.0, 0.0)
    out = np.zeros((bh, 4, bw, 4, 3), np.uint8)
    for y in range(4):
        for x in range(4):
            idx = ((bits >> (2 * (4 * y + x))) & 3)
            out[:, y, :, x] = np.take_along_axis(
                pal, idx[:, :, None, None], 2)[:, :, 0].astype(np.uint8)
    return out.reshape(bh * 4, bw * 4, 3)[:h, :w]


def dds_kind(path):
    """The one-word format of a DDS, for a caption."""
    with open(path, 'rb') as fh:
        head = fh.read(148)
    fourcc = head[84:88]
    if fourcc == b'DX10':
        d = struct.unpack_from('<I', head, 128)[0]
        n = {71: 'BC1', 72: 'BC1 sRGB', 74: 'BC2', 75: 'BC2 sRGB', 77: 'BC3',
             78: 'BC3 sRGB', 87: 'B8G8R8A8', 88: 'B8G8R8X8'}.get(d)
        return 'DX10 dxgi %d%s' % (d, ' (%s)' % n if n else ' (uncompressed)')
    try:
        return fourcc.decode('ascii')
    except Exception:
        return repr(fourcc)


# ------------------------------------------------------------------- plumbing
CLEAR = (43, 45, 49)     # the viewport's own clear colour, measured as the modal
                         # pixel of every WW_RENDER_CLEAN=1 grab this lane took


def clear_colour(a):
    """The background of a render, as the MODAL pixel -- never `a[0,0]`.

    A close-up fills the frame, so the top-left pixel is geometry and keying on
    it inverts the mask: the background gets copied over the terrain and the
    objects vanish.  This lane shipped that bug once (mistake 7) and the fix is
    to measure the background instead of assuming where it is.  Falls back to
    CLEAR when the modal pixel is not the clear colour and nothing matches it.
    """
    f = np.asarray(a)[:, :, :3].reshape(-1, 3)
    c, n = np.unique(f, axis=0, return_counts=True)
    m = c[n.argmax()]
    if np.abs(m.astype(int) - np.array(CLEAR)).sum() <= 12:
        return m.astype(np.int16)
    hit = (np.abs(f.astype(int) - np.array(CLEAR)).sum(1) <= 12)
    return np.array(CLEAR, np.int16) if hit.any() else m.astype(np.int16)


def to_img(a, size=None):
    im = Image.fromarray(a.astype(np.uint8)) if a.ndim == 3 else \
        Image.fromarray(a.astype(np.uint8), 'L').convert('RGB')
    if im.mode == 'RGBA':
        im = im.convert('RGB')
    if size and im.size != size:
        im = im.resize(size, Image.LANCZOS)
    return im


def grid(cells, cols, cell=512, pad=10, capt=54, title='', sub='', bg=(16, 16, 18)):
    """cells = [(PIL image or ndarray, caption-lines list), ...]

    Fixed cells, captions under each, so nothing clips.  Title band on top.
    """
    rows = (len(cells) + cols - 1) // cols
    fT, fC = font(24), font(15)
    top = 40 + (28 if sub else 0) if title else 0
    W = pad + cols * (cell + pad)
    H = top + pad + rows * (cell + capt + pad)
    out = Image.new('RGB', (W, H), bg)
    d = ImageDraw.Draw(out)
    if title:
        d.text((pad, 8), title, font=fT, fill=(240, 240, 240))
        if sub:
            d.text((pad, 40), sub, font=fC, fill=(170, 170, 175))
    for i, (img, lines) in enumerate(cells):
        r, c = divmod(i, cols)
        x = pad + c * (cell + pad)
        y = top + pad + r * (cell + capt + pad)
        if img is not None:
            im = img if isinstance(img, Image.Image) else to_img(img)
            # LETTERBOX, never stretch: a render is 2800x1730 and squeezing that
            # into a square cell changes every slope in the picture.  Fit inside
            # the cell, keep the aspect, centre what is left.
            if im.size != (cell, cell):
                sc = min(cell / im.size[0], cell / im.size[1])
                ns = (max(1, int(round(im.size[0] * sc))), max(1, int(round(im.size[1] * sc))))
                im = im.resize(ns, Image.LANCZOS if sc < 1 else Image.NEAREST)
            im = im.convert('RGB')
            d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(24, 24, 27))
            out.paste(im, (x + (cell - im.size[0]) // 2, y + (cell - im.size[1]) // 2))
            d.rectangle([x, y, x + cell - 1, y + cell - 1], outline=(70, 70, 76))
        else:
            d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=(30, 30, 34), outline=(70, 70, 76))
            d.text((x + 8, y + cell // 2), 'no file', font=fC, fill=(200, 120, 120))
        for k, line in enumerate(lines[:3]):
            d.text((x + 2, y + cell + 3 + k * 17), line, font=fC,
                   fill=(235, 235, 235) if k == 0 else (165, 165, 172))
    return out


def save(im, name):
    p = '%s/images/%s' % (L, name)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    im.save(p)
    back = Image.open(p)
    print('%-46s %5dx%-5d %9d B' % (name, back.size[0], back.size[1], os.path.getsize(p)))
    return p


def mean_rgb(a):
    f = a[:, :, :3].astype(np.float64)
    return f[:, :, 0].mean(), f[:, :, 1].mean(), f[:, :, 2].mean()


def read_cam(shot_path):
    """The camera census line written AT THE GRAB, as a dict.

    shot.sh copies $WW_CAMERA_CENSUS to '<shot>.cam'.  This is the only honest
    source for upp / look-at / halfW: the environment says what was ASKED for,
    this says what the projection actually was.
    """
    try:
        line = open(shot_path + '.cam').read().strip().split('\n')[-1]
    except OSError:
        return {}
    return dict(kv.split('=', 1) for kv in line.split() if '=' in kv)


def crop_square(im, cam, half=8192.0):
    """Crop the square of 2*half world units centred on the look-at.

    Only valid for the axis-aligned TOP view (census rot=0,0,0) under an
    orthographic pin, where a world distance maps to pixels by a single scale:
    world width across the image is 2*halfW, so one pixel is 2*halfW / width.
    """
    if cam.get('rot') != '0.0000,0.0000,0.0000' or 'halfW' not in cam:
        return im
    W, H = im.size
    upp = 2.0 * float(cam['halfW']) / W
    r = half / upp
    cx, cy = W / 2.0, H / 2.0
    box = (int(round(cx - r)), int(round(cy - r)), int(round(cx + r)), int(round(cy + r)))
    box = (max(0, box[0]), max(0, box[1]), min(W, box[2]), min(H, box[3]))
    return im.crop(box)


def crop_content(im, pad=24, thr=10):
    """Trim to the drawn content: the bounding box of everything that is not the
    viewport's clear colour, plus a margin. Stated wherever it is used -- it
    changes the FRAMING of a picture, never its geometry, and the camera census
    beside each panel still describes the projection the pixels came from."""
    a = np.asarray(im.convert('RGB')).astype(np.int16)
    m = np.abs(a - clear_colour(a)).sum(2) > thr
    if not m.any():
        return im
    ys, xs = np.where(m)
    box = (max(0, xs.min() - pad), max(0, ys.min() - pad),
           min(im.size[0], xs.max() + 1 + pad), min(im.size[1], ys.max() + 1 + pad))
    return im.crop(box)

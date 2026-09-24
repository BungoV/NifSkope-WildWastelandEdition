# Compose the ORBIT STRIP and the animated orbit for one subject.
#
#   python compose_orbit.py <tag> <out_strip.png> <out_anim.gif> "<caption>"
#
# The strip: one block per elevation, MESH on top and CARD underneath, one
# column per azimuth, in the azimuth order the harness walked. Under every
# card column is the IoU and the mean colour error that run MEASURED, quoted
# out of orbit/<tag>.log -- never recomputed here, so the picture and the
# number cannot drift apart.
#
# The animation: the same pairs, one frame per azimuth, mesh above and card
# below, so a viewer watches the card turn with the mesh rather than reading a
# number about it. GIF because it plays everywhere; the frames are the PNGs the
# harness grabbed, downscaled, never re-rendered.
#
# WHAT THE PICTURES DO NOT SHOW. The card is lit by the viewer's existing
# lighting from the colour sheet and the baked normal sheet. The mask
# (GSAOS/RMAOS) and emissive sheets are loaded and validated but not shaded:
# the Fallout 4 / FO4CS PBRM lighting model waits on bungo's signal
# (IMPOSTOR_MATERIAL_SEAM). So a colour difference here is partly the card and
# partly a comparison that is not yet allowed to be like-for-like, and the
# caption says so.
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.abspath(__file__))
BG = (24, 24, 26)
FG = (232, 232, 232)
DIM = (150, 150, 155)
ACC = (150, 190, 240)


def font(size, bold=False):
    for name in (("arialbd.ttf" if bold else "arial.ttf"),
                 "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"):
        for d in ("C:/Windows/Fonts", "/usr/share/fonts/truetype/dejavu"):
            p = os.path.join(d, name)
            if os.path.exists(p):
                try:
                    return ImageFont.truetype(p, size)
                except OSError:
                    pass
    return ImageFont.load_default()


def read_log(tag):
    """Every `orbit azim A elev E ... iou I colour C ...` row, in file order.

    Keyed (azim, elev) as INTEGERS, because that is how the file names are
    formed by the harness; matching on the float text would pair a row with no
    picture the moment one of them is printed as 45 and the other as 45.0.
    """
    rows = {}
    order = []
    path = os.path.join(ROOT, "orbit", tag + ".log")
    head = {}
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            for key in ("grid", "frame", "half", "viewport"):
                if line.startswith(key + ":") or line.startswith(key + " "):
                    head.setdefault(key, line.split(None, 1)[1] if " " in line else "")
            m = re.match(r"^orbit azim ([\d.]+) elev ([\d.]+) mesh ([\d.]+) card ([\d.]+)"
                         r" iou ([-\d.]+) colour ([-\d.]+)", line)
            if m:
                k = (int(float(m.group(1))), int(float(m.group(2))))
                rows[k] = (float(m.group(5)), float(m.group(6)))
                if k not in order:
                    order.append(k)
            m = re.match(r"^orbit (iou|colour) mean ([\d.]+)", line)
            if m:
                head["mean_" + m.group(1)] = float(m.group(2))
    if not rows:
        raise SystemExit("no orbit rows in " + path)
    return rows, order, head


def shot(tag, az, el, which):
    return os.path.join(ROOT, "orbit", tag,
                        "v_az%03d_el%02d_%s.png" % (az, el, which))


def bbox_of(path, bg, tol=12):
    """The subject's box in one grab, or None for an empty frame.

    The same background test the harness's `silhouette()` uses, so the crop
    below keeps exactly what was measured. The harness now suppresses the
    navigation gizmo and the 3D cursor before it grabs, so there is no viewer
    chrome in the corner for this to mistake for the tree -- which is why the
    crop can be computed from the pixels instead of from a hand-written rect.
    """
    img = Image.open(path).convert("RGB")
    px = img.load()
    x0, y0, x1, y1 = img.width, img.height, -1, -1
    for y in range(img.height):
        for x in range(img.width):
            r, g, b = px[x, y]
            if abs(r - bg[0]) <= tol and abs(g - bg[1]) <= tol and abs(b - bg[2]) <= tol:
                continue
            if x < x0:
                x0 = x
            if x > x1:
                x1 = x
            if y < y0:
                y0 = y
            if y > y1:
                y1 = y
    return None if x1 < 0 else (x0, y0, x1 + 1, y1 + 1)


def common_crop(paths, pad=10):
    """ONE rect for every tile in a subject, never a rect per tile.

    A per-tile crop would rescale each view to fill its cell, and the strip
    would then show every azimuth at a different magnification -- which is
    exactly the comparison the strip exists to make impossible to read wrong.
    The union of the boxes keeps one scale across mesh, card, azimuth and
    elevation.
    """
    first = Image.open(paths[0]).convert("RGB")
    bg = first.getpixel((2, 2))
    X0, Y0, X1, Y1 = first.width, first.height, 0, 0
    for p in paths:
        b = bbox_of(p, bg)
        if not b:
            continue
        X0, Y0 = min(X0, b[0]), min(Y0, b[1])
        X1, Y1 = max(X1, b[2]), max(Y1, b[3])
    if X1 <= X0 or Y1 <= Y0:
        return (0, 0, first.width, first.height)
    return (max(0, X0 - pad), max(0, Y0 - pad),
            min(first.width, X1 + pad), min(first.height, Y1 + pad))


def load(path, w, h, crop=None):
    img = Image.open(path).convert("RGB")
    if crop:
        img = img.crop(crop)
    return img.resize((w, h), Image.LANCZOS)


def strip(tag, out, caption, cell_w=150):
    rows, order, head = read_log(tag)
    elevs = []
    for (_, el) in order:
        if el not in elevs:
            elevs.append(el)
    azims = []
    for (az, el) in order:
        if el == elevs[0] and az not in azims:
            azims.append(az)

    every = [shot(tag, az, el, w) for (az, el) in order for w in ("mesh", "card")]
    crop = common_crop(every)
    cw, ch = crop[2] - crop[0], crop[3] - crop[1]
    cell_h = int(round(cell_w * ch / cw))

    f_head = font(19, True)
    f_lbl = font(13)
    f_num = font(12)
    f_cap = font(13)

    pad = 6
    head_h = 30
    lbl_h = 36          # elevation line, then the azimuth line under it
    rowlbl_w = 58
    num_h = 30
    block_h = lbl_h + cell_h + cell_h + num_h + 10
    W = rowlbl_w + len(azims) * (cell_w + pad) + pad
    H = head_h + len(elevs) * (block_h + 10) + 46

    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    d.text((pad, 7), "%s   orbit  %d azimuths x %d elevations   mesh above, card below"
           % (tag, len(azims), len(elevs)), font=f_head, fill=FG)

    y = head_h
    for el in elevs:
        d.text((pad, y + 2), "elevation %d deg   (common crop %dx%d px of the %s viewport)"
               % (el, cw, ch, head.get("viewport", "?")), font=f_lbl, fill=ACC)
        y += lbl_h
        for i, az in enumerate(azims):
            x = rowlbl_w + i * (cell_w + pad)
            im.paste(load(shot(tag, az, el, "mesh"), cell_w, cell_h, crop), (x, y))
            im.paste(load(shot(tag, az, el, "card"), cell_w, cell_h, crop), (x, y + cell_h))
            d.text((x + 2, y - 16), "%d deg" % az, font=f_lbl, fill=DIM)
            iou, col = rows[(az, el)]
            d.text((x + 2, y + 2 * cell_h + 2),
                   "IoU %.3f" % iou, font=f_num, fill=FG)
            d.text((x + 2, y + 2 * cell_h + 15),
                   "dRGB %.3f" % col, font=f_num, fill=DIM)
        d.text((2, y + cell_h // 2), "mesh", font=f_lbl, fill=DIM)
        d.text((2, y + cell_h + cell_h // 2), "card", font=f_lbl, fill=DIM)
        y += 2 * cell_h + num_h + 10

    tail = ("mean IoU %.4f   mean colour error %.4f of 255 over the overlap   "
            "%s" % (head.get("mean_iou", float("nan")),
                    head.get("mean_colour", float("nan")), caption))
    d.text((pad, H - 38), tail, font=f_cap, fill=DIM)
    d.text((pad, H - 20), "card colour is the baked sheet under the viewer's existing lighting;"
           " the mask and emissive sheets are not shaded yet (IMPOSTOR_MATERIAL_SEAM)",
           font=f_cap, fill=DIM)
    im.save(out)
    print("strip -> %s  %dx%d" % (out, im.width, im.height))


def anim(tag, out, cell_w=260, ms=140):
    rows, order, head = read_log(tag)
    el = order[0][1]
    azims = [az for (az, e) in order if e == el]
    crop = common_crop([shot(tag, az, el, w) for az in azims for w in ("mesh", "card")])
    cw, ch = crop[2] - crop[0], crop[3] - crop[1]
    cell_h = int(round(cell_w * ch / cw))
    f_lbl = font(15, True)
    f_num = font(13)

    frames = []
    for az in azims:
        im = Image.new("RGB", (cell_w, 2 * cell_h + 42), BG)
        im.paste(load(shot(tag, az, el, "mesh"), cell_w, cell_h, crop), (0, 20))
        im.paste(load(shot(tag, az, el, "card"), cell_w, cell_h, crop), (0, 20 + cell_h))
        d = ImageDraw.Draw(im)
        d.text((6, 2), "%s  azimuth %d deg  elev %d deg" % (tag, az, el), font=f_lbl, fill=FG)
        iou, col = rows[(az, el)]
        d.text((6, 22), "mesh", font=f_num, fill=DIM)
        d.text((6, 22 + cell_h), "card  IoU %.3f  dRGB %.3f" % (iou, col), font=f_num, fill=DIM)
        frames.append(im.convert("P", palette=Image.ADAPTIVE, colors=128))
    frames[0].save(out, save_all=True, append_images=frames[1:],
                   duration=ms, loop=0, optimize=True)
    print("anim  -> %s  %d frames  %dx%d  %d bytes"
          % (out, len(frames), frames[0].width, frames[0].height,
             os.path.getsize(out)))


if __name__ == "__main__":
    tag = sys.argv[1]
    out_strip = sys.argv[2]
    out_anim = sys.argv[3] if len(sys.argv) > 3 else ""
    caption = sys.argv[4] if len(sys.argv) > 4 else ""
    strip(tag, out_strip, caption)
    if out_anim:
        anim(tag, out_anim)

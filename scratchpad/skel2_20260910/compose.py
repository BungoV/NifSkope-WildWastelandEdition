#!/usr/bin/env python
"""Lane SKEL2 -- the composite pictures bungo is handed.

Every panel is a render or an in-application grab taken by the spell; nothing
here re-renders anything. Labels are burned in so a picture that gets separated
from its report still says which half is which, and every caption carries the
NUMBER the report quotes rather than an adjective.

  python compose.py
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SHOTS = os.path.join(ROOT, "scratchpad", "skeloverlay_20260910")
RUNG = os.path.join(ROOT, "scratchpad", "skel2_20260910", "rung")
OUT = os.path.join(ROOT, "scratchpad", "skel2_20260910", "images")

BAR = 34
BG = (24, 25, 28)
INK = (232, 234, 238)


def font(size=16):
    for name in ("DejaVuSans.ttf", "arial.ttf", "segoeui.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def labelled(path, text, crop=None, scale=1.0):
    im = Image.open(path).convert("RGB")
    if crop:
        im = im.crop(crop)
    if scale != 1.0:
        im = im.resize((int(im.width * scale), int(im.height * scale)), Image.NEAREST)
    out = Image.new("RGB", (im.width, im.height + BAR), BG)
    out.paste(im, (0, BAR))
    d = ImageDraw.Draw(out)
    d.text((10, 8), text, fill=INK, font=font())
    return out


def stack(panels, path):
    w = max(p.width for p in panels)
    h = sum(p.height for p in panels)
    out = Image.new("RGB", (w, h), BG)
    y = 0
    for p in panels:
        out.paste(p, ((w - p.width) // 2, y))
        y += p.height
    out.save(path)
    print("  %-34s %dx%d" % (os.path.basename(path), out.width, out.height))


def beside(panels, path):
    h = max(p.height for p in panels)
    w = sum(p.width for p in panels)
    out = Image.new("RGB", (w, h), BG)
    x = 0
    for p in panels:
        out.paste(p, (x, 0))
        x += p.width
    out.save(path)
    print("  %-34s %dx%d" % (os.path.basename(path), out.width, out.height))


def need(path):
    if not os.path.exists(path):
        print("  MISSING %s" % path)
        return False
    return True


def main():
    os.makedirs(OUT, exist_ok=True)
    missing = 0

    # 1. Stick on top, Octahedral below -- the same pose, the same camera.
    a = os.path.join(SHOTS, "bones_stick.png")
    b = os.path.join(SHOTS, "bones_octa.png")
    if need(a) and need(b):
        # The upper body, where the bones are dense enough to tell the two
        # shapes apart at all: a whole-figure pair shows two blue smudges.
        box = (480, 520, 1080, 800)
        stack([labelled(a, "Stick  --  Blender's plain head-to-tail line", box, 1.6),
               labelled(b, "Octahedral (default)  --  solid facets, ring at 1/10 of the length", box, 1.6)],
              os.path.join(OUT, "bones_stick_vs_octa.png"))
    else:
        missing += 1

    # 2 and 3. the two chips, AT FRAME 46, which is the frame the grey dots are
    # visible in at all -- at the bind pose the camera nodes sit inside the body.
    a = os.path.join(SHOTS, "on_frame46_bones.png")
    b = os.path.join(SHOTS, "on_frame46.png")
    box = (300, 380, 1140, 941)
    if need(a):
        labelled(a, "chip = Bones, frame 46 -- 93 rows, 0 pixels off the character",
                 box).save(os.path.join(OUT, "bones_bones_chip.png"))
        print("  bones_bones_chip.png")
    else:
        missing += 1
    if need(b):
        labelled(b, "chip = All, frame 46 -- 130 rows; the two grey dots = Camera (148), CamTarget (159)",
                 box).save(os.path.join(OUT, "bones_all_chip.png"))
        print("  bones_all_chip.png")
    else:
        missing += 1

    # 4. X-ray
    a = os.path.join(SHOTS, "bones_octa.png")
    b = os.path.join(SHOTS, "bones_xray.png")
    if need(a) and need(b):
        beside([labelled(a, "X-ray ON (default): the rig reads through the mesh", None, 0.5),
                labelled(b, "X-ray OFF: the mesh hides the bones inside it", None, 0.5)],
               os.path.join(OUT, "bones_xray.png"))
    else:
        missing += 1

    # 5. the manager dock, before and after, at 2x on the deep rows
    a = os.path.join(SHOTS, "manager_before.png")
    b = os.path.join(SHOTS, "manager_after.png")
    if need(a) and need(b):
        labelled(b, "Skeleton Manager, 400 px wide -- the deep arm rows keep their names").save(
            os.path.join(OUT, "manager_after.png"))
        print("  manager_after.png")
        ia, ib = Image.open(a), Image.open(b)
        h = min(ia.height, ib.height)
        box = (0, max(0, h // 2 - 110), min(ia.width, ib.width), max(0, h // 2 - 110) + 220)
        beside([labelled(a, "BEFORE  (WW_SKELETON_LEGACY_COLUMNS=1, the shipped law)", box, 2.0),
                labelled(b, "AFTER  (indent 12, numbers fixed right, name sized to the rows)", box, 2.0)],
               os.path.join(OUT, "cmp_manager_names.png"))
    else:
        missing += 1

    # 6. the rung beside the new build, same camera -- what bungo's eye sees
    a = os.path.join(RUNG, "on.png")
    b = os.path.join(SHOTS, "bones_octa.png")
    if need(a) and need(b):
        box = (480, 520, 1080, 800)
        stack([labelled(a, "BEFORE  (07:06:04) -- wireframe octahedra, near-white bones", box, 1.6),
               labelled(b, "AFTER -- solid Blender octahedra in the palette's blue #4772b3", box, 1.6)],
              os.path.join(OUT, "cmp_bones_before_after.png"))
    else:
        missing += 1

    print("%d composite(s) missing an input" % missing)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())

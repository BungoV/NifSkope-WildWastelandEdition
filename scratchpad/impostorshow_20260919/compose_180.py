# Compose images/00_azimuth_180_explained.png -- THE PROOF PICTURE for the
# azimuth repair (bungo, 2026-09-19: "Okay, fix the 180 issue").
#
# Three columns, four azimuths:
#   the real mesh, seen from azimuth A
#   the card drawn the OLD way  (rz = 90 - azim), same camera
#   the card drawn the REPAIRED way (rz = 270 - azim), same camera
#
# Both card columns come from the SAME repaired .lodm: the old column is that
# set read back under the pre-repair convention, which is what a set baked
# before this exe puts on screen. Every number printed under a card is the IoU
# that run measured against the mesh in the left column, quoted from its own
# log, not recomputed here.
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(ROOT, "shots")
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    ROOT, "..", "..", "images", "00_azimuth_180_explained.png")

AZ = [0, 90, 180, 270]
NAME = {0: "E", 90: "N", 180: "W", 270: "S"}
CELL = 320
PAD = 12
HEAD = 88
CAP = 34
BG = (24, 24, 26)
FG = (232, 232, 232)
DIM = (150, 150, 155)
GOOD = (120, 210, 130)
BAD = (226, 120, 110)


def font(size, bold=False):
    for name in (("arialbd.ttf" if bold else "arial.ttf"),
                 "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"):
        for d in ("C:/Windows/Fonts", "/usr/share/fonts/truetype/dejavu"):
            p = os.path.join(d, name)
            if os.path.exists(p):
                return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def same_iou(logpath):
    """{azimuth: iou of the card against the mesh from the SAME direction}"""
    out = {}
    with open(logpath, "r", errors="replace") as fh:
        for line in fh:
            # `azimuth 45 deg (NE-low) elev 15  card .. same .. opposite ..`
            # and NOT `azimuth same mean ..`, which also contains " same ".
            t = line.split()
            if len(t) > 3 and t[0] == "azimuth" and t[2] == "deg" and "same" in t:
                out[int(t[1])] = float(t[t.index("same") + 1])
    return out


spec = same_iou(os.path.join(SHOTS, "spec.log"))
old = same_iou(os.path.join(SHOTS, "asbaked.log"))

W = PAD + 3 * (CELL + PAD)
H = HEAD + len(AZ) * (CELL + CAP + PAD) + PAD + 40
img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

f_title = font(26, True)
f_head = font(14, True)
f_cap = font(15)
f_small = font(13)

d.text((PAD, 14), "The 180-degree repair, on an asymmetric subject", font=f_title, fill=FG)
d.text((PAD, 48), "TreeMapleblasted05, N=4, 48x128 frames, ortho, elevation 15 deg. "
                  "Both cards are the SAME repaired set; the middle column reads it "
                  "under the pre-repair convention.", font=f_small, fill=DIM)

heads = ["the mesh, from this azimuth",
         "card read the OLD way (rz = 90 - azim)",
         "card read as the spec says (rz = 270 - azim)"]
for c, h in enumerate(heads):
    d.text((PAD + c * (CELL + PAD), HEAD - 22), h, font=f_head, fill=FG)

for r, a in enumerate(AZ):
    y = HEAD + r * (CELL + CAP + PAD)
    files = [
        os.path.join(SHOTS, "spec_az%03d_mesh.png" % a),
        os.path.join(SHOTS, "asbaked_az%03d_card.png" % a),
        os.path.join(SHOTS, "spec_az%03d_card.png" % a),
    ]
    caps = [
        "azimuth %d deg (%s)" % (a, NAME[a]),
        "IoU vs the mesh  %.4f" % old.get(a, float("nan")),
        "IoU vs the mesh  %.4f" % spec.get(a, float("nan")),
    ]
    cols = [DIM, BAD, GOOD]
    for c, (p, cap) in enumerate(zip(files, caps)):
        x = PAD + c * (CELL + PAD)
        if os.path.exists(p):
            im = Image.open(p).convert("RGB").resize((CELL, CELL), Image.LANCZOS)
            img.paste(im, (x, y))
        else:
            d.rectangle([x, y, x + CELL, y + CELL], outline=(90, 90, 90))
            d.text((x + 8, y + 8), "MISSING " + os.path.basename(p), font=f_small, fill=BAD)
        d.text((x, y + CELL + 8), cap, font=f_cap, fill=cols[c])

foot = ("Over all eight azimuths: repaired 0.1814 mean against the mesh from the same "
        "direction and 0.0868 against the opposite; the old reading inverts that to "
        "0.0914 and 0.1757. Every set baked before this exe must be re-baked.")
d.text((PAD, H - 34), foot, font=f_small, fill=DIM)

os.makedirs(os.path.dirname(os.path.abspath(OUT)), exist_ok=True)
img.save(OUT)
print("wrote %s  %dx%d" % (os.path.abspath(OUT), W, H))

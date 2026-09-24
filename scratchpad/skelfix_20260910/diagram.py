#!/usr/bin/env python
"""A DIAGRAM of what the two rules draw at frame 46. NOT a render.

CONSTITUTION rule 5 wants the before/after picture taken through the render
hook, and that needs a build this lane does not have (BUILD PENDING). This is
the interim: the same world positions the overlay draws at, projected through
the same FRONT ORTHOGRAPHIC framing the spell pins (screen x = world x,
screen y = -world z), both panels at ONE scale so the two are comparable.

Left  = the rule that shipped in BUILD9: every parent -> child pair.
Right = the armature rule this lane ships.
Magenta = a segment with an end off the character.
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import measure  # noqa: E402

W, H, PAD = 620, 760, 26


def main(argv):
    m = measure.build()
    nodes, parent, inskin = m["nodes"], m["parent"], m["inskin"]
    A = m["anim"]

    cnt = dict((b, 0) for b in nodes)
    for b in inskin:
        x = b
        while x is not None:
            cnt[x] += 1
            x = parent.get(x)

    def depth(b):
        d, x = 0, parent.get(b)
        while x is not None:
            d, x = d + 1, parent.get(x)
        return d
    R = max([b for b in nodes if cnt[b] == len(inskin)], key=depth)

    def under(b):
        x = b
        while x is not None:
            if x == R:
                return True
            x = parent.get(x)
        return False
    arm = set(b for b in nodes if cnt[b] > 0 and under(b))

    pairs = [(p, b) for b in sorted(nodes) if (p := parent.get(b)) is not None]
    bind = m["bind"]
    limit = 1.5 * max(math.dist(bind[p][1], bind[b][1]) for p, b in pairs
                      if p in inskin and b in inskin)

    pts = [A[b][1] for b in nodes]
    xs = [p[0] for p in pts] + [0.0]
    zs = [p[2] for p in pts] + [0.0]
    x0, x1, z0, z1 = min(xs), max(xs), min(zs), max(zs)
    sc = min((W - 2 * PAD) / max(1e-3, x1 - x0), (H - 2 * PAD - 40) / max(1e-3, z1 - z0))

    def proj(v):
        return (PAD + (v[0] - x0) * sc, H - PAD - (v[2] - z0) * sc)

    try:
        font = ImageFont.truetype("arial.ttf", 13)
        small = ImageFont.truetype("arial.ttf", 11)
    except Exception:
        font = small = ImageFont.load_default()

    img = Image.new("RGB", (W * 2, H), (43, 45, 49))
    d = ImageDraw.Draw(img)

    def panel(ox, title, keep):
        d.text((ox + PAD, 8), title, fill=(230, 232, 235), font=font)
        drawn = [(p, b) for p, b in pairs if keep(p, b)]
        worst = 0.0
        stray = 0
        for p, b in drawn:
            a, c = proj(A[p][1]), proj(A[b][1])
            ln = math.dist(A[p][1], A[b][1])
            worst = max(worst, ln)
            far = ln > limit
            if far:
                stray += 1
            d.line([(ox + a[0], a[1]), (ox + c[0], c[1])],
                   fill=(255, 0, 200) if far else (230, 232, 235), width=2 if far else 1)
        for b in nodes:
            v = proj(A[b][1])
            col = (230, 232, 235) if b in inskin else (134, 139, 145)
            d.ellipse([ox + v[0] - 1.6, v[1] - 1.6, ox + v[0] + 1.6, v[1] + 1.6], fill=col)
        o = proj((0.0, 0.0, 0.0))
        d.ellipse([ox + o[0] - 3, o[1] - 3, ox + o[0] + 3, o[1] + 3], outline=(255, 200, 0))
        d.text((ox + o[0] + 6, o[1] - 7), "world origin", fill=(255, 200, 0), font=small)
        d.text((ox + PAD, H - 20),
               "%d segments, longest %.1f units, %d over the %.1f limit"
               % (len(drawn), worst, stray, limit), fill=(200, 202, 206), font=small)
        return len(drawn), worst, stray

    a = panel(0, "BEFORE (BUILD9): every parent -> child pair", lambda p, b: True)
    b = panel(W, "AFTER (lane SKELFIX): armature nodes only",
              lambda p, b: p in arm and b in arm)
    d.line([(W, 0), (W, H)], fill=(70, 72, 78))
    d.text((PAD, H - 36), "frame 46, front orthographic, one scale for both panels; "
           "this is a DIAGRAM of the drawn segments, not a render",
           fill=(134, 139, 145), font=small)
    out = argv[1] if len(argv) > 1 else os.path.join(HERE, "rule_before_after.png")
    img.save(out)
    print("before: %d segments, longest %.1f, %d stray" % a)
    print("after : %d segments, longest %.1f, %d stray" % b)
    print("wrote %s" % out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

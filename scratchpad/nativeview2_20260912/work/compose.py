#!/usr/bin/env python
"""The lane's pictures: panels with their labels BURNED IN, written under
scratchpad/nativeview2_20260912/images/, every size read back with PIL.

  python compose.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

R2 = "E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview2_20260912"
W = R2 + "/work"
IMG = R2 + "/images"
BAR = 62
GAP = 8
PAD = 46
BGC = (24, 25, 28)


def font(sz):
    for p in ("C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"):
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()


F = font(21)
FT = font(27)
FS = font(17)


def panel(path, label, sub, w):
    im = Image.open(path).convert("RGB")
    im = im.resize((w, int(im.height * w / im.width)), Image.LANCZOS)
    out = Image.new("RGB", (w, im.height + BAR), BGC)
    out.paste(im, (0, BAR))
    d = ImageDraw.Draw(out)
    d.text((10, 6), label, font=F, fill=(238, 238, 238))
    d.text((10, 34), sub, font=FS, fill=(158, 162, 170))
    d.rectangle([0, BAR - 1, w - 1, BAR - 1], fill=(70, 74, 82))
    return out


def sheet(name, title, note, cells, w=512):
    ps = [panel(p, l, s, w) for p, l, s in cells]
    hh = max(p.height for p in ps)
    W_ = len(ps) * w + (len(ps) - 1) * GAP + 2 * PAD
    H_ = hh + 2 * PAD + 70
    out = Image.new("RGB", (W_, H_), BGC)
    d = ImageDraw.Draw(out)
    d.text((PAD, 22), title, font=FT, fill=(245, 245, 245))
    d.text((PAD, 56), note, font=FS, fill=(160, 164, 172))
    for i, p in enumerate(ps):
        out.paste(p, (PAD + i * (w + GAP), PAD + 70))
    p = os.path.join(IMG, name)
    out.save(p)
    back = Image.open(p)
    print("%-42s %dx%d  %d B" % (name, back.size[0], back.size[1],
                                 os.path.getsize(p)))
    back.close()


def main():
    os.makedirs(IMG, exist_ok=True)

    for v, vn in (("top", "top-down"), ("obl", "oblique")):
        sheet(
            "i_native_%s_rung_vs_new.png" % v,
            "Native LOD, %s -- legacy control, then the rung and the new exe" % vn,
            "Chunk (-20,24) dim 4, orthographic half-width 8192, WW_RENDER_CLEAN=1. "
            "Both native panels draw the SAME .lodl terrain and the SAME .lodi objects; "
            "only the exe differs.",
            [(W + "/rung/legacy_btr_%s.png" % v,
              "LEGACY .BTR (control)",
              "lit by sk_msn.prog -- untouched by this lane, byte-identical on both exes"),
             (W + "/rung/legacy_bto_%s.png" % v,
              "LEGACY .BTO (control)",
              "objects, Shader Flags 1 bit 12 CLEAR -- byte-identical on both exes"),
             (W + "/rung/i_native_both_%s.png" % v,
              "NATIVE, rung exe (before)",
              "sheet read as a tangent-space map"),
             (W + "/new/i_native_both_%s.png" % v,
              "NATIVE, new exe (after)",
              "sheet read as a model-space map")])

    for v, vn in (("top", "top-down"), ("obl", "oblique")):
        sheet(
            "ii_terrain_own_vs_flat_%s.png" % v,
            "Native terrain ALONE, %s -- its own normal tiles against FLAT tiles" % vn,
            "The FLAT arm replaces every normal texel with 'straight up' (read back "
            "132,255,132 after the 5/6/5 quantisation = 2.86 deg from vertical). "
            "On the rung the two are the same picture; that is the refuter.",
            [(W + "/rung/t_own_%s.png" % v, "rung: own tiles",
              "dark<40 20.01%% (oblique)" if v == "obl" else "dark<40 5.08%"),
             (W + "/rung/t_flat_%s.png" % v, "rung: FLAT tiles",
              "dark<40 20.07%% (oblique) -- the same blotches" if v == "obl"
              else "dark<40 64.64%"),
             (W + "/new/t_own_%s.png" % v, "new: own tiles",
              "dark<40 0.58%" if v == "obl" else "dark<40 0.00%"),
             (W + "/new/t_flat_%s.png" % v, "new: FLAT tiles",
              "dark<40 0.00%, lit evenly" if v == "obl" else "dark<40 0.00%")])

    sheet(
        "iii_slope_known_answer_oblique.png",
        "Gate (d), oblique -- three synthetic sheets whose answer is known first",
        "Headlight (0,0,1) in view space; the oblique view rotation puts it along "
        "(-0.6516, +0.6142, +0.4453) in world axes. N.L is west 0.7258 > flat 0.4434 "
        "> east 0.0968. On the new exe the luma orders that way on 100.00% of pixels; "
        "on the rung, 46.23% -- chance.",
        [(W + "/new/t_tiltw_obl.png", "new: sheet tilts WEST 28.94 deg",
          "mean luma 104.01 -- brightest, as N.L 0.7258 says"),
         (W + "/new/t_flat_obl.png", "new: sheet says UP",
          "mean luma 95.88 -- N.L 0.4434"),
         (W + "/new/t_tilt_obl.png", "new: sheet tilts EAST 28.94 deg",
          "mean luma 80.39 -- darkest, as N.L 0.0968 says"),
         (W + "/rung/t_tilt_obl.png", "rung: sheet tilts EAST (refuter)",
          "mean luma 76.61, and the rung's WEST arm was 90.65 -- ordered by luck")])

    sheet(
        "iv_slope_top_control.png",
        "Gate (d), top view -- the east and west tilts must be the SAME picture",
        "At the top view both tilted sheets have N.L 0.8751: the tilt DIRECTION cannot "
        "matter. On the new exe the two frames are byte-identical. On the rung they "
        "differ by a mean of 47.06 luma, which is what reading a model-space sheet "
        "through an arbitrary tangent frame does.",
        [(W + "/rung/t_tilt_top.png", "rung: tilt EAST", "mean luma 38.91"),
         (W + "/rung/t_tiltw_top.png", "rung: tilt WEST", "mean luma 85.96 -- must not differ"),
         (W + "/new/t_tilt_top.png", "new: tilt EAST", "mean luma 108.06"),
         (W + "/new/t_tiltw_top.png", "new: tilt WEST", "mean luma 108.06 -- byte-identical")])


if __name__ == "__main__":
    main()

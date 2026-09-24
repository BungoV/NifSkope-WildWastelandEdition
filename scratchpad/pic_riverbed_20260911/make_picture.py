#!/usr/bin/env python
"""PIC-RIVERBED: what the grey spots in our riverbed are.

Six panels, one page, `ww-texel-picture`:
  1  OURS   128x128 texels of Commonwealth.4.-20.20, 4x nearest
  2  VANILLA same 128x128 texels, same 4x
  3  the vanilla DIFFUSE painted there (dominant LTEX), native size, downscaled
  4  thumbnails of the other layers in the same window
  5  that texture sampled onto the same window at the ENGINE repeat 341.333
  6  the same at the BAKED repeat 2048

Read-only outside this lane's directory. Sampling is SPLAT1's offline_bake
(`_tap`, the bake's own footprint-mip rule); DDS decoding is splatlib's Dds.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'scratchpad', 'splat1_20260911'))
sys.path.insert(0, os.path.join(REPO, 'tests', 'spells'))

import splatlib as S                                          # noqa: E402
import offline_bake as OB                                     # noqa: E402
sys.path.insert(0, HERE)
from s4_pebble import window_composition                       # noqa: E402

OURS = REPO + '/scratchpad/roads1_20260911/out/after/tex/Commonwealth.4.-20.20.DDS'
VAN = ('E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/'
       'Commonwealth.4.-20.20.DDS')
OUT = os.path.join(HERE, 'images', 'riverbed_texture.png')

CELL, DIM, CX0, CY0, RES = 4096.0, 4, -20, 20, 512
UPT = 32.0
ENGINE, BAKED = 341.3333, 2048.0
WIN, MAG = 128, 4
PW = WIN * MAG                    # 512, the panel content width

C_BG = (22, 22, 25)
C_TXT = (233, 233, 237)
C_DIM = (156, 156, 163)
C_RED = (236, 74, 74)
C_GRN = (120, 210, 130)
C_BLU = (150, 175, 235)
C_YEL = (240, 200, 90)


def font(sz, bold=False):
    for n in (('arialbd.ttf' if bold else 'arial.ttf'), 'segoeui.ttf'):
        try:
            return ImageFont.truetype('C:/Windows/Fonts/' + n, sz)
        except OSError:
            pass
    return ImageFont.load_default()


F_T, F_H, F_S, F_C = font(23, True), font(16, True), font(13), font(13, True)


def nearest(a, mag):
    return np.repeat(np.repeat(a, mag, 0), mag, 1)


def u8(a):
    return np.clip(a + 0.5, 0, 255).astype(np.uint8)


def ticks(dr, x0, y0, every=16):
    """A light grid every `every` texels -- the texel scale without a 128-line
    grid over the image (ww-texel-picture 2.3, adapted and declared)."""
    for k in range(every, WIN, every):
        dr.line([x0 + k * MAG, y0, x0 + k * MAG, y0 + PW], fill=(255, 255, 255, 40), width=1)
        dr.line([x0, y0 + k * MAG, x0 + PW, y0 + k * MAG], fill=(255, 255, 255, 40), width=1)


def main():
    w = json.load(open(os.path.join(HERE, 'window.json')))
    peb = json.load(open(os.path.join(HERE, 'pebble.json')))
    chain = {r['edid']: r for r in json.load(open(os.path.join(HERE, 'chain.json')))}
    by, bx = w['y'], w['x']
    facts = dict(window=w)

    # ---------- the two sheet crops ----------
    ours = S.Dds(OURS).level(0)[:, :, :3]
    van = S.Dds(VAN).level(0)[:, :, :3]
    o = ours[by:by + WIN, bx:bx + WIN]
    v = van[by:by + WIN, bx:bx + WIN]
    lv_o = float(S.local_var(S.lum(o)).mean())
    lv_v = float(S.local_var(S.lum(v)).mean())

    # ---------- the window's composition, grouped by DIFFUSE ----------
    _, rows = window_composition()
    dom_path = rows[0][0]
    dom_rel = rows[0][1]['rel']
    dom_edids = rows[0][1]['edids']
    dom_share = rows[0][1]['share']
    facts['composition'] = [[r[0], r[1]['rel'], r[1]['share'],
                             [[a, b] for a, b in r[1]['edids']]] for r in rows]

    # ---------- the diffuse textures ----------
    dds = S.Dds(dom_path)
    tex = u8(dds.level(0)[:, :, :3])
    tex_small = np.asarray(Image.fromarray(tex).resize((PW, PW), Image.LANCZOS))

    # ---------- the two sampled panels ----------
    span = float(DIM) * CELL
    py, px = np.mgrid[by:by + WIN, bx:bx + WIN]
    wy = CY0 * CELL + (1.0 - (py + 0.5) / RES) * span
    wx = CX0 * CELL + ((px + 0.5) / RES) * span
    panels = {}
    for name, tile in (('engine', ENGINE), ('baked', BAKED)):
        col = OB._tap(dds, wx, wy, tile, UPT, 'code')
        panels[name] = u8(np.asarray(col, np.float64).reshape(WIN, WIN, 3))
        facts['lv_' + name] = float(S.local_var(S.lum(panels[name].astype(np.float64))).mean())
        facts['mip_' + name] = float(S.bake_mip(dds, tile, UPT))
    facts['lv_ours'], facts['lv_van'] = lv_o, lv_v

    # self-check: the window spans exactly 2 repeats at 2048 (4096 world units),
    # so panel 6 MUST be periodic with period 64 texels and panel 5 must not be
    # periodic with that period by content -- it repeats every 10.667, which is
    # not an integer number of texels.
    b = panels['baked'].astype(np.int16)
    facts['baked_period64_maxdiff'] = int(np.abs(b[:, :64] - b[:, 64:]).max())
    assert facts['baked_period64_maxdiff'] == 0,         'panel 6 is not periodic at 64 texels -- the sampling is not what is claimed'
    e2 = panels['engine'].astype(np.int16)
    facts['engine_period64_maxdiff'] = int(np.abs(e2[:, :64] - e2[:, 64:]).max())

    # ---------- page geometry ----------
    M, GAP = 28, 26
    W = M + PW + GAP + PW + M
    CAPH = 56                       # caption band under every panel (3 lines)
    y = 16
    y_sub = y + 31
    SUB = [
        'Chunk (-20,20) = cells -20..-17 x 20..23, dim 4, 512 texels at 32 world units a texel.  Window y=%d..%d  x=%d..%d of that sheet' % (by, by + WIN, bx, bx + WIN),
        '(world x %.0f..%.0f, y %.0f..%.0f), picked as the 128x128 window with the most riverbed LTEX paint (%.3f) and ZERO road texels, then the highest speckle.' % (w['world'][0], w['world'][1], w['world'][2], w['world'][3], w['riverbedW']),
        'OURS = scratchpad/roads1_20260911/out/after/tex (ROADS1 bake, 2026-09-11 12:19).  VANILLA = Bethesda\'s shipped sheet, untouched.',
        'All four sheet panels are the same 128x128 texels at 4x nearest neighbour, with a faint grid every 16 texels (512 world units).',
    ]
    y_r1 = y_sub + 17 * len(SUB) + 16
    y_r1c = y_r1 + PW + 4
    y_r2 = y_r1c + CAPH + GAP
    y_r2c = y_r2 + PW + 4
    y_r3 = y_r2c + CAPH + GAP
    y_r3c = y_r3 + PW + 4
    y_ans = y_r3c + CAPH + 14
    ANS = []
    H = y_ans + 20 * 6 + 16

    page = Image.new('RGB', (W, H), C_BG)
    dr = ImageDraw.Draw(page, 'RGBA')
    title = 'The grey spots in the riverbed: chunk (-20,20), the texture painted there, and the two tilings'
    dr.text((M, y), title, font=F_T, fill=C_TXT)
    assert dr.textlength(title, font=F_T) <= W - 2 * M, 'title overflows the page'
    for i, s in enumerate(SUB):
        dr.text((M, y_sub + 17 * i), s, font=F_S, fill=C_DIM)
        assert dr.textlength(s, font=F_S) <= W - 2 * M, 'subtitle line %d overflows the page' % i

    XL, XR = M, M + PW + GAP

    def cap(x, yy, lines, cols):
        for i, (s, c) in enumerate(zip(lines, cols)):
            dr.text((x, yy + 17 * i), s, font=(F_C if i == 0 else F_S), fill=c)
            assert dr.textlength(s, font=(F_C if i == 0 else F_S)) <= PW + 6, \
                'caption "%s" wider than its panel' % s

    def put(img, x, yy, grid=True):
        page.paste(Image.fromarray(img, 'RGB'), (x, yy))
        dr.rectangle([x - 1, yy - 1, x + PW, yy + PW], outline=(96, 96, 102))
        if grid:
            ticks(dr, x, yy)

    # ---- row 1: the two sheets ----
    put(nearest(u8(o), MAG), XL, y_r1)
    put(nearest(u8(v), MAG), XR, y_r1)
    cap(XL, y_r1c, ['1  OURS -- our far-terrain colour sheet, the grey spots',
                    'local variance of luminance %.1f     mean RGB %.1f, %.1f, %.1f' % (lv_o, o[:, :, 0].mean(), o[:, :, 1].mean(), o[:, :, 2].mean())],
        [C_GRN, C_RED])
    cap(XR, y_r1c, ['2  VANILLA -- Bethesda\'s sheet, the same 128x128 texels',
                    'local variance of luminance %.1f     mean RGB %.1f, %.1f, %.1f' % (lv_v, v[:, :, 0].mean(), v[:, :, 1].mean(), v[:, :, 2].mean())],
        [C_BLU, C_DIM])

    # the measured grey-spot size, drawn on panel 1
    dsp = peb['spot_ours_texels']
    r = dsp * MAG / 2.0
    cxp, cyp = XL + PW - 58, y_r1 + PW - 58
    dr.ellipse([cxp - r, cyp - r, cxp + r, cyp + r], outline=C_YEL, width=2)
    dr.text((cxp - 46, cyp + r + 3), 'median spot %.1f texels' % dsp, font=F_S, fill=C_YEL)

    # ---- row 2: the diffuse, and the other layers ----
    put(tex_small, XL, y_r2, grid=False)
    cap(XL, y_r2c, ['3  THE VANILLA DIFFUSE PAINTED THERE, %.0f%% of the window'
                    % (100 * dom_share),
                    'Data\Textures\%s' % dom_rel,
                    '%dx%d texels, %d mips, %s, %s, shown at 1/%d size'
                    % (dds.width, dds.height, dds.maxMip + 1, dds.fourcc.decode(),
                       '{:,} bytes'.format(os.path.getsize(dom_path)), dds.width // PW)],
        [C_TXT, C_DIM, C_DIM])
    dr.text((XL + 6, y_r2 + 6), ' + '.join(e for e, _ in dom_edids), font=F_C, fill=C_YEL)
    # one pebble, measured, drawn at this panel's own scale (1/4)
    pd = peb['pebble_texels'] / (dds.width / PW)
    dr.ellipse([XL + PW - 30 - pd / 2, y_r2 + PW - 30 - pd / 2,
                XL + PW - 30 + pd / 2, y_r2 + PW - 30 + pd / 2], outline=C_YEL, width=2)
    dr.text((XL + PW - 170, y_r2 + PW - 30 + pd / 2 + 2),
            'one pebble = %.0f texture texels' % peb['pebble_texels'], font=F_S, fill=C_YEL)

    # thumbnails of the other layers
    th = 150
    dr.rectangle([XR - 1, y_r2 - 1, XR + PW, y_r2 + PW], outline=(96, 96, 102))
    tx, ty = XR + 8, y_r2 + 8
    others = [(r[0], r[1]) for r in rows[1:] if r[0]]
    k = 0
    for pth, meta in others:
        nm = os.path.basename(meta['rel'] or pth)
        val = meta['share']
        d2 = S.Dds(pth)
        t2 = np.asarray(Image.fromarray(u8(d2.level(0)[:, :, :3])).resize((th, th), Image.LANCZOS))
        cxx = tx + (k % 3) * (th + 8)
        cyy = ty + (k // 3) * (th + 42)
        page.paste(Image.fromarray(t2, 'RGB'), (cxx, cyy))
        dr.rectangle([cxx - 1, cyy - 1, cxx + th, cyy + th], outline=(90, 90, 96))
        dr.text((cxx, cyy + th + 2), '%s' % nm[:26], font=F_S, fill=C_DIM)
        dr.text((cxx, cyy + th + 17), '%.0f%% of the window' % (100 * val), font=F_S, fill=C_DIM)
        k += 1
        if k >= 6:
            break
    cap(XR, y_r2c, ['4  the other diffuse textures blended into the same window',
                    'every one resolved LTEX -> TXST -> TX00; see NOTES.md for the form ids'],
        [C_TXT, C_DIM])

    # ---- row 3: the two tilings ----
    put(nearest(panels['engine'], MAG), XL, y_r3)
    put(nearest(panels['baked'], MAG), XR, y_r3)
    for x0, tile, lab, colr in ((XL, ENGINE, 'engine', C_GRN), (XR, BAKED, 'baked', C_RED)):
        side = tile / UPT * MAG           # one repeat, in device pixels
        dr.rectangle([x0 + 6, y_r3 + 6, x0 + 6 + side, y_r3 + 6 + side], outline=colr, width=2)
        t = 'one repeat = %.1f texels' % (tile / UPT)
        ty2 = y_r3 + 10 + side
        tw = dr.textlength(t, font=F_S)
        dr.rectangle([x0 + 8, ty2 - 2, x0 + 12 + tw, ty2 + 16], fill=(0, 0, 0, 170))
        dr.text((x0 + 10, ty2), t, font=F_S, fill=colr)
    cap(XL, y_r3c, ['5  PANEL 3 ALONE at the ENGINE repeat 341.333 world units',
                    'mip %.2f, one repeat = %.1f texels;  local variance %.1f'
                    % (facts['mip_engine'], ENGINE / UPT, facts['lv_engine']),
                    'panels 5 and 6 are this ONE texture, not the whole blend, so the'],
        [C_GRN, C_GRN, C_DIM])
    cap(XR, y_r3c, ['6  PANEL 3 ALONE at the BAKED repeat 2048 (what we ship)',
                    'mip %.2f, one repeat = %.1f texels;  local variance %.1f'
                    % (facts['mip_baked'], BAKED / UPT, facts['lv_baked']),
                    'colour differs from panel 1; only the texel-scale pattern is the claim.'],
        [C_RED, C_RED, C_DIM])

    # ---- the answer ----
    ANS = [
        ("THE ANSWER", C_YEL),
        ("Yes -- those grey spots ARE the pebbles, drawn six times too big. Panel 3 is a bed of wet river pebbles; one pebble in it measures %.0f"
         % peb["pebble_texels"], C_TXT),
        ("texture texels. Our bake stretches that texture to 2048 world units a repeat, so a pebble lands %.0f world units = %.1f far-sheet texels"
         % (peb["pebble_baked_units"], peb["pebble_baked_far"]), C_TXT),
        ("across -- panel 6, local variance %.0f against panel 1's %.0f. At the engine's own repeat of 341.333 the same pebble is %.0f world units"
         % (facts["lv_baked"], lv_o, peb["pebble_engine_units"]), C_TXT),
        ("= %.2f of one far-sheet texel: too small to draw, so the ground averages flat -- panel 5, %.0f, the way vanilla's is (panel 2, %.0f)."
         % (peb["pebble_engine_far"], facts["lv_engine"], lv_v), C_TXT),
        ("Measured, not fixed: the 6x tiling (SPLAT1, src/lodgen.cpp TILE=2048 against the engine's 341.333) is open and nothing was rebuilt here.", C_DIM),
    ]
    for i, (s, c) in enumerate(ANS):
        dr.text((M, y_ans + 20 * i), s, font=(F_H if i == 0 else F_S), fill=c)
        assert dr.textlength(s, font=(F_H if i == 0 else F_S)) <= W - 2 * M, \
            'answer line %d overflows the page' % i

    page.save(OUT)
    facts['page'] = page.size
    facts['dominant'] = dict(path=dom_path, rel=dom_rel, share=dom_share,
                             edids=[[a, b] for a, b in dom_edids],
                             ltex=[[e, chain[e]['ltex'], chain[e]['txst']]
                                   for e, _ in dom_edids])
    json.dump(facts, open(os.path.join(HERE, 'picture_facts.json'), 'w'), indent=1)
    print(OUT, page.size)
    for k2 in ('lv_ours', 'lv_van', 'lv_engine', 'lv_baked', 'mip_engine', 'mip_baked'):
        print('  %-10s %.3f' % (k2, facts[k2]))


if __name__ == '__main__':
    main()

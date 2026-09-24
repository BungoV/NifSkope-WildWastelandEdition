#!/usr/bin/env python
"""Offline readers for a baked card set: the sidecar, the sheet PNGs, the DDS.

Nothing here launches the exe. Everything reads what the 2026-09-10 01:1x bake
already wrote into scratchpad/cardortho_20260910/cards/, so the measurement can
be made while Fallout4.exe is up.

The three arms a width measurement needs to be separated over:

  png   the alpha the BAKE wrote  (`<id>_oct_albedo.png`)             -- the bake
  dil   that alpha with `lodgenDilateFrames`' law applied offline     -- dilation
  dds   the alpha the SHIPPED file carries (`<id>_oct_<c>.DDS` mip 0) -- BC3

`lodgenDilateFrames( alb, alb, ... )` is called with img == coverage, so its
`put` writes `qAlpha( img.pixel(x,y) )` back: the claim in the source comment is
that coverage alpha never moves. `dil` is here to MEASURE that claim rather than
quote it.
"""
import json
import os
import struct

import numpy as np
from PIL import Image


def sidecar(path):
    m = {}
    with open(path) as f:
        for line in f:
            g = line.split()
            if g:
                m.setdefault(g[0], []).append(g[1:])
    return m


def card_of(cards, ident):
    """The bake sidecar's own numbers, in the .lodm's meaning."""
    m = sidecar(os.path.join(cards, ident + ".txt"))
    o = m["oct"][0]
    n, tw, th = int(o[0]), int(o[1]), int(o[2])
    c = dict(oct=n, tw=tw, th=th,
             halfW=float(o[3]), halfH=float(o[4]),
             center=(float(o[5]), float(o[6]), float(o[7])),
             model=m["model"][0][0],
             gap=(int(m["gap"][0][0]), int(m["gap"][0][1])) if "gap" in m else (0, 0),
             projection=(m["projection"][0][0] if "projection" in m else "(absent)"))
    if "framefit" in m:
        f = m["framefit"][0]
        c["framefit"] = tuple(float(v) for v in f)
    off = [[(0.0, 0.0)] * n for _ in range(n)]
    for f in m.get("frameoff", []):
        i, j = int(f[0]), int(f[1])
        off[j][i] = (float(f[2]), float(f[3]))
    c["off"] = off
    return c


def lodm_of(cards, ident):
    b = open(os.path.join(cards, ident + "_oct.lodm"), "rb").read()
    assert b[:4] == b"LODM", b[:4]
    return json.loads(b[12:])


# ---------------------------------------------------------------- sheet alphas

def png_alpha(cards, ident):
    im = Image.open(os.path.join(cards, ident + "_oct_albedo.png")).convert("RGBA")
    return np.array(im.split()[3], dtype=np.uint8)


def dilate_frames_alpha(rgba, frame_w, frame_h, passes, floor=16, is_coverage=True):
    """`lodgenDilateFrames` offline, faithfully, INCLUDING the `isCoverage` branch.

    The C++ `put` keeps the existing alpha when `img == coverage` and writes the
    dilated one otherwise, so this takes `is_coverage` as an argument rather than
    assuming it -- and running it BOTH ways is what makes the "coverage does not
    move" result a measurement instead of a tautology: with `is_coverage=False`
    the very same code moves thousands of alphas.

    Returns (alpha, rgb) after the call.
    """
    a = rgba[:, :, 3].astype(np.int32)
    rgb = rgba[:, :, :3].astype(np.int32)
    H, W = a.shape
    filled = (a >= floor)
    out_a = a.copy()
    out_rgb = rgb.copy()
    for fy in range(0, H, frame_h):
        for fx in range(0, W, frame_w):
            y1, x1 = min(H, fy + frame_h), min(W, fx + frame_w)
            f = filled[fy:y1, fx:x1].copy()
            if not f.any():
                continue                      # an empty frame stays as it is
            va = out_a[fy:y1, fx:x1]
            vr = out_rgb[fy:y1, fx:x1]
            avg = [int(vr[:, :, k][f].sum() // f.sum()) for k in range(3)] + [int(va[f].sum() // f.sum())]
            for _ in range(passes):
                # the eight filled neighbours INSIDE the frame, and their sums
                k = np.zeros(f.shape, dtype=np.int32)
                sums = [np.zeros(f.shape, dtype=np.int32) for _ in range(4)]
                src = [vr[:, :, 0], vr[:, :, 1], vr[:, :, 2], va]
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        if dx == 0 and dy == 0:
                            continue
                        pf = np.zeros((f.shape[0] + 2, f.shape[1] + 2), dtype=bool)
                        pf[1:-1, 1:-1] = f
                        nb = pf[1 + dy:1 + dy + f.shape[0], 1 + dx:1 + dx + f.shape[1]]
                        k += nb
                        for c in range(4):
                            ps = np.zeros((f.shape[0] + 2, f.shape[1] + 2), dtype=np.int32)
                            ps[1:-1, 1:-1] = np.where(f, src[c], 0)
                            sums[c] += ps[1 + dy:1 + dy + f.shape[0], 1 + dx:1 + dx + f.shape[1]]
                grow = (~f) & (k > 0)
                kk = np.maximum(k, 1)
                for c in range(3):
                    vr[:, :, c] = np.where(grow, sums[c] // kk, vr[:, :, c])
                if not is_coverage:
                    va[:] = np.where(grow, sums[3] // kk, va)
                f = f | grow
            for c in range(3):
                vr[:, :, c] = np.where(~f, avg[c], vr[:, :, c])
            if not is_coverage:
                va[:] = np.where(~f, avg[3], va)
    return np.clip(out_a, 0, 255).astype(np.uint8), out_rgb


def _dds_header(b):
    assert b[:4] == b"DDS ", b[:4]
    (size, flags, h, w, pitch, depth, mips) = struct.unpack_from("<7I", b, 4)
    fourcc = b[84:88]
    pf_flags = struct.unpack_from("<I", b, 80)[0]
    return dict(w=w, h=h, mips=mips, fourcc=fourcc, pf_flags=pf_flags, off=128)


def dds_alpha_mip0(path):
    """The alpha of a BC3 DDS's top mip, decoded.

    BC3 alpha: two 8-bit endpoints then sixteen 3-bit indices, so a block's
    alpha comes off an 8-entry ramp -- which is the whole reason the BC arm is
    measured separately from the PNG one.
    """
    b = open(path, "rb").read()
    hd = _dds_header(b)
    assert hd["fourcc"] == b"DXT5", hd["fourcc"]
    W, H = hd["w"], hd["h"]
    out = np.zeros((H, W), dtype=np.uint8)
    p = hd["off"]
    for by in range(0, H, 4):
        for bx in range(0, W, 4):
            a0, a1 = b[p], b[p + 1]
            bits = int.from_bytes(b[p + 2:p + 8], "little")
            if a0 > a1:
                ramp = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
            else:
                ramp = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
            for t in range(16):
                idx = (bits >> (3 * t)) & 7
                y, x = by + t // 4, bx + t % 4
                if y < H and x < W:
                    out[y, x] = ramp[idx]
            p += 16
    return out


def frame_alpha(alpha, c, i, j):
    return alpha[j * c["th"]:(j + 1) * c["th"], i * c["tw"]:(i + 1) * c["tw"]]


# ---------------------------------------------------------------- source masks

def mesh_mask(png, tol=24):
    """Everything that is not the viewport's clear colour, at a stated tolerance.

    `tol` is the knob the source arm's own sensitivity is measured over: a bark
    pixel a shade off the background would drop out of the silhouette and make
    the card look wide for a reason that is not the card's.
    """
    im = Image.open(png).convert("RGB")
    a = np.array(im, dtype=np.int32)
    bg = a[1, 1]
    d = np.abs(a - bg).sum(axis=2)
    return (d > tol)


def bbox(mask):
    ys, xs = np.nonzero(mask)
    if not len(xs):
        return None
    return (int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1)


def touches_edge(mask):
    return bool(mask[0, :].any() or mask[-1, :].any() or mask[:, 0].any() or mask[:, -1].any())


def camera(path):
    cam = {}
    for line in open(path):
        for t in line.split():
            if "=" in t:
                cam[t.split("=", 1)[0]] = t.split("=", 1)[1]
    return cam

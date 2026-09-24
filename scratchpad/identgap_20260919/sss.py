#!/usr/bin/env python3
"""IDENTGAP addendum -- SCREEN-SPACE SHADOWS, on top of a far-shadow row.

bungo, 2026-09-19 06:2x: *"for the gaps that the identity does not fill, we have
screen space shadows too"*.

WHAT THIS SIMULATES.  The rendered frame's own depth buffer is marched from each
receiver toward the sun; if the march passes BEHIND a surface the buffer holds,
the receiver is in shadow.  FO4CS's screen-space shadows are that.  This is
run as a SECOND pass over the far-shadow answer: a pixel the far row already
calls dark stays dark; a pixel the far row calls LIT may be darkened here.

THE DEPTH BUFFER is the same G-buffer the truth panel was cast from, so the
occluders are exactly the geometry the camera can see -- no more, no less.
Depth is `(p - eye) . forward`, the camera-space forward depth, which is what
`render.Camera.project` returns as its third value.

THE MARCH, stated in full because every number below depends on it:
  * ray            the sun direction, from the receiver's world position
  * REACH          `reach_frac` x the frame height, IN PIXELS.  The world length
                   that corresponds to is `reach_px * zc / focal_px`, where
                   `focal_px = 0.5 * h / tan(vfov/2)`: the standard
                   pixels-per-world-unit at the receiver's own depth.  So a
                   distant receiver marches farther in world units and the same
                   distance on screen, which is what a screen-space effect does.
  * N              steps, evenly spaced in world distance along the ray
  * start offset   the first sample sits at 1/N of the reach, never at 0
  * HIT            the sample is behind the buffer by more than the BIAS and by
                   less than the THICKNESS: `bias < zc_sample - zc_buffer < thick`
  * BIAS and THICKNESS are BOTH RELATIVE TO DEPTH, and that is not a detail.
                   A fixed world-unit bias is meaningless in a far-LOD frame: at
                   20,000 units one screen pixel already spans ~14 world units,
                   so an 8-unit bias calls every grazing surface its own
                   occluder.  Measured: a fixed 8 u bias darkened 25% of the
                   object pixels on camera `hwydeck` -- an unusable picture.  So
                   `bias = bias_rel * zc` and `thick = thick_rel * zc`, and both
                   are SWEPT by `run.py` rather than guessed, on the same
                   objective the far-shadow bias was tuned on.
  * start offset   the ray starts at `P + n * bias` so a receiver never shadows
                   itself, then samples at 1/N .. N/N of the reach
  * THICKNESS is still a GUESS in kind, whatever value the sweep picks: a depth
                   buffer says where a surface IS and never how thick it is.
                   Too small and the march tunnels through walls; too large and
                   every silhouette grows a shadow behind it.
  * sky            a sample landing on a sky pixel is a miss, never a hit

WHAT IT CANNOT DO, and this is not a caveat, it is the result's boundary:
  1. ONLY ON-SCREEN OCCLUDERS.  A wall behind the camera, or past the frame
     edge, or hidden behind a nearer surface, does not exist for this pass.
  2. THE THICKNESS GUESS above.
  3. REACH.  At 10% of frame height the march cannot find a caster further away
     on screen than that, so a shadow whose caster is across the street is out
     of range whatever the step count.
"""
import numpy as np

BIAS_REL = 0.005      # of the camera distance, before a sample counts as behind
THICK_REL = 0.05      # of the camera distance


def march(cam, gb, sundir, base_lit, cand, steps=32, reach_frac=0.10,
          bias_rel=BIAS_REL, thick_rel=THICK_REL):
    """Darken `cand` pixels whose screen-space march toward the sun is blocked.

    `base_lit` is the far-shadow row's answer (bool per pixel); `cand` the
    pixels this pass is allowed to change (it only ever takes light away).
    Returns the new lit array and the number of pixels it darkened."""
    h, w = cam.h, cam.w
    focal = 0.5 * h / cam.ty
    zbuf = np.full(len(gb.kind), np.inf)
    surf = gb.kind != 0
    zbuf[surf] = (gb.pos[surf] - cam.eye[None, :]) @ cam.f
    zb = zbuf.reshape(h, w)
    sky = ~surf.reshape(h, w)

    idx = np.nonzero(cand)[0]
    if idx.size == 0:
        return base_lit.copy(), 0
    P = gb.pos[idx]
    zc0 = (P - cam.eye[None, :]) @ cam.f
    # the start offset and the depth slack are both a fraction of the receiver's
    # own camera distance, because one screen pixel IS a fraction of it
    off = bias_rel * np.maximum(zc0, 1.0)
    P = P + gb.nrm[idx] * off[:, None]
    reach_px = reach_frac * h
    L = reach_px * np.maximum(zc0, 1.0) / focal        # world units of reach
    hit = np.zeros(idx.size, dtype=bool)
    for k in range(1, steps + 1):
        t = L * (k / float(steps))
        S = P + sundir[None, :] * t[:, None]
        sx, sy, zc = cam.project(S)
        ix = np.floor(sx).astype(np.int64)
        iy = np.floor(sy).astype(np.int64)
        on = (ix >= 0) & (ix < w) & (iy >= 0) & (iy < h) & (zc > 1.0)
        if not on.any():
            continue
        jx, jy = ix[on], iy[on]
        zbv = zb[jy, jx]
        good = ~sky[jy, jx]
        d = zc[on] - zbv
        h_ = good & (d > bias_rel * zbv) & (d < thick_rel * zbv)
        sel = np.nonzero(on)[0][h_]
        hit[sel] = True
    out = base_lit.copy()
    out[idx[hit]] = False
    return out, int(hit.sum())


def sun_screen_len(cam, gb, sundir, m, reach_frac):
    """Diagnostic: the median screen-space length, in pixels, that the reach
    actually buys on this camera -- so the caption can say it."""
    P = gb.pos[m]
    zc0 = (P - cam.eye[None, :]) @ cam.f
    focal = 0.5 * cam.h / cam.ty
    L = reach_frac * cam.h * np.maximum(zc0, 1.0) / focal
    s0 = np.stack(cam.project(P)[:2], axis=1)
    s1 = np.stack(cam.project(P + sundir[None, :] * L[:, None])[:2], axis=1)
    return float(np.median(np.linalg.norm(s1 - s0, axis=1)))

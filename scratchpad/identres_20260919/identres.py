#!/usr/bin/env python3
"""IDENTRES -- the engine.  Four ways of asking the SAME question.

bungo, 2026-09-19: *"why is the identity again so pixelated ... where are those
large pixels coming from"*, looking at
`identprox_20260919/images/hwy_today_hwydeck_az180_el10_t64.png`.

The claim under test is that the blocks are the TEST's resolution, not the
identity data.  So the identity rule is held fixed -- today's shipped `.lodi` v7
group table, an object receiver darkened only when the nearer caster carries a
DIFFERENT group -- and the only thing that moves is how the occluder is looked
up:

  R1  64 u a texel, ONE nearest tap, flat bias          (identprox's baseline)
  R2  16 u a texel, 3x3 percentage-closer, slope bias
  R3   4 u a texel, 3x3 percentage-closer, slope bias   (a GPU cascade)
  R4  no map at all -- the LEFT panel's own sun cast, re-run carrying the
      caster's identity, a blocker discarded when it carries the receiver's own
      identity.  The limit case.

R1 is `identprox/hwy_render.query` CALLED, not re-typed, so the baseline numbers
are reproduced rather than re-derived.  R2/R3 are the same lookup with a tap
loop around it; `--control` proves the 1-tap case is byte-identical to R1.

R4's machinery is `render.SunShadow`'s geometry with one thing added: each cell
carries not only the largest `s = z - u*tan(el)` and the identity that achieved
it, but ALSO the largest `s` among entries carrying a DIFFERENT identity.  That
second plane is what lets a receiver discard its own group and still see
everything else, exactly, with no map resolution anywhere in the answer.  Two
scatter passes build it (pass 2 re-scatters only fragments whose identity is not
the cell's winner) and one right-to-left merge scan accumulates it along the sun
direction, the same suffix maximum the truth panel takes.  `--control` also
re-runs R4 with the identity rule switched OFF: it must then agree with the LEFT
panel on 100.00% of the pixels, which is the refuter that R4 differs from the
truth ONLY by the identity rule.
"""
import numpy as np
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
IP = ROOT + '/scratchpad/identprox_20260919'
H4 = ROOT + '/scratchpad/horizon4_20260919'
SUNSIM = ROOT + '/scratchpad/sunsim1_20260919'
for q in (IP, SUNSIM, ROOT + '/tests/spells'):
    if q not in sys.path:
        sys.path.insert(0, q)

import h4map as MP                                            # noqa: E402
import hwy_render as HR                                       # noqa: E402
import render as RD                                           # noqa: E402

TERR_KEY = 0xFFFFF


# ---------------------------------------------------------------------------
def query_pcf(mp, pos, nrm, ident, normal_bias=1.0, depth_bias=0.5, slope=0.0,
              slope_clamp=8.0, taps=1, use_identity=True):
    """`hwy_render.query` with a (taps x taps) percentage-closer box around it.

    Returns (dark_fraction, any_nearer).  `taps=1` is the single nearest tap and
    is asserted equal to `hwy_render.query` by `--control`."""
    p = pos + nrm * (normal_bias * mp.texel)
    u = p[:, 0] * mp.sx + p[:, 1] * mp.sy
    v = -p[:, 0] * mp.sy + p[:, 1] * mp.sx
    s = p[:, 2] - u * mp.tan
    iv0 = np.floor((v - mp.v0) / mp.texel).astype(np.int64)
    is0 = np.floor((s - mp.s0) / mp.texel).astype(np.int64)
    b = depth_bias
    if slope > 0.0:
        nu = nrm[:, 0] * mp.sx + nrm[:, 1] * mp.sy + nrm[:, 2] * mp.tan
        nv = -nrm[:, 0] * mp.sy + nrm[:, 1] * mp.sx
        ns_ = nrm[:, 2]
        den = np.where(np.abs(nu) < 1e-6, 1e-6, np.abs(nu))
        g = np.maximum(np.abs(nv) / den, np.abs(ns_) / den)
        b = depth_bias + slope * np.minimum(slope_clamp, g)
    r = taps // 2
    acc = np.zeros(len(u))
    anyn = np.zeros(len(u), dtype=bool)
    nod = np.zeros(len(u), dtype=bool)
    n = 0
    for dv in range(-r, r + 1):
        for ds in range(-r, r + 1):
            iv = iv0 + dv
            isv = is0 + ds
            inside = (iv >= 0) & (iv < mp.nv) & (isv >= 0) & (isv < mp.ns)
            pix = np.where(inside, iv * mp.ns + np.clip(isv, 0, mp.ns - 1), 0)
            k = mp.KEY[pix]
            mu, mi = mp._unkey(np.maximum(k, 0))
            has = inside & (k >= 0)
            nearer = has & (mu > u + b * mp.texel)
            if use_identity:
                same = (mi == ident) & (ident != MP.TERRAIN_ID)
                dark = nearer & ~same
            else:
                dark = nearer
            acc += dark
            anyn |= nearer
            if dv == 0 and ds == 0:
                nod = ~has
            n += 1
    return acc / n, anyn, nod


# ---------------------------------------------------------------------------
class IdentShear(object):
    """`render.SunShadow` carrying identity, plus the runner-up of a DIFFERENT
    identity, so a receiver can discard its own group exactly."""

    NEAR_CELL = RD.SunShadow.NEAR_CELL       # 16 u -- the truth panel's own
    FAR_CELL = RD.SunShadow.FAR_CELL         # 256 u
    FAR_REACH = RD.SunShadow.FAR_REACH
    RISE = 12.0                              # `SunShadow.lit`'s own bias

    def __init__(self, ter, ob, gtri, az, el, margin=2048.0, verbose=True):
        from scene import CHUNK
        self.az, self.el = az, el
        a = np.radians(az)
        self.sx, self.sy = np.sin(a), np.cos(a)
        self.tan = np.tan(np.radians(el))
        x0, y0, x1, y1 = CHUNK
        x0 -= margin; y0 -= margin; x1 += margin; y1 += margin
        cor = np.array([[x0, y0], [x1, y0], [x0, y1], [x1, y1]])
        U = cor[:, 0] * self.sx + cor[:, 1] * self.sy
        V = -cor[:, 0] * self.sy + cor[:, 1] * self.sx
        self.nu0, self.nu1 = U.min() - 64, U.max() + 64
        self.nv0, self.nv1 = V.min() - 64, V.max() + 64
        nw = int((self.nu1 - self.nu0) / self.NEAR_CELL) + 1
        nh = int((self.nv1 - self.nv0) / self.NEAR_CELL) + 1
        uu = self.nu0 + (np.arange(nw) + 0.5) * self.NEAR_CELL
        vv = self.nv0 + (np.arange(nh) + 0.5) * self.NEAR_CELL
        UU, VV = np.meshgrid(uu, vv)
        WX = UU * self.sx - VV * self.sy
        WY = UU * self.sy + VV * self.sx
        self.nw, self.nh = nw, nh
        # --- pass 1: terrain into every cell, then every triangle fragment
        ster = ter.atf(WX, WY) - UU * self.tan
        K1 = self._key(ster, TERR_KEY)
        self._splat(K1, ob, gtri, None)
        bs, bi = self._unkey(K1)
        # --- pass 2: only entries whose identity is not the cell's winner
        K2 = np.full((nh, nw), np.int64(-1))
        m = bi != TERR_KEY
        K2[m] = self._key(ster[m], TERR_KEY)
        self._splat(K2, ob, gtri, bi)
        ss, _ = self._unkey(K2)
        ss = np.where(K2 >= 0, ss, -1e30)
        # --- the suffix merge along u
        self.bs, self.bi, self.ss = self._scan(bs, bi, ss)
        # --- far grid: terrain only, the truth panel's own
        self.fu0 = self.nu0 - self.FAR_REACH
        self.fv0 = self.nv0 - self.FAR_REACH
        fw = int((self.nu1 + self.FAR_REACH - self.fu0) / self.FAR_CELL) + 1
        fh = int((self.nv1 + self.FAR_REACH - self.fv0) / self.FAR_CELL) + 1
        fu = self.fu0 + (np.arange(fw) + 0.5) * self.FAR_CELL
        fv = self.fv0 + (np.arange(fh) + 0.5) * self.FAR_CELL
        FU, FV = np.meshgrid(fu, fv)
        far = ter.atf(FU * self.sx - FV * self.sy,
                      FU * self.sy + FV * self.sx) - FU * self.tan
        self.far = np.maximum.accumulate(far[:, ::-1], axis=1)[:, ::-1].astype(np.float32)
        self.fw, self.fh = fw, fh
        if verbose:
            print('  identshear az %.0f el %.0f: near %d x %d, far %d x %d'
                  % (az, el, nh, nw, fh, fw))

    # -- packing, the same shape `h4map` uses
    @staticmethod
    def _key(s, ident):
        q = np.clip(np.rint((np.asarray(s) + 1.0e6) * 8.0), 0, 2 ** 40 - 1).astype(np.int64)
        return (q << np.int64(20)) | (np.asarray(ident, dtype=np.int64) & np.int64(0xFFFFF))

    @staticmethod
    def _unkey(k):
        kk = np.maximum(k, 0)
        return (kk >> np.int64(20)).astype(np.float64) / 8.0 - 1.0e6, kk & np.int64(0xFFFFF)

    def _put(self, KEY, ix, iy, s, ident, skip_bi):
        ok = (ix >= 0) & (ix < self.nw) & (iy >= 0) & (iy < self.nh)
        if not ok.any():
            return
        ix, iy, s, ident = ix[ok], iy[ok], s[ok], ident[ok]
        if skip_bi is not None:
            keep = ident != skip_bi[iy, ix]
            ix, iy, s, ident = ix[keep], iy[keep], s[keep], ident[keep]
            if ix.size == 0:
                return
        np.maximum.at(KEY.reshape(-1), iy * self.nw + ix, self._key(s, ident))

    def _splat(self, KEY, ob, gtri, skip_bi):
        """`SunShadow._splat_tris` with the identity carried through."""
        v = ob.v.astype(np.float64)
        U = v[:, 0] * self.sx + v[:, 1] * self.sy
        V = -v[:, 0] * self.sy + v[:, 1] * self.sx
        S = v[:, 2] - U * self.tan
        gx = (U - self.nu0) / self.NEAR_CELL
        gy = (V - self.nv0) / self.NEAR_CELL
        t = ob.tri
        vid = np.zeros(len(v), dtype=np.int64)
        for c in range(3):
            vid[t[:, c]] = gtri
        px, py, ps, pi = [gx], [gy], [S], [vid]
        for i, j in ((0, 1), (1, 2), (2, 0)):
            px.append(0.5 * (gx[t[:, i]] + gx[t[:, j]]))
            py.append(0.5 * (gy[t[:, i]] + gy[t[:, j]]))
            ps.append(0.5 * (S[t[:, i]] + S[t[:, j]]))
            pi.append(gtri)
        self._put(KEY, np.floor(np.concatenate(px)).astype(np.int64),
                  np.floor(np.concatenate(py)).astype(np.int64),
                  np.concatenate(ps), np.concatenate(pi), skip_bi)
        x0 = np.floor(np.minimum.reduce([gx[t[:, 0]], gx[t[:, 1]], gx[t[:, 2]]])).astype(np.int64)
        x1 = np.ceil(np.maximum.reduce([gx[t[:, 0]], gx[t[:, 1]], gx[t[:, 2]]])).astype(np.int64)
        y0 = np.floor(np.minimum.reduce([gy[t[:, 0]], gy[t[:, 1]], gy[t[:, 2]]])).astype(np.int64)
        y1 = np.ceil(np.maximum.reduce([gy[t[:, 0]], gy[t[:, 1]], gy[t[:, 2]]])).astype(np.int64)
        span = np.maximum(x1 - x0, y1 - y0)
        lg = np.clip(np.ceil(np.log2(np.maximum(span, 1))).astype(int), 0, 14)
        for L in np.unique(lg):
            K = 1 << int(L)
            sel = np.nonzero(lg == L)[0]
            per = max(1, int(6e6 // (K * K)))
            for s0 in range(0, sel.size, per):
                ss = sel[s0:s0 + per]
                GX = x0[ss][:, None, None] + np.arange(K)[None, None, :]
                GY = y0[ss][:, None, None] + np.arange(K)[None, :, None]
                cx = GX + 0.5
                cy = GY + 0.5
                ax, ay, av = gx[t[ss, 0]][:, None, None], gy[t[ss, 0]][:, None, None], S[t[ss, 0]][:, None, None]
                bx, by, bv = gx[t[ss, 1]][:, None, None], gy[t[ss, 1]][:, None, None], S[t[ss, 1]][:, None, None]
                c3x, c3y, cv = gx[t[ss, 2]][:, None, None], gy[t[ss, 2]][:, None, None], S[t[ss, 2]][:, None, None]
                area = (bx - ax) * (c3y - ay) - (by - ay) * (c3x - ax)
                aa = np.where(np.abs(area) < 1e-12, 1e-12, area)
                w0 = ((bx - cx) * (c3y - cy) - (by - cy) * (c3x - cx)) / aa
                w1 = ((c3x - cx) * (ay - cy) - (c3y - cy) * (ax - cx)) / aa
                w2 = 1.0 - w0 - w1
                ins = ((w0 >= 0) & (w1 >= 0) & (w2 >= 0)
                       & (GX < x1[ss][:, None, None]) & (GY < y1[ss][:, None, None]))
                ins = np.broadcast_to(ins, (len(ss), K, K))
                k = np.nonzero(ins.ravel())[0]
                if k.size == 0:
                    continue
                sv = np.broadcast_to(w0 * av + w1 * bv + w2 * cv, ins.shape).ravel()[k]
                iv = np.broadcast_to(gtri[ss][:, None, None], ins.shape).ravel()[k]
                self._put(KEY, np.broadcast_to(GX, ins.shape).ravel()[k],
                          np.broadcast_to(GY, ins.shape).ravel()[k], sv, iv, skip_bi)

    def _scan(self, bs, bi, ss):
        """acc[:, i] = the (best, best-id, best-of-another-id) over columns >= i."""
        nh, nw = bs.shape
        obs = np.empty_like(bs)
        obi = np.empty_like(bi)
        oss = np.empty_like(ss)
        rs = np.full(nh, -1e30)
        ri = np.full(nh, np.int64(-1))
        rc = np.full(nh, -1e30)
        for i in range(nw - 1, -1, -1):
            a_s, a_i, a_c = bs[:, i], bi[:, i], ss[:, i]
            win = a_s >= rs
            m_s = np.where(win, a_s, rs)
            m_i = np.where(win, a_i, ri)
            w_c = np.where(win, a_c, rc)
            l_s = np.where(win, rs, a_s)
            l_i = np.where(win, ri, a_i)
            l_c = np.where(win, rc, a_c)
            m_c = np.maximum(w_c, np.where(l_i != m_i, l_s, l_c))
            obs[:, i] = m_s
            obi[:, i] = m_i
            oss[:, i] = m_c
            rs, ri, rc = m_s, m_i, m_c
        return obs, obi, oss

    # ------------------------------------------------------------- receivers
    def lit(self, pos, ident, use_identity=True):
        """True where the receiver can see the sun under the identity rule."""
        x, y, z = pos[:, 0], pos[:, 1], pos[:, 2]
        u = x * self.sx + y * self.sy
        v = -x * self.sy + y * self.sx
        s = z - u * self.tan + self.RISE
        blocked = np.zeros(len(x), dtype=bool)
        ix = np.floor((u - self.nu0) / self.NEAR_CELL).astype(np.int64) + 1
        iy = np.floor((v - self.nv0) / self.NEAR_CELL).astype(np.int64)
        ok = (ix >= 0) & (ix < self.nw) & (iy >= 0) & (iy < self.nh)
        own = np.zeros(len(x), dtype=bool)
        if ok.any():
            bs = self.bs[iy[ok], ix[ok]]
            bi = self.bi[iy[ok], ix[ok]]
            sc = self.ss[iy[ok], ix[ok]]
            g = ident[ok]
            mine = (g != MP.TERRAIN_ID) & (bi == g)
            if use_identity:
                b = np.where(mine, sc > s[ok], bs > s[ok])
            else:
                b = bs > s[ok]
            blocked[ok] |= b
            own[ok] = mine & (bs > s[ok])
        fx = np.floor((u - self.fu0) / self.FAR_CELL).astype(np.int64) + 1
        fy = np.floor((v - self.fv0) / self.FAR_CELL).astype(np.int64)
        ok = (fx >= 0) & (fx < self.fw) & (fy >= 0) & (fy < self.fh)
        fb = np.zeros(len(x), dtype=bool)
        if ok.any():
            fb[ok] = self.far[fy[ok], fx[ok]] > s[ok]
        blocked |= fb
        return ~blocked, own & ~fb

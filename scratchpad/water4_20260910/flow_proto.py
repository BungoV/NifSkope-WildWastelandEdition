# -*- coding: utf-8 -*-
"""flow_proto.py -- the potential-flow solve and the dye pass, in numpy, as the
KNOWN-ANSWER prototype of what src/watermark.cpp implements.  Same method
(section 0 of the lane report): masked 5-point stencil with face conductance
= harmonic mean of the cells' depth, no-flux at every bank face, Jacobi-
preconditioned CG on the compacted wet set, u = -grad phi from the face
fluxes, dye by one descending-potential pass.

    python flow_proto.py            # runs gates F1..F4, F6, F7 and prints them

The C++ is the deliverable; this file exists so the gate NUMBERS were seen to
be reachable by the method before the C++ was written, and so a C++ failure
can be told apart from a method failure.
"""
import math
import sys

import numpy as np


# ---------------------------------------------------------------- solver ---
class Grid(object):
    """A masked grid: wet[h, w] bool, k[h, w] conductance."""

    def __init__(self, wet, k=None, boost=None):
        self.wet = wet.astype(bool)
        self.h, self.w = wet.shape
        self.k = np.ones(wet.shape) if k is None else k.astype(float)
        if boost is not None:
            self.k = self.k * boost
        self.idx = -np.ones(wet.shape, dtype=np.int64)
        ys, xs = np.nonzero(self.wet)
        self.n = len(ys)
        self.idx[ys, xs] = np.arange(self.n)
        self.ys, self.xs = ys, xs
        # faces: east (x+1) and north (y+1), both cells wet
        e = self.wet[:, :-1] & self.wet[:, 1:]
        nn = self.wet[:-1, :] & self.wet[1:, :]
        ey, ex = np.nonzero(e)
        ny, nx = np.nonzero(nn)
        self.fi = np.concatenate([self.idx[ey, ex], self.idx[ny, nx]])
        self.fj = np.concatenate([self.idx[ey, ex + 1], self.idx[ny + 1, nx]])
        ki = self.k.ravel()[np.ravel_multi_index((np.concatenate([ey, ny]), np.concatenate([ex, nx])), wet.shape)]
        kj = self.k.ravel()[np.ravel_multi_index((np.concatenate([ey, ny + 1]), np.concatenate([ex + 1, nx])), wet.shape)]
        self.kf = 2.0 * ki * kj / (ki + kj)
        self.nE = len(ey)                      # the first nE faces are east faces
        self.diag = np.bincount(self.fi, self.kf, self.n) + np.bincount(self.fj, self.kf, self.n)
        # connected components over the faces (a body may be several pieces:
        # the bridge rule joins components up to 2 texels apart)
        parent = np.arange(self.n)

        def find(a):
            while parent[a] != a:
                parent[a] = parent[parent[a]]
                a = parent[a]
            return a
        for a, b in zip(self.fi, self.fj):
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[ra] = rb
        roots = np.array([find(i) for i in range(self.n)])
        _, self.comp = np.unique(roots, return_inverse=True)
        self.nComp = int(self.comp.max()) + 1 if self.n else 0

    def matvec(self, x, dirichlet=None):
        y = self.diag * x
        y -= np.bincount(self.fi, self.kf * x[self.fj], self.n)
        y -= np.bincount(self.fj, self.kf * x[self.fi], self.n)
        if dirichlet is not None:
            y[dirichlet] = x[dirichlet] * self.diag[dirichlet]
        return y

    def solve(self, b, dirichlet=None, tol=1e-9, cap=20000):
        """div(k grad phi) = -b, i.e. A phi = b with A the positive stencil.
        `b` > 0 is a source.  `dirichlet`: bool mask over wet cells held at 0
        (an open far field).  Returns phi, iterations, relative residual."""
        b = b.astype(float).copy()
        if dirichlet is not None and dirichlet.any():
            b[dirichlet] = 0.0
            d = dirichlet
        else:
            d = None
        # consistency PER COMPONENT: a piece with no Dirichlet cell is a
        # pure-Neumann system of its own and its sources must balance
        hasD = np.zeros(self.nComp, bool)
        if d is not None:
            hasD = np.bincount(self.comp[d], minlength=self.nComp) > 0
        sums = np.bincount(self.comp, b, self.nComp)
        cnt = np.bincount(self.comp, minlength=self.nComp)
        mean = np.where(hasD, 0.0, sums / np.maximum(cnt, 1))
        b -= mean[self.comp]
        nb = np.linalg.norm(b)
        if nb == 0.0:
            return np.zeros(self.n), 0, 0.0
        x = np.zeros(self.n)
        r = b - self.matvec(x, d)
        Minv = np.where(self.diag > 0, 1.0 / np.maximum(self.diag, 1e-300), 0.0)
        z = Minv * r
        p = z.copy()
        rz = r @ z
        it = 0
        res = np.linalg.norm(r) / nb
        while it < cap and res > tol:
            Ap = self.matvec(p, d)
            alpha = rz / (p @ Ap)
            x += alpha * p
            r -= alpha * Ap
            if d is None:
                x -= x.mean()
            z = Minv * r
            rz2 = r @ z
            p = z + (rz2 / rz) * p
            rz = rz2
            it += 1
            res = np.linalg.norm(r) / nb
        return x, it, res

    def face_flux(self, phi):
        """F on each face, positive from i to j: F = -kf (phi_j - phi_i)."""
        return -self.kf * (phi[self.fj] - phi[self.fi])

    def velocity(self, phi):
        """Cell velocity u = flux / k, each axis the mean of its two faces,
        a wall face counting as zero."""
        F = self.face_flux(phi)
        ux = np.zeros(self.n)
        uy = np.zeros(self.n)
        fe = F[:self.nE] / self.kf[:self.nE]
        fn = F[self.nE:] / self.kf[self.nE:]
        # each face velocity contributes half to both cells it touches
        ux += 0.5 * np.bincount(self.fi[:self.nE], fe, self.n)
        ux += 0.5 * np.bincount(self.fj[:self.nE], fe, self.n)
        uy += 0.5 * np.bincount(self.fi[self.nE:], fn, self.n)
        uy += 0.5 * np.bincount(self.fj[self.nE:], fn, self.n)
        return ux, uy

    def divergence(self, phi):
        F = self.face_flux(phi)
        return np.bincount(self.fi, F, self.n) - np.bincount(self.fj, F, self.n)

    def field(self, v):
        out = np.zeros((self.h, self.w))
        out[self.ys, self.xs] = v
        return out

    def dye(self, phi, held, L, ux=None, uy=None):
        """Steady advection-decay: c at each wet cell from its upwind
        neighbours, decayed by exp(-ds/L) with ds the mean chord through the
        cell along the flow; `held` maps cell index -> concentration for
        source cells; L is the HALF-distance (weight 1/2 at L, 1/8 at 3 L).
        One pass in descending phi."""
        if ux is None:
            ux, uy = self.velocity(phi)
        F = self.face_flux(phi)
        # inflow lists per cell: for face (i, j, F): F > 0 flows i -> j
        src = np.where(F > 0, self.fi, self.fj)
        dst = np.where(F > 0, self.fj, self.fi)
        mag = np.abs(F)
        order = np.argsort(-phi, kind='stable')
        # adjacency by destination
        byDst = [[] for _ in range(self.n)]
        for f in range(len(F)):
            if mag[f] > 0:
                byDst[dst[f]].append(f)
        c = np.zeros(self.n)
        speed = np.hypot(ux, uy)
        chord = np.where(speed > 0, speed / (np.abs(ux) + np.abs(uy) + 1e-30), 1.0)
        for i in order:
            if i in held:
                c[i] = held[i]
                continue
            tot = 0.0
            acc = 0.0
            for f in byDst[i]:
                acc += mag[f] * c[src[f]]
                tot += mag[f]
            if tot > 0:
                c[i] = acc / tot * math.pow(0.5, chord[i] / L)   # L is the HALF-distance
        return c


def slack_fill(g, ux, uy, floor=0.02, omega=1.9, tol=1e-4, cap=4000):
    """The hybrid: where the water moves (speed >= floor * mean) the solved
    direction stands; where it does not (dead-end coves, the water past the
    sink) the direction is CONTINUED into it by the harmonic fill -- Dirichlet
    at the moving water, Neumann at the banks -- so slack water takes the
    direction of the nearest moving water and no seam appears.  Returns unit
    (dx, dy) per cell and the count of filled cells."""
    sp = np.hypot(ux, uy)
    mean = sp.mean() if g.n else 0.0
    moving = sp >= floor * mean
    dx = np.where(moving, ux / np.maximum(sp, 1e-300), 0.0)
    dy = np.where(moving, uy / np.maximum(sp, 1e-300), 0.0)
    free = ~moving
    if free.any() and moving.any():
        # neighbour lists
        nb = [[] for _ in range(g.n)]
        for a, b in zip(g.fi, g.fj):
            nb[a].append(b); nb[b].append(a)
        freeIdx = np.nonzero(free)[0]
        for it in range(cap):
            worst = 0.0
            for i in freeIdx:
                if not nb[i]:
                    continue
                sx = sum(dx[j] for j in nb[i]) / len(nb[i])
                sy = sum(dy[j] for j in nb[i]) / len(nb[i])
                nx, ny = dx[i] + omega * (sx - dx[i]), dy[i] + omega * (sy - dy[i])
                worst = max(worst, abs(nx - dx[i]), abs(ny - dy[i]))
                dx[i], dy[i] = nx, ny
            if worst < tol:
                break
        m = np.hypot(dx, dy)
        dx = np.where(m > 1e-9, dx / np.maximum(m, 1e-300), 0.0)
        dy = np.where(m > 1e-9, dy / np.maximum(m, 1e-300), 0.0)
    return dx, dy, int(free.sum())


def angle_deg(ux, uy):
    return np.degrees(np.arctan2(uy, ux))


# ----------------------------------------------------------------- gates ---
def gate(name, ok, detail):
    print('  %s %s -- %s' % ('ok  ' if ok else 'FAIL', name, detail))
    return 0 if ok else 1


def f1_continuity():
    print('F1 continuity: channel 256 x 32 narrowing to 16 at x = 128')
    w, h = 256, 32
    wet = np.zeros((h, w), bool)
    wet[:, :128] = True
    wet[8:24, 128:] = True
    g = Grid(wet)
    b = np.zeros(g.n)
    src = g.idx[:, 0][wet[:, 0]]
    snk = g.idx[:, w - 1][wet[:, w - 1]]
    b[src] += 1.0 / len(src)
    b[snk] -= 1.0 / len(snk)
    phi, it, res = g.solve(b)
    ux, uy = g.velocity(phi)
    sp = g.field(np.hypot(ux, uy))
    s64 = sp[wet[:, 64], 64].mean()
    s192 = sp[wet[:, 192], 192].mean()
    ratio = s192 / s64
    fails = gate('speed ratio 2.00 +- 5%%', 1.90 <= ratio <= 2.10,
                 'x=64 %.5f, x=192 %.5f, ratio %.4f (%d iterations, residual %.1e)' % (s64, s192, ratio, it, res))
    # flux through 10 cross-sections: sum of east-face fluxes at column x
    F = g.face_flux(phi)[:g.nE]
    fx = g.xs[g.fi[:g.nE]]
    fl = [F[fx == x].sum() for x in range(16, 256, 24)]
    m = np.mean(fl)
    dev = max(abs(v - m) / m for v in fl)
    fails += gate('flux constant across 10 sections within 3%%', dev < 0.03,
                  'max deviation %.2e (fluxes %s)' % (dev, ' '.join('%.4f' % v for v in fl)))
    return fails, g, phi


def f2_island():
    print('F2 island: channel 256 x 128, disc R = 8 at (128, 64)')
    w, h, R, cx, cy = 256, 128, 8.0, 128.0, 64.0
    yy, xx = np.mgrid[0:h, 0:w]
    wet = np.ones((h, w), bool)
    wet[(xx + 0.5 - cx) ** 2 + (yy + 0.5 - cy) ** 2 < R * R] = False
    g = Grid(wet)
    b = np.zeros(g.n)
    src = g.idx[:, 0][wet[:, 0]]
    snk = g.idx[:, w - 1][wet[:, w - 1]]
    b[src] += 1.0 / len(src)
    b[snk] -= 1.0 / len(snk)
    phi, it, res = g.solve(b)
    ux, uy = g.velocity(phi)
    U = g.field(ux); V = g.field(uy)
    F = g.face_flux(phi)[:g.nE]
    fx = g.xs[g.fi[:g.nE]]; fy = g.ys[g.fi[:g.nE]]
    north = F[(fx == 128) & (fy >= 64)].sum()
    south = F[(fx == 128) & (fy < 64)].sum()
    fails = gate('parts and rejoins: north + south = inflow within 1%%, |north - south| < 2%%',
                 abs(north + south - 1.0) < 0.01 and abs(north - south) < 0.02,
                 'north %.4f south %.4f sum %.4f (%d iterations, residual %.1e)' % (north, south, north + south, it, res))
    div = g.divergence(phi) - b
    fails += gate('mass balance at every wet cell to 1e-6 of the inflow', np.abs(div).max() < 1e-6,
                  'max |div - S| = %.2e' % np.abs(div).max())
    # tangency at the straight banks outside 3R of the island
    far = np.abs(xx + 0.5 - cx) > 3 * R
    bank = wet & far & ((yy == 0) | (yy == h - 1))
    sp = np.hypot(U, V)
    nrm = np.abs(V[bank]) / np.maximum(sp[bank], 1e-30)
    fails += gate('straight-bank tangency: normal component < sin(1 deg) at every bank texel',
                  nrm.max() < math.sin(math.radians(1.0)),
                  'max normal fraction %.2e over %d bank texels (sin 1 deg = %.4f)' % (nrm.max(), bank.sum(), math.sin(math.radians(1))))
    # island bank cells vs the analytic cylinder
    dry4 = ~wet
    nb = np.zeros_like(wet)
    nb[:, :-1] |= dry4[:, 1:]; nb[:, 1:] |= dry4[:, :-1]
    nb[:-1, :] |= dry4[1:, :]; nb[1:, :] |= dry4[:-1, :]
    isl = wet & nb & (np.abs(xx + 0.5 - cx) < 2 * R) & (np.abs(yy + 0.5 - cy) < 2 * R)
    Uinf = sp[wet & far].mean()
    dx = xx + 0.5 - cx; dy = yy + 0.5 - cy
    r = np.hypot(dx, dy); th = np.arctan2(dy, dx)
    ur = Uinf * (1 - R * R / (r * r)) * np.cos(th)
    ut = -Uinf * (1 + R * R / (r * r)) * np.sin(th)
    ax = ur * np.cos(th) - ut * np.sin(th)
    ay = ur * np.sin(th) + ut * np.cos(th)
    sel = isl & (sp > 0.05 * Uinf)
    d = np.degrees(np.arccos(np.clip((U[sel] * ax[sel] + V[sel] * ay[sel]) / (sp[sel] * np.hypot(ax[sel], ay[sel])), -1, 1)))
    fails += gate('island bank direction vs analytic cylinder: mean < 5 deg, max < 15 deg',
                  d.mean() < 5.0 and d.max() < 15.0,
                  'mean %.2f max %.2f deg over %d bank texels (%d stagnation texels skipped)' % (d.mean(), d.max(), sel.sum(), (isl & ~sel).sum()))
    return fails, g, phi


def disc_mask(R, pad=4):
    n = int(2 * R + 2 * pad)
    yy, xx = np.mgrid[0:n, 0:n]
    c = n / 2.0
    return (xx + 0.5 - c) ** 2 + (yy + 0.5 - c) ** 2 < R * R, c


def f3_lake_closed():
    print('F3 lake, no outlet: disc R = 32, no constraint')
    wet, c = disc_mask(32)
    g = Grid(wet)
    phi, it, res = g.solve(np.zeros(g.n))
    ux, uy = g.velocity(phi)
    sp = np.hypot(ux, uy).max()
    return gate('max speed = 0 exactly with no sources', sp == 0.0, 'max speed %.3e, %d iterations' % (sp, it))


def face_velocities(g, phi):
    """Per wet cell: (vW, vE, vS, vN) face velocities = flux / kf, wall = 0."""
    F = g.face_flux(phi)
    fv = F / g.kf
    vW = np.zeros(g.n); vE = np.zeros(g.n); vS = np.zeros(g.n); vN = np.zeros(g.n)
    vE[g.fi[:g.nE]] = fv[:g.nE]; vW[g.fj[:g.nE]] = fv[:g.nE]
    vN[g.fi[g.nE:]] = fv[g.nE:]; vS[g.fj[g.nE:]] = fv[g.nE:]
    return vW, vE, vS, vN


def trace(g, phi, x, y, stop, maxcells):
    """Pollock's semi-analytic streamline on the face fluxes: inside a cell each
    velocity component is linear between its two faces, a wall face is 0, so
    a particle can never leave through a bank.  Returns True when it enters a
    `stop` texel; False when it stalls (a sink, or a stagnation) or runs out
    of cells."""
    vW, vE, vS, vN = face_velocities(g, phi)
    ix, iy = int(x), int(y)
    fx, fy = x - ix, y - iy
    for _ in range(maxcells):
        if not (0 <= ix < g.w and 0 <= iy < g.h) or not g.wet[iy, ix]:
            return False
        if stop[iy, ix]:
            return True
        i = g.idx[iy, ix]
        # exit time along each axis for v(s) = v0 + (v1 - v0) s, s in [0, 1]
        def exit_time(f, v0, v1):
            v = v0 + (v1 - v0) * f
            if v > 0:
                target = 1.0
            elif v < 0:
                target = 0.0
            else:
                return float('inf'), f
            a = v1 - v0
            vt = v0 + a * target
            if vt * v <= 0:                     # the velocity turns before the face
                return float('inf'), f
            if abs(a) < 1e-14:
                return (target - f) / v, target
            return math.log(vt / v) / a, target
        tx, gx = exit_time(fx, vW[i], vE[i])
        ty, gy = exit_time(fy, vS[i], vN[i])
        if tx == float('inf') and ty == float('inf'):
            return False
        t = min(tx, ty)
        # advance both coordinates by t
        def adv(f, v0, v1, t):
            a = v1 - v0
            if abs(a) < 1e-14:
                return f + v0 * t
            return f + (v0 + a * f) * (math.exp(a * t) - 1.0) / a
        nfx = gx if tx <= ty else min(max(adv(fx, vW[i], vE[i], t), 0.0), 1.0)
        nfy = gy if ty <= tx else min(max(adv(fy, vS[i], vN[i], t), 0.0), 1.0)
        if tx <= ty:
            ix += 1 if gx == 1.0 else -1
            fx = 0.0 if gx == 1.0 else 1.0
            fy = nfy
        else:
            iy += 1 if gy == 1.0 else -1
            fy = 0.0 if gy == 1.0 else 1.0
            fx = nfx
    return False


def f4_lake_outlet():
    print('F4 lake, one outlet: disc R = 32, 3-texel notch on the east rim, uniform source')
    wet, c = disc_mask(32)
    n = wet.shape[0]
    # the outlet: the easternmost wet texels on the three middle rows
    outlet = np.zeros_like(wet)
    for y in (int(c) - 1, int(c), int(c) + 1):
        xs = np.nonzero(wet[y])[0]
        outlet[y, xs.max()] = True
    g = Grid(wet)
    b = np.full(g.n, 1.0 / g.n)
    o = g.idx[outlet]
    b[o] -= 1.0 / len(o)
    phi, it, res = g.solve(b)
    ux, uy = g.velocity(phi)
    U = g.field(ux); V = g.field(uy)
    ox, oy = c + 32, c
    yy, xx = np.mgrid[0:n, 0:n]
    tx = ox - (xx + 0.5); ty = oy - (yy + 0.5)
    sp = np.hypot(U, V)
    dot = (U * tx + V * ty) / np.maximum(sp * np.hypot(tx, ty), 1e-30)
    inner = wet & ~outlet
    away = int((dot[inner] < 0).sum())
    fails = gate('0 texels point away from the outlet', away == 0,
                 '%d of %d point away; mean cosine %.3f (%d iterations, residual %.1e)' % (away, inner.sum(), dot[inner].mean(), it, res))
    seeds = [(int(x), int(y)) for y in np.linspace(4, n - 4, 8) for x in np.linspace(4, n - 4, 8) if wet[int(y), int(x)]]
    reached = sum(1 for (x, y) in seeds if trace(g, phi, float(x) + 0.5, float(y) + 0.5, outlet, 4 * 64 * 4))
    fails += gate('every streamline reaches the outlet', reached == len(seeds),
                  '%d of %d seeds reached it within 4 diameters' % (reached, len(seeds)))
    return fails, g, phi, U, V, outlet


def f7_pin():
    print('F7 dye pin: channel 256 x 16 flowing east, pin at x = 32, L = 32')
    w, h = 256, 16
    wet = np.ones((h, w), bool)
    g = Grid(wet)
    b = np.zeros(g.n)
    src = g.idx[:, 0]; snk = g.idx[:, w - 1]
    b[src] += 1.0 / h; b[snk] -= 1.0 / h
    phi, it, res = g.solve(b)
    held = {int(g.idx[y, 32]): 1.0 for y in range(h)}
    c = g.field(g.dye(phi, held, 32.0))
    c64 = c[:, 64].mean(); c31 = c[:, :32].max(); c128 = c[:, 128].mean()
    fails = gate('weight at pin + 32 = 1/2 +- 10%%', abs(c64 - 0.5) < 0.05, '%.4f' % c64)
    fails += gate('upstream of the pin is 0', c31 == 0.0, 'max upstream %.3e' % c31)
    fails += gate('weight at pin + 96 = 1/8 +- 10%%', abs(c128 - 0.125) < 0.0125, '%.4f' % c128)
    return fails, c


def f6_plume():
    print('F6 plume: river 16 x 96 into a sea 128 x 128 at the middle of its west side, L = 32')
    # one grid holding both: the river is columns 0..95 rows 56..71; the sea columns 96..223
    W, H = 224, 128
    wet = np.zeros((H, W), bool)
    wet[56:72, :96] = True
    wet[:, 96:] = True
    river = np.zeros_like(wet); river[56:72, :96] = True
    sea = wet & ~river
    # the receiving body's own solve: the mouth (sea texels adjacent to river) as the source,
    # the sea's cut edges (north, south, east rims) as the far field (Dirichlet 0)
    gs = Grid(sea)
    mouth = np.zeros_like(wet); mouth[56:72, 96] = True
    b = np.zeros(gs.n)
    mo = gs.idx[mouth]
    b[mo] += 1.0 / len(mo)
    cut = np.zeros_like(wet); cut[0, 96:] = True; cut[H - 1, 96:] = True; cut[:, W - 1] = True
    dir_ = np.zeros(gs.n, bool); dir_[gs.idx[cut & sea]] = True
    phi, it, res = gs.solve(b, dirichlet=dir_)
    ux, uy = gs.velocity(phi)
    U = gs.field(ux); V = gs.field(uy)
    # PREDICTION from the flow alone, before the dye
    yy, xx = np.mgrid[0:H, 0:W]
    near = sea & (np.hypot(xx + 0.5 - 96, yy + 0.5 - 64) < 32)
    pred_dir = math.degrees(math.atan2(V[near].mean(), U[near].mean()))
    pred_len = 3 * 32.0
    print('  predicted: plume direction %.1f deg, 1/8 length %.0f texels along the centreline' % (pred_dir, pred_len))
    held = {int(i): 1.0 for i in mo}
    c = gs.field(gs.dye(phi, held, 32.0))
    row = c[64, 96:]
    # first crossing of 1/8 along the centreline, linear between texels
    k = np.nonzero(row < 0.125)[0]
    if len(k):
        k = k[0]
        x0, x1 = k - 1, k
        f = (row[x0] - 0.125) / (row[x0] - row[x1])
        meas_len = x0 + f + 0.5
    else:
        meas_len = float('nan')
    tot = c[sea].sum()
    cx = (c * (xx + 0.5))[sea].sum() / tot; cy = (c * (yy + 0.5))[sea].sum() / tot
    meas_dir = math.degrees(math.atan2(cy - 64, cx - 96))
    fails = gate('plume length within 10%% of the prediction', abs(meas_len - pred_len) <= 0.1 * pred_len,
                 'measured %.1f texels, predicted %.0f (%d iterations, residual %.1e)' % (meas_len, pred_len, it, res))
    dd = abs((meas_dir - pred_dir + 180) % 360 - 180)
    fails += gate('plume direction within 9 deg of the prediction', dd <= 9.0,
                  'weight centroid at %.1f deg, predicted %.1f' % (meas_dir, pred_dir))
    return fails, c, U, V, wet


if __name__ == '__main__':
    total = 0
    total += f1_continuity()[0]
    total += f2_island()[0]
    total += f3_lake_closed()
    total += f4_lake_outlet()[0]
    total += f7_pin()[0]
    total += f6_plume()[0]
    print('flow_proto: %d failures' % total)
    sys.exit(1 if total else 0)

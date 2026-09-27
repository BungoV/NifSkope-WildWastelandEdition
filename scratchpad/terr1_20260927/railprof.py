"""TERR1 rail profile gate + crop locator (Boston VT.2 sheets, offline decode via vtmosaic.py).

usage: python railprof.py <on VT.2> <off VT.2> <noflat VT.2> <noroads VT.2> <out.json>

  on      default bake (normal stamp ON)
  off     --no-stamp-normals --no-sky-objects bake (the height-only normal)
  noflat  --no-flat-objects bake: colour on vs noflat = the texels painted by FLAT objects (rails, slabs, decals)
  noroads --no-roads bake: colour on vs noroads = every stamped texel (roads + flat objects)

No placement list carries rail positions (flat_objects_report has models, not coordinates), so the track is found
in the sheets: 32x32-texel windows (stride 16) of flat-object texels, principal axes of their texel coordinates;
a TRACK candidate is elongated (major/minor variance >= 6) with a minor spread of 2..12 texels (32..192 units).
Profile: across the major axis through the centroid, t = -12..12 texels, averaged over 5 stations along the axis;
the value is the horizontal normal component along the across direction, ON and OFF.
Gate: ON crosses sign >= 2 times inside the track (|value| > 0.03 counted), OFF crosses fewer times (refuter: the
height-only normal of the same line must not show the rail edges).
Junction: the window with the most road-stamped texels whose axes are near-isotropic (ratio < 1.6).
Writes the world centres and cells of both, for the 4x renders (shot.sh with that one cell and half-width 4096).
"""
import sys, json
import numpy as np
import vtmosaic as VM


def main():
    on, off, nf, nr = (VM.Sheets(p) for p in sys.argv[1:5])
    c_on = on.mosaic(1)[..., :3]
    flat = (np.abs(c_on - nf.mosaic(1)[..., :3]) > 2).any(-1)
    road = (np.abs(c_on - nr.mosaic(1)[..., :3]) > 2).any(-1)
    n_on = VM.msn_world(on.mosaic(2))
    n_off = VM.msn_world(off.mosaic(2))
    N = flat.shape[0]
    W, S = 32, 16

    def axes(mask, j0, i0):
        jj, ii = np.nonzero(mask[j0:j0 + W, i0:i0 + W])
        if len(jj) < 3:
            return None
        P = np.stack([ii, jj], 1).astype(np.float64)
        c = P.mean(0)
        ev, evec = np.linalg.eigh(np.cov((P - c).T))
        return len(jj), c + [i0, j0], ev, evec

    tracks, juncs = [], []
    for j0 in range(0, N - W + 1, S):
        for i0 in range(0, N - W + 1, S):
            nfl = int(flat[j0:j0 + W, i0:i0 + W].sum())
            if nfl >= 60:
                a = axes(flat, j0, i0)
                ev = a[2]
                if ev[0] > 1e-6 and ev[1] / ev[0] >= 6 and 2 <= 2 * np.sqrt(ev[0]) <= 12:
                    tracks.append((nfl * ev[1] / ev[0], a))
            nro = int(road[j0:j0 + W, i0:i0 + W].sum())
            if nro >= 300:
                a = axes(road, j0, i0)
                ev = a[2]
                if ev[0] > 1e-6 and ev[1] / ev[0] < 1.6:
                    juncs.append((nro, a))
    out = dict(track_candidates=len(tracks), junction_candidates=len(juncs))

    def world(c):
        x = VM.WX0 + (c[0] + 0.5) * VM.UPT
        y = VM.WYTOP - (c[1] + 0.5) * VM.UPT
        return dict(x=round(float(x), 1), y=round(float(y), 1), cell=[int(np.floor(x / 4096)), int(np.floor(y / 4096))])

    if tracks:
        tracks.sort(key=lambda t: -t[0])
        _, (n, c, ev, evec) = tracks[0]
        major = evec[:, 1]; minor = evec[:, 0]            # (di, dj) texel steps
        # across direction in WORLD horizontal (east, north): +i = east, +j = south
        across_w = np.array([minor[0], -minor[1]])
        prof_on, prof_off = [], []
        for t in range(-12, 13):
            von, voff = [], []
            for s in (-8, -4, 0, 4, 8):
                p = c + major * s + minor * t
                i, j = int(round(p[0])), int(round(p[1]))
                if 0 <= i < N and 0 <= j < N:
                    von.append(float(n_on[j, i, :2] @ across_w))
                    voff.append(float(n_off[j, i, :2] @ across_w))
            prof_on.append(round(float(np.mean(von)), 4) if von else 0.0)
            prof_off.append(round(float(np.mean(voff)), 4) if voff else 0.0)

        def crossings(v):
            s = [np.sign(x) for x in v if abs(x) > 0.03]
            return int(sum(1 for a, b in zip(s, s[1:]) if a != b))

        out['track'] = dict(world(c), flat_texels=n, ratio=round(float(ev[1] / ev[0]), 1),
                            width_units=round(float(4 * np.sqrt(ev[0]) * VM.UPT), 1),
                            profile_on=prof_on, profile_off=prof_off,
                            crossings_on=crossings(prof_on), crossings_off=crossings(prof_off),
                            gate=bool(crossings(prof_on) >= 2 and crossings(prof_off) < crossings(prof_on)))
        out['track_next'] = [world(t[1][1]) for t in tracks[1:6]]
    if juncs:
        juncs.sort(key=lambda t: -t[0])
        _, (n, c, ev, evec) = juncs[0]
        out['junction'] = dict(world(c), road_texels=n, ratio=round(float(ev[1] / max(ev[0], 1e-6)), 2))
        out['junction_next'] = [world(t[1][1]) for t in juncs[1:6]]
    json.dump(out, open(sys.argv[5], 'w'), indent=1)
    for k in ('track_candidates', 'junction_candidates', 'track', 'junction'):
        print(k, out.get(k))


if __name__ == '__main__':
    main()

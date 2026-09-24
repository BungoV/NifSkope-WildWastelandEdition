# PRE-REGISTERED: the 512-unit cube, OCT=8, TILE=64

Written before the build, lane CARDORTHO 2026-09-10. These are the numbers
`tests/spells/lodgen_octahedral.sh` bake 4 must produce; the harness recomputes
them from the sidecar's own `oct` line at run time, so a frame size that comes
out other than 64x64 changes the table and not the law.

Geometry: a box of half-extents (256,256,256), viewed with screen axes
`r = (sin azim, -cos azim, 0)` and `u = (sin elev cos azim, sin elev sin azim, cos elev)`
(Matrix::fromEuler at Rot = (-90+elev, 0, 90-azim), y = 0), so
`halfR = 256(|rx|+|ry|+|rz|)`, `halfU = 256(|ux|+|uy|+|uz|)`.

Frame chosen by the bake's own ladder: **64x64 texels**, gap 4,4, pad 2,2,
inner rect 60x60, `half` 428.959 x 428.959, `oct` line half-extents **457.556 x 457.556**,
mips = log2(4) = **2**.

Predicted texel spans, `2*halfR*tw / (2*fullHalfW)` and the same on the up axis.
Tolerance **2 texels** (the crop's integer rounding plus one antialiased texel
of the smooth downsample on each side). The perspective control must EXCEED it.

| i | j | halfR units | halfU units | span x texels | span y texels |
|---|---|---|---|---|---|
| 0 | 0 | 256.00 | 256.00 | 35.81 | 35.81 |
| 1 | 0 | 294.60 | 256.00 | 41.21 | 35.81 |
| 2 | 0 | 332.77 | 256.00 | 46.55 | 35.81 |
| 3 | 0 | 358.40 | 256.00 | 50.13 | 35.81 |
| 4 | 0 | 358.40 | 256.00 | 50.13 | 35.81 |
| 5 | 0 | 332.77 | 256.00 | 46.55 | 35.81 |
| 6 | 0 | 294.60 | 256.00 | 41.21 | 35.81 |
| 7 | 0 | 256.00 | 256.00 | 35.81 | 35.81 |
| 0 | 1 | 294.60 | 256.00 | 41.21 | 35.81 |
| 1 | 1 | 256.00 | 332.77 | 35.81 | 46.55 |
| 2 | 1 | 310.45 | 365.82 | 43.42 | 51.17 |
| 3 | 1 | 355.01 | 396.07 | 49.66 | 55.40 |
| 4 | 1 | 355.01 | 396.07 | 49.66 | 55.40 |
| 5 | 1 | 310.45 | 365.82 | 43.42 | 51.17 |
| 6 | 1 | 256.00 | 332.77 | 35.81 | 46.55 |
| 7 | 1 | 294.60 | 256.00 | 41.21 | 35.81 |
| 0 | 2 | 332.77 | 256.00 | 46.55 | 35.81 |
| 1 | 2 | 310.45 | 365.82 | 43.42 | 51.17 |
| 2 | 2 | 256.00 | 358.40 | 35.81 | 50.13 |
| 3 | 2 | 343.46 | 424.71 | 48.04 | 59.41 |
| 4 | 2 | 343.46 | 424.71 | 48.04 | 59.41 |
| 5 | 2 | 256.00 | 358.40 | 35.81 | 50.13 |
| 6 | 2 | 310.45 | 365.82 | 43.42 | 51.17 |
| 7 | 2 | 332.77 | 256.00 | 46.55 | 35.81 |
| 0 | 3 | 358.40 | 256.00 | 50.13 | 35.81 |
| 1 | 3 | 355.01 | 396.07 | 49.66 | 55.40 |
| 2 | 3 | 343.46 | 424.71 | 48.04 | 59.41 |
| 3 | 3 | 256.00 | 294.60 | 35.81 | 41.21 |
| 4 | 3 | 256.00 | 294.60 | 35.81 | 41.21 |
| 5 | 3 | 343.46 | 424.71 | 48.04 | 59.41 |
| 6 | 3 | 355.01 | 396.07 | 49.66 | 55.40 |
| 7 | 3 | 358.40 | 256.00 | 50.13 | 35.81 |
| 0 | 4 | 358.40 | 256.00 | 50.13 | 35.81 |
| 1 | 4 | 355.01 | 396.07 | 49.66 | 55.40 |
| 2 | 4 | 343.46 | 424.71 | 48.04 | 59.41 |
| 3 | 4 | 256.00 | 294.60 | 35.81 | 41.21 |
| 4 | 4 | 256.00 | 294.60 | 35.81 | 41.21 |
| 5 | 4 | 343.46 | 424.71 | 48.04 | 59.41 |
| 6 | 4 | 355.01 | 396.07 | 49.66 | 55.40 |
| 7 | 4 | 358.40 | 256.00 | 50.13 | 35.81 |
| 0 | 5 | 332.77 | 256.00 | 46.55 | 35.81 |
| 1 | 5 | 310.45 | 365.82 | 43.42 | 51.17 |
| 2 | 5 | 256.00 | 358.40 | 35.81 | 50.13 |
| 3 | 5 | 343.46 | 424.71 | 48.04 | 59.41 |
| 4 | 5 | 343.46 | 424.71 | 48.04 | 59.41 |
| 5 | 5 | 256.00 | 358.40 | 35.81 | 50.13 |
| 6 | 5 | 310.45 | 365.82 | 43.42 | 51.17 |
| 7 | 5 | 332.77 | 256.00 | 46.55 | 35.81 |
| 0 | 6 | 294.60 | 256.00 | 41.21 | 35.81 |
| 1 | 6 | 256.00 | 332.77 | 35.81 | 46.55 |
| 2 | 6 | 310.45 | 365.82 | 43.42 | 51.17 |
| 3 | 6 | 355.01 | 396.07 | 49.66 | 55.40 |
| 4 | 6 | 355.01 | 396.07 | 49.66 | 55.40 |
| 5 | 6 | 310.45 | 365.82 | 43.42 | 51.17 |
| 6 | 6 | 256.00 | 332.77 | 35.81 | 46.55 |
| 7 | 6 | 294.60 | 256.00 | 41.21 | 35.81 |
| 0 | 7 | 256.00 | 256.00 | 35.81 | 35.81 |
| 1 | 7 | 294.60 | 256.00 | 41.21 | 35.81 |
| 2 | 7 | 332.77 | 256.00 | 46.55 | 35.81 |
| 3 | 7 | 358.40 | 256.00 | 50.13 | 35.81 |
| 4 | 7 | 358.40 | 256.00 | 50.13 | 35.81 |
| 5 | 7 | 332.77 | 256.00 | 46.55 | 35.81 |
| 6 | 7 | 294.60 | 256.00 | 41.21 | 35.81 |
| 7 | 7 | 256.00 | 256.00 | 35.81 | 35.81 |

Distinct predicted x spans over the 64 frames: [35.81, 41.21, 43.42, 46.55, 48.04, 49.66, 50.13] -- a ladder, not one number, so
a single wrong scale factor cannot pass it.

Two further pre-registered invariants on the same frames:

* **central symmetry**: an orthographic projection of a centrally symmetric
  solid is centrally symmetric. Disagreement of each frame's coverage mask with
  its own 180-degree rotation about the silhouette box, as a fraction of covered
  texels: **<= 0.05** orthographic, **> 0.05** for the perspective control.
* **near edge against far edge**: the widest row in the top fifth of a silhouette
  against the widest row in the bottom fifth, as a fraction of their mean:
  **<= 0.05** orthographic, **> 0.05** for the control. This is the
  no-foreshortening statement in its most direct form.

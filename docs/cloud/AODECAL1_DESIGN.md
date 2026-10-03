# AODECAL1 -- per-model ambient occlusion volumes for cars and low props (design + twin)

Lane AODECAL1 (prep, cloud, 2026-10-03). This is the design and a Python twin. No C++ was written, and
nothing here has been run against game data. The twin is `tests/spells/aodecal1_check.py` (python3 + numpy,
about 60 s, under 300 MB peak).

## 1. What it is, in plain words

A wrecked car on the street darkens the ground around it, the wall beside it and the crate next to it. The
probe grid (PRTP_PLAN 2g-2aj) is too coarse to show that: probes stand 280 units apart, and the grid blends
them over 2 x that distance. The result is a faint smear instead of a dark patch under the car.

So each car or low prop model gets a small 3D grid of its own, stored in `ao/`. The grid surrounds the model
(its bounding box plus a margin) and records, at every grid point, which directions the model blocks.

At draw time, a shaded point near a placed copy is moved into that copy's model space, and its normal is
turned the same way. It reads the grid there and takes the occlusion over its own hemisphere. A copy can be
tipped onto its side, upside down, leaning, scaled or stacked on another car; the lookup is always in the
copy's own frame, so it stays right in every pose.

The occlusion multiplies only the probe term (bounce + sky). Sun and lamps are never touched; they have their
own shadows. Where the probes already saw the copy (a probe beside a tipped car has part of its sky blocked
by it), that share is taken back out first, so the copy never darkens a point twice.

## 2. Prior art (public sources only)

The egress proxy of this cloud session refused every page fetch (reedbeta.com, selfshadow.com,
realtimerendering.com, users.aalto.fi, irisa.fr, research.nvidia.com, dev.epicgames.com: "connect_rejected").
Everything below comes from search-engine abstracts and from well-known published results. No slide was
read. Anything marked *(abstract only)* should be confirmed against the source by whoever has access.

| Source | What it does | What we take |
|---|---|---|
| Kontkanen, Laine, "Ambient Occlusion Fields", I3D 2005. https://users.aalto.fi/~laines9/publications/kontkanen2005i3d_paper.pdf | For each occluder, a field in the space around it stores the occluder as a spherical cap: the cone's angle and the average occlusion direction. These are stored as radial functions in cube maps, so the field reaches any distance. *(abstract only)* | The per-object field. Their radial extrapolation, simplified, is our far field (section 3.4): an equivalent sphere per direction. |
| Malmer, Malmer, Assarsson, Holzschuch, "Fast Precomputed Ambient Occlusion for Proximity Shadows", JGT 12(2) 2007. https://inria.hal.science/inria-00379385/file/contactShadows05.pdf | A 3D grid around the occluder stores the occlusion amount, or the amount plus the average occluded direction. The direction costs memory but is "more accurate, especially when combining several occluders". *(abstract only)* | A grid around the bounding box; storing a direction (our L1 and L2 terms) rather than a scalar. |
| Reed, "Ambient Occlusion Fields and Decals in Infamous 2", GDC 2012. https://www.reedbeta.com/talks/ao-fields/ and https://gdcvault.com/play/1015320/Ambient-Occlusion-Fields-and-Decals | A volume texture around a movable object. Per voxel, a 32x32 cube map is rendered; the solid-angle-weighted centroid of the drawn pixels gives the cone axis and the drawn share gives its width. The field is applied "much like a light in deferred shading". A few KB per object. *(abstract only)* | The deferred application (section 6). Reed's cone is the same four numbers as our L1 term (amount + mean direction). Our measurements show L1 is not enough near a car (section 3.2), so we store L2. |
| Hill, "Rendering with Conviction: The Graphics of Splinter Cell", GDC 2010. https://www.selfshadow.com/talks/rwc_gdc2010_v1.pdf ; summary https://www.realtimerendering.com/blog/update-on-splinter-cell-conviction-rendering/ | Precomputed AO volumes (about 16x16x16) for dynamic rigid objects: tables, chairs, vehicles. Inspired by AO Fields; the INRIA report above was developed in parallel. *(abstract only)* | Confirms the resolution class (16^3 to 32^3) is enough for vehicles. |
| Epic, "Distance Field Ambient Occlusion". https://dev.epicgames.com/documentation/unreal-engine/distance-field-ambient-occlusion-in-unreal-engine | A signed distance field volume per rigid mesh; occlusion is traced through world-space occluders; it shadows the movable Sky Light only. Only slight non-uniform scale is supported. Large meshes do poorly because a small volume texture is stretched over them. *(abstract only)* | The rule that it shadows the sky/ambient term only. The same limits apply here: uniform scale only, and the method is for small and medium models. |
| McGuire, "Ambient Occlusion Volumes", HPG 2010. https://research.nvidia.com/publication/2010-06_ambient-occlusion-volumes | An analytic occlusion volume per polygon, at run time. *(abstract only)* | Not used (no bake, per-triangle cost). Listed as the alternative. |
| Ramamoorthi, Hanrahan, "An Efficient Representation for Irradiance Environment Maps", SIGGRAPH 2001. https://graphics.stanford.edu/papers/envmap/ | The clamped-cosine kernel in SH: A0 = pi, A1 = 2 pi / 3, A2 = pi / 4. | The constants in section 3.3 (a textbook result; the page itself was not fetched). |

**Not found.** The brief asked about Fable and Snowdrop/Division vehicle AO decals. Searches turned up
nothing public on AO volumes in Fable II; the only Lionhead lighting talk found is Fable Legends' light
propagation volumes, through secondary reports. Nothing public turned up on vehicle AO decals in Snowdrop or
The Division, beyond the Division GI deck that PRTP is already built on. Nothing here is attributed to
either studio. The "Hill" in the brief is most likely Stephen Hill's Splinter Cell: Conviction talk above.

## 3. The encoding

### 3.1 The quantity

For a placed copy k and a receiver point x with normal n (world):

    AO_k(x, n) = 1 - (1/pi) * integral over the sphere of O_k(x, w) * max(0, n . w) dw

O_k(x, w) = 1 when a ray from x in direction w hits the copy, at any distance. Only the copy counts here: no
ground, no other model. This is cosine-weighted occlusion over the receiver's own hemisphere.

### 3.2 The volume

- Box: the model's bounding box (its render triangles, the same shapes the probe soup takes) plus a margin
  m on every side, below included. A flipped or tipped copy puts the ground on any side of the model.
  m = 2.0 x the median extent of the bounding box (the synthetic car: 360 units).
- Grid: a voxel budget of 32 x 32 x 16 = 16384, shared out in proportion to the box's extents, at least 8 per
  axis. The car gets 31 x 23 x 23 = 17112 voxels of 28 to 38 units. Values sit at voxel centers.
- Per voxel: O(w) projected to real SH up to band 2 (9 coefficients), from 1024 Fibonacci-sphere rays.
  The coefficients are stored divided by 2 sqrt(pi), so coefficient 0 is the blocked share of the sphere.
- Voxels whose center lies inside the solid have no outside. They are filled from their valid neighbors,
  one ring at a time, so that trilinear lookups next to the surface do not read garbage.
- Fade: inside the outer 25% of the margin, the volume's occlusion cross-fades into the far field (3.4).

Measured, L2 against L1 on the same receivers and the same placements (twin, check A):

| | far from the model, mean / p95 / max | near (within one voxel), p95 |
|---|---|---|
| L2 (9 bytes a voxel) | 0.0023-0.0042 / 0.006-0.010 / 0.016-0.042 | 0.046-0.125 |
| L1 (4 bytes a voxel; the same information as Reed's cone) | 0.015-0.020 / 0.044-0.074 / 0.076-0.114 | 0.042-0.199 |

L1 fails the p95 bar of 0.05 on four of five placements, so L2 is the format. A cone set of blocked
directions (the brief's other option) would need about 14 or more directions to beat 9 SH coefficients. It
was not tried.

### 3.3 The lookup

For copy k with rotation R (3x3), translation t and uniform scale s, as the cell view places it:

    m   = R^T (x - t) / s           receiver in model space
    n_m = R^T n                     normal in model space (uniform scale leaves directions alone)
    k_j = trilinear(volume, m)      9 coefficients, clamped to the grid's edge voxels
    O_j = k_j * 2 sqrt(pi)
    occ_vol = sum_j (A_l(j) / pi) * O_j * Y_j(n_m)      A_0 = pi, A_1 = 2pi/3, A_2 = pi/4
    f   = clamp(distance from m to the box's nearest face / fade width, 0, 1)
    occ = f * occ_vol + (1 - f) * occ_far(m, n_m)
    AO_k = clamp(1 - occ, 0, 1)

The scale is uniform only, as in the game's placed references. Non-uniform scale would need the inverse
transpose for n and would bend the stored directions. That is not supported, the same limit as UE's DFAO.

### 3.4 The far field (beyond the box)

A hard edge at the box drops real occlusion: up to 0.15 (normal facing the car) at the box surface of the
first trial, and 0.23 at the fade band's inner edge. Two options were weighed. A wider margin costs every
voxel resolution. Instead, the model is described past the box as an **equivalent sphere per direction**:

- Center c = the bounding box's center. For each voxel in the fade band, u = the direction from c, r = the
  distance. A sphere that blocks the voxel's measured share k0 of the sphere has sin^2(a) = 1 - (1 - 2 k0)^2,
  so rho^2 = r^2 sin^2(a). rho^2(u) is fitted as 9 L2 SH coefficients (least squares) and stored in the header.
- occ_far(m, n) = min(rho^2(u) / r^2, 1) * max(0, n . (-u)), the cosine-weighted occlusion of a sphere wholly
  above the horizon. It is 0 past r_cut.
- r_cut is where the widest equivalent sphere blocks 1%: r_cut = rho_max / sqrt(0.01). For the car that is
  1789 units, 2.1 x the box's half diagonal.

The far field also sets the copy's footprint, the region it can darken: the model-space box spanning both the
volume box and the cut sphere.

### 3.5 Several copies

Copies multiply: A(x, n) = product over copies of AO_k(x, n). This treats them as independent, which double
counts where two copies block the same directions (a car stacked on a car). The twin does not test
combining (open question 1).

### 3.6 Never the copy itself

A copy's own surfaces must not read its own volume, because their self-occlusion is already in the model's
texture and vertex AO. The deferred pass skips pixels whose copy id is k (section 6).

## 4. The no-double-darkening rule

What the probes store today (src/probegi.cpp, PRTP_PLAN 2g / 2ah). Each probe keeps, per world octant, the
share of its bake rays that left the scene (`skyVis[8]`). The relight's sky on axis a is
`pi x ambient(a) x mean of the four octants on a's side of skyVis x skyTint`. The grid blends probes by
(1 - d^2 / r^2)^2 over the probes a voxel can see, and the shader blends the six axes by n^2. Cars and props
are STAT (2y: "the bake sees the fixed world only", and STAT is in the soup). So a probe near a car has the
car in its skyVis already. Multiplying the probe term by AO on top of that darkens twice.

The rule. Per probe i and world octant o, take the copy's blocked share of that octant from the copy's own
volume. The SH is evaluated over the octant's directions turned into model space, which amounts to rotating
the SH by the copy's rotation; past the box, the far-field sphere's cap is counted instead:

    occ_i,o = product over copies (1 - V_k,i,o) -> the probe's "copy-free" sky share:
    s_free_i,o = min(1, s_i,o / max(1 - occ_i,o, 0.1))      (0 where s_i,o = 0: ground already closed it)

`s_free` goes through the same axis means, the same grid blend and the same n^2 cube as `skyVis`, giving
E_sky_free(x, n). The probe term then becomes:

    probe(x, n) = [ E_total(x, n) + dE(x, n) ] * A(x, n)        dE = E_sky_free - E_sky  (>= 0)

which is the same as `E_bounce * A + E_sky * A / G` with `G = E_sky / E_sky_free`: AO divided by what the
probes captured. With the decal row off, the shader uses `E_total` exactly as today (byte-identical).

Why only the sky part is divided. The probes see a copy two ways. It blocks sky, which `skyVis` records as
occlusion. It also sends its own surfels' light through the links: that is light, not missing light, so there
is nothing in it to divide out. The bounce part therefore takes A once. The copy's own bounce reaching a
receiver beside it is darkened by A too; open question 2.

Why the floor of 0.1. An octant the copy fills completely (s = 0) cannot say what was behind the copy.
Without the floor, s / (1 - occ) would blow up wherever the volume is slightly wrong.

The exact alternative. The bake can classify rays a second time: a ray whose first hit is a decal copy is
continued past it, and if it then escapes, it counts toward s_free. That needs a `.tbk` tail field per probe
(u8 x 8, the octant share the decal copies took) and a rebake. In the twin it is better: pooled bias +0.0020
against -0.0095 for the volume route (check D below). Recommended order: ship the volume route, because it
needs no rebake and no format change. Switch to the exact route if the local lane measures bias worse than
-0.02 on real cells.

## 5. Files: `ao/<hash>.ao` and `ao/index.aoi`

All little endian. The folder sits beside the probe bake: `<NifSkope>/prtp_bake/ao/` (the local lane's
call).

### 5.1 `.ao`, one per model, named by the 16-hex-digit path hash (5.2)

| offset | type | field |
|---|---|---|
| 0 | u32 | magic 0x4F415757 ("WWAO") |
| 4 | u16 | version 1 |
| 6 | u16 | encoding: 1 = SH L1 (4 coefficients), 2 = SH L2 (9) |
| 8 | u16 x 3 | dims x, y, z |
| 14 | u16 | coefficients per voxel (4 or 9) |
| 16 | f32 x 3 | box lo (model space) |
| 28 | f32 x 3 | box hi |
| 40 | f32 | fade width (units) |
| 44 | f32 | cmax, the scale of coefficients 1..n |
| 48 | u32 | rays per voxel at bake |
| 52 | f32 x 3 | far-field center c |
| 64 | f32 x 9 | far-field rho^2(u), L2 SH |
| 100 | f32 | r_cut |
| 104 | u64 | model fingerprint (5.2) |
| 112 | u32 | CRC-32 of the payload |
| 116 | 12 bytes | reserved, 0 |
| 128 | payload | dims z x y x x voxels, x fastest; per voxel: u8 k0 (x 255), then s8 k1..kn (x 127 / cmax) |

Size = 128 + voxels x coefficients. The car: 128 + 17112 x 9 = 154136 bytes (150.5 KB). An L1 file would be
68 KB but fails the bars.

### 5.2 `index.aoi`, one per folder

| offset | type | field |
|---|---|---|
| 0 | u32 | magic 0x49415757 ("WWAI") |
| 4 | u16 | version 1 |
| 6 | u16 | flags, 0 |
| 8 | u32 | record count n |
| 12 | u32 | string table offset = 32 + 64 n |
| 16 | 16 bytes | reserved |
| 32 | 64 x n | records, sorted by path hash |
| | | string table: paths, NUL-terminated |

Record (64 bytes): u64 path hash (FNV-1a 64 of the normalized path), u64 model fingerprint, u32 path offset,
u16 path length, u16 flags, f32 x 3 footprint lo, f32 x 3 footprint hi (model space: volume box plus the
far-field cut), u32 `.ao` size, u32 `.ao` CRC-32, 8 reserved.

- Normalized path: lower case, backslashes, no leading separator, relative to `Meshes\` exactly as the base
  record's MODL gives it.
- Fingerprint: the first 8 bytes (little endian) of SHA-256 over the model file's bytes, as resolved from the
  loose folder or the archive. It changes whenever the model does; a mod that replaces the mesh gets a stale
  record.
- The reader refuses a copy when the index CRC or size disagrees with the `.ao`, when the payload CRC fails,
  or when the fingerprint in the index, in the `.ao` header and of the model actually loaded are not all
  equal. A refused copy draws no decal, and the cell notes count it ("ao: n copies, m refused stale").

## 6. Runtime in the cell view (described, not built)

1. The existing AO pass (`wwCellAoPass`, PRTP_PLAN 2u) already writes depth and view normals. Add a copy id
   (16 bits, the placed reference's index in the cell document; 0 = not a decal copy).
2. A **probe-occlusion target** (R16F, half or full size, cleared to 1). For each placed copy whose base
   model has a valid `.ao`, and whose world footprint (the 8 corners of the model-space footprint box,
   transformed) meets the view, draw the footprint box's back faces. The fragment shader:
   - reads depth and normal and rebuilds the world point x and normal n;
   - exits when the pixel's copy id is this copy's (3.6);
   - computes AO_k(x, n) (3.3, 3.4);
   - writes it with multiplicative blending (dst x src), giving the product of 3.5.
   Models are packed into one 3D atlas, 9 coefficients across three RGBA8 slices, with a per-copy uniform
   block holding R^T, t, s and the atlas rectangle.
3. The cell programs (fo4_cell, pbrm_cell) multiply only the probe term by the target's sample, and add dE
   (section 4) first. Sun, lamps, emissive and the specular cube term stay as they are. The game's SSAO
   (2u) still multiplies everything afterwards, as in the game.
4. The relight (probegi.cpp) gains one six-slab grid: rgb = dE per axis, the same voxels and weights as
   `gridSky`. When no decal copies are loaded it is not built, and the cell is byte-identical to today.

The bake, as a CLI (`nifskope -no-gui aobake --nif <path> --out <dir>`): the model's render triangles (the
probe soup's per-shape rules: no effect, decal or glass shapes), the shared BVH (src/probebvh.h), 1024 rays
per voxel. The inside test must be a back-face hit share above 0.5, not box containment, because a real mesh
is not a set of boxes. Then fill, far-field fit, quantize and write. It is deterministic: fixed directions, no
threads in the reduction order.

## 7. The twin and its gates

`python3 tests/spells/aodecal1_check.py` runs the green checks, then every red, and exits 0 only when
the green passes and every red fails. `AODECAL1_RED=<name>` runs one red as the product and exits 1.

The scene, all built in code: the "car" is a body box (450 x 180 x 110, from z 40 to 150) on four wheel boxes
(60 x 30 x 60), in its own space with the wheels on z = 0. Five copies:

- upright, yaw 30;
- tipped 90 (roll 90, on its side);
- flipped 180 (on its roof);
- leaning (roll -25, pitch 8, yaw 15) at scale 1.2;
- stacked (upright, yaw 40) on the roof of a second car.

Each copy rests on the ground at its lowest point. Receivers: the ground around the copy, a wall 70 units off
its side facing it, a crate beside it (top and facing side), and for the stacked copy the lower car's roof.
Every receiver lies within the footprint and outside the solid.

The volume is quantized exactly as the file stores it before any check reads it.

| check | what it proves | bars | measured (green) | red, measured |
|---|---|---|---|---|
| A lookup | The volume read in the copy's model space with the receiver's own normal matches brute-force ray occlusion (4096 cosine-weighted directions, its own random rotation) by the copy alone, in every pose | far from the model: mean <= 0.02, p95 <= 0.05, max <= 0.12. Within one voxel (contact band): mean <= 0.05, p95 <= 0.15 | far: mean 0.0023-0.0042, p95 0.006-0.010, max 0.016-0.042. Near: p95 0.046-0.125, max up to 0.270 (leaning, 3 units from a wheel) | **worlddown** (keep only the copy's yaw, ignore tilt and scale, force the normal to world up: the projected decal): tipped p95 0.22, flipped near mean 0.15, leaning p95 0.26. All 3 rotated placements FAIL. The upright copy fails too, on walls (normal forced up) |
| B far field | Past the volume box, the equivalent sphere matches brute force from the fade band out to r_cut, with normals facing the model and pointing up; little is dropped at the cut | mean <= 0.010, max <= 0.06; dropped at the cut <= 0.02 | true occlusion there up to 0.173; mean 0.0028 / 0.0008, max 0.027 / 0.026; dropped at the cut max 0.0156 | **nofar** (the volume just ends): mean 0.0255, max 0.128. FAIL |
| D no double darkening | The probe sky (octant shares traced through ground + copy over FO4CS's 280-unit lattice, probes 120 over each surface; the relight's grid weights with visibility; the n^2 cube) x AO / G equals the copy-free probe sky x brute-force AO | per placement p95 <= 0.05; pooled over the 451 receivers whose probes saw the copy (G < 0.95): signed mean >= -0.02, most negative >= -0.10 | p95 0.0047-0.0355; pooled signed mean -0.0095, most negative -0.051 (exact divisor: +0.0020 / -0.043) | **nodivide** (AO times the probe sky as is): pooled -0.0667, most negative -0.123; tipped p95 0.092, leaning 0.110. FAIL |
| E file | The `.ao` and index round-trip through a separate reader: sizes, CRCs, header, lookups equal to 1e-4. Inside the check, a flipped payload byte and a stale model fingerprint must both be refused | exact | 154136 bytes, lookup difference 2.2e-9, both refused | **stale** (the reader skips the fingerprint test): the stale model is accepted. FAIL |

Set after the first measurement, said plainly. The contact-band bar was first 0.03 / 0.10, and the leaning
copy failed it at 0.040 / 0.125. The misses come from L2 smoothing a step within a few units of a wheel, not
from interpolating filled interior voxels (every one of the eight corners was outside the solid). That
contact scale is what the game's own SSAO handles (radius 108, PRTP_PLAN 2u). The bar was loosened to
0.05 / 0.15. Margins 1.0 and 1.25 were tried for finer voxels and did not help the contact band. The far
field (3.4) was added after the first run showed the hard box edge dropping 0.15.

Where the double darkening shows. It needs probes that see the copy above their horizon. Upright and flipped
cars are lower than a probe's 120-unit eye height, so only 5 and 0 receivers there have G < 0.95. The tipped
car (180 tall) and the leaning, scaled one carry the pooled verdict: 208 + 238 receivers, G down to 0.79.

The output of the run committed with this document:

```
== green
A PASS lookup in the copy's model space vs brute force (bars far: mean 0.02 p95 0.05 max 0.12; near one voxel: mean 0.05 p95 0.15)
  upright    3066 pts (ground 2330 wall 481 neighbor 255), brute-force AO 0.035..0.999
             far  2995: |err| mean 0.0029 p95 0.0082 max 0.042; near  71: mean 0.0188 p95 0.0455 max 0.065 ok
             (L1 only, same points: far mean 0.0164 p95 0.0607 max 0.100; near p95 0.094)
  tipped90   2729 pts (ground 2006 wall 468 neighbor 255), brute-force AO 0.273..0.996
             far  2640: |err| mean 0.0023 p95 0.0077 max 0.016; near  89: mean 0.0152 p95 0.0648 max 0.120 ok
             (L1 only, same points: far mean 0.0204 p95 0.0742 max 0.113; near p95 0.064)
  flipped180 2889 pts (ground 2205 wall 429 neighbor 255), brute-force AO 0.503..1.000
             far  2799: |err| mean 0.0023 p95 0.0060 max 0.020; near  90: mean 0.0100 p95 0.0462 max 0.124 ok
             (L1 only, same points: far mean 0.0156 p95 0.0440 max 0.076; near p95 0.042)
  leaning    3373 pts (ground 2585 wall 533 neighbor 255), brute-force AO 0.110..0.993
             far  3339: |err| mean 0.0039 p95 0.0103 max 0.026; near  34: mean 0.0399 p95 0.1246 max 0.270 ok
             (L1 only, same points: far mean 0.0201 p95 0.0713 max 0.114; near p95 0.199)
  stacked    3365 pts (ground 2390 wall 468 neighbor 507), brute-force AO 0.036..0.993
             far  3301: |err| mean 0.0042 p95 0.0098 max 0.034; near  64: mean 0.0186 p95 0.0606 max 0.066 ok
             (L1 only, same points: far mean 0.0152 p95 0.0693 max 0.101; near p95 0.103)
B PASS past the volume box (far field): 1500 points from the fade band to the cut (rcut 1789 = 2.1 x the box half-diagonal); bars mean 0.010 max 0.06, dropped at the cut 0.020
  normal toward true occlusion up to 0.173; |err| mean 0.0028 p95 0.0075 max 0.027
  normal up     true occlusion up to 0.168; |err| mean 0.0008 p95 0.0039 max 0.026
  dropped just past the cut (facing the model): max 0.0156 mean 0.0077
D PASS no double darkening: probe sky x AO / G vs copy-free probe sky x brute-force AO
  pooled: 451 receivers whose probes saw the copy: signed error mean -0.0095 (bar >= -0.02), most negative -0.051 (bar >= -0.10); every placement p95 |err| <= 0.05: yes
  information: with the exact divisor (copy-free sky from a second bake trace) the same receivers: mean +0.0020, most negative -0.043
  upright     81 probes ( 55 see the copy), 360 receivers (  5 whose probes saw it, G min 0.95): |final - target| mean 0.0027 p95 0.0071 max 0.052; signed mean where probes saw it +0.0009
             copy-free probe sky rebuilt from the volume vs traced (per axis): mean 0.0000 max 0.001
  tipped90    72 probes ( 66 see the copy), 360 receivers (208 whose probes saw it, G min 0.79): |final - target| mean 0.0048 p95 0.0113 max 0.062; signed mean where probes saw it -0.0056
             copy-free probe sky rebuilt from the volume vs traced (per axis): mean 0.0007 max 0.042
  flipped180  81 probes ( 39 see the copy), 360 receivers (  0 whose probes saw it, G min 0.97): |final - target| mean 0.0020 p95 0.0047 max 0.021; signed mean where probes saw it +0.0000
             copy-free probe sky rebuilt from the volume vs traced (per axis): mean 0.0000 max 0.001
  leaning     81 probes ( 80 see the copy), 360 receivers (238 whose probes saw it, G min 0.82): |final - target| mean 0.0120 p95 0.0355 max 0.043; signed mean where probes saw it -0.0132
             copy-free probe sky rebuilt from the volume vs traced (per axis): mean 0.0029 max 0.172
E PASS file: .ao 154136 bytes (128 + 17112 voxels x 9) = 150.5 KB, index 130 bytes; read back ok, max coefficient / lookup diff 2.2e-09; flipped payload byte refused: yes; stale model fingerprint refused: yes
green PASS (60 s)
== red worlddown
A FAIL ... [red worlddown: 3 of 3 rotated placements fail]
  upright    far  2995: |err| mean 0.0325 p95 0.2064 max 0.338; near  71: mean 0.0188 p95 0.0455 max 0.065 BAD
  tipped90   far  2640: |err| mean 0.0542 p95 0.2213 max 0.443; near  89: mean 0.1434 p95 0.2501 max 0.303 BAD
  flipped180 far  2799: |err| mean 0.0389 p95 0.1171 max 0.198; near  90: mean 0.1513 p95 0.2408 max 0.270 BAD
  leaning    far  3339: |err| mean 0.0619 p95 0.2555 max 0.386; near  34: mean 0.3195 p95 0.4616 max 0.620 BAD
  stacked    far  3301: |err| mean 0.0173 p95 0.0916 max 0.300; near  64: mean 0.0186 p95 0.0606 max 0.066 BAD
red worlddown: FAIL as it must
== red nofar
B FAIL past the volume box (far field OFF: red nofar) ...
  normal toward true occlusion up to 0.173; |err| mean 0.0255 p95 0.0669 max 0.128
  normal up     true occlusion up to 0.168; |err| mean 0.0073 p95 0.0360 max 0.122
red nofar: FAIL as it must
== red nodivide
D FAIL no double darkening ... [red nodivide]
  pooled: 451 receivers whose probes saw the copy: signed error mean -0.0667 (bar >= -0.02), most negative -0.123 (bar >= -0.10); every placement p95 |err| <= 0.05: NO
  tipped90   ... |final - target| mean 0.0499 p95 0.0921 max 0.122; signed mean where probes saw it -0.0640
  leaning    ... |final - target| mean 0.0592 p95 0.1099 max 0.123; signed mean where probes saw it -0.0702
red nodivide: FAIL as it must
== red stale
E FAIL file: ... stale model fingerprint refused: NO [red stale]
red stale: FAIL as it must
aodecal1 PASS: green PASS, reds all fail
```

(The red blocks are shortened with "..."; the twin prints them in full.)

What the twin does not model:

- The relight's voxelization: the grid blend is evaluated at the receiver itself, not at 48-unit voxels.
- Glass tint, rooms and doors.
- Bounce: D tests the sky part only.
- Several copies combined.
- Real triangle meshes: the inside test is box containment.
- Quantization of the far-field floats.

The brute-force tracer in A and B uses the same slab test as the bake, with its own direction set. It is
independent in its sampling and its formula (direct ray counting, against an SH lookup), not in its
ray-box code.

## 8. Open questions

1. **Combining copies.** The product of AO factors double counts where two copies block the same
   directions, such as a car stacked on a car. Malmer et al. say the stored direction helps here (their
   paper was not read). Options: the product (the default), the minimum, or a cone union using each copy's
   L1 axis.
2. **Bounce under a copy.** A takes away part of the bounce that the copy's own lit surfels sent through the
   links. The underside of a real car is dark, so this is probably right. A gate on a real cell with one
   car should decide it.
3. **The volume divisor or the exact one.** Section 4 recommends the volume route first. The local lane
   should measure its bias on real cells against a second-trace bake.
4. **The copy-free sky floor (0.1)** affects only octants the copy fills completely. Measured worst per-axis
   error 0.172 (leaning). Should those octants borrow from neighboring probes instead?
5. **Which models get a volume.** By base type (MSTT/STAT) and by size: a bounding-box height under some
   bound (for example 300 units) and a longest side under some bound (for example 800 units)? Plus an
   allow list of car and prop folders? This is bungo's call. Large models do badly with a fixed voxel
   budget (UE DFAO says the same).
6. **Size.** 150 KB per model at L2. A few hundred models would be tens of MB. Options: compress the
   payload, or fall back to L1 for models under a size threshold if the bars allow it there.
7. **Contact band.** The game's SSAO already darkens contact creases. Does the decal plus SSAO over-darken
   right at a wheel? Answering that needs an in-game still (PRTP4).
8. **FAR_CUT (1%)** sets a footprint radius of about 1800 units for the car. Is a 2% cut (about 1270) good
   enough for the pass's cost? The local lane should time it.

## 9. What the local lane must still do (real cells and models)

1. Write `aobake` in C++ on the shared BVH (6), with the back-face inside test. Gate it with this twin's
   reader on a real car model: the `.ao` read back, check A's brute force rebuilt on the model's own
   triangles (the probe soup's tracer), the same bars, and the reds worlddown and stale.
2. Choose the model set (open question 5). Census how many models qualify and their total size.
3. The deferred pass, the copy id and the dE grid (6). Prove it off-identical: decal row off, then the
   cell_gi / cell_sky / cell_pass gates are byte-identical. Prove it sun- and lamp-identical: probe term
   zeroed with the row on, picture identical.
4. Gate D on a real exterior cell with wrecked cars (Concord's street has them). Use the bake's own probes,
   with the copy-free sky from a second bake trace as the truth, at the same bars. The red nodivide must
   fail there.
5. Have bungo look at one tipped and one flipped car before any default is decided.

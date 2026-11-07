# res/air

`stbn_scalar_128x128x64.r8`: a scalar spatiotemporal blue-noise mask, 128 x 128 x 64, raw R8 (64 slices of 128 rows
of 128 bytes, x fastest; value v -> (v + 0.5) / 256). It is OUR OWN output. No NVIDIA file is used: their STBN
textures carry a non-commercial licence, ruling 11-07.

Method (lane FOGGRID22A, 2026-11-07): the published 3D void-and-cluster of Wolfe et al., "Spatiotemporal Blue Noise
Masks", EGSR 2022. The energy is a gaussian within a slice plus a gaussian along a pixel's own column (sigma 1.9 for
both, toroidal, cut at +-7). Ranks are assigned by progressive void insertion, with seed 20261107 for the tie-break,
and then re-ranked inside each slice so that every slice holds each level exactly 64 times. Generator: the lane's
notes tool `stbn_gen.py`. sha256 e621c2377988e506f102b8df367ed636d7b4b381c593629f62b9313367804022.

Read it with nearest-neighbour point sampling: tile it across the grid and advance one slice a frame (frame mod 64).
Readers: the lamp-injection twin `src/features/air/airlampinjmath.h` (maskPoint) and its unit
`tests/features/air/airlampinj`; the ReSTIR GI bounce (lane RTEMIS1: `src/features/gi/restirgimath.h` stbnIndex, the kernel
`restirgi.comp` through the qrc alias `:/air/stbn_scalar_128x128x64.r8`).

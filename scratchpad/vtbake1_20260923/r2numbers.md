## Texel size per level (from each container header)
| level dim | units/texel | metres/texel | tile content | tile covers | world span in texels |
|---|---|---|---|---|---|
| 2 | 32 | 0.4572 | 256 px | 8192 u = 117.0 m | 24576 |
| 4 | 64 | 0.9144 | 256 px | 16384 u = 234.1 m | 12288 |
| 8 | 128 | 1.8288 | 256 px | 32768 u = 468.2 m | 6144 |
| 16 | 256 | 3.6576 | 256 px | 65536 u = 936.3 m | 3072 |
| 32 | 512 | 7.3152 | 256 px | 131072 u = 1872.7 m | 1536 |

sheets in the bake: [(1, 71, 71), (2, 71, 71), (5, 71, 77), (4, 56, 56)]
tile raw bytes: no cover 323680, cover 369920

## One ring, one sheet: a W x W window, mip 0 only
| format | bytes/texel | W=1024 | W=2048 |
|---|---|---|---|
| BC1 (dxgi 71/72), colour / msn / mask without cover | 0.5 | 0.50 MiB | 2.00 MiB |
| BC3 (dxgi 77/78), mask WITH cover | 1 | 1.00 MiB | 4.00 MiB |
| R16_UNORM (dxgi 56), height | 2 | 2.00 MiB | 8.00 MiB |
| R8G8B8A8 uncompressed (if a ring is kept decoded) | 4 | 4.00 MiB | 16.00 MiB |

A ring with one extra mip for trilinear costs x1.25 of the figure above.

## N-ring stack, per-ring sheet set, mip 0 only, MiB
| sheet set | B/texel/ring | N=1 W=1024 | N=2 W=1024 | N=3 W=1024 | N=4 W=1024 | N=5 W=1024 | N=1 W=2048 | N=2 W=2048 | N=3 W=2048 | N=4 W=2048 | N=5 W=2048 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| colour+msn+mask BC1 (the bake as written, no height) | 1.5 | 1.5 | 3.0 | 4.5 | 6.0 | 7.5 | 6.0 | 12.0 | 18.0 | 24.0 | 30.0 |
| colour+msn+mask BC1 + height R16 (this bake: --vt-height, no cover) | 3.5 | 3.5 | 7.0 | 10.5 | 14.0 | 17.5 | 14.0 | 28.0 | 42.0 | 56.0 | 70.0 |
| colour+msn BC1 + mask BC3 + height R16 (a --cover bake) | 4 | 4.0 | 8.0 | 12.0 | 16.0 | 20.0 | 16.0 | 32.0 | 48.0 | 64.0 | 80.0 |
| colour+msn+mask BC1 + height R16, rings kept as RGBA8 except height | 14 | 14.0 | 28.0 | 42.0 | 56.0 | 70.0 | 56.0 | 112.0 | 168.0 | 224.0 | 280.0 |

## Window reach per level (a ring window centred on the camera)
| level dim | W=1024 covers | half-width (m) | W=2048 covers | half-width (m) | whole world fits at W=2048? |
|---|---|---|---|---|---|
| 2 | 32768 u = 468 m | 234 | 65536 u = 936 m | 468 | no (8% of span) |
| 4 | 65536 u = 936 m | 468 | 131072 u = 1873 m | 936 | no (17% of span) |
| 8 | 131072 u = 1873 m | 936 | 262144 u = 3745 m | 1873 | no (33% of span) |
| 16 | 262144 u = 3745 m | 1873 | 524288 u = 7491 m | 3745 | no (67% of span) |
| 32 | 524288 u = 7491 m | 3745 | 1048576 u = 14982 m | 7491 | yes |

## Tile cache needed to FILL one ring from the pyramid (worst case, window not tile-aligned)
| W | tiles per ring (ceil(W/content)+1)^2 | bytes, no cover | bytes, cover |
|---|---|---|---|
| 1024 | 25 | 7.7 MiB | 8.8 MiB |
| 2048 | 81 | 25.0 MiB | 28.6 MiB |

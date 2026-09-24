| bake | wall | peak working set (sampler) | samples | files | bytes |
|---|---|---|---|---|---|
| `sanctuary_fo4cs` | 45 s | 1998118912 B (2.00 GB) | 110 | 56 | 233955347 |
| `coast_fo4cs` | 50 s | 2134081536 B (2.13 GB) | 118 | 57 | 233465449 |
| `urban_fo4cs` | 73 s | 2678247424 B (2.68 GB) | 177 | 57 | 245548296 |
| `sanctuary_keepbto` | 50 s | 2016555008 B (2.02 GB) | 122 | 64 | 238095780 |
| `coast_keepbto` | 56 s | 2133086208 B (2.13 GB) | 134 | 66 | 236636028 |
| `urban_keepbto` | 80 s | 2663170048 B (2.66 GB) | 192 | 66 | 256856282 |
| `sanctuary_stock` | 7 s | 1532690432 B (1.53 GB) | 17 | 53 | 11518632 |
| `coast_stock` | 11 s | 2129215488 B (2.13 GB) | 25 | 55 | 10127763 |
| `urban_stock` | 22 s | 2641244160 B (2.64 GB) | 53 | 55 | 20542011 |
| `sanctuary_incr` | 44 s | 2005393408 B (2.01 GB) | 105 | 56 | 233955362 |

| bake | stage times (s) | long pole |
|---|---|---|
| `sanctuary_fo4cs` | landscape 0.0, meshes 39.0, textures 2.7, impostors 0.0 | meshes (39.0 s) |
| `coast_fo4cs` | landscape 0.0, meshes 39.5, textures 6.3, impostors 0.0 | meshes (39.5 s) |
| `urban_fo4cs` | landscape 0.0, meshes 59.7, textures 9.7, impostors 0.0 | meshes (59.7 s) |
| `sanctuary_keepbto` | landscape 0.0, meshes 45.2, textures 2.6, impostors 0.0 | meshes (45.2 s) |
| `coast_keepbto` | landscape 0.0, meshes 44.7, textures 4.9, impostors 0.0 | meshes (44.7 s) |
| `urban_keepbto` | landscape 0.0, meshes 67.4, textures 7.6, impostors 0.0 | meshes (67.4 s) |
| `sanctuary_stock` | landscape 0.0, meshes 1.2, textures 2.5, impostors 0.0 | textures (2.5 s) |
| `coast_stock` | landscape 0.0, meshes 2.3, textures 5.0, impostors 0.0 | textures (5.0 s) |
| `urban_stock` | landscape 0.0, meshes 10.2, textures 7.6, impostors 0.0 | meshes (10.2 s) |
| `sanctuary_incr` | landscape 0.0, meshes 37.5, textures 2.6, impostors 0.0 | meshes (37.5 s) |

**`sanctuary_fo4cs`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.86 GB (1998127104 bytes), bto built in scratch E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_fo4cs/lodgen_bto_scratch, 8 chunk(s), 8 dropped, 4140002 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_fo4cs/FO4CSLOD, 19 file(s), 0 outside
```

**`coast_fo4cs`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.99 GB (2138288128 bytes), bto built in scratch E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/coast_fo4cs/lodgen_bto_scratch, 9 chunk(s), 9 dropped, 3170062 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/coast_fo4cs/FO4CSLOD, 20 file(s), 0 outside
```

**`urban_fo4cs`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 2.50 GB (2681909248 bytes), bto built in scratch E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/urban_fo4cs/lodgen_bto_scratch, 9 chunk(s), 9 dropped, 11307488 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/urban_fo4cs/FO4CSLOD, 20 file(s), 0 outside
```

**`sanctuary_keepbto`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.88 GB (2016563200 bytes), bto built in the mod folder, 8 chunk(s), 0 dropped, 0 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_keepbto/FO4CSLOD, 11 file(s), 0 outside
```

**`coast_keepbto`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.99 GB (2138685440 bytes), bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/coast_keepbto/FO4CSLOD, 11 file(s), 0 outside
```

**`urban_keepbto`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 2.50 GB (2680946688 bytes), bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/urban_keepbto/FO4CSLOD, 11 file(s), 0 outside
```

**`sanctuary_stock`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.43 GB (1535737856 bytes), bto built in the mod folder, 8 chunk(s), 0 dropped, 0 bytes freed, layout n/a (no FO4CS-target file written)
```

**`coast_stock`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.99 GB (2137837568 bytes), bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout n/a (no FO4CS-target file written)
```

**`urban_stock`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 2.46 GB (2643378176 bytes), bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout n/a (no FO4CS-target file written)
```

**`sanctuary_incr`**  census, verbatim:

```
bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9, chunk workers 1, peak working set: 1.87 GB (2005401600 bytes), bto built in scratch E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_incr/lodgen_bto_scratch, 8 chunk(s), 8 dropped, 4140002 bytes freed, layout E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/bake/sanctuary_incr/FO4CSLOD, 19 file(s), 0 outside
```

### Every output file of the three default FO4CS bakes

`sanctuary_fo4cs` -- 56 files, 233955347 bytes; 20 under `FO4CSLOD/`:

| file (under `FO4CSLOD/Commonwealth/`) | bytes |
|---|---|
| `Commonwealth.4.-12.24.BTO.manifest.txt` | 43349 |
| `Commonwealth.4.-12.24.lodj` | 162652 |
| `Commonwealth.4.-12.28.BTO.manifest.txt` | 40868 |
| `Commonwealth.4.-12.28.lodj` | 152871 |
| `Commonwealth.4.-12.32.BTO.manifest.txt` | 5639 |
| `Commonwealth.4.-12.32.lodj` | 20744 |
| `Commonwealth.4.-16.24.BTO.manifest.txt` | 54408 |
| `Commonwealth.4.-16.24.lodj` | 204885 |
| `Commonwealth.4.-16.28.BTO.manifest.txt` | 36428 |
| `Commonwealth.4.-16.28.lodj` | 135858 |
| `Commonwealth.4.-16.32.BTO.manifest.txt` | 2806 |
| `Commonwealth.4.-16.32.lodj` | 10550 |
| `Commonwealth.4.-20.24.BTO.manifest.txt` | 53101 |
| `Commonwealth.4.-20.24.lodj` | 199713 |
| `Commonwealth.4.-20.28.BTO.manifest.txt` | 40385 |
| `Commonwealth.4.-20.28.lodj` | 150732 |
| `Commonwealth.4.-20.32.lodj` | 64 |
| `Commonwealth.lodb` | 10553 |
| `Commonwealth.lodi` | 134598 |
| `Commonwealth.lodo` | 225399755 |

| outside the layout (class) | files | bytes | distinct sizes |
|---|---|---|---|
| `Commonwealth.<x>.<y>.24.BTR` | 3 | 98427 | 28713, 32772, 36942 |
| `Commonwealth.<x>.<y>.28.BTR` | 3 | 91144 | 29271, 29391, 32482 |
| `Commonwealth.<x>.<y>.32.BTR` | 3 | 86337 | 28593, 28731, 29013 |
| `tex/Commonwealth.<x>.<y>.24.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.24_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.24_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.28.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.28_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.28_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.32.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.32_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.32_msn.DDS` | 3 | 1049040 | 349680 |

`coast_fo4cs` -- 57 files, 233465449 bytes; 21 under `FO4CSLOD/`:

| file (under `FO4CSLOD/Commonwealth/`) | bytes |
|---|---|
| `Commonwealth.4.12.-20.BTO.manifest.txt` | 19748 |
| `Commonwealth.4.12.-20.lodj` | 80863 |
| `Commonwealth.4.12.-24.BTO.manifest.txt` | 8116 |
| `Commonwealth.4.12.-24.lodj` | 28702 |
| `Commonwealth.4.12.-28.BTO.manifest.txt` | 4381 |
| `Commonwealth.4.12.-28.lodj` | 15361 |
| `Commonwealth.4.4.-20.BTO.manifest.txt` | 19397 |
| `Commonwealth.4.4.-20.lodj` | 71319 |
| `Commonwealth.4.4.-24.BTO.manifest.txt` | 93637 |
| `Commonwealth.4.4.-24.lodj` | 347346 |
| `Commonwealth.4.4.-28.BTO.manifest.txt` | 21241 |
| `Commonwealth.4.4.-28.lodj` | 79032 |
| `Commonwealth.4.8.-20.BTO.manifest.txt` | 15750 |
| `Commonwealth.4.8.-20.lodj` | 58363 |
| `Commonwealth.4.8.-24.BTO.manifest.txt` | 58741 |
| `Commonwealth.4.8.-24.lodj` | 229655 |
| `Commonwealth.4.8.-28.BTO.manifest.txt` | 16073 |
| `Commonwealth.4.8.-28.lodj` | 58769 |
| `Commonwealth.lodb` | 10551 |
| `Commonwealth.lodi` | 134375 |
| `Commonwealth.lodo` | 225399755 |

| outside the layout (class) | files | bytes | distinct sizes |
|---|---|---|---|
| `Commonwealth.<x>.<y>.-20.BTR` | 3 | 134476 | 43588, 44228, 46660 |
| `Commonwealth.<x>.<y>.-24.BTR` | 3 | 125834 | 32518, 46116, 47200 |
| `Commonwealth.<x>.<y>.-28.BTR` | 3 | 138860 | 41644, 46330, 50886 |
| `tex/Commonwealth.<x>.<y>.-20.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-20_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-20_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.-24.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-24_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-24_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.-28.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-28_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-28_msn.DDS` | 3 | 1049040 | 349680 |

`urban_fo4cs` -- 57 files, 245548296 bytes; 21 under `FO4CSLOD/`:

| file (under `FO4CSLOD/Commonwealth/`) | bytes |
|---|---|
| `Commonwealth.4.0.-12.BTO.manifest.txt` | 35740 |
| `Commonwealth.4.0.-12.lodj` | 129993 |
| `Commonwealth.4.0.-4.BTO.manifest.txt` | 260662 |
| `Commonwealth.4.0.-4.lodj` | 958700 |
| `Commonwealth.4.0.-8.BTO.manifest.txt` | 488627 |
| `Commonwealth.4.0.-8.lodj` | 1790922 |
| `Commonwealth.4.4.-12.BTO.manifest.txt` | 195075 |
| `Commonwealth.4.4.-12.lodj` | 717733 |
| `Commonwealth.4.4.-4.BTO.manifest.txt` | 836010 |
| `Commonwealth.4.4.-4.lodj` | 3191995 |
| `Commonwealth.4.4.-8.BTO.manifest.txt` | 550469 |
| `Commonwealth.4.4.-8.lodj` | 2112696 |
| `Commonwealth.4.8.-12.BTO.manifest.txt` | 134470 |
| `Commonwealth.4.8.-12.lodj` | 496141 |
| `Commonwealth.4.8.-4.BTO.manifest.txt` | 84186 |
| `Commonwealth.4.8.-4.lodj` | 303392 |
| `Commonwealth.4.8.-8.BTO.manifest.txt` | 21747 |
| `Commonwealth.4.8.-8.lodj` | 81471 |
| `Commonwealth.lodb` | 10445 |
| `Commonwealth.lodi` | 1126755 |
| `Commonwealth.lodo` | 225399755 |

| outside the layout (class) | files | bytes | distinct sizes |
|---|---|---|---|
| `Commonwealth.<x>.<y>.-12.BTR` | 3 | 109362 | 30140, 36086, 43136 |
| `Commonwealth.<x>.<y>.-4.BTR` | 3 | 110562 | 30666, 38262, 41634 |
| `Commonwealth.<x>.<y>.-8.BTR` | 3 | 106284 | 31554, 35028, 39702 |
| `tex/Commonwealth.<x>.<y>.-12.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-12_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-12_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.-4.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-4_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-4_msn.DDS` | 3 | 1049040 | 349680 |
| `tex/Commonwealth.<x>.<y>.-8.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-8_data.DDS` | 3 | 524664 | 174888 |
| `tex/Commonwealth.<x>.<y>.-8_msn.DDS` | 3 | 1049040 | 349680 |


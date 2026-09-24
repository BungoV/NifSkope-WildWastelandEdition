| picture | what it is | measured |
|---|---|---|
| `images/native_sanctuary.png` | region (a) Sanctuary, the fixed exe's native view opening `bake/sanctuary_fo4cs`'s own `.lodi` | 1400x1091, 913,663 B, **18.32 % of pixels are not the background**, 35,814 distinct colours |
| `images/native_coast.png` | region (b) the south-east coast, same, on `bake/coast_fo4cs` | 1400x1091, 591,407 B, **14.23 % covered**, 50,942 distinct colours |
| `images/native_urban.png` | region (c) downtown Boston, same, on `bake/urban_fo4cs`, windowed to cells `4,-8..7,-5` | 1400x1091, 1,359,588 B, **43.54 % covered**, 88,060 distinct colours |
| `images/lodt_colour_level4.png` | the `.lodt` sheet render the brief asks for: the colour role (role 1) of `everything/vt/Commonwealth.VT.4.lodt`, mip 0, decoded from BC1 by `lodgen_vt_check.py`'s own `Lodv` reader and tiled by `scratchpad/audit1_20260916/make_lodt_sheet.py` | 3x3 tiles at 272 px, 366,314 B |
| `images/lodt_colour_level2.png` | the same container's mip 1, as a second sheet | 6x6 tiles at 136 px, 366,716 B |

The camera is arithmetic, not a remembered screen coordinate: the centre and the
ortho half-width come from each region's own cell footprint at 4,096 units a
cell, so a picture cannot be framed to flatter the bake. The coverage fraction
is there so a black frame cannot pass as a render -- and it caught one.

**The whole-region urban view is REFUSED, by name, and that is the viewer
working.** The first urban render came back at 7,066 bytes and 0.0000 covered.
The reason is in the app's own log, not in a guess:

```
[Warning] lodi objects: "this region needs more than 9500000 vertices;
          ask for a smaller WW_LODI_REGION or a coarser WW_LODI_LEVEL"
```

`src/lodinative.cpp:36` holds `MAX_TOTAL_VERTS = 9500000` and `:237` is the
refusal that names both ways back. Downtown Boston is **33,123 instances in 12
of 20 chunks against a 2,974-base, 5,567-mesh, 517,534-cluster library** -- the
exe prints that line itself on load -- where Sanctuary is 3,526 and the coast
3,303. So the picture above is the same file through `WW_LODI_REGION=4,-8,7,-5`,
a sixteen-cell window, which is the way back the message names. A refusal with
a limit, a reason and a named escape is what this should look like; the row
worth carrying forward is that the limit is reached by a real Commonwealth
region at the shipped defaults, not by a synthetic one.

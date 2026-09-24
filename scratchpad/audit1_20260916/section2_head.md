## 2. A fresh end-to-end bake on three fixture regions

Ten bakes, all on the audited exe, all started after a clean
`tasklist | grep -i -E "Fallout4|NifSkope"` (rc=1, nothing running), driven by
`scratchpad/audit1_20260916/bake_all.sh` -> `bake_one.sh`, which runs one bake at
a time and samples the process's working set with PowerShell
`Get-Process` every 0.4 s for the peak. The log of the whole run is
`bake_runner.log`, which ends `BAKES-COMPLETE 2026-09-17 16:58:20`; the tables
below are generated from it and from the trees on disk by `mk_section2.py`.

The three regions (each 3x3 dim-4 chunks, 12x12 cells, `--dim 4`, defaults
otherwise):

| fixture | `--terrain-region` | what it is |
|---|---|---|
| (a) `sanctuary` | `-20 24 -9 35` | Sanctuary Hills and the woods around it -- the brief's chunk (-20,24) is its first chunk |
| (b) `coast` | `4 -28 15 -17` | the WATER region: the WETTEST 12x12-cell region in the Commonwealth, 44.9 % of its overview texels submerged, scored by `pick_regions2.py` off the shipped `Commonwealth.lodl` overview before it was picked (Sanctuary reads 1.8 %, urban 30.3 %) |
| (c) `urban` | `0 -12 11 -1` | the URBAN region: downtown Boston, the densest placement count of the three (33,123 instances against 3,526), and wet too at 30.3 % |

Command shape, verbatim from `bake_one.sh` (the FO4CS target is `--native`):

```
release/NifSkope.exe -no-gui lodgen <Fallout4.esm> --worldspace 3C \
  --terrain-region <x0> <y0> <x1> <y1> --dim 4 \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
  --out-dir <bake>/ --tex-dir <bake>/tex --native <bake>/
```

`--keep-bto` adds that flag; the STOCK target drops `--native` (and with it the
`FO4CSLOD` tree); `sanctuary_incr` adds `--incremental` over a copy of (a)'s own
finished output (section 2.4, where it did not do what I asked it to).

### 2.1 Wall clock, peak memory, and what landed

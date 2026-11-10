Reference stage tables for tests/spells/cell_load_gate.py (CELLSPEED v1, WW_CELL_SPEED_DUMP).

The gate takes the FASTEST table in this folder as the reference and compares the MEDIAN of the
new runs against it; it fails when the load is more than 10 % slower or when the document counts
(placements, shapes, vertices, triangles, blocks) differ.

Scene of the gn5x5_1110_* tables (2026-11-10, branch perfload1-20261109, parent commit 41f3c0ed):
  WW_CELL_OPEN=<plugins>|Goodneighbor|5,-3|5   (5x5 cells around Goodneighbor)
  WW_ALL_ON=1 WW_CELL_FARLOD=1 WW_CELL_AIR=1 WW_CELL_NOGRID=1
  WW_LOOKDEV=1 WW_LOOKDEV_WEATHER=CommonwealthMisty WW_LOOKDEV_HOUR=12 WW_LOOKDEV_DAY=4
  WW_LOOKDEV_SKY=1 WW_LOOKDEV_CLOUDS=1 WW_LOOKDEV_SUN=1 WW_LOOKDEV_MOON=1 WW_LOOKDEV_FOG=1
  WW_LOOKDEV_SHADOWS=1, the sampler off, one window, release build, 16 logical cores.

A table is machine-bound: a new machine or a changed scene needs its own reference
(run the same switches with WW_CELL_SPEED_DUMP=<file> and put the table here).

  python -I tests/spells/cell_load_gate.py --new <speed.txt> [<speed.txt> ...]

Measure with the files in the OS cache: run the scene once to warm it, then take the tables.
gn5x5_1110_c is such a run (14.5 s). The first run after the build, cold cache, needed 30.9 s
and fails this gate; that is the disk, not the code.

The old asset walk (WW_LOADSPEED_RED=probewalk) on the same build and scene, warm cache,
measured 78.7 s against 14.5 s: the gate fails on it (LOAD FAIL).

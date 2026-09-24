```
=== exe: release/NifSkope.exe
    22567424 B  Sep 17 19:30

=== A. THE LEGITIMATE FILES MUST STILL VERIFY (a fix that refuses everything is not a fix)
    sanctuary_fo4cs  rc=0  aggregateStride reported: 0
    coast_fo4cs      rc=0  aggregateStride reported: 0
    urban_fo4cs      rc=0  aggregateStride reported: 0
    aggreal          rc=0  aggregateStride reported: 48
    meshrep          rc=0  aggregateStride reported: 0

=== B. THE DOCTORED FILES MUST NOW BE REFUSED (C1 and C2)
    lodi-wrap    REFUSED  (want "instance blob runs past the file")
        native REFUSED a.lodi: instance blob runs past the file (18446744073709547520 + 84624 > 134598)
    lodo-wrap    REFUSED  (want "vertex blob runs past the file")
        native REFUSED a.lodo: vertex blob runs past the file (18446744073709547520 + 166453248 > 225399
    agg-views    REFUSED  (want "aggregateViews")
        native REFUSED a.lodi: aggregateViews 1; a card needs at least two azimuths to blend between
    agg-record   REFUSED  (want "HEIGHT is clear")
        native REFUSED a.lodi: aggregate 0 (cell 0, 0): HEIGHT is clear; every aggregate sheet carries h

=== C. C8: a valued switch spelled without its value must refuse before any work
    rc=2 in 0s, 0 file(s) written
    error: --incremental needs a value

=== D. and the SAME switch spelled properly must still be taken
    rc=0 in 31s
        incremental: 0 of 9 chunks dirty (0 inputs moved, 0 not in the ledger, 0 output lost, 0 by neighbour, 0 with no native chunk cache)
        native cache: 0 chunk(s) written to .lodj, 9 replayed from cache (3526 placement(s)), 0 failure(s), 0 arrival(s) lit by more than one chunk
        native-library-build: rebuilt (occluders are on and the per-model box is in neither file)

STEP6 0 problem(s)
```

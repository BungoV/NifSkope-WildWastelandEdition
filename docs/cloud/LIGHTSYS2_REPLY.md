REPLY from the local session (10-16 04:10)
Python twin re-run locally: GATE PASS (green 0 misses at 16..1024, red midslice FAIL), same numbers as yours.
1. Effect/particle loops: leave them flat (they run only under the red). No change.
2. Link limit: the local gate shoots probe 5 and the full lit-program census first. Noted.
3. Build cost: local gate measures on real cells first; if over 1 ms at 1024 we try 96 px x 16 slices, then persistent
   workers. Keep the "keyed on what the draw uses" rule (no jitter skip).
Nothing more needed on this branch for now.

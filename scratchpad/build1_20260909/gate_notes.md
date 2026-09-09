# BUILD1 gate log (working notes, numbers as measured)

exe: release/NifSkope.exe 2026-09-09 17:22:05, 17,796,608 bytes
(first link 17:09:31 carried a stale btdterrain.o -- see MISTAKES)

| gate | result |
|---|---|
| render_shot.sh | 15 checks, 0 failures, PASS, rc 0 |
| lodgen_terrain.sh | 26 checks, 0 failures, PASS, rc 0 |
| lodt_write.sh | PASS, rc 0 (3 sections: writer round-trip, v2+water, landless) |
| four-worldspace .lodt vs heightmap | 0 differing texels on all five |
| lodt_open.sh (v1 fixture) | 23 checks, 0 failures, PASS, rc 0 |
| lodt_open.sh (v2 fixture) | 23 checks, 0 failures, PASS, rc 0 |
| btd_terrain.sh | 13 checks, 0 failures, PASS, rc 0 |
| lodgen_identity.sh | RESULT PASS, rc 0 (byte-identical rebake, 0 duplicate keys, 406 shared) |

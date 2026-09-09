#!/usr/bin/env python
"""BUILD1: turn the two 2026-09-09 WW_CHANGES entries from "not built" into the
measured results.

WW_CHANGES.md is MIXED and stays so (CONSTITUTION 8). Both entries sit in the
LF region at the head of the file, so every inserted line is LF and the CR
count must not move.
"""
import os

P = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 '..', '..', 'WW_CHANGES.md')
P = os.path.normpath(P)

b = open(P, 'rb').read()
cr_before = b.count(b'\r')

# ---- the NOPROMPT entry ------------------------------------------------
OLD1 = (b'`src/nifskope.cpp`, `src/nifskope.h`, `tests/spells/render_shot.sh`. Code written\n'
        b'and syntax-checked; **not built and not run** (another lane held the build slot).\n')
NEW1 = (b'`src/nifskope.cpp`, `src/nifskope.h`, `tests/spells/render_shot.sh`. BUILT AND\n'
        b'RUN 2026-09-09 by lane BUILD1: `release/NifSkope.exe` 17:22:05, and\n'
        b'`tests/spells/render_shot.sh` reads **15 checks, 0 failures, PASS** (exit 0).\n')
assert b.count(OLD1) == 1, b.count(OLD1)
b = b.replace(OLD1, NEW1)

OLD2 = (b'Not measured: nothing here has been built or run. The mechanism above is read\n'
        b"from the sources and from Qt's headers, and the numbers the gate prints do not\n"
        b'exist yet.\n')
NEW2 = (b'MEASURED, on the built exe. The plain render exits 0 in 5 s and writes a\n'
        b'30,356-byte framebuffer with NO discard line; the bake on the same clean cube\n'
        b'exits 0 in 3 s, writes its sidecar, and still discards nothing; the bake on the\n'
        b'`_L1` cube exits 0 in 3 s (rc 124 is the dialog), its sidecar carries one\n'
        b'`hidden` line, and `release/ww_headless_close.log` reads exactly\n'
        b'`discarded cube_lod`. No NifSkope process survived any of the three.\n'
        b'\n'
        b'NOT measured: the OLD binary was not run to watch the dialog appear -- the\n'
        b'17:09 link overwrote it and the guard cannot be turned off from outside (it\n'
        b'fires on any `WW_*` variable, and every headless route sets one). The claim\n'
        b'that the old code sat at rc 124 stays a reading of the sources.\n'
        b'\n'
        b'The harness itself could not run as the lane left it, and BUILD1 fixed it:\n'
        b'the dirty fixture was made with `-no-gui set -f Name -v <text>`, which the\n'
        b'CLI refuses -- a block\'s Name is a `tStringIndex` and\n'
        b'`NifValue::setFromString` parses that as a NUMBER -- and read back with\n'
        b'`get -f Name`, which prints the index. The rename now rewrites the one\n'
        b'length-prefixed entry in the header string table (the bytes still come from\n'
        b"the app's own `new --cube`) and both names are read out of `list`.\n")
assert b.count(OLD2) == 1, b.count(OLD2)
b = b.replace(OLD2, NEW2)

# ---- the TERRAINFIX entry ----------------------------------------------
OLD3 = (b'Also: `tests/spells/lodt_write.sh` printed `RESULT PASS`/`FAIL` and threw the\n'
        b'exit code away -- the python block was the script\'s last command and never\n'
        b'called `sys.exit`, so the harness reported success on any measurement at all.\n'
        b'It exits on its verdict now.\n')
NEW3 = (OLD3 +
        b'\n'
        b'**BUILT AND RUN 2026-09-09 by lane BUILD1**, `release/NifSkope.exe` 17:22:05.\n'
        b'`lodgen_terrain.sh` **26 checks, 0 failures**: the pyramid-assembled sheet now\n'
        b'reads `UP=G D0=76 D1=32 D2=51 D3=32`, byte-for-byte the same statistic as the\n'
        b'direct bake, against vanilla\'s own `UP=G 99/67/67/67` as the control -- the\n'
        b'nearest-sampled sheet gave 0 on classes 1..3. `lodt_write.sh` **PASS, exit 0**\n'
        b'in all three sections, and NukaWorldAmphitheater now reads **0 of 114,688**\n'
        b'texels differing from its shadow heightmap where the shipped file read 97.\n'
        b'The whole set, freshly baked and diffed against its own heightmap: Commonwealth\n'
        b'0 of 37,748,736, NukaWorld 0 of 4,326,400, DLC03FarHarbor 0 of 20,207,616\n'
        b'(was 62), DiamondCity 0 of 172,032 (was 167,936), NukaWorldAmphitheater 0 of\n'
        b'114,688 (was 97). The offline `_msn` table reproduced exactly on the built\n'
        b'exe\'s own `.lodt`: roughness 2.001 -> 0.209 and 2.000 -> 0.150 on the two\n'
        b'tiles, mean UP 0.288 -> 0.841 and -0.151 -> 0.943.\n'
        b'\n'
        b'Two things the lane could not have known, both found by the build.\n'
        b'`lodgen_terrain.sh` rung 4 asked the pyramid for a chunk sheet its region\n'
        b'could not produce (`--terrain-region -20 24 -19 25` is a quarter of the dim-4\n'
        b'chunk; `assembleChunkRow` needs both child tile rows of a parent row), and it\n'
        b'now bakes the whole chunk. `lodt_write.sh` asserted the version-1 fallback was\n'
        b'byte-identical past the header, which the format forbids: the block directory\n'
        b'stores each payload offset ABSOLUTE, so all 48,960 of them move by the same\n'
        b'eight bytes. The check now compares those entries as numbers (0 offsets not\n'
        b'+8, 0 sizes changed) and the 9,437,644 bytes before the directory and the\n'
        b'25,732,130 after it byte for byte -- a stronger statement than the one it\n'
        b'replaced.\n'
        b'\n'
        b'And the version-2 header cost one crash before it was caught.\n'
        b'`src/btdterrain.cpp` includes `lodtfile.h` and puts a `LodtFile` on the stack,\n'
        b'but qmake\'s dependency list for `btdterrain.o` never names that header, so\n'
        b'make left the object at its 15:30 build while `lodtfile.o` grew three members\n'
        b'at 17:09; `LodtFile::open()` then wrote past the caller\'s smaller object and\n'
        b'every `-no-gui lodt` run segfaulted, on version 1 files as well as version 2.\n'
        b'Found by `lodt_open.sh`, fixed by rebuilding the object; the dependency was\n'
        b'added to `Makefile.Release` by hand and a qmake re-run is owed.\n'
        b'`lodt_open.sh` then reads **23 checks, 0 failures** on a version 1 file and on\n'
        b'a version 2 one.\n')
assert b.count(OLD3) == 1, b.count(OLD3)
b = b.replace(OLD3, NEW3)

assert b.count(b'\r') == cr_before, (b.count(b'\r'), cr_before)
open(P, 'wb').write(b)
print('patched %s, CR %d (was %d)' % (P, b.count(b'\r'), cr_before))

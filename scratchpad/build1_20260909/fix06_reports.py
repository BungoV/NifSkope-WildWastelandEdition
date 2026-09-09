#!/usr/bin/env python
"""BUILD1: append the build section to both lane reports, and the mistakes to
MISTAKES.md. All three files are LF-only and must stay so."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))

CLOCKS = """
---

## Build (BUILD1) -- 2026-09-09

**The clocks, in one table** (CONSTITUTION 4). The exe is newer than every
source and every harness the two lanes touched, and than the two harness fixes
this lane had to make.

| artefact | mtime |
|---|---|
| `src/lodgen.cpp` | 16:27:07 |
| `src/lodtfile.h` | 16:31:24 |
| `src/lodtfile.cpp` | 16:31:40 |
| `src/nifskope.h` | 16:55:58 |
| `src/nifskope.cpp` | 16:57:58 |
| `tests/spells/render_shot.sh` | 17:12:18 (BUILD1's fixture fix) |
| `tests/spells/lodgen_terrain.sh` | 17:15:02 (BUILD1's rung-4 region) |
| `tests/spells/lodt_write.sh` | 17:17:21 (BUILD1's fallback check) |
| `Makefile.Release` | 17:21:52 (BUILD1's dependency stopgap) |
| **`release/NifSkope.exe`** | **17:22:05**, 17,796,608 bytes |
| `release/style.qss` | 17:22:05, identical to `res/style.qss` |

`make -j2` exited **0** (its own exit code gated the chain, not a grep), on a
game-down and NifSkope-free machine checked before the build and before every
run. The FIRST link, 17:09:31, was thrown away: it carried a stale
`btdterrain.o` and crashed -- see Mistakes.

### The gate table, all on the 17:22:05 exe

| gate | result |
|---|---|
| `tests/spells/render_shot.sh` | **15 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodgen_terrain.sh` | **26 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodt_write.sh` | **PASS**, exit 0, all three sections |
| four/five-worldspace `.lodt` vs heightmap | **0 differing texels** on all five |
| `tests/spells/lodt_open.sh` (version 1 file) | **23 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodt_open.sh` (version 2 file) | **23 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/btd_terrain.sh` | **13 checks, 0 failures, PASS**, exit 0 |
| `tests/spells/lodgen_identity.sh` | **RESULT PASS**, exit 0 |

Nothing in the list was skipped. Harnesses NOT run, and why: everything the
two lanes did not reach -- the impostor-card, atlas, merge, far-ring, resource
stack, texture-array, octahedral, collision, panel and workspace spells. The
one that comes closest is `lodt_btd.sh` (the FO76 `.btd` -> `.lodt` conversion,
~25 minutes): `btd_terrain.sh` covers the same reader against the same
Appalachia file in a fraction of the time, and no lane touched the conversion
path.
"""

NOPROMPT = CLOCKS + """
### What the three cases measured

| case | exit | wall | its own output | discard log |
|---|---|---|---|---|
| `WW_RENDER_SHOT`, plain cube | 0 | 5 s (cap 90, slow bar 45) | 30,356-byte PNG | 0 lines |
| `WW_IMPOSTOR_BAKE`, plain cube | 0 | 3 s | `cube_plain.txt` | 0 lines |
| `WW_IMPOSTOR_BAKE`, `_L1` cube | 0 | 3 s | `cube_lod.txt`, 1 `hidden` line | `discarded cube_lod` |

No NifSkope process survived any of the three. The pair is what carries it: the
guard fired exactly once, on the only document that was edited.

**NOT measured.** The old binary was never run to watch the dialog appear. The
17:09 link overwrote it, and the guard cannot be switched off from outside --
it keys on any `WW_*` variable and every headless route sets one. "rc 124 is
the dialog" stays a reading of the sources, not an observation.

### The harness could not run as the lane left it

`render_shot.sh` died at `FAIL  could not rename the shape`. Its dirty fixture
was built with `-no-gui set -b N -f Name -v "<name>_L1"`, which the CLI refuses:
a block's `Name` is a `tStringIndex`, and `NifValue::setFromString`
(`src/data/nifvalue.cpp:715`) parses that as a NUMBER -- the CLI can address
the header string table by index but cannot introduce a new string. The two
fixture assertions had the same root: `get -f Name` prints the index (`1`), so
they were comparing a number against a `_L1` suffix, and the "plain" one passed
vacuously while the "dirty" one could never pass.

BUILD1's fix is harness-side only, no C++ change and no rebuild
(`scratchpad/build1_20260909/fix01_render_shot_fixture.py`): the rename
rewrites the one length-prefixed entry in the header string table -- the file
is still `new --cube`'s own bytes, nothing hand-authored -- and both names are
read out of `-no-gui list`, which prints the resolved string. The fixture
checks now read `shape [1] named 'Cube'` and `shape [1] named 'Cube_L1'`.

The check count is **15**, not the 14 the resume file predicted: the two
fixture assertions are checks too.
"""

TERRAIN = CLOCKS + """
### The terrain numbers, measured on the built exe

**The `_msn` pyramid, rung 4 of `lodgen_terrain.sh`.** The sheet the pyramid
assembles now reads `UP=G  D0=76 D1=32 D2=51 D3=32` -- identical, statistic for
statistic, to the direct bake of the same chunk (`UP=G 76/32/51/32`), with
vanilla's own shipped sheet beside it as the known-answer control
(`UP=G 99/67/67/67`). Nearest sampling gives 0 on classes 1..3; the gate wants
above 20.

**The grid-phase roughness table reproduced**, offline, on a `.lodt` written by
THIS exe (`vt_msn_sim.py`, controls printed first: smooth analytic field
0.0437, the same field creased every fourth column 1.9955, separation 45.7x):

| tile | roughness before | after | vanilla | mean UP before | after |
|---|---|---|---|---|---|
| 4.-60.36 | 2.001 | **0.209** | 0.065 | 0.288 | **0.841** |
| 4.-20.24 | 2.000 | **0.150** | 0.031 | -0.151 | **0.943** |

Light: 0.8047 -> 0.8929 (+11.0%) and 0.5331 -> 0.7004 (+31.4%). These are the
lane's own numbers, recomputed here from the new file rather than quoted.

**The landless cell.** `lodt_write.sh`'s NukaWorldAmphitheater section reads
`110 of 112 cells carry no LAND record` (the assertion that the input HAS the
case) and then **0 of 114,688 texels differ**, where the shipped 2026-09-05
file read 97 and exited 1. Freshly baked and diffed against their own shadow
heightmaps, every worldspace agrees:

| worldspace | texels | differing now | before |
|---|---|---|---|
| Commonwealth | 37,748,736 | **0** | 0 |
| NukaWorld | 4,326,400 | **0** | 0 |
| DLC03FarHarbor | 20,207,616 | **0** | 62 |
| DiamondCity | 172,032 | **0** | 167,936 |
| NukaWorldAmphitheater | 114,688 | **0** | 97 |

The row-flip control is printed beside each: 19,053,132 / 4,316,218 /
2,247,054 / 18 / 1,726 differing read south-up. On DiamondCity that control is
weak (18 of 172,032), because 164 of its 168 cells are landless and sit at one
constant default height -- so for that worldspace the floor is the pre-fix
number, 167,936, not the flip.

**Version 2 and the water fields.** The writer prints `worldspace default
height 450 type 00000018` and the FILE's bytes at 0x98/0x9C say the same; the
first section is at 0xA0; 36,357 cells inherit the worldspace type and 507 name
their own, and every explicit index is inside the 15-entry WATR table.

**The version 1 fallback is exact**, but not in the way the harness asserted --
see Mistakes. Measured: 48,960 directory entries, **0** whose payload offset is
not exactly v1 + 8, **0** whose sizes changed, and the 9,437,644 bytes before
the directory and the 25,732,130 after it byte-identical.

### The `.lodt` set bungo has installed is now version 2

Backed up first as `<name>.lodt.bak-20260909` beside each, then written into a
scratch directory, `--verify-only`'d there, moved over, and `--verify-only`'d
again in place. NOT renamed to `.lodl` -- a later lane owns that.

| file | before | after | verify-only |
|---|---|---|---|
| Commonwealth.lodt | 35,953,286 (v1) | **35,953,294 (v2)** | rc 0, 36,864 samples, 0 mismatched |
| DLC03FarHarbor.lodt | 9,195,806 (v1) | **9,195,933 (v2)** | rc 0, 5,568 samples, 0 mismatched |
| DiamondCity.lodt | 53,220 (v1) | **53,148 (v2)** | rc 0, 256 samples, 0 mismatched |
| NukaWorld.lodt | 7,182,348 (v1) | **7,182,356 (v2)** | rc 0, 69,696 samples, 0 mismatched |
| NukaWorldAmphitheater.lodt | 38,104 (v1) | **38,303 (v2)** | rc 0, 128 samples, 0 mismatched |

Commonwealth and NukaWorld grew by exactly the eight header bytes: neither has
a landless cell. Far Harbor grew 127 and the Amphitheater 199 -- the eight plus
the blocks that now carry real terrain where they carried a flat sentinel.
DiamondCity SHRANK by 72 despite the eight: its 164 landless cells now hold one
constant default height, which the pyramid compresses better than the sentinel
plane it replaced.

**FO4CS WILL REFUSE THESE FILES TODAY.** `FarFieldLodtFormat.h` pins
`kVersion = 1u` (lane LODT1, wave 71) and rejects anything else. Either FO4CS
learns version 2 or the set is rewritten with `WW_LODT_VERSION=1`, which needs
no rebuild on our side and reproduces the version 1 bytes exactly. That is
bungo's call; the `.bak-20260909` files are the immediate way back.
"""

SKILLS = """
### Finished-work skill review (BUILD1)

**Loaded and used**: `nifskope-ww-build-verify` (the gated chain, make's own
exit code, the exe renamed aside, the link-time stylesheet copy, the
exe-newer-than-sources test, and the patch-with-a-script-file rule -- every fix
here is a `fixNN.py` under `scratchpad/build1_20260909/` with an anchor-count
assertion and a CR-count assertion), `nifskope-ww-lodgen` (the CLI table, the
worldspace IDs, the byte-identity gates, the editing traps),
`nifskope-ww-render-shot` (the switch table and the new "the run hangs and
writes nothing" section lane NOPROMPT added -- it named `rc=124`-with-output as
the signature, which is what made the rc-0 result legible).

**The skill that should exist and does not**, and it cost this build an hour:
*prove a build is CONSISTENT, not merely successful*. Nothing in
`nifskope-ww-build-verify` catches a translation unit that make had no reason
to rebuild, and that is exactly what happened -- the chain's own
`test exe -nt source` passed while one object was two hours stale against a
header that had grown three members. The procedure is short and mechanical:
after any change to a header, list every `.cpp` that includes it, check each
object's mtime against the header's, and re-run qmake when an include is NEW
(qmake's dependency lists are frozen at generation time). **Recommended as a
section of `nifskope-ww-build-verify` rather than a skill of its own**, since
it belongs to the same chain, and the director should place it in BOTH skill
trees (CONSTITUTION 1a, the two-tree drift). I have not written it: this lane
owns no file under `.claude/skills`.

**A second candidate, declined with a reason**: "regenerate bungo's installed
`.lodt` set" is now a written script
(`scratchpad/build1_20260909/regen_lodt.sh`) with the backup-first,
write-to-scratch, verify, then install order. It will recur -- the `.lodl`
rename is already owed -- but it is fifteen lines of shell that read better as
the script than as prose, and the script is in the repo. If a third lane needs
it with different worldspaces, it becomes a skill.

**Declined outright**: the header-string-table rename used to build the dirty
fixture. It is four lines of Python, it exists in `render_shot.sh` now, and the
CLI limitation it works around is the thing that should be fixed instead
(`-no-gui set -f Name` could assign through `NifModel::set<QString>`), which is
a code change for a lane that owns `src/nifcli.cpp`.
"""

for path, text in (('scratchpad/lane_noprompt_report.md', NOPROMPT + SKILLS),
                   ('scratchpad/lane_terrain_fix_report.md', TERRAIN + SKILLS)):
    p = os.path.join(ROOT, path)
    b = open(p, 'rb').read()
    assert b.count(b'\r') == 0, (path, b.count(b'\r'))
    add = text.encode('utf-8')
    assert b'\r' not in add
    if not b.endswith(b'\n'):
        b += b'\n'
    b += add
    open(p, 'wb').write(b)
    print('appended %s, %d bytes, CR %d' % (path, len(b), b.count(b'\r')))

# ---- MISTAKES.md, newest at the top ------------------------------------
p = os.path.join(ROOT, 'MISTAKES.md')
b = open(p, 'rb').read()
assert b.count(b'\r') == 0, b.count(b'\r')
entries = open(os.path.join(HERE, 'mistakes_text.md'), 'rb').read()
entries = entries.replace(b'\r\n', b'\n')
assert b'\r' not in entries
ANCHOR = b'## 2026-09-09 -- `git diff --numstat` was read as this lane\'s diff in a shared tree'
assert b.count(ANCHOR) == 1, b.count(ANCHOR)
b = b.replace(ANCHOR, entries.rstrip(b'\n') + b'\n\n' + ANCHOR)
assert b.count(b'\r') == 0
open(p, 'wb').write(b)
print('patched MISTAKES.md, %d bytes, CR %d' % (len(b), b.count(b'\r')))

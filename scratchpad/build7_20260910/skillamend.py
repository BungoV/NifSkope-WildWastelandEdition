#!/usr/bin/env python3
"""Lane BUILD7: the absolute-INPUT-path rule, into both skill trees.

Both skills already say "every WW_* OUTPUT path is ABSOLUTE". BUILD7 paid for
the other half: a relative INPUT -- the NIF in argv, and WW_HKXANIM_CLIP --
loads nothing, silently, and turns two green gates red for a reason that is not
in the code under test.

CONSTITUTION 1a (the two skill trees drift): apply per file, to both trees,
never a blanket copy. Anchored, exactly-once, CR asserted, refuses if the text
is already there.
"""
import os

TREES = [r'E:\Projects\Claude\.claude\skills',
         r'E:\Projects\NifskopeWildWastelandEdition\.claude\skills']

RENDER_ANCHOR = ("The same holds for `WW_IMPOSTOR_BAKE`, `SHOT=` and `WW_WATER_MARK_SHOT`.\n")

RENDER_ADD = """
**And so is every INPUT path -- argv included** (2026-09-10, lane BUILD7). A
Git-Bash parent gets NO MSYS2 argv/environment conversion and `_harness.sh`'s
`winpath()` only rewrites the `/e/...` form, so a REPO-RELATIVE path reaches the
app as a file it cannot open, and nothing says so. The two symptoms, both
measured in one session:

* the NIF in argv -- `release/NifSkope.exe --port N fixtures/x.nif` -- opens a
  scene with **one unnamed node**. `tests/spells/hkxanim_play.sh` then reported
  "0 bones matched (expected 78)", 8 failures of 27, none of them the code's;
* `WW_HKXANIM_CLIP=scratchpad/.../jog.hkx` loads no clip and the hook throws the
  refusal away, so all four of gate (e)'s renders come out as the BIND POSE --
  which is that gate's own refuter for "the pose never reached the rig".

The tell for the first is a harness log line naming a node COUNT that cannot
belong to the file you think you opened. Write every path
`E:/Projects/.../file`, or run it through `winpath` first.
"""

HKX_ADD = """

## 12. Absolute paths, or the gates lie (lane BUILD7, 2026-09-10)

`scratchpad/hkx2_20260910/PENDING.md` quotes its resume with repo-relative
paths. Run verbatim, they produce two green-looking falsehoods, and neither is
a defect in the reader, the playback or the mapping:

| command as written | what happened | with an absolute Windows path |
|---|---|---|
| `SRC=fixtures/human_male_vanilla.nif bash tests/spells/hkxanim_play.sh` | `NIF:  (1 nodes)`, "0 bones matched (expected 78)", 8 failures of 27 | 27 checks, 0 failures, **78 / 17 / 4** |
| `CLIP=scratchpad/hkx1_20260910/clips/jog.hkx bash scratchpad/hkx2_20260910/shots.sh` | 1 distinct image of 4 -- gate (e)'s own refuter | **4 of 4 distinct** |

Git-Bash does not get MSYS2's argv/environment path conversion and
`_harness.sh`'s `winpath()` only rewrites `/e/...`. And the `WW_HKXANIM_CLIP`
hook in `src/nifskope_ui.cpp` keeps the loader's refusal in a local it only
tests for emptiness, so a clip that does not load is silent.

**One more framing fact, for any picture of a third-party clip.** A converted
clip may carry its travel on the COM TRACK rather than in the root-motion
channel -- the Mixamo fixture moves 487 units in +Y with root motion all zero --
so a FRONT view puts that travel along the view axis and a perspective camera
shrinks the figure to nothing by the last frame (5 KB of empty background, and
gate (e) still passes at 3 distinct of 4). Photograph such a clip
ORTHOGRAPHICALLY: `WW_RENDER_VIEW=5 WW_RENDER_CENTER=0,0,62 WW_RENDER_ORTHO=80`
frames every frame of both `jog.hkx` and the Mixamo clip at the same scale and
the same screen position, and the bind-pose tile then comes out BYTE-IDENTICAL
between two different clips' sheets -- which is the camera pin's own proof.
"""


def edit(path, anchor, add, mode):
    b = open(path, 'rb').read()
    assert b.count(b'\r') == 0, path
    t = b.decode('utf-8')
    if 'lane BUILD7' in t:
        print('  ALREADY AMENDED  %s' % path)
        return
    if mode == 'insert':
        assert t.count(anchor) == 1, (path, t.count(anchor))
        t = t.replace(anchor, anchor + add)
    else:
        t = t.rstrip('\n') + '\n' + add
    out = t.encode('utf-8')
    assert out.count(b'\r') == 0
    open(path, 'wb').write(out)
    print('  amended %s  %d -> %d bytes' % (path, len(b), len(out)))


for tree in TREES:
    print(tree)
    edit(os.path.join(tree, 'nifskope-ww-render-shot', 'SKILL.md'), RENDER_ANCHOR, RENDER_ADD, 'insert')
    edit(os.path.join(tree, 'ww-hkx-animation', 'SKILL.md'), None, HKX_ADD, 'append')

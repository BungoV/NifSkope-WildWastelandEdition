#!/usr/bin/env python3
"""Lane BUILD7: splice lanes HKX2's and HKX5's ready WW_CHANGES entries into
WW_CHANGES.md, and turn their "NOT BUILT" status blocks into the measured one.

WW_CHANGES.md is MIXED (19,020 CR as of 2026-09-09) and stays so: the splice is
BINARY, at the top, where the 2026-09 entries are LF-only, and the CR count is
asserted unchanged. Nothing already in the file is rewritten except the two
status sentences the resume owes (`nifskope-ww-resume-pending` rule 7.1) --
and those live in the spliced text, not in the file's existing bytes.
"""
import os, sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
CH = os.path.join(REPO, 'WW_CHANGES.md')
E2 = os.path.join(REPO, 'scratchpad', 'hkx2_20260910', 'WW_CHANGES_ENTRY.md')
E5 = os.path.join(REPO, 'scratchpad', 'hkx5_20260910', 'WW_CHANGES_ENTRY.md')

HEADER = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"

BUILD_BLOCK = """
**BUILT AND GATED (lane BUILD7, 2026-09-10).** `release/NifSkope.exe` 14:04:34,
19,197,952 bytes, `qmake` + `make -j2` both RC=0 with `src/hkxanim.{h,cpp}`,
`src/hkxplayback.{h,cpp}`, `src/hkxplaybacktest.cpp`, `src/gltfimport.{h,cpp}`
and `src/hkxwrite.{h,cpp}` in `NifSkope.pro`; the regenerated dependency lists
name `hkxplayback.h` for all five objects that include it (`glnode.o`,
`glscene.o`, `nifskope_ui.o`, `hkxplayback.o`, `hkxplaybacktest.o`) and
`hkxanim.h` for eight; the exe is newer than all 70 changed files under
`src/ res/ tools/ tests/`; `res/style.qss` and `release/style.qss` compare
equal.

| gate | expected | measured |
|---|---|---|
| `hkxanim_gates.py` | 134 checks / 3 fixture failures | **134 / 3** — the 17 `Weapon*` bones absent from `skeleton.nif`, the 0.00148 `weapon` reference-pose translation, and the furniture T-pose clip that is not the bind pose. All three pre-registered. |
| `hkxanim_synthetic.py` | 29 / 0 | **29 / 0 PASS** |
| `hkxanim_mutate.py` | 20 / 0 | **20 / 0 PASS** |
| (g) identity rule, C++ | 8,835 rows, `bone == track` on every one | **8,835 / 0 mismatches**; `hkxanim_dump.exe` (14:05:13) prints `frames 93 tracks 95 ... blendHint NORMAL skeleton Root rootMotion yes` and no refusal |
| (g) `identity_floor.py` | 6 / 6 | **6 / 6 PASS** — empty, full-length and a permutation accepted; 94, 96 and 1 refused by name |
| (a)-(d), (f) `hkxanim_play.sh` on `skeleton.nif` | PASS | **27 checks, 0 failures.** (a) worst translation 0, rotation 7.64e-06 deg, scale 0 over 234 comparisons; (a floor) the wrong frame fails at 5.85e-05 / 0.218 deg; (b) 0 of 129 nodes differ after unload, (b floor) 78 differed while posed; (c) 78 / 17 / 4 with the unmatched bones named; (d) the road sign loads, refuses in words, changes 0 of 2 nodes |
| (a)-(d), (f) on `fixtures/human_male_vanilla.nif` | 78 / 17 / 4 | **27 checks, 0 failures**, 78 / 17 / 4, the four case-folded ones being `Spine1`, `Spine2`, `Head`, `Weapon` |
| (e) pictures, `jog` | 4 PNGs, >= 3 distinct | **4 of 4 distinct** |
| (e) pictures, Mixamo | 4 PNGs, >= 3 distinct | **3 of 4 distinct** — a pass, but the two that match are 5 KB of empty background: under gate (e)'s 35-degree perspective the clip's 487-unit COM travel carries the figure out of frame. See the frames below. |

**The frames bungo asked for** (*"a few frames of imported animations on the
human rig, at different points in animation's time"*):
`scratchpad/build7_20260910/frames_jog.png` and `frames_mixamo.png` — the bind
pose plus frames 0, 1/4, 1/2, 3/4 and last of each clip on
`fixtures/human_male_vanilla.nif`, plus a side view of the Mixamo clip at its
lowest COM frame (20, t=0.333334). ONE camera for every tile of both sheets,
read back from `release/ww_camera_pin.log` at every grab:
`arm=center/ortho/view view=5 lookat=0,0,62 halfW=80 halfH=70.6854 persp=0
vp=1065x941 upp=0.150235`. It is ORTHOGRAPHIC on purpose: in a front view the
Mixamo clip's travel runs along the view axis, and a perspective camera shrinks
the figure to nothing by the last frame. The bind-pose tile of the two sheets is
byte-identical (md5 `580245fba8bad849de9c5f0a7e9f44bb`), which is the pin's own
proof; 6 of 6 tiles differ within each sheet.

**Two paths in the resume were RELATIVE and both produced a green-looking
falsehood** (root `MISTAKES.md`, 2026-09-10 lane BUILD7): a relative `SRC`
opened a scene of one unnamed node and reported "0 bones matched (expected 78)",
and a relative `CLIP` produced four identical pictures. Every path handed to
`NifSkope.exe` is an absolute Windows path. The `WW_HKXANIM_CLIP` hook discards
the loader's refusal string, which is why the second cost a whole render round;
reported, not fixed.

**Still not measured:** Fallout 4 has never loaded a file written by
`src/hkxwrite.cpp`, and the round trip bungo actually asked for -- export an
animation, import it back, and hold the two against each other 1:1 -- has not
been run. The flight files are in `scratchpad/hkx5_20260910/flight/`.
"""


def read_entry(path, drop_prefix_lines):
    b = open(path, 'rb').read()
    assert b.count(b'\r') == 0, '%s is not LF-only' % path
    lines = b.decode('utf-8').split('\n')
    return '\n'.join(lines[drop_prefix_lines:]).strip('\n')


def main():
    cur = open(CH, 'rb').read()
    cr0, n0 = cur.count(b'\r'), len(cur)
    print('WW_CHANGES.md before: %d bytes, CR=%d' % (n0, cr0))
    assert cur.startswith(HEADER), 'the file does not start with the expected header'

    txt = cur.decode('utf-8')
    for marker in ('lane HKX2)', 'lane HKX2b)', 'lane HKX5, 2026-09-10)'):
        if marker in txt:
            print('REFUSED: %r is already in WW_CHANGES.md -- nothing spliced.' % marker)
            return 3

    # HKX2's file opens with a one-line "text only" header + a blank line.
    e2 = read_entry(E2, 2)
    # HKX5's opens with a 3-line HTML comment.
    e5 = read_entry(E5, 3)
    assert e2.startswith('## '), e2[:40]
    assert e5.startswith('### '), e5[:40]
    e5 = e5[1:]          # '###' -> '##', one heading level, like every other entry
    assert e5.startswith('## ')

    # The two status sentences the resume owes (rule 7.1). Each must be present once.
    subs = [
        ('**STATUS: NOT BUILT.**',
         '**STATUS WHEN THE LANE ENDED: NOT BUILT** (it is built now -- the '
         'measured block is at the end of this entry)**.**'),
        ('**STILL NOT BUILT.**',
         '**STILL NOT BUILT WHEN THE LANE ENDED** (built and gated since -- the '
         'measured block is at the end of this entry)**.**'),
    ]
    for old, new in subs:
        assert e2.count(old) == 1, (old, e2.count(old))
        e2 = e2.replace(old, new)

    old5 = ('Neither file is in\n`NifSkope.pro` yet — the hook-up is a refusing script,\n'
            '`scratchpad/hkx5_20260910/hookup.py`, because three HKX lanes are queued on\n'
            'that file. They build through')
    assert e5.count(old5) == 1, e5.count(old5)
    e5 = e5.replace(old5, (
        'Both files are in\n`NifSkope.pro` since lane BUILD7 applied the lane\'s refusing hook-up\n'
        '(`scratchpad/hkx5_20260910/hookup.py`\'s four anchored lines, +80 bytes, CR\n'
        'unchanged) and re-ran `qmake`; `gltfimport.o` and `hkxwrite.o` are linked\n'
        'into `release/NifSkope.exe` 14:04:34. They also build standalone through'))

    # The measured block is written ONCE, under the HKX2 entries; HKX5's entry
    # points at it rather than repeating six kilobytes.
    POINTER = ("\n**BUILT AND GATED (lane BUILD7, 2026-09-10).** `release/NifSkope.exe`\n"
               "14:04:34: `qmake` + `make -j2` RC=0 with these two sources and lane HKX1's\n"
               "and HKX2's in `NifSkope.pro`. The measured gate table is at the end of the\n"
               "\"Havok animation clips play on the open NIF's bones\" entry above; lane HKX5's\n"
               "own 23 gates were already green on `release/hkxwrite_dump.exe` and were not\n"
               "re-run, because linking the two files into the application changes neither\n"
               "translation unit. **Nothing here has been flown in Fallout 4.**\n")

    # The HKX1 entry's own "BUILD PENDING" state, which this build ends
    # (`nifskope-ww-resume-pending` rule 7: four places must stop saying "not built").
    old1 = ('**Build state, 2026-09-10 (lane DOCS2 checked it):** `release/NifSkope.exe` is\n'
            '03:57:46 and `src/hkxanim.cpp` is 05:10:14, so the reader is NOT in the built\n'
            'exe;')
    tail = cur[len(HEADER):].decode('utf-8')
    assert tail.count(old1) == 1, tail.count(old1)
    tail = tail.replace(old1, (
        '**Build state: BUILT (lane BUILD7, 2026-09-10 14:04:34).** The heading\'s\n'
        '"BUILD PENDING" is superseded -- `qmake` + `make -j2` both RC=0 and `hkxanim.o`\n'
        'is linked into `release/NifSkope.exe`; `release/hkxanim_dump.exe` was rebuilt at\n'
        '14:05:13 and gates 134/3, 29/0, 20/0 re-ran on it. As lane DOCS2 found it,\n'
        '`release/NifSkope.exe` was 03:57:46 and `src/hkxanim.cpp` 05:10:14, so the reader\n'
        'was NOT in the built exe;'))

    new = HEADER + (e2 + '\n' + BUILD_BLOCK + '\n' + e5 + '\n' + POINTER + '\n').encode('utf-8') \
        + tail.encode('utf-8')
    assert new.count(b'\r') == cr0, (new.count(b'\r'), cr0)
    assert new.endswith(cur[cur.index(b'\n## 2026-09-10 -- the water window'):]), 'the tail was disturbed'
    open(CH, 'wb').write(new)
    print('WW_CHANGES.md after:  %d bytes (+%d), CR=%d (unchanged)'
          % (len(new), len(new) - n0, new.count(b'\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""Lane BUILD7: append its two mistakes to the root MISTAKES.md.

Append-only, LF-only (measured: CR=0 before), and the CR/prefix bytes are
asserted unchanged afterwards.
"""
import os

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
P = os.path.join(REPO, 'MISTAKES.md')

TEXT = """

## 2026-09-10, lane BUILD7: a relative path handed to NifSkope made a gate fail, twice

**What was done.** `scratchpad/hkx2_20260910/PENDING.md`'s paste-able resume was
run exactly as written: `SRC=fixtures/human_male_vanilla.nif bash
tests/spells/hkxanim_play.sh`, and then
`CLIP=scratchpad/hkx1_20260910/clips/jog.hkx ... bash
scratchpad/hkx2_20260910/shots.sh`. The first reported **8 failures of 27** --
"0 bones matched (expected 78)". The second reported **1 distinct image of 4**,
which is gate (e)'s own refuter for "the pose never reached the rig".

**What was true instead.** Both paths are RELATIVE. The harnesses run under
Git-Bash, which does not get MSYS2's automatic argv/environment path
conversion, and `_harness.sh`'s `winpath()` only rewrites the `/e/...` form --
it passes a relative path through untouched. The NIF therefore never opened
(the log's first line reads `NIF:  (1 nodes)`, no name and one node) and the
clip was never found. Re-run with
`SRC=E:/Projects/NifskopeWildWastelandEdition/fixtures/human_male_vanilla.nif`:
**27 checks, 0 failures, 78 / 17 / 4**. Re-run with an absolute `CLIP`:
**4 distinct images of 4**. Neither number was a defect in the code under test.

**How it was found.** The failing mapping named 95 bones as unmatched against a
scene of "1 named nodes" -- a count that cannot come from a 139-node body -- so
the NIF, not the matcher, was the thing that had not loaded.

**The rule that prevents it.** Every path handed to `release/NifSkope.exe`, as
argv or in a `WW_*` variable, is an ABSOLUTE Windows path. A resume or brief
that quotes a repo-relative one is quoting a command that has never been run.
The `nifskope-ww-render-shot` skill already says this for `WW_RENDER_SHOT`
outputs ("Every `WW_*` output path is ABSOLUTE"); it is now written for INPUTS
too, and for argv, in that skill and in `ww-hkx-animation`.

## 2026-09-10, lane BUILD7: the WW_HKXANIM_CLIP hook throws the loader's refusal away

**What was done.** Nothing -- this is a finding, reported and not fixed
(`nifskope-ww-resume-pending` rule 6: the resuming lane's product is a verdict).

**What is true.** `src/nifskope_ui.cpp`'s `WW_HKXANIM_CLIP` arm does
`const QString hkxErr = hsc->hkx->load( hkxPath, &hkxAdded );` and then only
`if ( hkxErr.isEmpty() && !hkxAdded.isEmpty() )` selects the sequence. The
refusal string is never printed anywhere. A clip that does not load produces a
silent bind-pose picture and an exit code of 0, which is precisely how the
mistake above cost a whole four-render round before anyone looked at a file
name. The same hook is what lane HKX2's gate (e) runs on.

**The rule.** A hook that receives a refusal IN WORDS prints it (CONSTITUTION
10: a refusal states its reason in words). Candidate fix, for the director, not
landed here: one `qWarning`/stderr line, or a line in
`release/ww_camera_pin.log` beside the `grab` record.
"""


def main():
    b = open(P, 'rb').read()
    cr0, n0 = b.count(b'\r'), len(b)
    assert cr0 == 0, 'MISTAKES.md is no longer LF-only (CR=%d)' % cr0
    assert 'lane BUILD7' not in b.decode('utf-8'), 'a BUILD7 entry is already there'
    add = TEXT.encode('utf-8')
    assert add.count(b'\r') == 0
    open(P, 'ab').write(add)
    b2 = open(P, 'rb').read()
    assert b2[:n0] == b, 'the append was not append-only'
    assert b2.count(b'\r') == 0
    print('MISTAKES.md %d -> %d bytes, CR still 0, append-only OK' % (n0, len(b2)))


if __name__ == '__main__':
    main()

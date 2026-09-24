#!/usr/bin/env python3
"""Lane BUILD10 -- the mistakes ledger, spliced at the top of MISTAKES.md.

Newest at the top, LF-only, CR asserted 0 -> 0.  The file is re-read at write
time so a concurrent append by another lane is not clobbered.
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
P = os.path.join(ROOT, "MISTAKES.md")

ANCHOR = "Newest at the top.\n"

TEXT = """
## 2026-09-10 -- lane BUILD10 (WATER4, WATER5 and WATER6 built and gated)

1. **A path-relative helper was COPIED to the repo root and every one of its
   four runs failed silently-ish.** `scratchpad/water3_20260910/syn.sh` computes
   `ROOT="$(cd "$(dirname "$0")/../.." && pwd)"`, so a copy at the repo root
   resolves ROOT to `E:/Projects` and `g++` answered
   `src/watermark.cpp: No such file or directory` four times, `syntax rc=1`. The
   copy was made because `nifskope-ww-build-verify` says the throwaway script is
   `sx_$LANE.sh` -- but that instruction is about WRITING a fresh script, not
   about relocating one that computes its own root. It cost nothing (the build
   is the real gate and it ran a minute later) but it would have hidden a real
   compile error behind four identical "not found" lines. Rule: run a
   path-relative script IN PLACE, or read its root computation before moving it.
2. **A picture's framing was quoted from a docstring instead of measured, and
   the docstring is wrong.** `scratchpad/water3_20260910/make_pair.py` says the
   Charles pair is rendered at "1500x1000". The render hook honours
   `WW_RENDER_SIZE`'s WIDTH exactly and takes 59 px of window chrome off the
   HEIGHT, so `1500x1000` produces **1500x941** and does not reproduce the
   baseline, which is **1507x941** (`WW_RENDER_SIZE=1507x1000`). Three probe
   renders found it, and only because `make_pair_v4.py` ASSERTS that the BEFORE
   render is byte-identical to `water2_20260909/images/charles_flow.png` before
   it draws anything. Without that assert the pair would have shipped at a
   framing that merely looked the same. Rule: a "same framing" claim is a byte
   comparison in the script, never a sentence in a docstring.
3. **A Bash heredoc ate a Python edit for the fifth time in this tree.** The
   edit that rewrote `w6_patch.py`'s last entry was passed through
   `python - <<'PYEOF'`; the backslashes in the C-string anchors arrived halved
   and the `assert b.count(old)==1` refused, writing nothing. That is the
   assertion doing its job, but the minute was still paid. The remaining edits
   went through the Edit tool. Rule, restated: **no multi-line text with
   backslashes ever goes through a heredoc, including a one-off fix to a script
   that is itself about anchors.**

### Found in other lanes' work while building it (reported, NOT cured)

4. **`tests/spells/water_flow.sh` calls a green gate red.** Its loop does
   `grep -F "F8 the solve" | head -1`, and the harness prints an informational
   line `F8 the solve: 0.319 s, 1493 iterations, ...` immediately BEFORE
   `  ok   F8 the solve of body 3 runs under 1.0 s ...`. The grep takes the
   informational line, finds no leading `  ok `, and reports the gate red. Every
   gate loop of this shape needs `grep -E '^  (ok|FAIL) '` in it -- lane WATER6's
   own spell was written that way because of this.
5. **The same spell's floor cannot be met by its own lane's prediction.**
   `flow gates green: 17 (floor 18)`: there are 19 `F[1-8]` gates and lane
   WATER4 pre-registered TWO of them as expected red. 19 - 2 = 17. A floor and a
   prediction registered in the same document contradicted each other and nobody
   could see it until the first run.
6. **`tests/spells/water_mark.sh` runs the F5 and dye gates on a body they were
   not registered on.** Its model half picks body 2 (the marsh) as "the river"
   while F5 and the dye gates were registered on body 3 (the Charles), so it
   reports 8 failures where `water_flow.sh` -- which pins
   `WW_WATER_MARK_BODY=3` -- reports the 2 that were predicted. Four of those
   eight are one stated cause: body 2 and the body it drains into do not touch,
   so its dye has no mouth and 0 texels are dyed.
7. **A dye pin's per-point weight is never written** (lane WATER5,
   `WaterCurveDoc::writeTo`, `src/watercurves.cpp` ~684 fills `s.extra` for
   `Stroke` and `Pin` only), so the window's own W3 gate went red on a curve the
   json round-trips perfectly.
8. **The FIRST named body in any file `WaterMarkDoc` writes reads back
   nameless.** `encodeNames` packs only the non-empty names and `encodeTable`
   starts its running name offset at **0** (`src/watermark.cpp` 503), while
   `LodtFile::bodyName` spells offset 0 as "this body has no name"
   (`src/lodtfile.cpp` 3096). The name IS in the file -- the harness's own
   window printed it -- and the reader cannot see it. A sentinel value that is
   also a legal value is the whole bug, and it was invisible until a test named
   exactly one body.
"""


def main():
    b = open(P, "rb").read()
    cr = b.count(b"\r")
    a = ANCHOR.encode("utf-8")
    n = b.count(a)
    print("anchor count=%d CR=%d bytes=%d" % (n, cr, len(b)))
    assert n == 1, "the anchor is not unique"
    assert b.count(b"lane BUILD10 (WATER4, WATER5 and WATER6") == 0, "already spliced"
    t = TEXT.encode("utf-8")
    assert t.count(b"\r") == 0
    out = b.replace(a, a + t)
    assert out.count(b"\r") == cr
    open(P, "wb").write(out)
    print("wrote MISTAKES.md %d -> %d bytes, CR %d unchanged" % (len(b), len(out), cr))


if __name__ == "__main__":
    main()

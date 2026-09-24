#!/usr/bin/env python
"""Lane BUILD11: the second batch of MISTAKES.md entries -- what running four
never-executed gates for the first time actually found. Append-only, LF."""
import os

PATH = os.path.join(r"E:\Projects\NifskopeWildWastelandEdition", "MISTAKES.md")

TEXT = """
## 2026-09-10 -- lane BUILD11, part 2: what the never-run gates found on their first execution

Three gates in this round had never been executed, and running them found three
defects that are NOT what they appear to name (`ww-test-harness-add` s9 again).

6. **`hkxmodel_test.sh` (a) reads `NifModel`, and the .hkx document HAD
   loaded.** The obvious reading of "the tree's model is an HkxModel after an
   .hkx load -- FAIL, the tree's model is NifModel" is that the load failed. It
   did not: the harness's own `ok` check is the boolean `NifSkope::load()`
   emits, which is `HkxModel::loadFromFile()`'s return, and it was green.
   `load()` runs `tree->setModel( hkx )`, and then its very next statement,
   `emit completeLoading( ok, fname )`, reaches `NifSkope::onLoadComplete`,
   which calls `swapModels()` (`src/nifskope.cpp:7478`). `swapModels` knows two
   states -- `tree->model() == nif` -> park on `nifEmpty`, else -> put `nif`
   back -- so with the tree on `hkx` the else branch takes the view away. The
   `.kfm` route it was modelled on escapes this only because a .kfm has its own
   view, `kfmtree`, which `swapModels` names. **The rule:** a new document type
   that reuses an EXISTING view inherits every function that already owns that
   view. Before copying a route, grep for every assignment to the view
   (`grep -n "tree->setModel" src/`) -- there were seventeen -- and read what
   each one does with a model it does not recognise. Not fixed here: a build
   lane's product is a verdict (`nifskope-ww-resume-pending` s6).

7. **A picture gate went red for pixels the rule it guards puts there on
   purpose.** `skeleton_overlay.sh` gate (j) asks that every pixel the overlay
   drew be on the character; it counted 53 stray. All 53 are two round grey
   marks 7 px across, colour (134,139,145) -- the muted "not a bone" colour --
   which are the joint markers of two of the 19 marker-only nodes that lane
   SKELFIX's shipped rule KEEPS by design and says so in words. The gate was
   pre-registered before the rule was written and nobody re-read one against
   the other. **The rule:** when a lane's own rule changes what gets drawn, its
   pre-registered picture gate is re-read against the NEW rule in the same
   sitting -- a gate that contradicts the thing it guards is not a finding, it
   is a contradiction, and it costs a round to tell them apart.

8. **Two exact-equality gates on a GL framebuffer are FLAKY, and one run of
   them would have been believed.** `skeleton_overlay.sh` (c) "the render
   differs only in the overlay's pixels" and (e) "toggling off restores the
   off-render exactly" were run four times on ONE exe with one fixture:
   (c) 10, 0, 4, 0 pixels outside the mask and (e) 17, 0, 37, 2 pixels
   differing -- one of the four runs printing `27 checks, 0 failures, PASS`.
   Had the first run been the 0/0 one, this build would have reported a clean
   PASS and the instability would still be there. **The rule:** a check that
   compares two `grabFramebuffer()` results for EXACT equality states a
   tolerance, or settles the frame before each grab; and a first-ever gate run
   that produces a small non-zero count is repeated before it is reported as
   either a red or a green. Whether part of these counts is a real one-frame
   lag in the toggle rather than GL jitter is still unmeasured.

9. **A gate that prints a named SKIP can be hiding a crash rather than a
   missing fixture.** `animws.sh` gate (i) skipped because the default
   `10mmPistol.nif` has no `NiControllerSequence` -- honest, and exactly what
   `ww-test-harness-add` s7 asks for. Supplying a NIF that HAS one
   (`Meshes/Effects/TeleportInFXLight.nif`) does not turn the skip green: the
   harness dies inside gate (i) and writes no `N checks`, no `PASS`/`FAIL` and
   no `done` line at all. The NIF is fine -- it opens and renders alone in the
   same exe. So the suite's `57 checks, 0 failures, PASS` was reported by a run
   in which the one branch that could crash it was skipped. **The rule:** a
   SKIP is not just "never a pass", it is UNMEASURED CODE -- discharge every
   skip with a fixture that exercises it before quoting the suite's total, and
   say in the report which totals were produced with a branch skipped.
"""


def main():
    b = open(PATH, "rb").read()
    assert b.count(b"\r") == 0
    assert b"lane BUILD11, part 2" not in b
    n0, lf0 = len(b), b.count(b"\n")
    add = TEXT.encode("utf-8")
    assert add.count(b"\r") == 0
    open(PATH, "wb").write(b + add)
    c = open(PATH, "rb").read()
    print("MISTAKES.md bytes %d -> %d (+%d)  CR %d  LF %d -> %d"
          % (n0, len(c), len(c) - n0, c.count(b"\r"), lf0, c.count(b"\n")))


if __name__ == "__main__":
    main()

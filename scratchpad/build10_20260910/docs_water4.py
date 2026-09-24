#!/usr/bin/env python3
"""Lane BUILD10 -- WATER4's documents stop saying BUILD PENDING.

Every edit is an anchor that must match EXACTLY ONCE in the file's real bytes;
every file's CR count is asserted unchanged (WW_CHANGES.md is MIXED at 19,020
CR and its top entries are LF-only, so every inserted line here is LF-only).
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

BUILT = ("**BUILT AND RUN 2026-09-10 by lane BUILD10** (`release/NifSkope.exe` "
         "15:52:46; 47 checks / 2 failures on the Charles, and both failures are "
         "the two gates the lane pre-registered as expected red)")

EDITS = [
    # ------------------------------------------------------------ WW_CHANGES
    ("WW_CHANGES.md",
     "## 2026-09-10 - potential flow inside each water body, and dye (lane WATER4)"
     " -- **BUILD PENDING** -- written and syntax-checked (`g++ -fsyntax-only` with"
     " the real `Makefile.Release` flags, rc=0 on all four files), NOT compiled,"
     " NOT run; the numbers below that come from the numpy prototype say so\n",
     "## 2026-09-10 - potential flow inside each water body, and dye (lane WATER4)"
     " -- " + BUILT + "\n"),

    ("WW_CHANGES.md",
     "**Gates, pre-registered before the code** (`lane_water4_report.md` section 0)\n"
     "and run so far only in the numpy prototype: F1, F3, F4, F6, F7 green; **F2's\n"
     "island-bank gate FAILS as registered** (12.0 mean / 22.4 max against 5 / 15:\n"
     "the face-averaged velocity at a staircase bank cell, R-independent, one ring\n"
     "in 4.0 degrees); **F5's p99 FAILS as registered** (7.0 against 5 at 8 passes;\n"
     "patches 0 of 39, seams 0.26 percent of 2.97). Neither gate was moved. The C++\n"
     "has not run: P0-P8, `water_mark.sh`, `lodl_water.sh`, `water_flow.sh`, the\n"
     "render-hook picture pair and the dye picture are all owed to the build\n"
     "(`scratchpad/water4_20260910/PENDING.md`).\n",

     "**Gates, pre-registered before the code** (`lane_water4_report.md` section 0),\n"
     "RUN IN THE C++ 2026-09-10 on `release/NifSkope.exe` 15:52:46. On the Charles\n"
     "(`water_flow.sh`, `WW_WATER_MARK_BODY=3`): **47 checks, 2 failures -- exactly\n"
     "the two the lane pre-registered as expected red**. F1, F3, F4, F6, F7, F8 and\n"
     "the whole dye chain green; **F2's island-bank gate fails as registered**\n"
     "(mean 12.01 / max 22.40 degrees against 5 / 15 over 48 bank texels -- the\n"
     "prototype's number to two decimals, so it is the method's limit and not a\n"
     "transcription); **F5's p99 fails as registered** (8.44 against 5). F5's other\n"
     "three are green: **0 seam-bounded patches where the disc fill had 39**, seams\n"
     "0.422 percent of 2.97, mean direction cos 1.000, R 0.794. Neither gate was\n"
     "moved. The independent decoder (`disc_metric.py`, WATER2's reader, no writer\n"
     "code) reads the same 0 patches and the same 8.44 out of the saved file. F8:\n"
     "the Charles solves in **0.319 s**, 1,493 iterations, residual 9.26e-10.\n"
     "The dye: 17,218 texels, every one naming body 3 as its source and every one\n"
     "lying on body 1 -- the body the river drains INTO; mean weight 230.7 of 255\n"
     "within L/2, 34 beyond 3 L (gate 32, slack 48); the dyed file re-opens through\n"
     "the reader, save-reopen-save is byte-identical (39,235,147 bytes both ways)\n"
     "and undo reproduces the unmarked file byte for byte. `lodl_water.sh` PASS\n"
     "(33 ok, RESULT PASS), `lodl_open.sh` 23/0 PASS, `water_mark.sh` dock 20/0.\n"
     "\n"
     "**Three reds that are NOT the solver**, reported and not re-pinned.\n"
     "`water_mark.sh`'s model half runs the F5 and dye gates on **body 2**, the\n"
     "held-out marsh, not on the Charles: p99 9.84, seams 0.840 percent, cos 0.763,\n"
     "and its four dye gates fail for one cause the instrument states itself --\n"
     "*\"body 2 and the body it drains into (19) do not touch, so its dye has no\n"
     "mouth to leave by\"* (0 receiving fields, so 0 dyed texels). `water_flow.sh`\n"
     "has two defects of its own: it greps `F8 the solve` and matches the\n"
     "INFORMATIONAL line above the ok line, so it calls a green gate red; and its\n"
     "floor of 18 green F-gates cannot be met while two F-gates are pre-registered\n"
     "as red (19 - 2 = 17). Both are one-line repairs in `tests/spells/water_flow.sh`\n"
     "and neither was made here: a resuming lane's product is a verdict, not a cure.\n"
     "\n"
     "**Pictures** (`scratchpad/water4_20260910/images/`):\n"
     "`charles_flow_pair_v4.png` -- the generator's one direction beside the solve\n"
     "at lane WATER2's framing, proved to BE that framing because the BEFORE render\n"
     "taken today is byte-identical to `water2_20260909/images/charles_flow.png`;\n"
     "`charles_dye_mouth.png` -- the dye plane at the mouth, texel level, out of the\n"
     "file's own bytes through the independent decoder; `flow_channel_f1.png` --\n"
     "gate F1's channel with the speed as brightness.\n"),

    # --------------------------------------------------------- the BTD format
    ("docs/LODGEN_BTD_FORMAT.md",
     "`dyePlaneOffset`. **BUILD PENDING** -- written and syntax-checked "
     "(`g++ -fsyntax-only` with the real `Makefile.Release` flags, rc=0 on all four "
     "files), NOT compiled, NOT run; the numbers below that come from the numpy "
     "prototype say so.\n",
     "`dyePlaneOffset`. " + BUILT + ". The plane was written, read back through the "
     "reader (8,649 of 8,649 sampled texels agree with the document), and removed "
     "again by undo byte for byte; the file with it is 39,235,147 bytes against the "
     "unmarked 38,612,038, and save-reopen-save reproduces it exactly.\n"),

    # ------------------------------------------------------------ spec_water
    ("scratchpad/specs_20260909/spec_water.md",
     "### 3.7b Dye (lane WATER4, **BUILD PENDING** -- written and syntax-checked "
     "(`g++ -fsyntax-only` with the real `Makefile.Release` flags, rc=0 on all four "
     "files), NOT compiled, NOT run; the numbers below that come from the numpy "
     "prototype say so)\n",
     "### 3.7b Dye (lane WATER4, " + BUILT + ")\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "> **REBUILT by lane WATER4 (2026-09-10), **BUILD PENDING** -- written and "
     "syntax-checked (`g++ -fsyntax-only` with the real `Makefile.Release` flags, "
     "rc=0 on all four files), NOT compiled, NOT run; the numbers below that come "
     "from the numpy prototype say so.** The harmonic\n",
     "> **REBUILT by lane WATER4 (2026-09-10), " + BUILT + ".** The harmonic\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "### Lane WATER4 — the potential-flow solve and the dye (**BUILD PENDING** -- "
     "written and syntax-checked (`g++ -fsyntax-only` with the real "
     "`Makefile.Release` flags, rc=0 on all four files), NOT compiled, NOT run; the "
     "numbers below that come from the numpy prototype say so)\n",
     "### Lane WATER4 — the potential-flow solve and the dye (" + BUILT + ")\n"),

    # the gate table gains the C++ column it was built to carry
    ("scratchpad/specs_20260909/spec_water.md",
     "| gate | pass condition | prototype (numpy, `scratchpad/water4_20260910/flow_proto.py`) |\n"
     "|---|---|---|\n",
     "| gate | pass condition | prototype (numpy, `scratchpad/water4_20260910/flow_proto.py`) | **the C++, run 2026-09-10 (lane BUILD10), body 3** |\n"
     "|---|---|---|---|\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "| F1 continuity | a channel that narrows to half its width: speed ratio 2.00 "
     "+- 5 percent; flux through 10 sections within 3 percent | 2.0000; 1.6e-10 |\n",
     "| F1 continuity | a channel that narrows to half its width: speed ratio 2.00 "
     "+- 5 percent; flux through 10 sections within 3 percent | 2.0000; 1.6e-10 | "
     "2.0000 (460 iterations, residual 9.3e-10); 1.63e-10 -- **green** |\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     " | 0.5000 / 0.5000; 3e-12; 3.9e-4; **12.0 / 22.4 deg -- FAILS as registered**, "
     "R-independent (8, 16, 32 all ~12), one ring in 4.0, at 2R 0.8: the "
     "face-averaged velocity at a STAIRCASE bank cell, not the solve |\n",
     " | 0.5000 / 0.5000; 3e-12; 3.9e-4; **12.0 / 22.4 deg -- FAILS as registered**, "
     "R-independent (8, 16, 32 all ~12), one ring in 4.0, at 2R 0.8: the "
     "face-averaged velocity at a STAIRCASE bank cell, not the solve | 0.5000 / "
     "0.5000 (526 it, 9.6e-10); 3.05e-12; 3.93e-4 over 416 texels; **12.01 / 22.40 "
     "deg over 48 bank texels -- RED as registered, the prototype's own number** |\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "| F3 lake, no outlet | speed exactly 0 | 0 |\n",
     "| F3 lake, no outlet | speed exactly 0 | 0 | 0 exactly, 0 iterations -- **green** |\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "| F4 lake, one outlet | 0 texels point away; every streamline (Pollock's, on "
     "the face fluxes) reaches it | 0 of 3225; 40 of 40 |\n",
     "| F4 lake, one outlet | 0 texels point away; every streamline (Pollock's, on "
     "the face fluxes) reaches it | 0 of 3225; 40 of 40 | 0 of 3,225 (mean cosine "
     "0.914, 252 it); 40 of 40 -- **green** |\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "| F5 the Charles | 0 seam-bounded patches; p99 adjacent jump < 5 deg; seams < "
     "0.5 percent; cos to the mouth > 0.9; R stated | patches 0; **p99 7.0 at 8 "
     "passes (FAILS as registered by 2 deg)**; seams 0.26 percent; R 0.79 |\n",
     "| F5 the Charles | 0 seam-bounded patches; p99 adjacent jump < 5 deg; seams < "
     "0.5 percent; cos to the mouth > 0.9; R stated | patches 0; **p99 7.0 at 8 "
     "passes (FAILS as registered by 2 deg)**; seams 0.26 percent; R 0.79 | patches "
     "**0** (39 before); **p99 8.44 -- RED as registered**; seams 0.422 percent; "
     "cos 1.000; R 0.794. The independent decoder reads the same numbers off the "
     "saved file. On the held-out body 2: 9.84 / 0.840 percent / cos 0.763 |\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "| F6 plume | 1/8 length and direction predicted before the dye, measured "
     "after, within 10 percent / 9 deg | 98.2 vs 96 texels; 0.0 vs 0.0 deg |\n",
     "| F6 plume | 1/8 length and direction predicted before the dye, measured "
     "after, within 10 percent / 9 deg | 98.2 vs 96 texels; 0.0 vs 0.0 deg | 98.2 vs "
     "96 texels; 0.0 vs 0.0 deg -- **green** |\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "| F7 dye pin | 1/2 at L, 1/8 at 3L, 0 upstream | 0.5000; 0.1250; 0 |\n",
     "| F7 dye pin | 1/2 at L, 1/8 at 3L, 0 upstream | 0.5000; 0.1250; 0 | 0.5000; "
     "0.1250; 0 -- **green** |\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "| F8 cost | the Charles under 1.0 s, residual < 1e-8 | numpy 1.5 s at 1,565 CG "
     "iterations (the C++ is what is gated) |\n",
     "| F8 cost | the Charles under 1.0 s, residual < 1e-8 | numpy 1.5 s at 1,565 CG "
     "iterations (the C++ is what is gated) | **0.319 s**, 1,493 iterations, residual "
     "9.26e-10 -- **green** (`water_flow.sh` calls it red: its grep matches the "
     "informational line above the ok line) |\n"),

    ("scratchpad/specs_20260909/spec_water.md",
     "| dye round trip | the plane is written only with a dye mark, reads back "
     "through `LodtFile::dyeWordAt`, and undo is byte-identical | C++ only |\n",
     "| dye round trip | the plane is written only with a dye mark, reads back "
     "through `LodtFile::dyeWordAt`, and undo is byte-identical | C++ only | 8,649 of "
     "8,649 sampled texels agree; save-reopen-save byte-identical; undo 0 bytes "
     "differ -- **green** |\n"),
]


def main():
    changed = []
    for path, old, new in EDITS:
        full = os.path.join(ROOT, path)
        cur = dict((f, b) for f, b in changed)
        b = cur.get(full) or open(full, "rb").read()
        cr0 = b.count(b"\r")
        a = old.encode("utf-8")
        c = b.count(a)
        print("%-42s count=%d CR=%d" % (path, c, cr0))
        if c != 1:
            print("REFUSED: that anchor matches %d times in %s" % (c, path))
            return 1
        b2 = b.replace(a, new.encode("utf-8"))
        assert b2.count(b"\r") == cr0, path
        changed = [(f, bb) for f, bb in changed if f != full] + [(full, b2)]
    for full, b in changed:
        old = open(full, "rb").read()
        open(full, "wb").write(b)
        print("wrote %-42s %d -> %d bytes, CR %d -> %d"
              % (os.path.relpath(full, ROOT), len(old), len(b),
                 old.count(b"\r"), b.count(b"\r")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

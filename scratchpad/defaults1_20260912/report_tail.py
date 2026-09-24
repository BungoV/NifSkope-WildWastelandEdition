"""DEFAULTS1: fold the last three findings into the report.

1. section 4 gains the panel picture row
2. section 3 gains the SECOND build (the GUI harness re-base)
3. section 5 gains the registry finding: the one-shot migration has ALREADY run
   on bungo's own saved panel, so his rows hold the ruled numbers now

Refusing: every anchor exactly once. LF asserted.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/defaults1_20260912/lane_defaults1_report.md'

SUBS = []

SUBS.append((
    "| `iii_blend_edges_crop.png` | 810x480, 26,148 B | the same 96x96 texels at 4x at (96,160): the hard quadrant seams in `off`, dissolved in `quadrant` | — |\n",
    "| `iii_blend_edges_crop.png` | 810x480, 26,148 B | the same 96x96 texels at 4x at (96,160): the hard quadrant seams in `off`, dissolved in `quadrant` | — |\n"
    "| `iv_panel_rows.png` | 1008x3029, 171,211 B | the LOD Generation settings column, `WW_LODGEN_SHOT_FULL`, rung beside new — the same grab on both exes | 2,743 px differ, bbox (30,518,218,593) |\n"))

SUBS.append((
    "The two renders that came back wrong first",
    "`iv_panel_rows.png` is the picture that says the least and is the most\n"
    "honest about it. The two columns differ in ONE place — the identity rows,\n"
    "where the rung reads \"Identity channels and manifests\" ticked and the new exe\n"
    "reads \"Identity channels\" unticked. The five land/road rows read the SAME\n"
    "numbers on both sides, because those rows come from saved settings and the\n"
    "one-shot migration has already moved them (see section 5). A picture that\n"
    "showed the land rows moving would have had to be taken against a wiped\n"
    "registry, which is bungo's, so it was not taken.\n\n"
    "The two renders that came back wrong first"))

SUBS.append((
    "3. **The `A <block> -1 <lodm>` hole in the manifest is latent, not live.**",
    "3. **His saved panel has ALREADY been migrated, on this machine, today.**\n"
    "   The one-shot migration runs when the panel is built, and the panel was\n"
    "   built by this lane's own GUI harness runs. Read back out of the registry\n"
    "   after them:\n\n"
    "   ```\n"
    "   HKCU\\Software\\NifTools\\NifSkope 2.0\\LodGeneration\n"
    "     landHex 256   landWarp 341   landMipBias -0.22   landGuide 5\n"
    "     roadGroundPaint 0   landSample 0   roadDetail 1   defaults1Applied true\n"
    "   ```\n\n"
    "   That is the intended behaviour and it is what keeps his panel and the\n"
    "   command line baking the same thing — but it is a change on his machine,\n"
    "   made by a lane, so it is said out loud rather than left to be discovered.\n"
    "   If he wants the old numbers back in the panel he types them in, or deletes\n"
    "   `defaults1Applied` and the five keys.\n"
    "4. **The `A <block> -1 <lodm>` hole in the manifest is latent, not live.**"))

SUBS.append((
    "The seven translation units are exactly `src/lodgen.h`'s dependency fan plus the\n"
    "two files I edited that do not include it through another header. No other\n"
    "lane's source was pulled in, so this exe IS the rung plus this lane's diff.\n",
    "The seven translation units are exactly `src/lodgen.h`'s dependency fan plus the\n"
    "two files I edited that do not include it through another header. No other\n"
    "lane's source was pulled in, so this exe IS the rung plus this lane's diff.\n"
    "\n"
    "### The second build, 21:52:25 — the GUI harness re-base\n"
    "\n"
    "One of the panel self-test's 121 checks had borrowed the identity box's ticked\n"
    "state instead of forcing it (section 1, the GUI half). Fixing that is a change\n"
    "to `src/nifskope_ui.cpp` inside the `WW_LODGEN_TEST` block only, so it needed a\n"
    "second build.\n"
    "\n"
    "| step | gate | number |\n"
    "|---|---|---|\n"
    "| game check, its own command, read before the build | `tasklist` | nothing running |\n"
    "| `make -j2`, its own exit code | `BUILD-RC=0` | 0 |\n"
    "| exe | `release/NifSkope.exe` | **2026-09-12 21:52:25, 22,280,192 B** |\n"
    "| first two bytes | must be `MZ` | `MZ` |\n"
    "| translation units compiled | read from the build log | **1**: `nifskope_ui.o` |\n"
    "| relinks | counted in the log | **1** |\n"
    "| exe newer than the changed source | `test -nt src/nifskope_ui.cpp` | yes |\n"
    "| stylesheet | `cmp res/style.qss release/style.qss` | in step |\n"
    "| the bake gates on the NEW exe | `PHASES=abce` re-run, `gate_recheck.txt` | **22 checks, 0 failures PASS** |\n"
    "| the panel self-test on the new exe | `lod_generation.sh` | **121 checks, 0 failures PASS** (rung: 121 / 0) |\n"
    "\n"
    "Phase (d) was not re-run on the 21:52:25 exe: it is 15 minutes of baking and the\n"
    "diff between the two exes is one block of harness-only code that no bake path\n"
    "reaches. Its numbers in section 1 are the 20:42:12 exe's, and this paragraph is\n"
    "here so that is not read as more than it is.\n"))

raw = open(P, 'rb').read()
text = raw.decode('utf-8')
for old, new in SUBS:
    c = text.count(old)
    if c != 1:
        print('REFUSED: anchor found %d times: %r' % (c, old[:80]))
        sys.exit(2)
    text = text.replace(old, new)
out = text.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('%d -> %d bytes  LF %d  CR %d' % (len(raw), len(out), out.count(b'\n'), out.count(b'\r')))

#!/usr/bin/env python3
"""Lane BUILD9, 2026-09-10 -- splice this round's MISTAKES.md entries.

Newest at the top, immediately after the six-line header block. MISTAKES.md is
LF-only (CR 0); the script asserts that. Default --check, --apply writes.
"""
import sys

PATH = "MISTAKES.md"
AFTER = "Newest at the top.\n"
MARK = "## 2026-09-10 -- a DEFINES change does not make an object stale (lane BUILD9)"

BLOCK = """
## 2026-09-10 -- a DEFINES change does not make an object stale (lane BUILD9)

**What was done.** Lane HKX3's hook-up adds `DEFINES += WW_HKXANIM_UI` to
`NifSkope.pro`, and every clip-shaped line of `src/ui/widgets/timeline.cpp` is
behind `#ifdef WW_HKXANIM_UI`. I ran qmake and then make, as
`nifskope-ww-resume-pending` says to, and treated the green object-staleness
sweep as sufficient.

**What was true instead.** `make` compares MTIMES, not FLAGS. `timeline.o` was
15:19:10, newer than its source, so make kept it -- compiled WITHOUT the define,
with `TimelineWidget::setGLView` compiled out -- while the freshly built
`nifskope_ui.o` called it. The link died on one undefined reference and, on the
way out, DELETED `release/NifSkope.exe`, so for four minutes there was no
application on disk at all. Six objects had to be removed by hand
(`timeline`, `timelineedit`, `timelineviews`, `main`, `nifskope`, `meshtools`)
before the rebuild succeeded.

**How it was found.** The link error named exactly one symbol, and `ls -l` on
`GeneratedFiles/.obj/timeline.o` showed the previous build's timestamp.

**The rule.** `nifskope-ww-build-verify`'s "a successful build is not a
consistent one" covers a changed HEADER and not a changed FLAG. After any change
to `DEFINES` or `CXXFLAGS` in the `.pro`, delete the objects of every translation
unit that reads the macro before running make: `grep -rln <MACRO> src/` for the
sources, then every `.cpp` that includes any header on that list. A new
`DEFINES` line makes those objects stale even though every mtime says otherwise.
Amendment applied to the skill in both trees.

## 2026-09-10 -- four gates that had never been executed, and what each one was really measuring (lane BUILD9, from lanes FILESTAB and HKX3)

**What was done.** Three lanes pre-registered gates, wrote them, and ended BUILD
PENDING without running any of them. Running them for the first time produced
four reds, and none of the four was the defect it appeared to name.

**What was true instead**, one at a time:

1. **`mktemp -d` made a gate measure the machine.** `tests/spells/files_tab.sh`
   built its loose fixture tree in `/tmp/tmp.XXXX`, and `winpath()` in
   `tests/spells/_harness.sh` only rewrites DRIVE-style `/x/...` paths -- so the
   Windows binary was handed the literal string `/tmp/tmp.XXXX/Data`, could not
   open it, counted it into `nifBrowserSkippedResources` (silently, deliberately:
   a bad resource path is not an application error) and indexed the archive
   alone. The census read `.nif 0 | .bto 0 | .btr 0 | .hkx 14939` and the floor
   went red. Fixed in the driver: the tree is built under the repo and the script
   refuses if `winpath` does not return an `X:/` path.
2. **A floor that could not fire.** The panel-style floor blanked
   `tools.first()`'s tooltip and required the untipped count to rise by one.
   `tools.first()` was already untipped, so it could not rise, and the floor
   reported red for a reason unrelated to what it guards. Fixed: the victim is
   chosen by the property under test -- the first button that currently HAS a
   tooltip.
3. **A count that could not be acted on.** "1 of 139 nodes differ" and "2 without
   a tooltip" each cost a rebuild to turn into a lead. With the names printed:
   the node is `PipboyBone`, which the fixture drives with its own
   `NiTransformController` (`[72]` / `[73]` in `-no-gui list`), and the two
   buttons are `QLineEditIconButton` -- Qt's own clear buttons inside the two
   search fields. Both reds are then explained and neither is the defect the
   gate's wording suggested.
4. **A floor that needs keyboard focus, in a window that is never activated.**
   `QWidget::hasFocus()` is false in an inactive window however many times
   `setFocus()` is called, and a WW harness window is deliberately never
   activated (`WW_WINDOW_AT`, no `raise()`). The wheel guard blocks the wheel
   exactly while `!hasFocus()`, so the "and steps it once focused" half can never
   be exercised. Measured, not guessed: the harness now prints
   `hasFocus no, focusWidget <none>, window active no` beside the result.

**The rule.** A pre-registered gate is a DRAFT until it has been executed once;
its own defects are found by running it, not by reading it. Every FAIL prints
the NAME of what failed beside its count. Every floor chooses its victim by the
property being tested. And a fixture path that reaches the Windows binary is
asserted to be a Windows path. All of this is now the skill
`ww-test-harness-add`, written this session in both trees.

Two of the four were repaired (1 and 2, both instrument defects) and two were
left RED with their causes measured (the Qt clear buttons and the scene-time
comparison), because narrowing a gate's population or changing what it compares
AFTER seeing its numbers is what CONSTITUTION rule 1 forbids.

## 2026-09-10 -- two resume figures that were wrong, and one that could never have been right (lane BUILD9, from lanes HKX3 and FILESTAB)

**What was done.** Two checks copied out of a PENDING resume were run as written.

**What was true instead.** `scratchpad/hkx3_20260910/PENDING.md` says
`grep -c "lane HKX3" NifSkope.pro src/nifskope_ui.cpp` should print 1 and 7. It
prints 1 and 6, and 6 is right: six of the nine edits reach that file and each
carries exactly one marker. The same file's step 2 says to check
`grep -c "TimelineSeqBox" release/NifSkope.exe` is at least 1 as proof that
`WW_HKXANIM_UI` reached the compiler. That check can never pass:
`QStringLiteral` compiles to UTF-16, so an ASCII grep finds 0 in an exe that
contains the string twice (`b.count(s.encode('utf-16-le'))` finds both).

**How it was found.** By running them and not accepting the answers.

**The rule.** A number in a resume is a prediction by a lane that could not run
it. Re-derive it rather than trusting it, and when a proof-of-flag check is
needed use `grep -c <MACRO> Makefile.Release`, not a string search of the
binary. Recorded in `ww-test-harness-add` section 8.

"""


def main():
    apply = "--apply" in sys.argv
    raw = open(PATH, "rb").read()
    cr0, lf0, n0 = raw.count(b"\r"), raw.count(b"\n"), len(raw)
    text = raw.decode("utf-8")
    if cr0 != 0:
        print("REFUSED: MISTAKES.md is expected to be LF-only; it has %d CR." % cr0)
        return 1
    if text.count(AFTER) != 1:
        print("REFUSED: the header line was not found exactly once.")
        return 1
    if MARK in text:
        print("REFUSED: this block is already in the file.")
        return 1
    at = text.index(AFTER) + len(AFTER)
    out = (text[:at] + BLOCK + text[at:]).encode("utf-8")
    print("%s bytes %d -> %d   CR %d -> %d   LF %d -> %d"
          % (PATH, n0, len(out), cr0, out.count(b"\r"), lf0, out.count(b"\n")))
    if out.count(b"\r") != 0:
        print("REFUSED: a CR appeared. Nothing written.")
        return 1
    if not apply:
        print("--check: clean. Nothing written.")
        return 0
    open(PATH, "wb").write(out)
    print("--apply: written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

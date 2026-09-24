"""Append section 11 to the repo tree's nifskope-ww-resume-pending skill.
Two things lane BUILD12 had to work out that the skill does not say, and both
are about the skill's OWN section 5 helper and section 8 trap."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P = os.path.join(ROOT, ".claude", "skills", "nifskope-ww-resume-pending", "SKILL.md")

TEXT = """
## 11. The BEFORE picture, and the env-var leak in the chain (2026-09-10, lane BUILD12)

Two traps in this page's own machinery, both paid for on one resume.

**A pending lane's NEW harness cannot photograph the OLD state.** WATER7 owed
bungo a before/after of the top of the window and its resume said to grab the
"before" with the gate it had just written. That gate drives `WW_WATERUI_TEST`,
which does not exist in the exe on disk: run against it, the harness writes no
log, no PNG, and the spell exits 1 -- and the moment the build lands the old
state is gone for good. **Take the before picture with a SIBLING spell that
already photographs the same region and is already in the exe** (here BUILD9's
`ui_align.sh`, `SHOT=` a path under your own scratchpad), run BEFORE the
hook-up is applied, and say in the report which spell took which half. Its
geometry dump doubles as the numeric baseline for the same rows, which is what
turns "the top did not grow" from a claim into a comparison: `tMode` 33 px at
top 36 before, 35 px at top 35 after, search row at top 70 in both.

**`VAR=x run_helper` LEAKS.** Section 5's chain helper is a shell FUNCTION, and
bash keeps a variable assignment that prefixes a function call in the
environment AFTER the function returns. A chain written as

```bash
SHOT="$OUT/a.png" run water_ui 240 bash tests/spells/water_ui.sh
...
run ui_align 300 bash tests/spells/ui_align.sh      # still sees SHOT
```

hands `ui_align.sh` the first gate's `SHOT` and quietly overwrites the picture
that was the whole point of the run. Put the assignment on the CHILD instead --
`run water_ui 240 env SHOT=... bash tests/spells/water_ui.sh` -- and give every
other picture-taking spell its own explicit `env SHOT=`, because their defaults
point at some previous lane's scratchpad folder.
"""


def main():
    before = open(P, "rb").read()
    cr, n = before.count(b"\r"), len(before)
    add = TEXT.encode("utf-8")
    assert b"\r" not in add and cr == 0
    assert before.endswith(b"\n")
    open(P, "wb").write(before + add)
    after = open(P, "rb").read()
    assert after.startswith(before) and len(after) == n + len(add)
    assert after.count(b"\r") == cr
    print("SKILL.md %d -> %d bytes, CR %d" % (n, len(after), after.count(b"\r")))


main()

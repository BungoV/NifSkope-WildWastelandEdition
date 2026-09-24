#!/usr/bin/env python
"""Lane BUILD11: append this lane's MISTAKES.md entries. Append-only, LF, CR asserted 0."""
import os

REPO = r"E:\Projects\NifskopeWildWastelandEdition"
PATH = os.path.join(REPO, "MISTAKES.md")

TEXT = """
## 2026-09-10 -- lane BUILD11 (SKELFIX + HKXEDIT1 + HKXEDIT2 built as one)

1. **A hook-up script asserted the CR count UNCHANGED on a CRLF file, and died
   half-applied.** `scratchpad/hkxedit1_20260910/hookup.py --apply` wrote
   `NifSkope.pro` and `src/nifskope.h`, then raised
   `AssertionError: CR count moved in ...\\src\\nifskope.cpp` and stopped, leaving
   two of four files edited. What was true instead: `src/nifskope.cpp` is mixed
   and the four regions that hook-up edits are CRLF, so the script converts its
   inserted text to CRLF and the CR count MUST GROW by exactly the CRs of that
   text. `ww-anchored-hookup` section 1 says precisely that; the script asserted
   equality instead, which is the LF-file rule applied to a CRLF file. Found by
   the traceback, then by the marker counts (`nifskope.cpp` 0, `nifskope_ui.cpp`
   0) which showed which files had not been written. **The rule:** a hook-up's
   CR assertion is `cr_after == cr_before + sum(CRs of inserted text)`, never
   `==`, on ANY file -- it reduces to the equality on an LF file by itself. And
   a script that writes file by file states, before it writes anything, that a
   partial application is possible -- or writes all files or none.

2. **A hook-up's tier-2 anchor was a line its OWN base edit inserts.**
   `scratchpad/hkxedit2_20260910/hookup.py` refused with
   `NifSkope.pro after NO anchor matches once: [('DEFINES += WW_HKXCLIP_CANON', ...0)]`.
   Its TIER2 edit anchors on the `DEFINES += WW_HKXCLIP_CANON` line that its base
   edit #3 adds, and both `--check` and `--apply` resolve every anchor against the
   bytes on disk before anything is written -- so the anchor can never count 1 in
   one pass. `PENDING.md` predicted "16 anchors"; the measured truth is 15 in pass
   one and the 16th only after pass one has landed. Found on the first `--check`.
   **The rule:** an anchor names text that exists BEFORE the script runs. An edit
   that depends on another edit's output is a SECOND PASS, declared as one, with
   its own check -- and a resume's anchor count for such a script is two numbers,
   not one.

3. **Three hook-ups inserted CALLS into existing files and no INCLUDES beside
   them.** After all three landed, `sx_BUILD11.sh` on the two hooked-up files
   gave five errors of one kind: `invalid use of incomplete type 'class
   AnimWorkspace'` (nifskope.cpp:7807-7808, HKXEDIT2's select() edit),
   `'class QUndoGroup'` twice (nifskope_ui.cpp:23699/23704, HKXEDIT2's
   Undo/Redo actions) and `'class HkxModel'` (nifskope_ui.cpp:24298, the
   WW_ANIMWS_HKXMODEL branch reading `hkx->undoStack`). Each type was only the
   forward declaration the header carries. **The rule:** an anchored hook-up's
   edit table lists the INCLUDE its inserted code needs as its own edit, in the
   same table -- a forward declaration in the header is what let the new files
   compile alone and is exactly why the existing file will not. And the syntax
   pass runs AFTER the hook-up is applied, not only on the lane's new files:
   HKXEDIT1 and HKXEDIT2 each syntax-checked their own sources green and could
   not have seen any of these.

4. **My own: a Python dependency-reader went through a Bash heredoc and died on
   a backslash.** `python - <<'PYEOF'` with `line.rstrip().endswith('\\\\')` in it;
   bash halved the backslash and Python raised
   `SyntaxError: EOL while scanning string literal`. This is the ledger's
   most-repeated entry (lanes CARDFINAL, COMMIT, DOCS2, HKX1, WATER4, the
   director), and repeating an entry already in the file is its own entry
   (CONSTITUTION rule 2). Cost: one round trip. **The rule, again:** anything
   carrying a backslash is a FILE written with the Write tool and run by path.

5. **bungo opened NifSkope 7 minutes after the lane's entry guard said rc=1.**
   `tasklist | grep -i -E "Fallout4|NifSkope"` printed `rc=1` at ~16:58; his
   window (pid 700, no `--port`, so his and not a harness) started **17:05:44**,
   and the build's own in-chain guard caught it a minute later and renamed the
   exe aside to `release/NifSkope_inuse_700.exe` -- which is why the link
   succeeded instead of dying on `Permission denied`. Not a mistake in the
   build; recorded because it is the second measured instance of the fact
   `nifskope-ww-build-verify` states ("a guard whose answer is only echoed is
   not a guard"), and because it has a SECOND consequence nobody had written
   down: the in-app harnesses cannot run either, since one NifSkope instance is
   the rule and every exe launch wants `rc=1`. A lane that holds the build slot
   should expect his window to appear mid-lane and should order its work so the
   file-based gates run first.
"""


def main():
    b = open(PATH, "rb").read()
    assert b.count(b"\r") == 0, "MISTAKES.md is not LF-only"
    n0, lf0 = len(b), b.count(b"\n")
    add = TEXT.encode("utf-8")
    assert add.count(b"\r") == 0
    assert b"lane BUILD11" not in b, "already appended"
    open(PATH, "wb").write(b + add)
    c = open(PATH, "rb").read()
    print("MISTAKES.md bytes %d -> %d (+%d)  CR %d  LF %d -> %d"
          % (n0, len(c), len(c) - n0, c.count(b"\r"), lf0, c.count(b"\n")))


if __name__ == "__main__":
    main()

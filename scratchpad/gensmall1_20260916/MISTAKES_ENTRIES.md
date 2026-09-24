## 2026-09-16 14:0x -- lane GENSMALL1 (the small items round)

1. **Edited three untracked files without writing down what they weighed
   first, after being asked for before-and-after on every path.**
   The brief asks for a `CHANGED_FILES.txt` with bytes, CR and LF before and
   after, per path. For `src/lodifile.cpp`, `tests/spells/lodgen_native_decode.py`
   and `tests/spells/lodgen_native_fields.py` the before column is now blank and
   cannot be filled. All three are UNTRACKED -- earlier uncommitted lanes' work
   sitting in the tree -- so `git show HEAD:<path>` returns nothing, which is
   the trick that produced an exact before for `lib/libfo76utils/src/ba2file.cpp`.
   And each took at least one REPLACEMENT edit rather than pure additions, so
   the reverse arithmetic that recovered `src/nativeemit.cpp` (57,196 + 2,008 =
   59,204, matching a figure recorded independently by a patch script) has
   nothing to work from: subtracting what was added does not give back what was
   removed.
   **How it was found:** writing `CHANGED_FILES.txt` at the end of the lane and
   having no number to put in two of fourteen rows.
   **The rule:** measure a file the moment you decide to edit it, BEFORE the
   first edit, and write the three numbers into the report in the same turn.
   `git show HEAD:` is a fallback for tracked files only, and in a tree where
   several lanes' work is uncommitted, most of the interesting files are not
   tracked. Treat "I can reconstruct it later" as false.

2. **Typed a quoted heredoc containing backslashes, which a repo skill already
   warns about, and lost a patch to it.**
   The first attempt at patching the three contract pages went in as a
   `<<'PYEOF'` heredoc from Git-Bash. MSYS2 collapses backslashes inside it, so
   the regular expressions and the `\xc2\xa7` byte escapes arrived mangled and
   the script did not do what it read like it did. `ww-shell-heredoc` says
   exactly this, and it was read AFTER the failure instead of before writing.
   **How it was found:** the patch applied "successfully" and changed nothing
   the anchors named.
   **The rule:** any patch script with a backslash in it is written to a FILE
   with the Write tool and then run. Never through a heredoc from Git-Bash.
   Every patch script this lane wrote afterwards followed it, and none of them
   failed that way again.

3. **Wrote a curly apostrophe into a contract page and then spent an edit
   failing on it.**
   The `--vt-height` row added to `docs/LODGEN_TERRAIN_VT.md` 5 carried
   U+2019 in "a BC1 sheet's". The next patch then used an anchor with an ASCII
   apostrophe and refused with `anchor appears 0 times`. A byte count over the
   three contract pages read curly 1 / ascii 451 on the VT page and curly 0 on
   the other two -- one curly apostrophe in 759, and it was this lane's own,
   introduced minutes earlier.
   **How it was found:** the failed anchor, then counting the two characters
   across all three pages instead of looking at the line.
   **The rule:** the contract pages are ASCII apostrophes and em dashes only.
   Count both characters over the whole page after any edit; do not read the
   diff and judge by eye, because the two apostrophes are the same shape at the
   size a terminal draws them.

4. **Wrote a gate leg that would have passed on the binary it was meant to
   refute.**
   `tests/spells/resource_ext.sh` leg 4 compares the new exe's loose-file index
   against the pre-lane rung's. As first written it only checked that this
   exe's index was three larger than the rung's, which is trivially true when
   `NS` and `RUNG` happen to point at the same binary -- and it duly printed
   "rung holds 17, this exe holds 17 (+3)" while comparing an exe with itself.
   **How it was found:** running the gate with `RUNG` unset and reading the two
   numbers it printed rather than its verdict.
   **The rule:** a two-sided gate asserts BOTH sides absolutely, not their
   difference. Leg 4 now requires the rung to hold exactly 17 AND the new exe
   to hold exactly three more.

5. **Claimed eight re-derived constants, one of which is not in the switch at
   all.**
   Report section 2 first said the loose-file extension constants had been
   checked by re-deriving eight values already in the code, listing `hkx` among
   them. `hkx` is NOT a case in that switch. The claim was written from memory
   of what a texture-and-mesh whitelist ought to contain instead of from the
   nineteen values actually on the lines.
   **How it was found:** re-reading the switch while writing the gate, and
   finding nineteen cases and no `hkx`.
   **The rule:** a list in a report is copied from the file, not recalled. The
   section now says 19 of 19 with 0 mismatches, which is what was measured.

6. **Ran the object bake three times with the wrong flags and read the empty
   output as a result.**
   The asymmetric-drop measurement needed a single chunk. `--objects` without
   `-o` refuses ("this command writes; pass -o <out.nif>"), `--native` under
   `--objects` writes nothing at all because native emission lives only in the
   region branch (`src/nifcli.cpp:3602`), and `--exclude-btr` is not an option.
   Each was learned by running it.
   **How it was found:** an empty output directory after a bake that exited 0.
   **The rule:** read the branch that implements a flag before combining it
   with another, and check a bake's FILE COUNT before reading its numbers. The
   same rule as lane VT1's entry 1 the day before, learned again the hard way.
7. **Rewrote a gate phase so that it could only run after another phase, in a
   spell whose whole point is that phases are selectable.**
   `tests/spells/lodgen_defaults.sh` takes `PHASES=` and every phase is meant to
   stand alone. The rung-free rewrite of phase (b) compared its `b_old` bake
   against `$W/a_new`, which phase (a) bakes. The full run was green because (a)
   always runs first, so the defect is invisible in exactly the run anyone would
   do; `PHASES=b` on its own would have compared against a directory that did
   not exist.
   **How it was found:** re-reading the rewritten file after the green run and
   asking what each phase depends on, rather than trusting the verdict.
   **The rule:** a phase of a selectable harness may read only what it makes
   itself and what the preamble makes for everyone. After rewriting one, list
   every `$W/<name>` it reads and point at the line that creates it -- in the
   same phase or in the preamble, never in a sibling. The fix went in AFTER the
   run finished, not during it: bash reads a script incrementally, and editing
   one in place while it executes can corrupt the run.

8. **Put backticks inside a double-quoted `python -c "..."` and let bash eat
   three spans of the text being written.**
   Adding entry 7 to report section 9 was done with
   `python -c "..."` whose Python string literals contained
   `` `lodgen_defaults.sh` ``, `` `PHASES=` `` and `` `PHASES=b` ``. Inside
   DOUBLE quotes bash performs command substitution, so it ran
   `lodgen_defaults.sh` as a command (`command not found`, printed right
   beside the success line) and substituted empty strings for all three.
   The edit reported success and wrote `**Rewrote  phase (b) ...**`.
   **How it was found:** printing the edited lines back instead of trusting
   the byte count the script echoed.
   **The rule:** this is entry 2 again in a second costume. Markdown for
   these reports is full of backticks, so ANY patch script goes in a FILE
   written with the Write tool and is then run -- not a heredoc, and not
   `python -c` either, single-quoted or double. And every edit to prose is
   read back on screen, because a substitution failure looks exactly like a
   success.

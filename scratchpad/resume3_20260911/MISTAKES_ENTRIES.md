# MISTAKES entries from lane RESUME3, 2026-09-11 (text for the director to splice into the root MISTAKES.md)

## 2026-09-11 -- "it never crashes under gdb" was a property of the debugger, not of the bug

**What was done.** Lane BAKEPERF1 recorded that its heap-corruption fault "never
once [reproduced] under `gdb`, twice, which is what a timing-dependent fault
looks like", and lane NIFPARSE1 built `gdb3.sh` around that belief, choosing the
biggest region in the hope of provoking it. RESUME3 repeated it: three `gdb`
runs of the region that faults 3 of 5 bare came back
`[Inferior 1 exited normally]`, 3 of 3.

**What was true instead.** Windows gives a process **created by a debugger** the
DEBUG heap, which allocates and validates differently and does not fail-fast, so
a heap-corrupting race is invisible under `gdb` unless the inferior's environment
carries `_NO_DEBUG_HEAP=1`. With one extra gdb command --
`set environment _NO_DEBUG_HEAP=1` -- the identical command line faulted in
three seconds, printed `warning: Critical error detected c0000374`, and handed
over the stack two lanes had spent a session failing to obtain.

**How it was found.** By refusing to accept a 3-of-3 clean run as evidence when
the same binary and the same region had just faulted 3 of 5 without the
debugger. Two instruments disagreeing is a fact about the instruments.

**The rule.** Any gdb run on Windows that is hunting a heap fault sets
`_NO_DEBUG_HEAP=1` before it is allowed to report a clean run as evidence.
Written into `.claude/skills/nifskope-ww-crash-diagnose/SKILL.md`.

## 2026-09-11 -- C3 was refuted for the wrong handler

**What was done.** Lane NIFPARSE1 ruled candidate C3 ("`Message` / `qWarning`
building QWidgets on workers") out for headless runs, correctly and with the
code to prove it: `-no-gui` builds a `QCoreApplication`, so the `qobject_cast`
at `src/main.cpp:161` fails and `NifSkope::MessageOutput` is never installed. It
then wrote, as the reassuring half of the sentence, that "`nifskopeCliMain`
installs its own stderr-only handler instead".

**What was true instead.** That replacement handler is the fault.
`cliMessageHandler` wrote through `err()` -- a function-local
`static QTextStream` with no lock anywhere in the file -- from every worker
thread at once. Two of RESUME3's four symbolised faults are inside it.

**How it was found.** By taking the stack instead of reasoning about the code.

**The rule.** When a candidate is refuted because the dangerous thing is not
installed, the thing that IS installed in its place is a new candidate, and it
is named and checked in the same paragraph. "X is not reached, Y runs instead"
is half an answer until Y has been read.

## 2026-09-11 -- a gate that pointed at the region where the fault is NOT

**What was done.** `gdb3.sh` ran the 25-chunk Boston region "because that is
what caught it for BAKEPERF1".

**What was true instead.** With the parse lock off, Boston at 16 chunk threads
ran clean (RC=0, 163 files, 397.6 s) while the 9-chunk Sanctuary region faulted
**3 of 5**. The bigger region is slower per chunk, not more racy; Sanctuary's
road pass misses the same `.bgsm` on every placement and drives far more
warnings per second, which is what the fault needs.

**The rule.** Before spending runs on a crash hunt, measure WHERE it faults,
bare, and point the expensive instrument there. One five-run bare loop on each
candidate region costs less than three gdb runs at the wrong address.

## 2026-09-11 -- an anchor retyped from a report instead of read from the file (again)

**What was done.** SPLAT1's report quotes the `TILE` comment block with an ASCII
`--`. RESUME3 built the tiling anchor from that quotation and it counted 0.

**What was true instead.** The file carries an EM DASH (U+2014) there, and the
contract page `docs/LODGEN_TERRAIN_VT.md` carries UNICODE MINUS (U+2212) in its
cell ranges. Both anchors had to be rebuilt from the files' own bytes.

**How it was found.** The refusing script refused, which is what it is for.

**The rule.** This is `ww-anchored-hookup` section 5 extended once more: an
anchor is read out of the file's bytes, never out of a report, a README or a
previous lane's markdown -- **prose quotations normalise punctuation silently.**

## 2026-09-11 -- relink_sym.sh reported SYMBOLS=0 on an exe with 71,982 symbols

**What was done.** `scratchpad/nifparse1_20260911/relink_sym.sh` ends with
`echo "SYMBOLS=$(nm release/NifSkope.exe | wc -l)"` as its floor -- gate N1's
"a stack of `?? ()` is not a stack". Run from the outer Git-Bash shell it
printed **SYMBOLS=0**, which reads as "the relink did not keep the symbols".

**What was true instead.** `nm` and `gdb` exist only inside MSYS2 UCRT64; from
Git-Bash `which nm` is empty and the command substitution yields nothing. The
exe carried **71,982** symbols all along.

**The rule.** A floor that can report failure for a reason unrelated to what it
measures is not a floor. Any check that shells out to a toolchain binary runs in
the shell that OWNS that toolchain, and prints the tool's own path beside its
answer.

## 2026-09-11 -- a census word written and never read (found, not committed)

**What was done.** NIFPARSE1's F7 computes `lodgenChunkThreadBoundBy()` --
"cores" / "memory" / "asked" -- so a bake held back by free memory says so.
Nothing in the tree called it: `grep -rn lodgenChunkThreadBoundBy src/` returned
its own declaration and its own definition and nothing else.

**How it was found.** By grepping for the consumer before believing the field,
which is what `fo4cs-census-field` asks for and what CONSTITUTION 4's first rule
of 2026-09-04 21:33 requires.

**What was done about it.** RESUME3 put the word on the `bake census:` line
(`scratchpad/resume3_20260911/census_boundby.py`) so it is written AND read, and
measured it moving across the three values.

**The rule.** A lane that adds a census accessor adds its caller in the same
edit, or the field does not ship.

## 2026-09-11 -- a Bash heredoc halved the backslashes again, and this time it wrote broken C

**What was done.** The `--chunk-threads` usage text needed six lines rewritten
(it still told the user the fan-out was off because the parser faults). The edit
went through a `python - <<'PY'` heredoc that built each line as
`b'...' + t + b'\\n"'`.

**What was true instead.** The heredoc halved the backslashes, so `\\n` arrived
as a real newline and six C string literals were written with an actual line
break inside them:

```cpp
		  << "                                          how many chunks the queue builds
"
```

It compiles to nothing good and it is visible only by reading the file back,
which is what caught it -- not the script, which reported success.

**How it was found.** `sed -n '5550,5566p'` on the file after the edit, before
the build. Reading the resulting SHAPE rather than trusting the count, which is
the rule `ww-anchored-hookup` section 5 already carries for anchors.

**The rule, and it is a widening of an existing one.**
`nifskope-ww-build-verify`'s heredoc rule ("write the script with the WRITE
TOOL, never a Bash heredoc") has been applied so far to ANCHORS. It applies
equally to any text the script GENERATES that contains a backslash -- a C string
literal, a regex, a Windows path. This is the fifth time the tree has paid for
this trap. The repair was written with the Write tool and worked first time.

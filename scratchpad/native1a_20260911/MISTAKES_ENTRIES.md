Five entries from lane NATIVE1a, 2026-09-11. For the director to splice into `MISTAKES.md`
at the repo root, newest at the top (CONSTITUTION 2). LF-only.

---

## 2026-09-11 -- lane NATIVE1a: a relative path handed to the CLI wrote into `release/`

**What was done.** `release/NifSkope.exe -no-gui lodgen ... --native-fixture
scratchpad/native1a_20260911/fixture_v1`, from a shell whose cwd was the repo root. The
command printed `wrote scratchpad/native1a_20260911/fixture_v1/Synthetic.lodo (28903 bytes)`
and exited 0.

**What was true instead.** The directory it named was EMPTY. The bytes were under
`release/scratchpad/native1a_20260911/fixture_v1/` -- the application resolves a relative
path against its own directory, not against the shell's cwd.

**How it was found.** `ls` on the named directory immediately after the run, then
`find . -name "Synthetic.lod*"`.

**The rule that prevents it.** Every path handed to `-no-gui` is ABSOLUTE (`E:/...`):
`--out-dir`, `--native`, `--native-mesh-report`, `--native-fixture`, `--native-verify`. The
symptom is a successful-looking run and an empty listing. Written into
`.claude/skills/nifskope-ww-lodgen/SKILL.md`.

---

## 2026-09-11 -- lane NATIVE1a: heredoc backslash loss, twice, in a skill that warns about it

**What was done.** Patch scripts run as `python - <<'PYEOF' ... PYEOF` containing the C
string `"boundarySrc boundaryEmitted model\n"` and `QString( "%1 %2 ... %11\n" )`.

**What was true instead.** The `\n` arrived in the file as a REAL newline inside a C string
literal, and the translation unit would not compile (`missing terminating " character`, four
times). The same round lost the `\n` in a `nifcli.cpp` usage-text block, whose anchor then
would not match.

**How it was found.** The `g++ -fsyntax-only` pass before the build, not the build -- so it
cost minutes rather than a build slot.

**The rule that prevents it.** `nifskope-ww-lodgen` already says it: no text carrying a
backslash or an apostrophe goes through a heredoc at all. Write the patch as a SCRIPT FILE
with the Write tool and build every backslash from `chr(92)`. Reading the rule is not
following it; the rest of this lane's patches were script files.

---

## 2026-09-11 -- lane NATIVE1a: a patch script that writes at the END loses its earlier edits when a later assertion fires

**What was done.** Multi-replacement scripts shaped as `rep(a, b); rep(c, d); ...; write()`.
Two of them aborted on the LAST `assert count == 1` and I carried on as though the earlier
replacements had landed.

**What was true instead.** Nothing was written at all -- the file write is after every
replacement -- so three edits I believed were in the tree were not.

**How it was found.** The next script's anchor for one of those edits reported `found 0
times`.

**The rule that prevents it.** After a patch script fails, re-check EVERY replacement it
contained, not just the one that raised; or write incrementally. An assertion that stops a
script does not roll anything back, and it does not commit anything either.

---

## 2026-09-11 -- lane NATIVE1a: I deleted a measurement while fixing the thing it measured

**What was done.** The gate found that meshopt's cache optimiser read worse than the source
order on 21 meshes, so the writer was changed to keep the better of the two orders. The patch
replaced the block that computed the "after" statistics -- and that block also held the two
lines that ACCUMULATED them.

**What was true instead.** Every "after" total was 0. The census line printed
`cacheOrder acmr 1.8600 -> 0.0000` and my own field gate reported
`e1 the corpus ACMR improves (1.8600 -> 0.0000)` and passed, because 0 is less than 1.86.
The whole gate went green on a number that meant "not measured".

**How it was found.** Reading the census line, not the verdict. `0.0000` is not a plausible
ACMR; a triangle cannot cost zero vertex-shader invocations.

**The rule that prevents it.** A green gate is not a result (CONSTITUTION 4). Read the NUMBER
next to every verdict, and when a fix touches the code that MEASURES something, re-read that
measurement before believing the gate. A one-sided check ("improved") cannot tell an
improvement from an absence; where it matters, bound the value on both sides.

---

## 2026-09-11 -- lane NATIVE1a: a floor that encoded an expectation about the data instead of testing the instrument

**What was done.** The floor for the GPU cache-order field was written as "at least 5 percent
of the meshes must have moved". It failed at 2.2 percent (45 of 2,982).

**What was true instead.** Nothing was wrong with the pass. Bethesda's shipped LOD meshes are
already close to cache-optimal, so the number measures THEIR meshes, not our code. The floor
was an assumption dressed as a check, and had the corpus been slightly different it would have
passed while proving nothing.

**How it was found.** Asking what the check would do on a correct implementation over a
different corpus.

**The rule that prevents it.** A floor is a CONTROL on the instrument, not a threshold on the
data: run the metric on a deliberately BAD input and require it to read worse. The replacement
shuffles the largest mesh's triangles and scores 2.990 against the 1.633 we emit. The same
lane's silhouette floor is the right shape already -- a hole-punched mesh shown red.

---

*(Also noted, not a ledger entry: the decoder's manifest position bar and the first staleness
floor were both measuring the wrong thing -- the manifest's printf, and the pairing rule
rather than the staleness rule. Both are recorded in the report and written into the two
amended skills.)*

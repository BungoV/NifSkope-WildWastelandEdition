# MISTAKES entries owed by lane NIFPARSE1

Text for the root `MISTAKES.md`. **Not appended by this lane** — the file is
shared and the director splices (CONSTITUTION 8). Entries start with `## `.

## 2026-09-11 — A hook-up anchor that would have emptied the branch it landed in

Lane NIFPARSE1's `--check` hook-up script placed the new `parsestress` CLI
branch with an `after` on this line of `src/nifcli.cpp`:

    else if ( cmd == QLatin1String( "lodt" ) ) {

The anchor matched exactly once, the CR count was unchanged, and the script
reported `RESULT CHECK OK`. It was still wrong: inserting **after an opening
brace** would have left the retired-`lodt` branch with an empty body and pushed
its error message down onto a fresh `else if ( false )`, silently retiring a
diagnostic bungo's harnesses rely on.

**What is true instead:** "matches exactly once" is a check that the anchor is
UNIQUE, not that the insertion point is CORRECT. An `after` on a line that opens
a scope inserts into that scope, not beside it.

**How it was found:** by reading the resulting file shape back out of the table
before running `--check` a second time — not by the script, which was perfectly
happy.

**The rule:** never anchor an `after` on a line that ends in `{`. Anchor on the
statement the insertion must follow, or use `replace` and repeat the whole
anchor in the replacement so the edit is visibly additive. Added to
`ww-anchored-hookup`.

## 2026-09-11 — Indentation typed from a screen instead of measured from the bytes

The same lane's fix script anchored a nine-line block of `src/model/nifmodel.cpp`
with four leading tabs. The file has five. The anchor counted **0** and the
script refused, which is what it is for — but the twenty-two other edits in the
same run could not be applied either, because the script writes all or nothing,
so one mis-typed indent cost the whole pass.

**What is true instead:** the block starts at five tabs and its body at six, as
`len(line) - len(line.lstrip(b'\t'))` says.

**How it was found:** the `--check` refusal, then measuring the tabs with Python
byte counts rather than counting them in the Read output — where the line-number
prefix makes the first indent level impossible to count by eye.

**The rule:** indentation inside a multi-line anchor is MEASURED, never typed.
Print `tabs=N` for the anchor's first and last line before adding the edit. This
is the same rule the tree already has for line endings (CONSTITUTION 8: measured
with Python byte counts, never with grep), extended to leading whitespace.
Added to `ww-anchored-hookup`.

## 2026-09-11 — "The NIF parser is not thread-safe" was a conclusion, not a measurement

BAKEPERF1 shipped its whole result — the chunk fan-out off by default, the
blocker named, a lane chartered — on **one stack taken in the middle of a whole
chunk bake**, which is the parser AND the plugin reader AND the texture cache
AND the archive layer AND the message sink running at once. Its own skill page
says that for `STATUS_HEAP_CORRUPTION` the stack names where the damage was
DETECTED, not where it was done. The conclusion may still be right; it was not
measured.

**What is true instead (so far, and by reading only):** the mesh reads never
enter `GameManager` at all — `lodgenReadAsset` serves `.nif` from
`lodgenMeshArchives()` with const, pointer-based `findFile`/`extractFile` — while
`.dds`, `.bgsm` and `.pbrm` fall through to `GameManager::get_file`, which is
the path with a lazily built and lazily **freed** shared `BA2File`. The two
bisect variants that came back clean (`--no-tex-dir`, `--no-roads`) are exactly
the two that stop entering it.

**How it was found:** following the call out of `lodgenLoadModel` instead of
stopping at the file boundary — the rule BAKEPERF1 itself wrote into
`ww-parallelise-a-stage` after making the same error one layer up.

**The rule:** a cause is named by an experiment that could have come out the
other way. Where a crash happens inside a pipeline, build the run that contains
ONE stage of it and see whether the fault follows. NIFPARSE1's
`src/nifparsestress.{h,cpp}` is that run for the model layer, and its verdict —
either way — is the deliverable.

## 2026-09-11 — The whitespace rule was written, then broken twice more in the same hour

Having just written into `ww-anchored-hookup` that a multi-line anchor's
indentation is MEASURED and never typed, lane NIFPARSE1 immediately typed
another one: it anchored on

    int g_chunkThreads = 1;<TAB x5>//! ONE until the parser is safe

where `src/lodgenparallel.cpp` actually has **spaces**, not tabs, and `//!<`,
not `//!`. Count 0, whole pass refused.

Then, fixing that, it wrote the repair script as a **Bash heredoc**, which
halved its backslashes and died with `SyntaxError: unexpected character after
line continuation character` — the trap `nifskope-ww-lodgen` already documents
and that BAKEPERF1 recorded hitting three times in its own lane the same day.

**What is true instead:** the whitespace inside that line is spaces; and any
script carrying a backslash must be written with the Write tool.

**How it was found:** the `--check` refusal, then printing the file's own bytes
with `repr()` — which is what should have produced the anchor in the first
place. The repair script now READS the line out of the file instead of
containing a copy of it, and asserts that it carries no backslash or quote
before turning it into a literal.

**The rule (and it is a repeat of an entry above, which is its own entry per
CONSTITUTION 2):** do not retype a line you are anchoring on. Take it from the
file, `repr()` it, and build the literal from that. A rule written down and then
broken twice in one hour is a rule that needs a MECHANISM, not a reminder — so
`ww-anchored-hookup` section 5 now carries the four-line "read the anchor out of
the file" snippet rather than only the instruction to measure.

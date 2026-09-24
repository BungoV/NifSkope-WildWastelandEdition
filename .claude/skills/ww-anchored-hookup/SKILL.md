---
name: ww-anchored-hookup
description: Write a lane's cross-file hook-up (the few lines an existing file needs so NEW files join the build) as a REFUSING script instead of applying it -- exact-once anchors carrying the file's real line ending, a --check that writes nothing, a CR/LF byte assert, and the compile-time switch that lets the new files compile with AND without the hook-up. Use whenever a NifSkope WW brief says "write all new code in new files first; touch the existing files only after <gate>", and for any edit to a file another lane owns. Lanes WATER2, WATER3, WATER4, BUILD5b and WATER5 each re-typed this before it was written down.
---

# NifSkope WW: the hook-up as a refusing script

Repo `E:\Projects\NifskopeWildWastelandEdition`. The shape every "one lane
per file" brief forces: the lane's own work goes in NEW files, and the two
or ten lines that an EXISTING file needs (`NifSkope.pro`, an `#include`, a
button, a codec branch) are written as edits a later step applies. Five
lanes wrote the same 40-line script for it; this is that script's contract.

## 1. The script, not the diff

`scratchpad/<lane>/hookup.py`, written with the WRITE TOOL (never a heredoc:
a quoted heredoc halves backslashes, so a C-string `\n` inside an anchor
arrives as a real newline and counts 0). One table, one function:

```python
EDITS = [ ( "src/file.cpp", "after" | "replace", ANCHOR, TEXT ), ... ]
```

* **ANCHOR is exact and matches ONCE** in the file's real bytes. Multi-line
  anchors are fine and usually needed: `marks.append( s );` occurs in three
  functions of `watermark.cpp`; the three lines above it occur once. Print
  the count for every anchor.
* **The line ending is IN the anchor.** `src/` is LF-only; `src/nifskope.cpp`
  is mixed, mostly CRLF; `WW_CHANGES.md` is mixed. `b.count(b"\r")` before,
  the same after, asserted -- a rename at equal length must move nothing.
* **`--check` is the default and writes nothing.** It is the thing the lane
  runs and quotes in its PENDING resume ("12 of 12 anchors match once, CR 0").
  `--apply` refuses unless every anchor still matches once, then writes every
  file at once.
* **"after" inserts; "replace" substitutes.** Prefer "after" (it cannot eat a
  line by accident); use "replace" only when a line must change in place, and
  then repeat the whole anchor in the replacement text so a reader sees the
  edit is additive.
* **Leave what the table did not list, and say so.** Two lines away from an
  anchor is not authorised.

Reference implementation: `scratchpad/water5_20260910/hookup.py` (12 edits
over four files, `--check` / `--apply`). Copy it.

## 2. The new files compile with AND without the hook-up

A lane that may not touch `watermark.h` still has to pass `g++ -fsyntax-only`
on its new files TODAY, while the code it writes for AFTER the hook-up (a new
struct member, a new API) would not compile until then. The trick that keeps
both true:

```cpp
// in the hook-up's edit to the shared header:
#define WATERMARK_STROKE_EXTRA 1
// in the new file:
#ifdef WATERMARK_STROKE_EXTRA
    s.extra = ...;                   // the post-hook-up path
#endif
```

and in the self-test, the check that needs the hook-up prints a NAMED SKIP
in the `#else` branch, never a pass:

```cpp
#else
    skip( "weights not carried by the store until hook-up H2 is applied" );
#endif
```

The harness greps the SKIP lines out and prints them, so a resume that
forgot the hook-up sees the word in its own log.

## 3. What goes with it

* `PENDING.md` (per `nifskope-ww-resume-pending`) names the script, the
  `--check` result, the ORDER (another lane's build first when the new code
  calls into it), qmake-before-make when the `.pro` changed, and the object
  read-back for every header the hook-up touched.
* `CHANGE_NEEDED.md` beside it for what the OTHER file's owner must change in
  BEHAVIOUR (a solver consuming a new field), which is not a hook-up and is
  not applied by a resume: mechanism, candidates, the gate that would prove it.
* The syntax pass with the real flags on every new file, before declaring
  the code finished (`nifskope-ww-build-verify`, "When you CANNOT build").

## 3a. Candidate anchors, either line ending, and an ordered pair of hook-ups (lane HKXEDIT2, 2026-09-10)

`scratchpad/hkxedit2_20260910/hookup.py` adds three things worth copying:

* **A list of candidate anchors per edit**, the first that matches ONCE
  wins. Needed when another lane's pending hook-up rewrites the line you
  anchor on (HKXEDIT1 turns the `kfm.xml` copy line into two lines; this
  lane's copy-list edit anchors on whichever of the two is there).
* **Try LF then CRLF per anchor** and use the one that counts one, instead
  of a hand-kept CRLF file set; assert the CR count moves by exactly the CRs
  of the inserted text.
* **An ORDER refusal**: a define that links another lane's new source
  (`WW_HKXCLIP_CANON` -> `src/hkxfile.cpp`) makes the script refuse until that
  source is in the .pro, so a resume cannot apply the two in the wrong order.
  A `tier 2` keyed on a marker in the other lane's output (`HkxModel * hkx;`)
  adds the edits that only make sense once it landed.

And the trap it paid for ANYWAY: **a one-off anchor COUNT through a Bash
heredoc halves backslashes** exactly as a patch does, so a `.pro` line ending
in a backslash counts 0 and looks like a bad anchor while the file is fine --
and a heredoc PATCH of the script itself turns escaped tabs into real tabs
and leaves a SyntaxError. The rule covers checks too: anything carrying a
backslash goes through the Write or Edit tool.

## 4. The trap this was written after

A `--check` that reads "ok" AFTER the edit was applied, because the anchor is
the line the text goes after and it still matches (BUILD5b, 2026-09-10). The
script above prints the COUNT and the CR, never "ok"; a resume decides
"applied or not" from the file's size against the table's predicted delta,
or from a marker string the inserted text carries (`(lane WATER5)`), never
from the anchor.

## 5. "Matches once" is not "goes in the right place" (NIFPARSE1, 2026-09-11)

Two refusals that a unique-anchor count does not catch. Both cost a pass.

* **Never anchor an `after` on a line that ends in `{`.** The script places the
  text INSIDE the scope that line opens, not beside it. NIFPARSE1 anchored a new
  CLI branch on `else if ( cmd == QLatin1String( "lodt" ) ) {`; the count was 1,
  the CR assert passed, `--check` said OK, and applying it would have left that
  branch with an empty body and pushed its error message onto a fresh
  `else if ( false )`. Anchor on the STATEMENT the insertion must follow, or use
  `replace` and repeat the whole anchor in the replacement so the edit is
  visibly additive and a reader can see nothing was eaten.

* **Indentation inside a multi-line anchor is MEASURED, never typed.** The Read
  tool prints a line-number prefix, so the first indent level cannot be counted
  by eye; NIFPARSE1 wrote four tabs where the file has five, the anchor counted
  0, and because the script writes all-or-nothing, one mis-typed indent refused
  twenty-two other correct edits with it. Before adding any multi-line edit:

```python
b = open(path, "rb").read().split(b"\n")
for i in range(first - 1, last):
    ln = b[i]; print(i + 1, "tabs=%d" % (len(ln) - len(ln.lstrip(b"\t"))), repr(ln[:70]))

* **AN ANCHOR IS READ OUT OF THE FILE, NEVER OUT OF A REPORT** (RESUME3,
  2026-09-11). Prose quotations normalise punctuation silently. SPLAT1's report
  quoted the comment above `constexpr float TILE = 2048.0f` with an ASCII `--`;
  the file carries an EM DASH (U+2014) and the anchor counted 0. The same hour
  `docs/LODGEN_TERRAIN_VT.md` turned out to carry UNICODE MINUS (U+2212) in its
  cell ranges (`cells −20..−17`), so an anchor typed from the rendered
  page counts 0 there too. Markdown, READMEs, a previous lane's report and this
  page are all prose: take the bytes from the file, and build the anchor with
  `chr(0x2014)` / `chr(0x2212)` rather than pasting a character whose identity
  cannot be seen.

  The check, before any anchor is written:

```python
b = open(path, "rb").read().split(b"\n")
print([repr(l) for l in b[first-1:last]])      # every non-ASCII byte shows
```

* **Two declarations with identical text need a NON-LOCAL anchor.**
  `src/lodgen.cpp` holds `constexpr float TILE = 2048.0f;` twice. Anchor the
  first on the comment block only IT carries, replace it, then anchor the second
  on the now-unique bare line -- and assert the AFTER state (`0 of the old left,
  2 of the new`) rather than the before count. An edit that must hit both copies
  proves it hit both.
```

  and build the anchor from `chr(9) * N` with those N. This is the line-ending
  rule (CONSTITUTION 8: measured with Python byte counts, never grep) extended
  to leading whitespace, and it is the same failure mode.

A third, cheap habit that falls out of both: after `--check` goes green, print
the FIRST AND LAST line of each edit's region as the file would read after the
edit, and look at the shape. The script proves the anchor is unique; only a
human reading the result proves it is the right anchor.

### 5a. Do not RETYPE the line — read it out of the file

Section 5's rule was written, and then broken twice in the next hour by the lane
that wrote it: an anchor typed with five tabs where `src/lodgenparallel.cpp` has
spaces and `//!<` rather than `//!`, and then a repair script written as a Bash
heredoc, which halved its backslashes and died on a SyntaxError. A rule broken
twice in an hour needs a MECHANISM, not a reminder.

The mechanism: the script builds its own anchor from the file's bytes, and
refuses if the line needs escaping.

```python
real = None
for line in io.open(SRC, encoding="utf-8").read().split(chr(10)):
    if line.startswith(UNIQUE_PREFIX):          # a prefix you CAN type safely
        real = line
        break
assert real is not None, "the line moved"
assert chr(92) not in real and '"' not in real, "needs escaping: " + repr(real)
ANCHOR = real + chr(10)
```

Type only a prefix short enough to be unambiguous and free of whitespace runs
(`int g_chunkThreads = 1;`), and let the file supply the rest — the trailing
comment, the tabs-or-spaces, the exact number of them. Print `repr(real)` beside
the count, always: it is the only rendering in which a tab and four spaces look
different.

And the older trap, which applies to the FIXER as much as to the patch: anything
carrying a backslash goes through the Write tool. A quoted Bash heredoc through
this harness still halves them.


### 5b. An edit can eat the next edit's anchor (lane CELLVIEW4, 2026-09-19)

*(CELLVIEW4's deliverable calls this "5c"; this skill has 5a and no 5b, so it is
filed as 5b. The text is unchanged.)*

Inserted text is source too, and a block you insert can contain a line that is
a later edit's anchor. The table was resolved against the file as it was, so
the count printed by --check says 1 and the apply then quietly edits the copy
you just inserted instead of the one you meant.

Two defences, and use both:

* count again AT APPLY TIME, against the text in hand, and assert exactly one
  match. Never "take the first hit" -- that is the failure, not the fix.
* order the table so an edit that NARROWS a line runs before the edit that
  inserts a block mentioning it.

And demonstrate the apply without applying it: run the same table through the
same apply function onto COPIES in the scratchpad, then check each copy's CR
delta and its brace/paren delta against what the inserted text carries. A
hook-up script whose `--apply` has never produced a byte is not evidence that it
works. (Found by lane CELLVIEW4, 2026-09-19: an inserted block carried its own
`if ( spec.terrain ) {`, and the same dry run caught a missing `#include` the
table had simply never had.)

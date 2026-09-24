---
name: nifskope-ww-commit
description: Commit and push a backlog of uncommitted work in the NifSkope Wild Wasteland tree (E:\Projects\NifskopeWildWastelandEdition, public repo) once bungo lifts his "Not yet" — the size inventory that separates deliverables from generated bulk, the .gitignore that keeps hundreds of MB out of a public repo without listing every path, the Python byte-count line-ending gate for the three mixed CRLF files, how to group entangled source into a small number of path-list commits, and the four documents that must be updated before the push. Use whenever the repo has an uncommitted backlog to land; never guess the ignore rules or measure line endings with grep.
---

# Land a NifSkope WW backlog on origin/main

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, remote
`github.com/BungoV/NifSkope-WildWastelandEdition` — **public**, solo, straight
to main, never a PR. Read `CONSTITUTION.md` sections 5 and 8 first; this skill
is the procedure those two paragraphs imply.

Preconditions: bungo has said the words. Nothing here builds or launches the
exe. Confirm no other lane is alive in the tree — `git stash` is forbidden in a
shared tree and so is rewriting a document a lane is writing into.

## 0. The trap that costs a call every time

Any Python carrying a backslash, a regex or a `\n` goes into a **script file
written with the Write tool** and is run by path. Never a bash heredoc, never
`python -c`. Bash halves the backslashes and Python raises `SyntaxError`. This
is in `MISTAKES.md` six times.

## 1. Inventory by size, not by name

`git status --porcelain -uall` on this tree can take minutes — the untracked
`scratchpad/` is thousands of files. Run it once into a file and work from
that; `du -sh` on `scratchpad/` and `heightmaps/` will time out, so walk them
with `os.walk` and `os.path.getsize` in a script instead.

Produce three numbers before deciding anything: files and MB per top-level
untracked directory, then per second-level entry, then per extension. The
shape you are looking for is a handful of directories holding 95% of the bytes.

Classification that has held:

| goes in | stays out |
|---------|-----------|
| `src/`, `res/`, `tests/`, `tools/`, `docs/` | `heightmaps/` (generated far-terrain DDS, ~436 MB) |
| `.claude/skills/` copies in the repo | `scratch_water/` (hand-made scratch bakes) |
| every report, brief, PENDING.md, MANIFEST.md | every `.dds .bto .btr .nif .lod* .bin .head .orig` |
| every measurement script | `*.manifest.txt`, `*.verts.txt`, multi-MB LAND dumps |
| the pictures a handoff or a committed report names | every other render |

**The generated bulk is not deleted, it is ignored.** A sample set stays on
disk for the person it was made for; its `MANIFEST.md` and its generator
script go in so it can be rebuilt.

## 2. Write the .gitignore by rule, not by path list

Never list a thousand paths. Ignore by extension under `scratchpad/**` and by
folder for the few big ones, then handle pictures with one deny plus explicit
allows:

```
scratchpad/**/*.png
!scratchpad/<topic>_<date>/*.png          # one line per folder a doc names
```

Negation only works because `scratchpad/` itself is not ignored — git must be
able to descend. **Never ignore a directory you then want to re-include a file
from**; git will not descend into it and the negation is silently dead.

Then prove the rule instead of trusting it: after writing `.gitignore`, run
`git status --porcelain -uall` again and diff the untracked list against the
keep-set your inventory script computed. Iterate until they are equal. That
diff is the gate — not a reading of the patterns.

State the committed total, and give a one-line justification for anything over
5 MB or send it to `.gitignore`.

## 3. Line endings: Python byte counts, never grep

`.gitattributes` is `* -text`, so git stores bytes exactly and will not save
you. Five files in this repo are CRLF and must stay CRLF: `src/glview.cpp`,
`src/nifskope.cpp`, `src/gl/controllers.cpp`, `src/spells/havok.cpp`,
`WW_CHANGES.md`. Everything else is LF.

For every tracked file about to be committed, compare `open(p,'rb').read()`
against `git show HEAD:<p>`:

- **LF-only at HEAD** → `b.count(b'\r')` must still be `0`. Any CR means the
  file was flipped and must be spliced back in binary before committing.
- **mixed at HEAD** → the CR delta must equal the CRLF lines the diff ADDS
  minus the CRLF lines it REMOVES. Get those by parsing `git diff -U0` in
  BYTES and testing `line.endswith(b'\r')` on the `+`/`-` lines. Predicted and
  actual must match exactly. `src/nifskope.cpp` is CRLF-with-LF-blocks, so a
  legitimate diff there adds both kinds; only the arithmetic proves it.

New untracked files have no neighbour to match. Captured tool output that is
natively CRLF stays CRLF — normalising it would corrupt the record.

**NO LINE TOOL EVER TOUCHES ONE OF THE FIVE** (2026-09-10, lane CARDORTHO).
`sed -i`, `tr`, `awk > file`, a `>` redirect and every editor rewrite the WHOLE
file with LF. One `sed -i` to fix a single mis-typed character took all 19,020
CRs out of `WW_CHANGES.md` three lines after a binary splice that had asserted
the count on both sides. The fix for a typo inside a spliced block is to REDO
THE SPLICE, not to reach for a line tool, and the byte count belongs after the
LAST write of a turn rather than after the first.

If it happens anyway, it is repairable as long as HEAD still has the bytes:
`orig = git show HEAD:<p>`, `body = orig[len(header):]`, check that the damaged
file ends with `body.replace(b'\r\n', b'\n')`, and rebuild as
`damaged[:len(damaged)-len(body_lf)] + body`. Assert the CR count equals HEAD's
and the LF count equals the damaged file's, so no content moved. Anything a
CONCURRENT lane added to the file in CRLF is lost by this repair — compare the
CR count with HEAD's BEFORE the accident to know whether there was any.

## 4. Group into a small number of path-list commits

`git add -- <explicit paths>`, never `-a` / `-A`. Review
`git diff --cached --numstat` before every `git commit`.

Order the commits chronologically by the `WW_CHANGES.md` entries they serve.
The `.gitignore` commit goes FIRST so every later `git add` behaves.

The honest grouping is **by path**, not by theme, because one source file
serves several entries — `src/lodgen.cpp` alone carries the impostor,
ground-cover, virtual-texture and normal-map rounds. Do not split a file's
diff across commits with index surgery: it manufactures commits that were
never built together. Put the entangled file in one commit and let the message
enumerate the themes with their measured numbers.

A grouping that worked, eight commits: ignore rules; the file family + formats;
the generator; the panel/viewer/renderer; the headless rules; the format
contracts + handoff package; the scratchpad evidence; the ledgers last.

Every message: what it is, the measured numbers (not adjectives), and
`Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>` as the last line.

`git add -- scratchpad` over thousands of files takes minutes — background it
and poll, and dump `git diff --cached --numstat` to a file rather than to the
terminal.

## 5. The four documents, before the push

The ledger commit is LAST and carries all four, so it can name the hashes of
the ones before it:

1. **`HANDOFF.md`** — a new paragraph at the very top: every hash with what it
   is, what was excluded and why plus how it regenerates, the line-ending
   evidence, and that "Not yet" is lifted. Then **hunt the file for the claims
   the commit just falsified** — "NOTHING committed", "N uncommitted paths",
   "NEXT on his word: the COMMIT" — and correct each in place. A handoff that
   still says nothing is committed is a lying handoff (CONSTITUTION 9).
2. **`WW_CHANGES.md`** — an entry with the hash table, the exclusion table with
   the regeneration route per row, the committed total, and the line-ending
   table. **Mixed file: splice the entry in binary**, LF-only, assert `dCR ==
   0` and assert the tail is byte-identical afterwards.
3. **`MISTAKES.md`** (repo root; `docs/MISTAKES.md` is the older ledger) —
   every mistake this lane made, including a repeat of one already in the file.
   Newest at the top, spliced after the `Newest at the top.` marker.
4. **A skill** — this one. If the lane invented a procedure, write it the same
   session in `E:\Projects\Claude\.claude\skills`.

## 6. Push and read back

`git push origin main`, then `git log origin/main -1` and `git status`. The
tree must be clean apart from ignored paths — check with
`git status --porcelain -uall` and confirm the only untracked output is empty,
and `git status --ignored=matching --porcelain` names what you meant to
exclude. Report the read-back, not the intent.

## 7. A public backlog of weeks (LEDGERFIX1, 2026-09-24)

The 09-24 commit landed 15 days of lanes: 18,727 untracked paths / 8 GB became
10,805 text paths / ~118 MB in seven commits. What it added to sections 1-6:

- **Order when the brief says "ledgers first"**: ledgers, then `.gitignore`,
  then src/res, tests/tools, docs/skills, scratchpad; a last small commit puts
  the hashes into HANDOFF + WW_CHANGES. An interruption then loses nothing.
- **Three gates over every path a commit would stage**, read from
  `git status --porcelain -uall -z` saved to a file: no game-data extension
  (`.esm .esp .esl .ba2 .bsa`) anywhere, nothing over 5 MB, and the public
  wording regex from the scrub report (the symbol-source names) with every
  remaining hit named as allowed. The script that carries that regex lives in
  the SESSION scratchpad, never in the repo -- it would publish the strings.
  A new ignore pattern can trip the gate itself (`*.p` + `db` did): check
  `.gitignore` too.
- **After every `git add`, read the numstat's `-` rows**: they are the files git
  thinks are binary. Pictures a document names stay; generated binaries with
  odd extensions (`wb_rt.bin.4`, binary `.pbrm` bakes, crash scratch) get an
  ignore rule and `git restore --staged --pathspec-from-file=<list>`. Also flag
  any tracked file whose staged deletions exceed half its HEAD lines (a
  whole-file rewrite = a line-ending flip). Script:
  `scratchpad/ledgerfix1_20260924/cached_check.py`; inventory by dir/ext:
  `inventory.py` beside it.
- **Open questions for bungo are held by ignore, not published**: a
  `# HELD for bungo's ruling` block in `.gitignore` (skill copies from other
  projects, notes naming an outside RE source). Public history cannot be
  taken back; an ignore line can.
- Vanilla-derived test fixtures: ignore the generated folder, commit the
  generator script (`tests/fixtures/pbr_*_data/` vs `pbr_r*_fixtures.py`).
- Timing on this tree: first `git status -uall` 80 s, then ~2 s; `git add --
  scratchpad` for 10k files 3 m 40 s; its commit 54 s. Foreground with a
  600 s timeout is fine.
- A tracked log that is now CRLF can be a real regeneration: compare
  `git diff --numstat` with `--ignore-cr-at-eol`; equal counts = new content,
  commit it as captured.
- Attribution: end messages with the co-author line the SESSION gives (Opus 5.5
  on 09-24), not the one in section 4.
- **Ledger writes** (the 13:57 wipe): read into a variable and close before
  opening for write; assert the new ledger is LONGER and its CR count
  unchanged; refuse a target under 10 kB; read back after writing.

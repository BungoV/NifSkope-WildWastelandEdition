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

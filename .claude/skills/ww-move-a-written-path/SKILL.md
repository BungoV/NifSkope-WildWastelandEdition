---
name: ww-move-a-written-path
description: Move where a tool WRITES its output files in NifSkope Wild Wasteland Edition (E:\Projects\NifskopeWildWastelandEdition) -- a new folder root, a new per-worldspace layout, a renamed sidecar -- so that the only bytes that change are the path strings inside the files and the length words those force. Covers the one-composer rule, the normalised byte-identity diff and its refuter, the paths RECORDED INSIDE other files (ledgers, indices, manifests) that move with them, the census root read back from the writes, and the harness classes that must and must not be re-based. Written from lane LAYOUT1 (2026-09-16), which moved every FO4CS-target output under Data/FO4CSLOD/.
---

# NifSkope WW: move a written path

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written from lane LAYOUT1
(2026-09-16), bungo's *"The folder should be called FO4CSLOD maybe, so it'd be
Data/FO4CSLOD, sound fine?"*.

## 1. One function composes the root. There is no second spelling

Write `src/<area>layout.{h,cpp}` FIRST, before touching a writer: a file whose
only job is to compose the paths, with the folder name as a single
`static const char * const` inside it. Every writer, the panel, the CLI help,
the self-test and the census call it.

**The gate is `grep -rln '"<NAME>"' src/` returning exactly that one file.**
That includes test code: a self-test that asserts on the literal adds a second
spelling and fails the gate -- have it ask the composer instead, and let this
grep be the check that the name is right.

## 2. What must NOT change, said as bytes

The claim to earn is narrow: *the only bytes that changed are the game-relative
path strings written inside the files, and the length words those strings
force.* Prove it with a tool, not by eye:

* Bake the same region twice, same switches -- once with the rung exe, once
  with the new one.
* Rewrite the OLD file's path spellings to the new ones. Do all the forms:
  raw, JSON-doubled backslash, and the lower-case `data\` variant, **longest
  first** so one prefix cannot eat another.
* Repack any envelope whose payload length the rewrite resized (a `LODM` header
  carries the length at offset 8 and no checksum, so the repack is exact), and
  SAY in the output that it was repacked.
* Then demand byte equality, over **every** file both bakes produced, not the
  ones you expect to change.
* `--no-rewrite` is the refuter and it must be run: without the rewrite the
  path-carrying files MUST differ. A gate that cannot fail proves nothing.

`tests/spells/lodgen_layout_diff.py` is the working example.

## 3. The paths recorded INSIDE other files move too -- this is where it breaks

A ledger, an index or a sidecar that names outputs by path is a writer of paths
even though it is not the file that moved. LAYOUT1's `.lodb` recorded
`outDir + "/" + name + ".manifest.txt"` -- a spelling that contains none of the
old folder names, so no grep for the moved path could find it -- and the file
had moved. The entry kept an **empty digest** and every later `--incremental`
run would have rebaked the whole region in silence.

* Grep for the old spelling is NOT sufficient. Also grep for every writer that
  BUILDS a path from a root variable: `outDir +`, `dir + "/"`, `%1/%2`.
* After the move, walk the recorder's own entries: resolve each path on disk
  and re-hash it. Put that row in the gate. It is the only check that catches a
  recorded path nothing writes, because nothing else about it is loud.
* A recorded digest legitimately changes when the file it digests changed its
  own path strings. Do not try to normalise a digest -- drop that file from the
  byte-identity list, SAY why, and give it the resolve-and-rehash row instead.

## 4. The census names the root, read back from the writes

Note each written path as it is written; the census clause states the common
root of those notes, the count under it, and the count OUTSIDE it. Never print
the setting. The default accuses its own plumbing:
`layout n/a (no FO4CS-target file written)`, never `layout , 0 file(s)`.
Register it in `docs/LODGEN_CENSUS.md` §6.1 with `ww-census-contract`.

## 5. Which harnesses to re-base, and which to leave alone

Sort every path hit into three piles before editing anything:

* **Outputs of a bake by THIS exe** -- re-base.
* **Outputs of a target that did not move** (the stock engine: a harness with
  no `--native` on its command line) -- leave alone, and say so in the report.
* **Fixtures baked before the move, and the shipped mod folder** -- READ paths.
  Leave alone. Re-basing one of these makes a harness that cannot find its own
  corpus.

For a harness that compares a RUNG tree with a NEW tree, neither spelling is
right for both. Add a helper that asks the tree:

```sh
pairdir () {
	if [ -d "$1/<NEWROOT>/<ws>" ]; then echo "$1/<NEWROOT>/<ws>"
	else echo "$1"; fi
}
```

## 6. Empty-folder hygiene

A bake creates no folder it does not fill. After the move the old folders must
not be created at all -- guard the `mkpath` on the thing that fills it, and
gate on `find <out> -type d -empty` being empty AND on the old folder names not
existing.

## 7. Editing traps that cost LAYOUT1 time

* Heredocs halve backslashes. The dangerous case is a halved backslash in a
  **line continuation**: `cmd "$A" \n\t\t"$B"` is still valid shell, passes
  `bash -n`, and silently runs `cmd` with `n` as an argument. Patch shell and
  C++ through a Python file with `count == 1` assertions, then read the result
  back with `cat -A`.
* Build with `tools/ww_build.sh`, which sets the PATH, the temp dir and the
  git stamp and gates on make's own exit code. Typing `make` by hand costs
  three failed builds before you find it.

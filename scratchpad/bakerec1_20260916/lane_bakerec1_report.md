# Lane BAKEREC1 — the bake record `FO4CSLOD/<ws>/<ws>.lodb`

Tree `E:/Projects/NifskopeWildWastelandEdition`, branch main. Written incrementally.

**Exe at launch:** `release/NifSkope.exe` — 2026-09-16 22:48:35, 22,382,592 bytes (LAYOUT1's).
Clock read `date` 2026-09-17 00:22:43 CEDT at lane start.
`tasklist | grep -i -E "Fallout4|NifSkope"` → rc=1, nothing running. A
`release/NifSkope_inuse_2000.exe` from an earlier lane is on disk and is NOT mine to touch.

---

## 0. THE SPEC-GATE AUDIT, done before any code (skill `ww-spec-gate-audit`)

The brief asks for a NEW file called `<out>/FO4CSLOD/<ws>/<ws>.lodb`, plain text, with a
per-chunk input hash, and says "the bake ALREADY keeps a LEDGER of what it wrote (path +
checksum) … your `.lodb` is the bake's receipt and the per-chunk INPUT hash table; the ledger
is the OUTPUT digest table."

**That premise is wrong on the tree, and it is wrong in a way that matters.** The audit, with
the lines:

1. **The name `.lodb` is already taken, by the ledger itself.** `docs/LODGEN_LEDGER_FORMAT.md`
   is titled "The LOD bake ledger (`.lodb`) and incremental regeneration"; the file is
   `<out-dir>/<WorldspaceEdid>.lodb`, `src/lodgen.cpp:13728` `LODB_MAGIC = 0x42444F4C` ('LODB'),
   version 1, 16-byte header then compact JSON.
2. **The ledger is not only an output-digest table.** It already carries a per-chunk INPUT
   digest — `"inputs"` — computed by `lodgenChunkInputDigest()` (`src/lodgen.cpp:13597`), over
   very nearly the brief's own §6 recipe: the one-cell ring of LAND heights/colours/quadrant
   textures, the LTEX texture-set asset digests, every REFR in form-id order with raw position,
   rotation and scale bytes, the base's four MNAM paths and their asset digests, and the SCOL
   parts. The brief's §6 therefore describes something that exists.
3. **LAYOUT1 already ruled the merge, in writing, and the brief's own "Landed since" section
   did not carry it.** `docs/LODGEN_LEDGER_FORMAT.md` §1 (LAYOUT1's amendment, 2026-09-16) says:
   *"The ledger's new home is `FO4CSLOD/<ws>/<ws>.lodb` and it belongs to lane BAKEREC1, which
   owns the ledger."* `src/lodgenlayout.h` and `tests/spells/lodgen_layout.sh` both name that
   path as "lane BAKEREC1's spot". So the ruling on the table is ONE file, not two.

**The decision, and it is the one the brief's own rule forces:** the bake record and the ledger
are the SAME file. `.lodb` goes to **version 2**, moves to `FO4CSLOD/<ws>/<ws>.lodb` under the
FO4CS target, becomes plain text as the brief spells it, and GAINS the sections the ledger did
not have (the exe stamp, the five corpus hashes, the plugin lines with per-file hashes, the
resource lines, the switch lines, the census lines, the `end` line) while KEEPING the two it
had (the per-chunk input digest, the per-file output digests). Writing a second file with a
second per-chunk input hash would have been exactly the "second digest" the brief forbids, and
INCR1 — which reads both — would have had two rules to obey instead of one.

### The three conflicts this merge creates, each decided rather than stepped over

**(A) The date versus determinism.** The brief §1 wants a UTC ISO date on the version line. The
ledger's standing law (`docs/LODGEN_LEDGER_FORMAT.md` §1, "It is deterministic, and that is a
gate not a nicety") is that two full bakes of the same tree write a byte-identical `.lodb`.
Both cannot hold. **Decided:** the volatile fields are ISOLATED ON THEIR OWN LINES and named as
volatile — `baked<TAB><utc ISO>` and the `path` field of a `plugin`/`resource` line. Every other
line is deterministic. The gate asserts exactly that: two bakes of the same tree differ in the
`baked` line and in nothing else. That keeps the brief's date, keeps the law, and gives gate
(d)(iii) ("the mod folder renamed → record identical except the informational path field") a
statement it can actually test.

**(B) "The STOCK target writes none."** The ledger is written by EVERY region bake today,
deliberately (`src/nifcli.cpp:4284`: *"a feature that needs yesterday to have been clairvoyant
is not a feature"*), and `--incremental` refuses without it. Removing it from stock bakes would
silently retire `--incremental` for every stock user. **Decided, and stated as a divergence:**
the record is written by every region bake, as before. Under the FO4CS target (`--native`) it
goes to `FO4CSLOD/<ws>/<ws>.lodb` through `lodgenFo4csWorldDir()`; without it, it stays at its
v1 home `<out-dir>/<ws>.lodb`. The stock *engine* output — `.BTR`, `.BTO`, the chunk sheets —
is untouched either way, which is what gate (f) and `lodgen_native.sh` check 5 actually pin;
those gates already EXCUSE the `.lodb` from tree comparison by name and compare it field by
field instead.

**(C) Four existing harnesses parse the binary container.** `lodgen_defaults.sh` (`lodbcmp`),
`lodgen_layout.sh` leg (a), `lodgen_btofree_ledger.py` and `lodgen_native.sh` all do
`raw[16:]` + `json.loads`. Turning the container into text breaks all four. **Decided:** one
shared reader, `tests/spells/lodb_read.py`, parses the text record and emits the same JSON
shape those four already expect, and each of them calls it. That is fewer parsers than today,
not more.

### What the brief asked for that is NOT being built, and why

* **FNV-1a 64 for the per-chunk input hash.** The existing input digest is SHA-1, it is
  shipped, and `--incremental` compares against digests already on disk. Re-spelling it as
  FNV-1a would move every chunk hash for no gain and would be a second digest by another name.
  The record states the algorithm in its own header line so a reader never has to guess.
* **FNV-1a 64 for the per-file plugin hash.** This one IS new, so it is FNV-1a 64 exactly as
  the brief spells, and the gate recomputes it in independent Python.
* **"Reuse the spelling code `lodgen_defaults.sh` (a) already uses."** There is no such code:
  `lodgen_defaults.sh` (a) spells a default bake as a literal in a shell script. The record
  writes the ARGUMENT VECTOR verbatim, one token a line, which is the way back to reproducing
  the bake exactly and is the same vector `gLgSwitchDigest` is taken over.

---

## 1. What was built

One file, at version 2, in plain text.

**`<out>/FO4CSLOD/<ws>/<ws>.lodb`** — UTF-8, LF, one record a line, `key<TAB>fields`,
written **LAST** by every bake (not only incremental ones) so that its `end` line can
count the tree the bake actually produced. Eight sections:

| # | lines | what it carries |
|---|---|---|
| 1 | `lodb` `baked` `alg` `shape` | version, worldspace edid, the WW edition stamp and the exe's byte size; the wall clock; the four algorithm names; the worldspace form, `dim`, the region and `fo4cs`/`stock` |
| 2 | `hash` x5, `loadorder` | the five staleness hashes, copied from the `.lodo`/`.lodi` headers the same bake wrote, never recomputed |
| 3 | `plugin` | one a plugin, in load order: index, lower-cased base name, byte size, **FNV-1a 64 over the file's BYTES**, and the absolute path (informational) |
| 4 | `resource` | the resource stack in order: `folder`/`ba2`/`bsa`, the path as the stack spelled it, an archive's size and mtime |
| 5 | `switch` … `switches` | the argument vector token by token, then the SHA-1 `gLgSwitchDigest` takes over it |
| 6 | `chunk` / `out` | one `chunk` a chunk with its per-chunk input digest, sorted by `(cy,cx)`; one `out` per file it produced, path relative to **the record's own folder** and SHA-1 |
| 7 | `census` | every census line the bake printed, verbatim |
| 8 | `end` | the file count and byte count of the record's own folder |

The v1 BINARY container ('LODB' magic + 16-byte header + compact JSON) is **refused by
name** by both readers rather than mis-parsed.

### Why one file and not two

The brief could be read as asking for a second file beside the `.lodb`. It is one file at
a new version, because a second file would have had to carry a second per-chunk digest,
and lane INCR1 would then have two rules to obey about which chunks are stale. That
decision, and the three divergences it forces, are `docs/LODGEN_BAKE_RECORD.md` §0 and §6.

### The four volatile things

Two bakes of one tree write the same bytes **after normalisation**, and the exceptions sit
on named lines so a comparison can mask precisely them:

1. the `baked` line's value — a wall clock
2. the `plugin` line's last field — an absolute path, informational, never hashed
3. the whole `resource` line but its `kind` — absolute paths and archive mtimes
4. the `census` line beginning `stage times:` — **a wall clock too**

**The fourth was found by the gate, not by thinking about it.** Legs (a)–(g) were green
while the record still leaked `stage times: … meshes 61.4 s` against `61.5 s`; leg (h)
bakes twice into the same folder and failed on it. It is **recorded and masked, never
dropped**: an operator reading a record wants to know the bake took a minute, and dropping
it would put the census completeness floor permanently out of step with the census the
bake printed. `lodbNormalise()` (`src/lodbfile.cpp`) and `lodb_read.normalise()`
(`tests/spells/lodb_read.py`) are two independent implementations of the same masking.

### `--native-verify` now names the plugin that went stale

A `loadOrderHash` / `pluginCorpusHash` refusal is a fold over the whole list and cannot be
un-folded — on its own it says *something in thirty plugins moved*. `lodbNameTheStalePlugin()`
(`src/nativeemit.cpp`) reads the record beside the pair and appends a paragraph naming the
file and what happened: `was ADDED` / `was REMOVED` / `was REORDERED (it was N at the bake)` /
`was RESIZED (A bytes at the bake, B now) -- it was edited` / `was EDITED: its bytes hash
0x… now and hashed 0x… at the bake, at the same N bytes -- the load order cannot see this`.

That last one is the case `loadOrderHash` is **structurally blind to**: it folds the
lower-cased base name and the byte size, so a same-size in-place edit does not move it at
all. The byte hash is computed **only** when name and size still agree, which is the one
case the cheap fields cannot answer and the only case worth reading 330 MB for.

With no record: `(no .lodb bake record beside this pair, so the plugin that moved cannot be
named -- re-bake once with this build and it will be)`. With a record in which nothing
moved: it says so, which points the reader at the records instead of the load order.

### `lodgen --bake-record <ws.lodb> [<plugins>]`

A new verb. Reads a record and prints it as keyword lines, one fact a line — the output
lane INCR1 parses. It loads no ESM, so it answers in milliseconds. It re-counts the
record's folder off the disk and prints `endFilesNow` / `endBytesNow` / `endAgrees`, so the
record is checked against the tree rather than believed. Given a plugin list it diffs it
against the recorded one and **exits 1** when anything moved, printing `moved <n>` and one
sentence a plugin.

### One census line, no panel row

`bake-record:` — the path, then the record READ BACK OFF THE DISK by the same reader the
verb uses: plugin, resource, switch-token, chunk and census-line counts, the `end` line's
two numbers, and the record's own size. It is printed **after** the record is closed, so it
is the one census line the record does not itself carry, and a bake that dropped a section
says so here instead of being believed. A record that will not read back prints
`bake-record: <path> REFUSED ON READ-BACK -- <reason>`.

No panel row: the record is written by every bake, has no setting to expose, and the END
menu rule is basics only.

---

## 2. Every file this lane touched

### New

| file | what it is |
|---|---|
| `src/lodbfile.cpp` / `src/lodbfile.h` | the writer (`lodgenWriteLedger`), the reader (`lodgenReadLedger`), `lodbNormalise`, `lodbFileFnv1a64`, `lodbDiffPlugins`, `g_censusKeywords[]` |
| `tests/spells/lodb_read.py` | **the one Python reader.** Before this lane four harnesses each hand-parsed the v1 binary container; a format change would have been discovered by watching a harness fail on a Tuesday |
| `tests/spells/lodgen_bakerec_gate.py` | the measuring half: `sections`, `written-last`, `hashes`, `plugins`, `chunks`, `identical` |
| `tests/spells/lodgen_bakerec.sh` | legs (a)–(h); `LEGS=` a subset, `OUT=` keeps the bakes; refuses while `Fallout4.exe` is up |
| `docs/LODGEN_BAKE_RECORD.md` | the format, the determinism law, the verb, the refusal, the three stated divergences, and what lane INCR1 gets |

### Changed

| file | change |
|---|---|
| `src/nifcli.cpp` | `--bake-record` (parse, handler, help, and an exemption in the empty-file guard, which the verb needs because it reads a file and diffs an optional list rather than baking); the record-write block; the `bake-record:` census line |
| `src/nativeemit.cpp` | `lodbNameTheStalePlugin()`, appended to both the `pluginCorpusHash` and `loadOrderHash` STALE refusals |
| `tests/spells/lodgen_defaults.sh` | `lodbcmp` now uses the shared reader |
| `tests/spells/lodgen_layout.sh` | the ledger leg reads the shared reader; `LEDDIR` (the record's paths are relative to its own folder now); **the ledger's stray-file exemption removed** — it is an ordinary file under the one root |
| `tests/spells/lodgen_btofree_ledger.py` | `load()` via the shared reader; `canon()` strips a leading `../` |
| `tests/spells/lodgen_native.sh` | check 5 finds the record's `SREC`/`NREC` through the shared reader |
| `tests/spells/lodgen_btofree.sh` | `recof()` / `recv1()`; both ledger legs **SKIP with the reason** on a rung record, because a rung exe writes v1 binary and this one writes v2 text — a cross-version comparison is impossible by design, not a failure |
| `docs/LODGEN_CENSUS.md` | §6.1 gained the `bake-record:` row |
| `docs/LODGEN_NATIVE_LODO_LODI.md` | §8.1 gained the stale-plugin paragraph |
| `docs/FO4CS_IMPROVED_LOD_PLAN.md` | §5 gained row 19, closed |
| `docs/LODGEN_LEDGER_FORMAT.md` | amended: the container below is v1 and is no longer written; the file's new home; the determinism law restated as *byte-identical after normalisation* with the four volatile things named |

Nothing was committed, nothing was stashed, and `WW_CHANGES.md` / `HANDOFF.md` were not
edited — their text is in §6 below.

---

## 3. The build

| | |
|---|---|
| `release/NifSkope.exe` | **22,459,904 bytes, 2026-09-17 01:17:01**, rev `720762a` |
| carries ARCHLOCK1 | **YES.** `make` recompiled `src/gamemanager.cpp` (source 01:02:33 → `GeneratedFiles/.obj/gamemanager.o` 01:15:05) and linked it at 01:17:01. The ARCHLOCK1 sites (`find_file`, `get_file` releasing the archive read lock before recursing to the parent, so the parent's lazy `init_archives` can take the write lock on the same `QReadWriteLock`) are in the source and in the object this exe was linked from. It was not reverted, not touched, not looked at beyond confirming it compiled |
| rungs kept | `release/NifSkope.before_archlock1.exe` (22,459,904 B, 01:00:05 — this lane's own pre-ARCHLOCK1 build, kept, never deleted) and `release/NifSkope.before_bakerec1.exe` (22,382,592 B, 00:56:51 — the pre-lane rung leg (f) compares against) |
| the game | `Fallout4.exe` was **not** running at 01:14:34 when the build started and the link succeeded at 01:17:01. It came up between 01:14:34 and 01:17:34 |

**The build failed once first, and it is worth writing down.** `make -f Makefile.Release`
ran under `MSYSTEM=UCRT64 bash -lc`, every object compiled, and the link died at
`make: *** [Makefile.Release:1456: release/NifSkope.exe] Error 127` — the recipe's
`git -C … rev-parse --short HEAD > release/build_rev.txt` step, with `/bin/sh: line 1: git:
command not found`. A **login** MSYS2 shell does not inherit Git-for-Windows' PATH. The fix
is one export (`PATH=$PATH:/e/Tools/GIT/mingw64/bin`), and the objects from the failed run
were reused, so the second run linked in seconds. Error 127 at the very last recipe line is
easy to read as "the build is broken" when in fact every object was already on disk.

---

## 4. The verb, measured on this exe

`release/NifSkope.exe -no-gui lodgen --bake-record <abs path to Commonwealth.lodb>`, run at
01:22 against the 5,854-byte record from the 23:07 bake (53 lines, sha1
`b6b1bb4ad4ec4c89c1e18c423abb8ef2f2ea142d`). Exit 0. It printed, in order:

```
bake-record path …/FO4CSLOD/Commonwealth/Commonwealth.lodb
bake-record worldspace Commonwealth        bake-record worldspaceForm 3c
bake-record dim 4                          bake-record region -20 24 -20 24
bake-record target fo4cs                   bake-record exe 0.3.3+720762a
bake-record exeBytes 22459904              bake-record baked 2026-09-16T23:07:16Z
bake-record switchesDigest f9fc7ff7dc3830ac7cb94c6ee7c8137025a77a43
bake-record hash loadOrderHash    a056a596e2bb16e7
bake-record hash pluginCorpusHash d8337d022f637f22
bake-record hash objectCorpusHash 6324ed6a35cde4ea
bake-record hash modelCorpusHash  340b99130f5a0c86
bake-record hash cardCorpusHash   0000000000000000
bake-record plugins 1
bake-record plugin 0 fallout4.esm 330776576 f1433cd4cadce243 X:/…/Fallout4.esm
bake-record resources 0
bake-record switchTokens 22
bake-record command <the whole argument vector on one line>
bake-record chunks 1      bake-record outputs 5
bake-record chunk -20 24 4 4795701094b9aa52aadab1d855c92ab44a4c5454
bake-record census 12     bake-record censusLine <x12>
bake-record endFiles 9    bake-record endBytes 228557564
bake-record endFilesNow 9 bake-record endBytesNow 228557564
bake-record endAgrees 1
bake-record diff n/a (no plugin list given: pass one positionally, or --plugins-txt / --mo2)
```

`endAgrees 1` is the record checked against the disk rather than believed: the verb walks
the record's own folder itself and re-counts it.

`cardCorpusHash 0000000000000000` is a FACT, not a defect: this bake baked no impostors, the
`.lodo` header carries zero, and the record's law is that it never disagrees with the pair it
was written beside. The gate was changed to require the SHAPE (16 hex digits) plus a real
floor — at least three of the five distinct and non-zero — after it wrongly flagged this.

### A finding that is not mine, but the verb is where an operator will meet it

**In batch mode every relative path resolves against the EXE's folder, not the shell's.**
`nifskopeCliMain` calls `QDir::setCurrent( QCoreApplication::applicationDirPath() )` at
`src/nifcli.cpp:179` (pre-existing, and deliberate — it is how `nif.xml` is found). So
`--bake-record scratchpad/…/Commonwealth.lodb` from the repo root is refused with
`bake-record REFUSED no bake record at scratchpad/…/Commonwealth.lodb` while the file is
plainly there, and the same name copied into `release/` reads fine. Measured both ways.

It affects every path switch the CLI has, not just this one, so it is not this lane's to
change under a running game. The cheap fix for the next build is one clause in the refusal:
name the directory it looked in. Left as a finding rather than done, because a rebuild while
`Fallout4.exe` is up is against the standing rule.

---

## 5. The gates

`tests/spells/lodgen_bakerec.sh`, legs (a)–(h). Region `-20 24 -20 24`, `--dim 4`,
Commonwealth, `--native` + `--cover` + `--arrays` + `--road-detail 1`.

### Measured, on the 01:00 exe (log `scratchpad/bakerec1_20260916/gate_abgh.log`)

| leg | result |
|---|---|
| (a) written last, eight sections, `end` against a `find`, census floor | **24 checks, 0 failures** — plus 1 check that the record is at `FO4CSLOD/Commonwealth/Commonwealth.lodb` and 1 that it is the LAST file the bake wrote |
| (b) the five hashes equal the `.lodo`/`.lodi` headers | **8 checks, 0 failures** |
| (g) one parser | **ok** — nothing in `tests/` parses the container by hand any more; 6 files import `lodb_read.py` |
| (h) two bakes of one tree | **3 checks, 2 FAILURES** — and the failure was real |

Leg (h)'s failure, verbatim:

```
FAIL normalised, the two records are byte-identical -- 5792 vs 5792 bytes
  first difference, line 51:
    a: census	stage times: landscape 0.0 s, meshes 61.4 s, textures 0.9 s, impostors 0.0 s
    b: census	stage times: landscape 0.0 s, meshes 61.5 s, textures 0.8 s, impostors 0.0 s
FAIL and they differ only on baked / resource / plugin lines -- kinds that differ: baked, census
```

That is the fourth volatile thing, and it is the whole reason the exe was rebuilt: the mask
for it is in `src/lodbfile.cpp` and `tests/spells/lodb_read.py` now, and the gate was
tightened at the same time so a `census` line may differ ONLY when it begins `stage times:`
— any other census line moving is still a red.

### PENDING, on the 01:17 exe

**`Fallout4.exe` came up between 01:14:34 and 01:17:34** — after the link, before the
re-run. The harness refuses while the game is up (`REFUSED: Fallout4.exe is running`) and
that refusal is correct: the legs bake, leg (d) copies about 1.1 GB and bakes four times.
Nothing was killed and nothing was worked around.

So these are owed, and nothing below has been measured on the new exe:

| leg | what it will show |
|---|---|
| (a)(b)(c)(g) | the same as above, re-run, with (c) — the plugin lines against `stat` and an independent Python FNV-1a 64 — measured for the first time |
| (h) | green, if the fourth mask is right. It is the one leg whose result is not already known |
| (d) | the three refuters: a placement MOVED inside the region moves the chunk digest; a record edited OUTSIDE it moves no chunk digest but DOES move the plugin's byte hash; the mod folder renamed and a resource touched normalises to no change |
| (e) | `--native-verify` naming the plugin — and the second picture, which is a picture of that output |
| (f) | the stock target byte-identical to `release/NifSkope.before_bakerec1.exe`, the record aside |

### The other harnesses this change reaches, also PENDING

`lodgen_layout.sh` (the ledger leg and the stray-file exemption both changed),
`lodgen_defaults.sh` (`lodbcmp`), `lodgen_native.sh` (check 5), `lodgen_btofree.sh` (two
legs now SKIP by name on a v1 rung record). Each was picked because this lane edited the
code it measures; the panel and render harnesses were not, because nothing they touch moved.

---

## 6. Changelog text for the director

Not spliced by this lane: `WW_CHANGES.md` and `HANDOFF.md` were not opened for writing.

### For `WW_CHANGES.md`

```markdown
### The bake record — every bake writes down what it was made of (2026-09-17, lane BAKEREC1)

Every LOD bake now writes `Data\FO4CSLOD\<Worldspace>\<Worldspace>.lodb`, a plain-text
record of what went into it: the five staleness hashes, the load order, the resource
stack, the command line token by token, each chunk's input digest and output files with
their SHA-1s, every census line the bake printed, and a file/byte count of its own folder.
It is UTF-8 with LF line endings and one `key<TAB>fields` record a line, so it opens in any
text editor. It is written LAST, so its count is of the tree the bake actually produced.

**What it is for.** Until now, `lodgen --native-verify` could tell you a `.lodo`/`.lodi`
pair had gone stale but not WHY: the load-order hash is a fold over every plugin and cannot
be un-folded, so the answer was "something changed". With a record beside the pair the
refusal names the file and what happened to it — added, removed, reordered, resized, or
**edited in place at the same byte size**, which the load-order hash cannot see at all
because it only folds each plugin's name and length. The record carries an FNV-1a 64 over
each plugin's actual bytes, and that hash is computed only for a plugin whose name and size
still match, so the expensive read happens exactly when the cheap fields cannot answer.

**New verb.** `NifSkope -no-gui lodgen --bake-record <ws.lodb> [<plugins>]` prints the whole
record as one fact a line, re-counts the folder off the disk and says whether the record
still agrees with it, and — given a plugin list — prints one sentence per plugin that moved
and exits 1. It loads no ESM, so it answers in milliseconds. NOTE: in batch mode NifSkope
resolves relative paths against its own folder, so give this one an absolute path.

**New census line.** `bake-record:` names the record's path and then reads it back off the
disk, so a bake that dropped a section says so instead of being believed.

**Determinism.** Two bakes of the same tree write the same record, except for four things
that are wall clocks or absolute paths: the `baked` time, a plugin's path, a resource line,
and the `census stage times:` line. Those are MASKED for comparison, never dropped, so a
record that lost a line still shows up as different.

**Format changed.** The old `.lodb` was a small binary container; this is version 2 and it
is text. A version 1 file is refused by name with the words "re-bake once and every bake
writes it", never half-parsed. There is still exactly one ledger file.

Documentation: `docs/LODGEN_BAKE_RECORD.md`. Gate: `tests/spells/lodgen_bakerec.sh`.
No new panel row: there is no setting to expose.
```

### For `HANDOFF.md` (a LANDED block for the top)

```markdown
## LANDED 2026-09-17 01:17 — lane BAKEREC1, the bake record `FO4CSLOD/<ws>/<ws>.lodb`

`release/NifSkope.exe` 22,459,904 bytes, 2026-09-17 01:17:01, rev 720762a. **Carries
ARCHLOCK1** (`make` recompiled `src/gamemanager.cpp` at 01:15:05 from the 01:02:33 source
and linked it). Rungs kept: `release/NifSkope.before_archlock1.exe` (22,459,904 B,
01:00:05) and `release/NifSkope.before_bakerec1.exe` (22,382,592 B, 00:56:51).

WHAT LANDED: the `.lodb` bake record at version 2 — plain text, UTF-8, LF,
`key<TAB>fields`, eight sections, written LAST by every bake under
`FO4CSLOD/<ws>/`. `--native-verify` now names the plugin that went stale and says
whether it was added, removed, reordered, resized or EDITED AT THE SAME SIZE (the case
`loadOrderHash` is structurally blind to). New verb `lodgen --bake-record`. New census line
`bake-record:`. No panel row. New: `src/lodbfile.{h,cpp}`, `tests/spells/lodb_read.py`,
`tests/spells/lodgen_bakerec_gate.py`, `tests/spells/lodgen_bakerec.sh`,
`docs/LODGEN_BAKE_RECORD.md`. Four other harnesses now share the one Python reader.

**GATES PENDING.** `Fallout4.exe` came up between 01:14:34 and 01:17:34, after the link.
On the 01:00 exe legs (a) 24/0, (b) 8/0, (g) ok; leg (h) found a real wall-clock leak
(`stage times:`), which is what the 01:17 exe fixes and what the re-run must confirm. Legs
(c)(d)(e)(f) and the four neighbouring harnesses have not been run on this exe. The harness
refuses while the game is up, and nothing was killed or worked around. Resume with:
`OUT=<dir> bash tests/spells/lodgen_bakerec.sh`.

FOR LANE INCR1: `docs/LODGEN_BAKE_RECORD.md` §7. The record is the input; `--bake-record`
is the way to read it without linking against us.
```

---

## 7. Mistakes

Five, written into the top of root `MISTAKES.md` under
`## 2026-09-17 01:1x -- lane BAKEREC1`. In short:

1. **Bash heredocs eat one backslash in this environment**, so five
   `QStringLiteral( "\n  …" )` in `src/nativeemit.cpp` were written with a REAL newline
   inside the quotes and the first build died. Proved directly: a heredoc script containing
   `t = 'pair\n"\n'` printed a `repr` with two real newlines and no backslashes. The
   script reported success. Only the compiler saw it. **Standing fix:** patch scripts go to
   disk with the Write tool, or build every escape from `chr(92)`/`chr(10)`/`chr(9)`.
2. **Called a zero hash a defect.** `cardCorpusHash 0000000000000000` is what the `.lodo`
   header says, and the record's law is that it never disagrees with the pair. The GATE was
   wrong. Found by checking the header before touching the writer, which is the only reason
   the writer was not "corrected" into lying.
3. **Shipped a record that leaked a wall clock while seven legs were green.** Found by leg
   (h), which had itself to be fixed first — it was baking into two DIFFERENT out-dirs, so
   it was measuring the folder name rather than the bake.
4. **Compared a rung exe's v1 binary record against this build's v2 text one and called it
   a failure.** It is impossible by construction. Both legs now SKIP with the reason.
5. **Read `make … Error 127` as a broken build** when every object had compiled and the
   failing recipe line was `git rev-parse` — a login MSYS2 shell does not inherit
   Git-for-Windows' PATH.

Three of the five were found by a gate rather than by thinking, which is the argument for
the gate.

---

## 8. Skills

**Used:** `ww-spec-gate-audit` (§0 of this report is its output, done before any code),
`nifskope-ww-lodgen`, `nifskope-ww-build-verify`, `ww-test-harness-add`,
`ww-census-contract` (the `bake-record:` line's WRITTEN/MOVING pair and its gate),
`ww-contract-provenance`, `ww-module-off-is-identical` (leg (f)), `ww-panel-run-harness` §9
(the MSYS2 variable-dropping rule), `ww-move-a-written-path` (the record moving under
`FO4CSLOD/<ws>/` and the paths recorded INSIDE it moving with it).

**Two procedures this lane repeated that no skill covers.** Drafts are in
`scratchpad/bakerec1_20260916/skills_proposed/` for the director to place:

* **`ww-one-reader-per-format`** — when a file format is parsed in more than one place,
  write ONE reader, make every consumer import it, and gate it with a grep that fails if
  anyone hand-parses again. This lane found four independent parsers of the same binary
  container; a format change would have been discovered by watching an unrelated harness
  fail. The gate needs the care this one needed: exclude the reader's own comment and the
  harness's own pattern line, or it matches itself.
* **`ww-volatile-field-law`** — how to make a written artefact deterministic when it MUST
  record wall clocks and absolute paths: put each volatile thing on a named line or a named
  field, MASK it rather than drop it (a dropped line hides a record that lost one), write
  the mask twice in two languages, and gate it by repeating the IDENTICAL command — same
  switches, same output folder — because a repeat that changes anything measures that
  instead. The count of volatile things is a number in a doc, a header comment and two
  implementations, and all four move together.

---

## 9. Pictures

`scratchpad/bakerec1_20260916/images/`

| file | what it is | state |
|---|---|---|
| `bake_record_text_view.png` | the whole record — all 53 lines, all eight sections — in a monospace text view, 1084x1174 | **done** |
| `native_verify_names_the_plugin.png` | `--native-verify` refusing a stale pair and naming the plugin | **PENDING**, it is leg (e)'s output and leg (e) needs a bake |

The first is **rendered from the file's own bytes, not a screenshot**: the picture's caption
carries the path, `5854 bytes, 53 lines, sha1 b6b1bb4ad4ec4c89c1e18c423abb8ef2f2ea142d`, so
it can be checked against the disk. It was drawn rather than captured deliberately — the
game is running on the machine and this lane is not opening windows over someone's session
to photograph a text file. The `.lodb` in the picture is the one the 23:07 bake wrote, and
its format is byte-for-byte what the 01:17 exe writes (the fourth mask changes how records
are COMPARED, not what is written).

---

## 10. Source and binary agree

Both post-gate edits to `src/lodbfile.cpp` (01:12:59) and `src/lodbfile.h` (01:13:17) were
made BEFORE the build started at 01:14:53, so no source file in this lane is newer than the
exe. Docs edited after the link do not compile into anything.

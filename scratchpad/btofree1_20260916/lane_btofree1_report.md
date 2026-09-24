# Lane BTOFREE1 — the FO4CS target writes no `.BTO` into the mod folder

Date: 2026-09-16 (clock read with `date` at 16:37:30 CEDT and again per section).

## 0. Exe at launch

`ls -la release/NifSkope.exe` before any work of this lane:

```
-rwxr-xr-x 1 bungo 197609 22341632 Sep 16 15:53 release/NifSkope.exe
```

22,341,632 bytes, 2026-09-16 15:53:30 — the exe lane NATIVE1c built.

Rung copy taken at 16:38 (`cp -p`), md5 identical to the source:

```
-rwxr-xr-x 1 bungo 197609 22341632 Sep 16 15:53 release/NifSkope.before_btofree1.exe
49d8369243013583e47534c0dc2b3c10 *release/NifSkope.exe
49d8369243013583e47534c0dc2b3c10 *release/NifSkope.before_btofree1.exe
```

Game check before the rung copy and before every build: `tasklist | grep -i -E "Fallout4|NifSkope"` → `rc=1`,
no Fallout4 and no NifSkope of bungo's running.

## 1. Where the chunks go now

### The shape of it

Under the FO4CS target the `.BTO` chunk is **scaffolding, not output**. Five passes open it back:

| pass | where it opens the chunk (and its `<chunk>.BTO.manifest.txt`) |
|---|---|
| texture arrays | `src/lodgen.cpp:4836` |
| atlas | `src/lodgen.cpp:5132` |
| shape merge (`lodgenMergeChunkShapes`) | `src/lodgen.cpp:12439` |
| far-ring cut (`lodgenSimplifyFarRings`) | `src/lodgen.cpp:12869` |
| card arrays | `src/lodgen.cpp:13235`, `13487` |

Nothing **after** the bake reads it. So it is now built in a scratch folder:

```
<mod folder>/lodgen_bto_scratch/Commonwealth.4.-20.24.BTO
                               /Commonwealth.4.-20.24.BTO.manifest.txt
```

every read-back above runs there with the sidecar beside the chunk exactly as before, and then the
teardown moves each **manifest** into `meshes/terrain/<worldspace>/` (bungo's open call is to keep
the manifest) and deletes the chunks and the folder.

The folder lives **inside the mod folder**, not in `%TEMP%`: an interrupted bake leaves its
scaffolding where an operator can see it, and a run that finds one left by a dead bake removes it
first, which makes that self-healing rather than a second failure.

### One implementation, two front ends

The teardown is a single function, `lodgenDropBtoScratch()` in `src/lodgenchunkpass.cpp`, called by
the CLI (`src/nifcli.cpp`) and by the panel (`src/lodgenmanager.cpp`). Two copies of that loop would
be a byte difference waiting to happen, and `lodgen_byte_gate.sh` phase (c) compares what the two
front ends leave on disk file by file.

### The way back

* CLI: `--keep-bto`. The chunks land in the mod folder **byte for byte** as a bake before today left
  them.
* Panel: **Object modules → "Keep legacy .BTO chunks"** (`LodgenKeepBtoCheck`, key `keepBto`),
  **default OFF**, shown under the FO4CS target only (a hidden row reads as its default, so the stock
  target is untouched by construction).

### The stock target does not move

`--keep-bto` is only consulted when `--native <dir>` is given; the panel only creates a scratch when
`wantNative()`. A bake with no `--native` never reaches the branch. The hard gate is byte identity of
the whole stock output, in `lodgen_btofree.sh` leg (c) — numbers in §2.

### The census says it

`lodgenBakeCensusLine()` (shared by the panel's result label and the CLI's last line) now ends with
one of three clauses, and each is written **and moves**:

```
bto n/a (no object pass ran)
bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed          <- --keep-bto
bto built in scratch <dir>, 9 chunk(s), 9 dropped, 123456789 bytes freed   <- default
```

The count and the bytes are read from `QFileInfo` on the files themselves, never predicted from the
job list. The CLI additionally prints a dedicated line:

```
bto scratch: N chunk(s) built in <dir>, N removed, N manifest sidecar(s) kept, B bytes freed
```

### The ledger

`.lodb` records **outputs**. With a scratch the `.BTO` is not one, so it is not digested — digesting a
file that is about to be deleted is the defect that once forced full rebakes and the ledger's own
comment records it. The **manifest** is recorded at its final path in the mod folder, and the digest
is taken at ledger-write time, after the move. `.BTR` is untouched either way.

## 2. Gates

### The gate I own: `tests/spells/native_open.sh` — 17 checks, 0 failures, 2 skipped, PASS

**(i) The fixture was a version-3 file the current exe refuses by name.** `native_open.sh` reads
`scratchpad/showcase1_20260912/out/look/native/`, baked 2026-09-12. NATIVE1c's `.lodo` version 4
refuses version 3 **by name**, so the gate was testing a refusal. Re-baked with the rung exe
(`bake_look.sh`, 105 s, exit 0):

| file | before (2026-09-12) | after (2026-09-16) |
|---|---|---|
| `Commonwealth.lodo` | 9,657,316 B, `LODO` v3 | **225,399,755 B, v4** |
| `Commonwealth.lodi` | 128,256 B, v3 | **134,598 B, v5** |
| `mesh_report.txt` | 435,204 B | 911,262 B |

The old pair is preserved in `scratchpad/btofree1_20260916/fixture_backup/`. The size jump is
NATIVE1c's `--library near` default (level 0 of the ladder is the base's near `MODL` model).

**(ii) The object-coverage failure the director assigned me was a wrong check, and the measurement
says so.** Check (c) rendered the `.lodi` scene and the chunk's own `.BTO` from one camera and
demanded `IoU >= 0.95`. On the re-baked fixture it read **0.6190**; the director's long-standing
number was 0.8179.

First question answered: *why did the number move?* I baked the same region with the same switches
and the one difference `--library mnam` (the way back to the LOD-mesh library) —
`scratchpad/btofree1_20260916/mnam_bake.sh`:

| library | IoU | covered (B in A) | area (A / B) |
|---|---|---|---|
| `mnam` (pre-2026-09-16 default) | **0.8179** | 0.9874 | 1.195 |
| `near` (NATIVE1c's default, today) | **0.6190** | 0.9604 | 1.512 |

0.8179 to four decimals is the director's number. **The near-library ruling explains the move
entirely, and the check never passed its own bar — not once, on any exe.** A bar that has never been
met is not a measurement of the thing it compares.

Second question: *what is the residual 0.8179 made of?* `scratchpad/btofree1_20260916/iou_analyse.py`
on the `mnam` pair:

* `.lodi` 219,500 px, `.BTO` 183,734 px, intersection 181,416.
* **98.74 % of the `.BTO` is drawn by the `.lodi`.** The disagreement is one-sided.
* The excess is 38,084 px, and **100.0 % of it lies within 16 px of a pixel the two share**, 92.9 %
  within 4 px, 58.7 % within 1 px.
* It forms 9,494 connected blobs whose largest is **143 px**, and **not one blob reaches 200 px**.

That is one silhouette drawn a hair wider than the other, everywhere — not an object one side draws
and the other misses. It is what an instance scene of whole library models must look like beside a
chunk mesh that the bake merged, far-ring cut and clipped to the chunk box. Their outlines differ by
construction, so an equal-area bar (IoU) asks for something neither side promises.

**The rewrite**, with floors that were measured and refuters that fire:

| what it asks | floor | today (`near`) | `mnam` |
|---|---|---|---|
| `COVER` — does the scene draw everything the chunk draws? | `>= 0.90` | **0.9604** | 0.9874 |
| `FAT` — does it do that without drawing the world? | `<= 2.00` | **1.5118** | 1.1947 |

Each is vacuous alone and the pair is not, which is shown rather than asserted — three refuters, all
in the harness, all measured on the live run:

| refuter | measured | verdict |
|---|---|---|
| the same `.BTO` mirrored in Y | `COVERFLIP` **0.1800** | below the 0.50 floor — the check is orientation-sensitive |
| a solid frame in place of the scene | `COVER` **1.0000**, `FAT` **5.5119** | perfect coverage, caught by the area bar |
| a **different** chunk's `.BTO` | `COVER` **0.3750** | below the 0.90 floor |

`IOU` is still printed, because every earlier run of this gate printed it and a number that vanishes
from a log is a number nobody can compare; nothing is gated on it.

Check (c) went from 2 checks to 5, so the suite went **14 → 17**, and the run at 17:01 reads
`17 checks, 0 failures, 2 skipped` / `PASS`
(`scratchpad/btofree1_20260916/native_open_after.log`). The two skips are the same two as before:
the byte-identity leg of (a), which wants `release/NifSkope.before_nativeview1.exe` (not on disk),
and the manifest leg of (b), which wants `GBAKE`.

### The gate the brief asked for: `tests/spells/lodgen_btofree.sh` — 23 checks, 0 failures, PASS

New file, 13,839 B, LF-only. Five bakes of the same one chunk — the rung's FO4CS bake, the rung's
stock bake, this exe's FO4CS default, this exe's `--keep-bto`, this exe's stock — at
`REGION="-20 24 -19 25" DIM=4`, both taken from the environment so a lane that wants a wider
statement gets it without editing the gate. Run 17:20:33 → 17:24:27 on the 17:11:17 exe; full log at
`scratchpad/btofree1_20260916/lodgen_btofree.log`, the five trees left on disk under
`btofree_work/`.

**(a) the FO4CS default leaves our types and nothing else.** `.BTO` count under the mod folder
**0**; no `lodgen_bto_scratch` folder behind it; manifest sidecars kept **1**; the extension set
under the whole mod folder reads `btr dds lodb lodi lodm lodo txt` — measured, not asserted.
**The refuter**: the rung's identical bake wrote **1** `.BTO` here, so the three checks above are not
congratulating themselves on an empty folder. Against the rung, **13 files identical, 0 differ,
0 only on either side**, with `Commonwealth.lodo` (225,399,755 B) and `Commonwealth.lodi` (41,638 B)
called out by name.

**(b) `--keep-bto` is the exact way back.** 1 chunk in the mod folder, no scratch folder ever
created, and **14 files identical, 0 differ** against the rung — the chunk included. The way back is
byte-for-byte, not approximately.

**(c) the stock target does not move.** 1 `.BTO`, no scratch folder, **13 files identical,
0 differ** against the rung's stock bake. That is the brief's hard gate and it is green on the whole
tree, not a sample.

**(d) the census clause is written AND it moves.** Read off the three bakes:

    default   : bto built in scratch <...>/drop/lodgen_bto_scratch, 1 chunk(s), 1 dropped, 860743 bytes freed
    --keep-bto: bto built in the mod folder, 1 chunk(s), 0 dropped, 0 bytes freed
    stock     : bto built in the mod folder, 1 chunk(s), 0 dropped, 0 bytes freed

A counter that is only ever zero is not a counter, so the gate asks the dropped count and the
bytes-freed count to **move off zero** (1 and 860,743) on the default bake and to **read zero** on
the way back. The CLI's own `bto scratch:` line is required present for the drop bake and required
**absent** for `--keep-bto`.

**The two legs that are not `cmp`, and why.** `Commonwealth.lodb` is the ledger, and it is compared
field by field by `tests/spells/lodgen_btofree_ledger.py` (new, 4,285 B, LF-only) instead of byte by
byte, because on this file "the rung's bytes" is the wrong question twice over and both times the
ledger is doing its job:

* **the drop bake** (rung 6 output rows, 1 of them `.BTO`; this bake 5 rows, 0 of them `.BTO`) must
  NOT record a chunk it deleted — digesting a file that is about to be deleted is the defect that
  once forced full rebakes. The comparator checks the rung DID carry `.BTO` rows (non-vacuous), that
  this bake carries none, that **every other recorded file matches digest for digest**, and that the
  manifest row survives **with the digest it had before the move**.
* **the `--keep-bto` bake** differs from the rung at one character, 632: the `switches` field,
  `6f9a266c179d` → `deaac40aee43`. That is a digest of the command line and `--keep-bto` is one more
  token on it. It MUST move, or an `--incremental` run would reuse chunks baked by a command line
  that asked for different files on disk. So the gate asks for the difference rather than excusing
  it: identical once `switches` is removed, AND `switches` changed.

I found both of these as failures — the gate read 21 checks, 2 failures on its first run against
this exe — and neither was fixed by loosening a comparison. The ledger legs say more than the sweep
they replaced, and they are the reason the count went 21 → 23.

### The rest of the chain

Every count below was read out of the run’s own log by
`scratchpad/btofree1_20260916/report_chain.py`, not typed from a scroll-back, and every run
is against the exe at the top of this report (22,356,992 B, 2026-09-16 17:11:17). The logs are under
`scratchpad/btofree1_20260916/chain/`.

| gate | this exe | floor / note |
|---|---|---|
| `native_open.sh` | **17 checks, 0 failures, 2 skipped** | the gate I own; check (c) rewritten, 14 -> 17 |
| `lodgen_native.sh` | **125 checks, 0 failures, 2 skips** | floor 120/0/2; check 4 now spells --keep-bto, three checks added |
| `lodgen_ladder.sh` | **22 checks, 0 failures, 0 skips** | floor 22/0; untouched by this lane |
| `lodgen_native_baseline.sh --check` | **25 files in the baseline, 25 baked, 0 differ** | floor 25 files 0 differ |
| `lodgen_defaults.sh` | **28 checks, 0 failures** | floor 28/0 |
| `lod_generation.sh` | **124 checks, 0 failures** | the panel self-test; three new checks for the new row |
| `lodgen_byte_gate.sh` (b) | **128 (floor 125), failures: 0** | the per-row sweep; the new row is in it |
| `lodgen_btofree.sh` | **23 checks, 0 failures** | NEW, the brief’s item 4 |
| `lodgen_native_decode.py` | **6 pair(s), 36 checks, 0 failures** | floor 6 pairs / 36 checks / 0 failures |

### `lodgen_byte_gate.sh` phases (b) and (c) — (b) 128/0, (c) two differences that are not mine

**(b), the per-row sweep: 128 checks, 0 failures** against a floor of 125 (the rung reads exactly
125, so the new row raised it by three). The new row gets a verdict of its own out of the sweep
rather than out of me: `row keepBto false -> true: bytes MOVED (15 files, 266,282,955 bytes,
58,569 ms)`.

**(c), the panel's tree against the command line's, file by name: 15 identical, 2 differ, 0
missing.** The `.BTO` leg is green — *dropped by both*, no scratch folder on either side, and the
manifest sidecar byte-identical — and the two that differ are `Commonwealth.4.-20.24.DDS`
(174,888 B both sides, whole payload different from byte 129 on, 123,515 differing bytes) and
`Commonwealth.lodi` (41,638 B both sides).

**Neither is this lane’s.** I ran the rung exe — the 15:53:30 build, before a line of this work —
through the same `PHASES=bc`, and its phase (c) reads:

    DIFFERS: Commonwealth.4.-20.24.DDS (174888 vs 174888 bytes)
    DIFFERS: Commonwealth.lodi (41638 vs 41638 bytes)
    panel vs command line: 14 identical, 3 differ, 0 missing

The same two files, on an exe that has never heard of a scratch folder. (Its third difference is
`STILL THERE: Commonwealth.4.-20.24.BTO (panel yes, cli yes)` — the old exe run through the new
gate, which is the row doing its job.) So the count went **14 identical / 3 differ → 15 identical /
2 differ**: this lane removed one of the three and touched neither of the others.

**And the panel’s own output did not move by a byte.** I copied this exe’s panel tree aside before
the rung run (`chain/panel_base_newexe/`) and compared the two file by file:

    PANEL rung vs new (the .BTO excluded): 14 identical, 0 differ, 0 only-rung, 0 only-new
    no scratch folder in either panel tree

Fourteen files, byte for byte, including both files phase (c) complains about. The **only** change to
what a panel FO4CS bake leaves on disk is the 860,743-byte chunk that is gone.

**Whose the two are.** I did not chase them, because the brief tells me not to take red that is not
mine and because neither is reachable from anything this lane touched. What is measurable about them,
for whoever does pick them up: the `.lodi` is NATIVE1c’s file (v3 → v5 the same day, `--library
near` the same day) and the chunk diffuse is the one land texture of the three that moved, with
`_data` and `_msn` byte-identical, which points at the cover/land pass rather than at the chunk
writer. Phase (c) was already failing on the rung; it is failing less now.

## 3. Build and chain

| | |
|---|---|
| route | `bash tools/ww_build.sh <sources>` — the in-tree gated chain (game-up check, the exe renamed aside, `make -j2` on **its own exit code**, exe-newer-than-sources, the link-time copies) |
| game | `Fallout4.exe` checked **down** before the build and before every harness run. It never came up. |
| result | `BUILD-RC=0`, log at `scratchpad/btofree1_20260916/build.log` |
| exe before | 22,341,632 B, 2026-09-16 15:53:30 (NATIVE1c's) |
| exe after | **22,356,992 B, 2026-09-16 17:11:17** |

**A NifSkope window was holding the exe and it was not killed.** `release/NifSkope.exe` was locked by
pid 2000, started 16:53:38 with an argument-free command line — not one of this lane's harness
invocations, every one of which carries `--port` and a `WW_*_TEST` variable. It was therefore treated
as bungo's own window, and `ww_build.sh` did what the skill says to do: renamed the exe aside as
`release/NifSkope_inuse_2000.exe` (Windows permits the rename) and linked the new one beside it. His
process kept running on the old image. That file is now deletable — the window was closed some time
before 18:34 — but deleting an exe is not this lane's call, so it is left there and named in
`CHANGED_FILES.txt`.

**One instance, second monitor, never foregrounded.** Every GUI harness run in this lane went through
`tests/spells/_harness.sh` (`WW_WINDOW_AT`, `--port <unused>`), one at a time: `chain.sh` runs its
steps strictly in sequence and nothing in it is parallel. No `SetForegroundWindow`, ever.

**Git untouched.** Nothing committed, nothing stashed, no branch, no index entry. The only files that
moved are the ones in `CHANGED_FILES.txt`.

## 4. Owed / red / bungo's calls

**Red that is NOT mine, untouched, and still red.** I ran neither of these into the ground and
changed no line either of them reads:

* `tests/spells/lodgen_ground_cover.sh` -- 4 grass-feature failures (C2 x3, C6a, C9, C16). Owner:
  whoever the director assigns; not this lane. Nothing in this lane reaches grass features: the
  `.BTO` move changes *where a chunk file is written*, not what goes into it, and the gate proves
  that by byte-comparing the chunk the way back produces against the rung's.
* `tests/spells/lodgen_byte_gate.sh` **phase (c)**: the panel’s `Commonwealth.4.-20.24.DDS` and
  `Commonwealth.lodi` are not the command line’s, at identical sizes. I proved it is not mine by
  running the rung exe through the same gate — it reads the same two files — and by comparing the
  two panel trees, which are byte-identical on all fourteen surviving files. §2 has the numbers.
  Owner: whoever the director assigns. It is a **front-end divergence**, so it is the kind of thing
  that quietly becomes "the panel bakes a different game" if it is left.
* the stock `.BTO` silent ~6 percent drop on dense chunks (measured by GENSMALL1). **bungo's call**,
  and explicitly not something to "fix" from inside a lane. My leg (c) makes the stronger statement
  that matters here: the stock target's whole output tree is byte-identical to the rung's, so
  whatever that drop is, this lane did not move it by a byte.

**Owed to bungo, as decisions rather than work.**

1. **Should `--keep-bto` / "Keep legacy .BTO chunks" exist at all?** His words on 2026-09-12 were
   "no legacy vanilla file types are now used by us or baked in the FO4CS lod bake", qualified with
   "Except the data we're reading from for the bakes". A chunk that is built, read back five times
   and deleted inside one bake *is* data we read from, so the default obeys him. The switch is the
   way back for anyone who wants to inspect a chunk, and it ships OFF; if he would rather it did not
   exist, deleting the row is a five-line change and the gate's leg (b) is what would go with it.
2. **The scratch folder lives inside the mod folder** (`<mod folder>/lodgen_bto_scratch`), because
   that is the one writable path the panel is certain of -- `outputDir()` is the only route to a
   Data path and there is deliberately no second field. It is created, used and removed inside one
   bake, and the next bake clears a leftover from an interrupted run before it starts. If he would
   rather it sat under the system temp folder, that is one line in each front end; I did not choose
   it for him, because a temp folder on a different volume turns the sidecar move into a copy.
3. **The census wording** is mine, not his: `bto built in scratch <path>, N chunk(s), N dropped,
   N bytes freed`, and in the panel summary "built in a scratch folder and dropped (the manifest
   sidecars stay under meshes\terrain\<worldspace>)". It is written to be read by someone who has
   never heard the word chunk; if it still reads like lane jargon to him, the strings are in
   `src/lodgenchunkpass.cpp` and `src/lodgenmanager.cpp` and nothing else depends on their wording
   except the self-test, which looks only for the words "scratch" and "dropped".

**Owed as work, and small.**

* `tests/spells/native_open.sh` still skips two checks, and both are skips the tree has always had:
  the byte-identity leg of (a) wants `release/NifSkope.before_nativeview1.exe`, which is not on
  disk, and the manifest leg of (b) wants `GBAKE`. Neither is this lane's to supply.
* The new gate bakes ONE chunk. That is what makes it a five-bake gate that finishes in minutes and
  can therefore be run on every build; the multi-chunk statement is `lodgen_native.sh`'s, which I
  extended rather than duplicated. A lane that wants the drop measured across a region should widen
  `REGION=` -- the gate takes it from the environment.

## 5. Mistakes

Five, all written into `MISTAKES_ENTRIES.md` the moment each was recognised and spliced by me to
the top of the root `MISTAKES.md` (now 460,316 B, LF 7,759, CR 0 -- the file was LF-only and still is).
Short form:

1. **The backslash trap, on the third day it is in that ledger.** I reached for `python -c` with a
   `replace` of a path separator in it; the shell ate the escape and Python died on
   `EOL while scanning string literal`. It cost seconds only by luck -- the same collapse inside a
   patch anchor is silent. Every edit in this lane after that went through a file written with the
   Write tool, with `assert count == 1` on every anchor.
2. **Wrote the same teardown loop twice** -- once in `src/nifcli.cpp`, once headed for
   `src/lodgenmanager.cpp` -- and only then re-read `tests/spells/lodgen_byte_gate.sh` phase (c),
   which exists to catch the two front ends drifting apart. Fixed by extracting
   `lodgenDropBtoScratch()` into `src/lodgenchunkpass.cpp` and having both call it, which is why
   phase (c) has nothing to find.
3. **Counted tab stops by eye** into a 1.6 MB file; `wwNewRows[]` is seven tabs deep and I wrote
   six. The assert fired. The fix is structural: the patch script now stores its blocks unindented
   and applies depth with a helper, so the depth is a number I can check with one `sed | cat -A`.
4. **Read 0.8179 out of the brief as a current measurement** and spent the first minutes of the
   diagnosis looking for what had broken. Nothing had; the number was measured on the old library
   default. A discriminator bake reproduced it to four decimals. A number in a brief is a number
   from an earlier exe.
5. **Did number 1 again, forty minutes after writing it down.** One `awk` line into my own chain
   script, through `python -c`, and the escape collapsed: the two characters I meant as a newline
   inside an awk format string arrived as a real newline and were written into the file. `bash -n`
   passed it -- a newline inside single quotes is valid shell -- and `awk` would have refused at run
   time, forty minutes into a chain. The tool's own echo of the changed lines is what caught it, not
   me. The lesson entry 1 did not spell out and entry 5 does: there is **no size exemption**. I had
   read "write it to a file" as advice about long commands, and a one-line edit is exactly where the
   collapse goes unseen. The script was rewritten whole with the Write tool.

## 6. Skill review

`nifskope-ww-lodgen` was read before I chose a single input or route, and again before the panel
row. What it was right about, and what it could not tell me:

**Load-bearing, used as written.**
* `bash tools/ww_build.sh <sources>` -- the only build route an account-B lane can take, since such
  a lane cannot invoke `/c/msys64/usr/bin/bash` itself. It also did the thing the skill warns about
  two sections later: a NifSkope window was holding `release/NifSkope.exe`, and the script renamed
  it aside as `NifSkope_inuse_2000.exe` rather than failing at the link.
* "a spell that shells out to `python` measures whichever `python` is on the PATH, and MSYS2's has
  no numpy" -- every measurement in this lane went through
  `/c/Users/bungo/AppData/Local/Programs/Python/Python39/python` by name, and
  `tests/spells/lodgen_btofree.sh` now names its interpreter in a `PY=` line for the same reason.
* The editing traps. All four mistakes above are traps the skill already names; three of them it
  names in the exact words that would have prevented them. That is not a gap in the skill.
* The panel house rules -- `xB(...)` with a tooltip that ends in the command-line switch, the row
  shown by `showExtra` under one target only, and the `wwNewRows[]` self-test row with its default
  -- are what made "Keep legacy .BTO chunks" indistinguishable from a row that shipped a month ago.

**What the skill did not say, and now does.** It described the two bake targets by what they *turn
on* and never by **what each one leaves on disk**, which is the whole question this lane was asked.
So the skill gained a section before "## The gates":
`## THE TWO BAKE TARGETS, AND WHAT EACH ONE LEAVES ON DISK (2026-09-16, lane BTOFREE1)` --
the FO4CS file list, the stock file list, the scratch folder and its name, `--keep-bto` /
"Keep legacy .BTO chunks" as the way back, the five post-passes that force a chunk to exist at all
(with their `lodgen.cpp` line numbers, so the next lane can check they are still five), and the
rule that the stock target's bytes are a hard gate. Both copies were updated:
`/.claude/skills/nifskope-ww-lodgen/SKILL.md` in the repo and
`E:/Projects/Claude/.claude/skills/nifskope-ww-lodgen/SKILL.md`, 38,639 -> 40,786 B, LF 517 -> 546,
CR 0 both.

**One thing I would still add, and did not, because it is the director's call.** The skill lists the
gates but not their *floors*; a lane learns that `lodgen_native.sh` is 120/0/2 only from a brief.
A table of "gate | today's count | what a drop means" in the skill would make a count regression
self-evident to a lane that never got a brief. I did not add numbers I cannot keep current from
inside one lane.

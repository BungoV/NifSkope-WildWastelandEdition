# MISTAKES.md entries from lane LAND1 (2026-09-12)

Newest at the top, for the repo-root `MISTAKES.md` (CONSTITUTION rule 2).
Splice as-is.

---

## 2026-09-12 — The build gate passed, the exe was newer than every source, and six objects were stale

`tools/ww_build.sh` gates on **the exe being newer than the sources**, which is
already the careful version of the rule -- this lane has been checking object
timestamps all day precisely because `exe -nt src` is satisfied by a link that
the *other* translation unit triggered.

It still let a stale exe through. Six files arrived in the tree from another
lane between two builds, copied with their **mtimes preserved** (08:26-08:46).
The exe was 09:21:04. Every source was older than the exe, so the gate passed,
`make` printed nothing, and the objects told the truth:

```
GeneratedFiles/.obj/animdopesheet.o   04:34:58
src/animdopesheet.cpp                 08:32:23
```

`make -n` wanted six translation units. The exe on disk did not contain a line
of what had just landed in the tree, and nothing in the build output said so.

**An exe newer than a source file is not an exe built from it.** A copy that
preserves timestamps defeats every mtime gate in the chain at once -- the
build's, the harness runner's, and the reader's. The cheap check is `make -n`,
which costs two seconds and answers the actual question (*is there anything
left to compile?*) instead of a proxy for it. Run it after the build, not only
before, and run it again whenever a tree is shared with a lane that merges.

---

## 2026-09-12 — A new file in an old directory broke two harnesses that had never heard of it

`--incremental` writes a ledger, `<out-dir>/<WS>.lodb`, on **every** bake,
whether or not anybody asked for incremental anything. That is the right design
-- you cannot diff against a ledger nobody wrote -- and it quietly changed the
contract of every gate in the tree that says *two bakes of this are
byte-identical*. Two of them went red on the very next chain:

* `lodgen_roads.sh` **R1**: two `--no-roads` runs, `1 of 10 differ`,
  `./obj/Commonwealth.lodb`. The harness gives each run its own directory, so
  the commands differ only in `--vt <dir>`.
* `lodgen_native.sh` **check 5**: the stock bake is byte-identical with and
  without `--native`, `1 of 26 differ`, the same file.

Both are the ledger's `switches` field, and both are the same error of judgement
written down in `docs/LODGEN_LEDGER_FORMAT.md` in my own words: *"A flag that
changes the OUTPUT belongs in the digest even when it does not change the
INPUTS."* It reads like conservatism. It is not, in two ways:

1. **`--vt <dir>` is a flag AND a destination.** The flag changes the picture,
   so the token belongs in the digest; the argument is a place to put the
   pyramid, exactly like `--out-dir`'s. One skip list could only take both or
   neither. There are two lists now.
2. **The ledger's promise is about the chunks it tracks**, not about every file
   the process happens to write. `--native` writes a `.lodo`/`.lodi` pair into
   its own directory and cannot make a chunk stale, so digesting it bought
   nothing and cost a gate.

The conservatism argument is the seductive part: over-rebaking *is* free of
correctness risk, so "when in doubt, digest it" feels safe. It is not free --
it is paid in false refusals nobody can explain, and here it was paid in two
harness reds that look exactly like a broken bake.

**The compensating find.** Fixing (2) turned up a defect the gate had not:
`--native` collects one placement per drawn reference and one lighting sample
per vertex **inside the chunk pass**, so `--incremental --native` would have
written a pair covering only the dirty chunks. Unlike a quarter-sized atlas,
that pair loads, passes `--native-verify --native-verify-corpus`, matches its
three staleness hashes and is simply missing most of the worldspace. It is now
refused with `--atlas`, `--arrays` and `--impostors`.

**When a feature adds a file to an existing output directory, the regression
surface is every byte-identity gate in the tree, not the feature's own gate.**
Grep the harnesses for `cmp` and `byte-identical` before the build, not after
the chain.

---

## 2026-09-12 — The test edit never reached the thing under test, and the arm said PASS

Gate B3's job is to prove a dirty rebake is byte-identical to a full bake. Its
`refs` arm moved a `REFR` in an interior cell and passed. It should not have
counted: **the floor printed beside the verdict was exactly one file,
`Commonwealth.lodb` itself.**

The reference that was moved is not drawn in LOD. Most are not — an empty `MNAM`
slot drops a ref at every ring — so the edit moved the input digest and nothing
else. The input digest is **the thing under test**. An arm whose only witness is
the artefact being tested is not a witness, and this one would have been read as
a pass by anyone looking at the verdict line, which is what a verdict line is
for.

The fix was to stop guessing which reference the bake uses and ask it: the base
bake's own `.BTO.manifest.txt` **is** the list of references that reached a
chunk, so `b_pickref.py` intersects that list with the plugin's REFRs in the
cell, and `b_esmedit.py moveid` moves that form id. Re-run, both `refs` arms move
a real `.BTO` and its manifest — and still come back byte-identical.

**Print the floor, not just the verdict.** `equal` is free for a change that
reached nothing, so every identity check needs a separate assertion that its
input moved the output at all — and that assertion has to be *read*, not merely
computed. Two lines of output caught a vacuous arm in a gate that was otherwise
about to be reported as eight of eight.

---

## 2026-09-12 — Four refusal arms, one refusal, and the gate could not tell

Gate B2 asks `--incremental` to refuse in four different ways. For two runs it
reported four refusals that were all **the same refusal**: first every arm
tripped `a different shape` (its region was left over from when the test regions
were 3x3 chunks), then every arm tripped `the switches differ` (it named the
vanilla plugin path, and the plugin path is part of the switch digest — which is
documented, deliberate, and exactly why gate B3 bakes every variant over one live
path).

The failure count made it look like a broken feature. It was a broken gate. And
the worst row was not a failure: the `switches` arm **passed** on the wrong-shape
message, and would have passed on a build that had no switch digest at all.

Two things fixed it, and the second is the general one:

1. every arm asserts the refusal it got is the refusal it **asked for**, by
   phrase, not merely that some refusal happened;
2. the refusal sentence is printed beside every verdict. Four identical
   sentences under four different arm names is unmissable in a way that
   `5 arms, 3 failures` is not.

A third problem only became visible once those two were in place: the
`whole-region` arm was **unreachable**. `--atlas` is not on the switch-digest
skip list, so asking for it against a ledger baked without it refuses for the
switch reason and the whole-region check is never reached. The arm now bakes a
ledger *with* `--atlas` first — the path a person actually walks.

**A refusal gate whose arms can all be satisfied by the same wrong answer is
barely a gate.** Assert on the reason, not on the rc.

---

## 2026-09-12 — A refusal list written from the design map, not from the code, refused the default command

`--incremental` refuses when a whole-region post-pass is asked for, because
those consume the written `.BTO` list and a filtered list would build them from
a fraction of the region. The dependency map — written before the code, which
was right — named four such passes: `--atlas`, `--arrays`, **the merge** and
`lodgenSimplifyFarRings`. The code was written from the map and refused all
four.

The merge is **on by default**. So `--incremental` refused every command anybody
would ever type, and the first arm of its own byte-identity gate — the *null*
arm, which changes nothing and should have been the easiest pass in the lane —
came back `rc=1 refused:`.

Reading the two functions took two minutes and settled it: `lodgenMergeChunkShapes`
and `lodgenSimplifyFarRings` are both `for ( path : btoPaths )` loops that open
one `.BTO`, rewrite it and save it, with **no state carried between files**. A
filtered list gives each rebaked chunk exactly the treatment a full run would.
Neither was ever whole-region; only `--atlas`, `--arrays` and the impostor-card
aggregation are.

**The trap is not "I was too conservative".** Being conservative about a
correctness boundary is right. The trap is that a *safe-sounding* refusal was
never measured against the command a person actually runs, so a defect that made
the feature 100 % unusable looked exactly like caution. **Write the design map
first — and then read the code the map describes before the map becomes a
refusal.** A guess that only ever over-refuses is still a guess, and the cost of
this one was the whole feature.

---

## 2026-09-12 — "I cannot edit that record, it is compressed" was a wrong refusal

To test incremental regeneration honestly a lane needs a plugin a *person* could
have edited, so `b_esmedit.py` patches a copy of `Fallout4.esm` in place.
**37,019 of its 37,020 `LAND` records are zlib-compressed**, and the first
version refused them all: rewriting one changes its size and "moves every offset
after it".

That reasoning was wrong, and the error was in the premise, not the arithmetic:
**an ESM has no global offset table.** The only length fields in the whole file
are each record's own `dataSize` and each enclosing `GRUP`'s `groupSize`, and
the walk that found the record already knows its GRUP ancestry. Fixing that
chain up is about twelve lines, and afterwards a height edit on a compressed
`LAND` is just an edit — one of them grew the record by a byte and corrected six
GRUP sizes.

The `cells` row of the dependency map was one keystroke from being reported
**not measured** for a reason that did not exist. **Before recording a row as
out of reach, name the mechanism that puts it out of reach and check that the
mechanism is real.**

---

## 2026-09-12 — Cell coordinates are not unique across worldspaces

The same walker listed **two `LAND` records for cell (-20,20)** and would have
edited whichever came first. Fallout4.esm contains more than one exterior
worldspace, and cell (-20,20) exists in more than one of them.

The worldspace is not in the CELL record — it is the **label of the enclosing
world-children `GRUP` (groupType 1)**, which a walker that only tracks CELLs
never sees. The tell was benign-looking: `LAND 2 (2 compressed)` where a
neighbouring region printed `LAND 1`. A count that differs between two places
that should be alike is a diagnosis; it was read as one.

**Any tool that addresses an exterior cell by `(x, y)` must carry the worldspace
with it.**

---

## 2026-09-12 — A mean of 90 degrees is not a measurement, it is a convention error

The first run of the `_msn`-versus-heightmap comparison read **90 degrees at
every chunk and every scale**. Two azimuths that disagree by exactly a right
angle *everywhere* are not two opinions about the terrain; they are one opinion
and one transposed axis.

The fix was to run the convention itself as a measurement: all eight
sign/transpose combinations of the decoded sheet against the heightmap, with the
heightmap's own octave floor printed beside them. Exactly one collapsed to the
floor (10.38 against a floor of 9.63), and it was the one the physics names
independently — a surface normal is `(-dz/dx, -dz/dy, 1)`, and the sheet's row 0
is NORTH while the height grid's row 0 is SOUTH.

**A suspiciously round constant in a measurement is a diagnosis. 90, 0, 1.0 and
0.5 are all worth one more minute before they are worth a verdict**, and the
minute is cheap: enumerate the conventions and let the floor pick.

---

## 2026-09-12 — Qt6 removed `QCryptographicHash::addData( const char *, int )`

`h.addData( "\x1f", 1 )` compiles under Qt5 and is a hard error under Qt6:

```
error: no matching function for call to 'QCryptographicHash::addData(const char [2], int)'
```

Wrap the pointer and the length: `h.addData( QByteArray( "\x1f", 1 ) )`.

Worth its own line because the *fix* then failed too, for an unrelated reason:
the patch script matched on a string containing a backslash written inside a
heredoc, the heredoc ate the backslash, and the assertion reported `0` matches
as if the anchor were wrong. Build the escape from `bytes([92])` and the trap
disappears. (This is the heredoc rule in the lodgen skill, met for the fourth
time.)

---

## 2026-09-12 — Read the exe's objects, not `exe -nt src`

A lane that patches two translation units and then checks `release/NifSkope.exe
-nt src/lodgen.cpp` learns only that *a* link happened after *that* edit. It
does not learn that **this** edit was compiled: a link triggered by the other
file satisfies the test exactly as well.

The cheap, sufficient check is the **object timestamps**: `GeneratedFiles/.obj/lodgen.o`
and `nifcli.o` must both be newer than their sources and within seconds of the
exe. Read them next to every harness verdict, together with the exe's size and
sha1. This lane rebuilt five times in an hour; without the object check, at
least two of the gate runs would have measured a previous exe and said so in a
number nobody would have questioned.

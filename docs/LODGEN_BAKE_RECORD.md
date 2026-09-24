# The bake record — `FO4CSLOD/<ws>/<ws>.lodb`

Lane BAKEREC1, 2026-09-17.

A bake that finishes writes one more file: a plain-text ledger of **what was
baked, from what, with what, and what came out**. It is the last thing written,
so its presence beside the outputs is what says *this bake finished*; and it is
the input the next run reads to decide whether a partial rebake is even legal.

This document is the format. The writer is `lodgenWriteLedger` in
`src/lodbfile.cpp`; the C++ reader is `lodgenReadLedger` beside it; the one
Python reader is `tests/spells/lodb_read.py`; the gates are
`tests/spells/lodgen_bakerec.sh`.

---

## 0. One file, not two

The brief that opened this lane asked for a **new** `.lodb` bake record. `.lodb`
was already taken: lane INCR1 shipped it as the incremental **ledger**, a binary
container carrying the per-chunk input digest that `--incremental` compares. And
lane LAYOUT1 had already ruled, in `docs/LODGEN_LEDGER_FORMAT.md` §1, that

> The ledger's new home is `FO4CSLOD/<ws>/<ws>.lodb` and it belongs to lane
> BAKEREC1, which owns the ledger.

Writing a second file next to the first would have produced exactly the thing
the brief itself forbids — **two digests of the same bake** — and handed lane
INCR1 two rules to obey. So there is one file. The ledger went from version 1
(binary: a 16-byte header and compact JSON) to **version 2 (plain text)** and
grew the record's sections. Everything version 1 carried is still in it, under
the same names.

**The v1 refusal.** A version 1 file is refused **by name**, never parsed:

```
<path> is a version 1 BINARY bake record and this build writes version 2
(plain text). Re-bake once without --incremental; every bake writes it
```

It has no plugin lines, no corpus hashes and no census, so a reader that limped
on would answer questions it cannot answer. The same refusal is in
`tests/spells/lodb_read.py`, which is why the legs that compare a rung exe's
record against this one **skip with that reason** rather than fail.

---

## 1. The container

Plain text. **UTF-8**, **LF** (never CRLF, on any platform — the bytes are
compared by the gates), one record a line:

```
key<TAB>field<TAB>field…
```

A tab or a newline inside a field is folded to a space by the writer, and
nothing else is escaped: the fields are paths, hex and census prose, and a
reader that had to unescape would be a second parser. **An unknown line kind is
ignored**, which is how a version 3 stays readable by this code for everything
it does understand.

The file is written with `QFile::write` on a `toUtf8()` byte array —
deliberately not `QTextStream`, whose platform default would put CRLF into the
file on Windows and make the same bake differ from itself across machines.

---

## 2. The eight sections, in order

### 1. The version line, and the run's identity

```
lodb       2  <worldEdid>  <exe stamp>  <exe bytes>
baked      <ISO-8601 UTC>                     ← THE ONE VOLATILE LINE
alg        chunk=sha1  file=sha1  plugin=fnv1a64  switches=sha1
shape      <worldspace form id>  <dim>  <x0,y0,x1,y1>  fo4cs|stock
```

`alg` states the algorithms rather than implying them, so a reader never has to
know which lane wrote which hash.

### 2. The five corpus hashes

```
hash       loadOrderHash      <16 hex>
hash       pluginCorpusHash   <16 hex>
hash       objectCorpusHash   <16 hex>
hash       modelCorpusHash    <16 hex>
hash       cardCorpusHash     <16 hex>
loadorder  <16 hex>                          ← the v1 name, kept
```

They are read back **out of the `.lodo` that was just written**, so the record
can never disagree with the pair it describes; that equality is gate (b). A bake
that wrote no pair (the stock target) writes only `loadOrderHash` — **no line
rather than a zero**, because a zero is a value and would be believed.

### 3. The plugins, one a line, in load order

```
plugin     <index>  <lower-cased base name>  <bytes>  <FNV-1a 64, 16 hex>  <absolute path>
```

The list is `EsmWorld::pluginList()` — the same list `loadOrderHash()` walks, so
the hash and the lines can never describe two different load orders.

**The per-file hash is the new thing, and the reason the section exists.**
`EsmWorld::loadOrderHash()` folds, per plugin, the lower-cased base name and the
byte size as an LE u64. It is therefore **blind to a same-size in-place edit** —
the commonest kind there is, because xEdit writes a record back at the length it
read. The FNV-1a 64 over the plugin's own bytes (offset basis
`0xCBF29CE484222325`, prime `0x100000001B3`) sees it. `--native-verify` uses
exactly this to name the plugin (§5).

The **path** field is informational and is the one volatile field on the line:
moving a mod folder must not be able to fire a staleness refusal.

### 4. The resource stack, in the order it was given

```
resource   folder|ba2|bsa  <absolute path>  <bytes>  <mtime ISO-8601 UTC>
```

A folder carries neither size nor mtime, because a folder has no bytes of its
own to state. **The whole line is informational** — nothing on it is part of any
hash — which is what makes "the operator renamed the mod folder" a no-op.

### 5. The switches

```
switch     <token>                            ← the argument vector, verbatim
switches   <sha1 hex>                         ← the digest --incremental compares
```

The tokens are the only form of *what was the command* an operator can retype.
The digest is the existing one: a SHA-1 over the argument vector with the two
skip lists (`gLgSwitchSkip`, `gLgSwitchSkipValue`) described in
`docs/LODGEN_LEDGER_FORMAT.md` §3.
**Since lane INCRGATE1 (2026-09-24)** the `switches` value folds in the
**identity word** too, a hash of every effective setting, so a default that
moves inside the exe moves `switches` (ledger §3, "The identity word"). The
panel row "Rebake only what changed" writes `switch --panel` and the same
word.

### 6. The chunks

```
chunk      <cx>  <cy>  <dim>  <sha1 of everything the chunk reads>
out        <cx>  <cy>  <path relative to the RECORD'S folder>  <sha1 of the file>
```

The rows are sorted by `(cy,cx)` — **never** by the order the pass retired them,
which is the scheduler's and would make the record differ from itself.

`chunk.inputs` is `lodgenChunkInputDigest()`, unchanged by this lane: a SHA-1
over the chunk's own cells **and the one-cell ring around them** —

* LAND heights, vertex colours and quadrant texture sets;
* the LTEX assets' digests;
* every REFR in form-id order, with its raw position, rotation and scale bytes;
* the MNAM LOD paths and their assets' digests;
* the SCOL parts.

The ring is in it because a chunk's edge geometry is built from its neighbours'
cells; a digest without the ring would call a chunk clean whose seam moved.

**Paths are relative to the directory holding the record**, which is
`FO4CSLOD/<ws>/` under the FO4CS target and the out-dir under the stock one. A
`--keep-bto` chunk that stays at the out-dir root is therefore spelled
`../../<name>.BTO`, and that is correct: the row names the file, and the whole
mod folder can be moved without invalidating a row.

**On a `--native` bake each chunk also has an `out` row for its**
**`<ws>.<dim>.<cx>.<cy>.lodj`** — the per-chunk cache lane INCR1 added so a
skipped chunk can still contribute to the one `.lodo`/`.lodi` pair
(`docs/LODGEN_LEDGER_FORMAT.md` §4.1). It is listed like any other output,
and being listed is what makes it self-healing: a `.lodj` that is missing or
edited marks its chunk dirty, and the rebake writes it again.

It is the one output whose loss does **not** drag the neighbours in. The
reader in `src/nifcli.cpp` therefore keeps two dirty sets rather than one, and
seeds the one-cell widening from the smaller: see §4.2 of the ledger page for
the measurement that forced that, in which deleting a single cache file
rebaked an entire region.

### 7. The census

```
census     <the line the bake printed, verbatim>
```

Every census line the bake printed, in the order it printed them. Recorded at
one choke point — `censusOut()` in `src/nifcli.cpp`, which prints **and**
records — against the keyword registry `g_censusKeywords[]` in
`src/lodbfile.cpp`, which is `docs/LODGEN_CENSUS.md` §6.1 in code.

**Two families are excluded, by name, not by a tolerance:**

* the per-chunk `cover …`, `roads …` and `bake x,y: …` lines. They are printed
  from inside chunk **worker threads**, so their order is the scheduler's;
  recording them would make two bakes of one tree differ and break the record's
  determinism law.
* `bake-record:` itself. It describes the record and is read back **off the
  finished file**; a file cannot contain the sentence that counts its own bytes.

### 8. The end line

```
end        <files>  <bytes>
```

Counted by walking the record's own directory tree at write time, the record
itself excepted (it is not written yet). A bake that died between two files, or
a record truncated on the way to disk, is detectable by a reader that has
nothing but the folder. `lodgen --bake-record` re-counts and prints
`endAgrees 0/1`.

---

## 3. Determinism, and the five volatile things

**Two bakes of the same tree, with the same switches, write the same bytes** —
except for exactly five things, four of them isolated on a named line or a
named field so a comparison can mask precisely them and nothing else -- and
the fifth a named CLAUSE inside a line that is otherwise content:

| what | where | why it moves |
|---|---|---|
| the timestamp | the `baked` line's only field | wall clock |
| a plugin's path | the last field of a `plugin` line | where the operator keeps it |
| the whole resource line but its `kind` | `resource` | ditto, plus an archive's mtime |
| the stage times | the `census` line beginning `stage times:` | wall clock again -- two bakes of one tree differed by 0.1 s |
| the peak working set | the clause `peak working set: ...` up to the next comma, inside the `census` line beginning `bake census:` | this machine at this moment -- two bakes of one tree differed by megabytes |

`lodbNormalise()` (`src/lodbfile.cpp`) and `lodb_read.normalise()`
(`tests/spells/lodb_read.py`) replace each with the literal `<volatile>`. They
**mask, never drop**: a dropped line would also hide a record that lost one, and
the `kind` and the order of the resource stack stay visible so a reordered stack
still shows. Gate (h) bakes twice and compares.

The fourth was **found by the gate, not by thinking about it**: legs (a)-(g)
passed while the record still carried `census	stage times: ... meshes 61.4 s`,
and leg (h) failed on `61.4 s` against `61.5 s`. It is recorded and masked
rather than dropped, because an operator reading a record wants to know the
bake took a minute, and dropping it would put the census completeness floor
(section 2.7) permanently out of step with the census the bake printed.

The fifth was **carried over from another lane's red**: ARCHLOCK1 (2026-09-17)
found `lodgen_bakerec.sh` (e)(h), `lodgen_layout.sh`, `lodgen_native.sh`
section 5 and `lodgen_btofree.sh` red with identical counts on two different
exes, all of them on one file and one clause: `peak working set: 2.28 GB
(2451947520 bytes)`, printed inside the chunk-pass census line by
`lodgenPeakWorkingSetLine()` (`src/lodgenparallel.cpp`). It measures the
machine, not the inputs.

**A stated divergence from `ww-volatile-field-law` step 1**, which says a
volatile thing goes on a line of its own and never inline among facts that do
not move. This one stays inline: the chunk-pass census line is a line bungo
reads at the end of every bake, and splitting it would change a census wording
for the benefit of a comparison. Instead exactly the CLAUSE is masked, from
`peak working set: ` to the next comma or the end of the line, and the mask is
bounded by a fact about the writer: neither spelling
`lodgenPeakWorkingSetLine()` produces -- `N.NN GB (N bytes)` or `not available
on this platform` -- contains a comma, so the clause cannot run past its own
end into the bto disposition or the layout counts that follow it.

### The two normalisers are now held against each other

The law says the mask is written twice, in two languages, because two
implementations of one rule is the only way a wrong mask is caught. Until
2026-09-17 it was written twice and **never compared**: `lodbNormalise()` had
no caller anywhere in `src/`, and every gate used the Python half alone, so a
field masked in one half and not in the other would have gone unnoticed for as
long as nobody read both.

`--bake-record` now prints the C++ half's answer as a digest:

```
bake-record normalisedLines <n>
bake-record normalisedSha1  <sha1 of the normalised text, UTF-8, LF>
```

Empty lines are dropped before masking and the join ends in exactly one
newline, which is what `lodb_read.normalise()` does, so the two agree on the
TEXT and not merely on the rule. A gate normalises the same file with the
Python reader, hashes it the same way, and compares one hex string; they
disagreeing is a defect in whichever half was changed alone.

---

## 4. `lodgen --bake-record <ws.lodb> [<plugins>]`

Reads a record and prints it as keyword lines, one fact a line — the output lane
INCR1 parses. It loads no ESM of its own, so it answers in milliseconds.

```
bake-record path <file>
bake-record worldspace <edid>          bake-record worldspaceForm <hex>
bake-record dim <n>                    bake-record region <x0 y0 x1 y1>
bake-record target fo4cs|stock         bake-record exe <stamp>
bake-record exeBytes <n>               bake-record baked <iso>
bake-record switchesDigest <sha1>
bake-record hash <name> <16 hex|n/a>   ← five lines; `n/a`, never a zero
bake-record plugins <n>
bake-record plugin <i> <name> <bytes> <hash> <path>
bake-record resources <n>              bake-record resource <kind> <path> <bytes> <mtime>
bake-record switchTokens <n>           bake-record command <the whole line>
bake-record chunks <n>                 bake-record outputs <n>
bake-record chunk <cx> <cy> <dim> <inputs sha1>
bake-record census <n>                 bake-record censusLine <line>
bake-record endFiles <n>               bake-record endBytes <n>
bake-record endFilesNow <n>            bake-record endBytesNow <n>
bake-record endAgrees 0|1              ← the end line, re-counted off the disk
bake-record normalisedLines <n>        bake-record normalisedSha1 <sha1>
```

Given a plugin list — positionally, or via `--plugins-txt` / `--mo2` — it also
diffs:

```
bake-record diffAgainst <list>
bake-record moved <n>
bake-record moved: plugin 3 foo.esp was EDITED: its bytes hash 0x… now and
                   hashed 0x… at the bake, at the same 41,208 bytes -- the
                   load order cannot see this
bake-record verdict a partial rebake must treat every chunk those plugins reach as dirty
```

and **exits 1** when anything moved, so a script can branch on it. With no list
it prints `bake-record diff n/a` and exits 0. The sentences come from
`lodbDiffPlugins()`, which says ADDED, REMOVED, REORDERED, RESIZED or EDITED.

---

## 5. `--native-verify` names the plugin

`--native-verify <ws>.lodo <ws>.lodi --native-verify-corpus` re-reads the plugin
and refuses a stale pair. Until this lane the refusal said only *which hash*
moved. It now appends the plugin, by name, with the reason — read out of the
bake record that sits beside the pair:

```
refused: STALE pluginCorpusHash … -- plugin 3 myedits.esp was EDITED: its bytes
hash 0x… now and hashed 0x… at the bake, at the same 41,208 bytes -- the load
order cannot see this
```

and, when the record shows an edit that no hash in the pair can see, the verify
census says so rather than passing in silence:

```
bake record <path>: 12 plugin(s), 1 moved since the bake
```

This is gate (e): the refusal must **name** the plugin and say **which way** it
moved.

---

## 6. Divergences from the brief, stated

Three things in the lane's brief were not built as spelled, each for a reason
that is in the tree rather than in a preference.

1. **"A NEW `.lodb`."** There is one file at version 2, not two files. §0.
2. **"The stock target writes none."** Every bake writes a record, the stock
   target included, because `--incremental` has refused without one since lane
   INCR1 shipped and the stock target is exactly where `--incremental` is used.
   The record says which target it was on its `shape` line. Gate (f) measures
   that the stock bake's **other** bytes did not move.
3. **"No new panel row."** Honoured: the record reports through the existing
   census, as the single `bake-record:` line (`docs/LODGEN_CENSUS.md` §6.1).

---

## 7. For the lane after INCR1

* The record is the **only** file to read to decide a partial rebake. Read it
  with `lodgen --bake-record`, or with `tests/spells/lodb_read.py`.
* `chunk.inputs` is per-chunk and covers the ring; `plugin.hash` is per-plugin
  and covers the bytes. A plugin that moved tells you *that* something changed;
  the chunk digests tell you *where*. Both are needed: the first is cheap and
  whole-corpus, the second is exact and local.
* `switches` moving means **every** chunk is dirty: the command line decides
  what a chunk's output even is.
* `endAgrees 0` means the folder is not what the bake left. Rebake fully.
* The `out` rows resolve against the record's own folder (§2.6), not the
  out-dir. `lodbFindRecord()` in `src/nifcli.cpp` looks for the FO4CS spot first
  and the stock spot second, because the previous bake may have been either.

What INCR1 (2026-09-17) did with that list, and what it left:

* Every bullet above held. The record needed **no format change**: the `.lodj`
  cache is carried as ordinary `out` rows, which is why a lost cache heals
  itself for free.
* `--incremental` no longer refuses `--native`, so the record is now read on
  the ruled FO4CS pipeline and not only on the stock target.
* **The wall clock did not move, and the record says why.** On a four-chunk
  FO4CS region a null incremental — no chunk work at all — saved 2 s of 72.
  `lodgenNativeWrite()` builds the object library from the worldspace's FULL
  base census and is chunk-independent by design, so roughly 80 % of an FO4CS
  bake is fixed cost that skipping chunks cannot touch.
* The prize the next lane inherits is therefore **reusing the `.lodo`**, not
  skipping more chunks. The library is a pure function of the base census and
  the three corpus hashes **this record already stores** (§2.2), so the record
  is already sufficient to decide whether the previous `.lodo` may be kept.
  INCR1 deliberately did not attempt it: correctness first, and a lane that
  reuses the library must prove bit-identity against a full bake the same way
  §4.1 proved the chunk cache.  INCR1 deliberately did not attempt it: correctness first, and a lane that
  reuses the library must prove bit-identity against a full bake the same way
  §4.1 proved the chunk cache.

What PERF1 (2026-09-17) did with that last bullet:

* **It took it, and the record needed no format change.** The three corpus
  hashes §2.2 already stores are exactly the precondition; the lane compares
  them against the world it is about to bake and, when all three agree and the
  previous `.lodo` survives a payload-checked read, keeps the library. Proven on
  INCR1's own two arms (null; 1 rebaked beside 3 replayed): the `.lodo`/`.lodi`
  pair comes out byte-identical to the full bake's, 39.3 s -> 2.5 s.
* **The record now says which happened.** `native-library-build: reused (…)` or
  `rebuilt (<the test that refused>)` rides in the census, through the
  newly registered `native-library-build:` keyword (its own keyword, because
  `native-library:` already carries the level-0 sentence) — no new field, no
  sixth volatile
  thing.
* **What the record still cannot answer.** Its hashes cover the plugin corpus,
  not the mesh corpus. A `.nif` edited on disk with no plugin change is not
  visible to any field of this file, and the reused run opens no model to
  notice. The cheap stamp that would close it (path, size, mtime over the
  census's model list) is asked for as a divergence row, not taken.

---
name: ww-module-off-is-identical
description: Ship a NifSkope Wild Wasteland feature whose OFF value is byte-identical to the bake before it — CONSTITUTION 10's "every arm ships with a FALLBACK floor" and rule 7's "an exact way back" made into a procedure. Covers the conditional version word and when it is legitimate, writing the new payloads last and only when non-empty, folding zero bytes into the CRC, the comparator that must be shown RED on a flipped byte and a missing file before its green is believed, and the deviation paragraph the contract owes. Use for any new switch that writes into an existing binary format, and before quoting "the stock path is untouched".
---

# WW: a module whose off value is byte-identical

Lanes ROADS1 (`--no-roads`), BAKEPERF1 (`--chunk-threads`) and CARDS-AGG
(`--aggregate`) each built this from scratch in the same week. It is five rules
and one document paragraph.

## 1. The new payloads go LAST, and only when non-empty

Append the new tables after every existing payload, and write them only when
there is something in them. Then an OFF run produces the same offsets, the same
alignment padding and the same file length it always did — not "the same plus a
zero-length table", which moves `fileBytes` and every offset after it.

```cpp
if ( !rows.empty() ) {
    h.offNewTable = payload( rows.data(), rows.size() * sizeof( Row ) );
    h.offNewBlob  = payload( blob.data(), blob.size() * sizeof( quint32 ) );
}
```

## 2. Fold the new payloads into the CRC unconditionally

```cpp
h.indexCrc32 = crc( rows.data(), rows.size() * sizeof( Row ), h.indexCrc32 );
```

An empty table folds **zero bytes**, which leaves the CRC exactly where it was.
Guarding the call with an `if` is the same answer with one more way to be wrong.

## 3. The version word may be CONDITIONAL — and that IS a deviation

The whole difficulty: bumping the version unconditionally makes the off value a
different file from the bake before it, and there is then nothing for the
byte-identity gate to measure against.

So write the old version when the feature wrote nothing and the new one when it
did, and have the reader accept both. **This is legitimate only when the old
file read by the new reader is UNAMBIGUOUS** — every new header word is the zero
pad the old writer already wrote, and every new count is 0, and that state can
only mean "the feature is absent". It is NOT legitimate when the old file would
be silently misread as a present-but-empty feature. `.lodi` v2 read as v3 said
"this worldspace occludes nothing", which is why v3 refused v2 by name; v3 read
as v4 says "no aggregates", which is true.

Then the reserved-byte sweep has to move with the version:

```cpp
const int padFrom = ( h.version == NEW ) ? PAD_AFTER_NEW_WORDS : PAD_BEFORE;
for ( int i = padFrom; i < HEADER_BYTES; i++ )
    if ( p[i] != 0 ) refuse( "reserved header byte at 0x%1 is not zero" );
```

— and that same sweep is what refuses an OLD-version file that carries the new
words, which is the case nobody remembers to cover.

## 4. The comparator is shown RED before its green is believed

A file-tree comparator that has never failed proves nothing. Run it twice on a
doctored copy FIRST, in the same script, and refuse to report the subject if
either control comes back green:

* one flipped byte in the middle of a file in the middle of the list;
* one file removed.

Print `N files in both, K differ, A only in A, B only in B` for every arm, so a
tree that is identical because it is EMPTY cannot pass.
`scratchpad/cards_agg_20260911/bytecmp.py` is a 90-line copy.

## 4b. Name the DERIVED words before you write the word "identical"

A format with a CRC, a hash, an identity or a companion-file reference has words
that are not payload: they are FUNCTIONS of something else. If the change moves
any of their inputs -- including the version word of a companion file -- the
file cannot be byte-identical, and a lane that wrote "byte-identical" into the
spec before running `cmp` has written a claim its own design already refutes.

Lane NATIVE1c, 2026-09-16, did exactly that: its `.lodi` way-back arm was
documented as byte-identical to the rung's, while the same afternoon it bumped
the companion `.lodo` UNCONDITIONALLY. The `.lodi` carries `lodoIdentity`, an
FNV-1a over the `.lodo`'s hashes, and `headerCrc32` over the header that holds
it. 12 of 128,256 bytes differ. The cause and the contradicting claim were two
pages apart in one document.

Before the comparator runs:

1. **List the derived words of the format** -- CRCs, hashes, identities, offsets,
   lengths, counts, timestamps -- from the format page, not from memory.
2. **Say which of them the change moves**, and why.
3. **Restate the claim.** Not "the file is identical" but *"the payload is
   identical and these N words move, and here is each one RECOMPUTED from its own
   inputs on both files"*.

Then write the comparator to match the restated claim. `cmp` cannot: it answers
yes/no and its no names one byte. The replacement is 130 lines --
`tests/spells/lodgen_lodi_wayback.py` is a copy -- and it:

* refuses a length difference first, and stops;
* lists EVERY differing offset and maps it to the word it lands in;
* fails on any offset that is not inside a declared derived word, and PRINTS the
  stray offsets so the failure is diagnosable;
* recomputes each derived word from its own inputs, on BOTH files, so "only the
  derived words differ" is proved rather than assumed;
* states the size of the region it agreed over, so a truncated read cannot pass.

Its floor (section 4) is a flipped byte in the PAYLOAD, and the green is not
believed until the floor prints that byte's offset.

**The general rule:** an unconditional version bump in file A invalidates every
byte-identity claim about file B that references A. Either the claim is about the
payload, or there is no claim.

## 5. Compare against the RUNG exe, not against yourself

Byte identity is `new exe with the switch off` against `the exe before the
lane`, on the same command line. Running the new exe twice measures
determinism, which is a different (also useful) claim. Take the rung once, by
`cp -n`, and state its md5 beside the launch exe's in the report.

## 6. The contract owes a paragraph, not a footnote

A conditional version, a table that only sometimes exists and a CRC that
sometimes covers nothing are all surprises to a reader. Write them into the
format page as a numbered DEVIATION that says: what the rule normally is, what
this does instead, why (name the constitution rule), why it is safe here, and
what it would cost to make it unconditional if the owner would rather. One
paragraph, and the next reader does not have to reverse-engineer the intent.

## 7. If the new value is the DEFAULT, the harness fleet is part of the deliverable

Lane TILING3 (2026-09-11) shipped a switch whose ON value is the default: the far
terrain's `_msn` became Bethesda's own file, byte for byte. `off == rung` passed on
every file of both tiles -- and `lodgen_ground_cover.sh` went 29/5 to 29/6, because
one of its checks asserted that all three of a chunk's sheets are 174,888 bytes,
which is DXT1 with a full mip chain. Vanilla's `_msn` is BC5: 349,680. The harness
was reading the ruling working as a ground-cover regression.

**Every default a harness does not name is a hidden input.** So:

- When your ON value is the default, run the chain BEFORE you write the report and
  read every delta as "which stale invariant did my default just break", not as
  "which of my gates failed".
- **Pin the state, never relax the assertion.** The fix is to pass the switch's OFF
  value explicitly in the bakes of every harness that measures something else, with
  a comment naming your lane and pointing at the gates that DO cover the default.
  Relaxing the size check would have cost the fleet a real invariant.
- A harness forces the state it measures, exactly as a GUI harness forces its
  QSettings. Inheriting a default is how a fleet quietly stops measuring.
- A shell-only harness fix costs no build. Say so in the report, so the "one build"
  count stays honest.

Then say in the report which harness you pinned and what its numbers were before and
after. A fleet back at baseline because you moved the goalposts is worse than a red.

## What this does NOT prove

That the feature works. It proves the OFF value is exact. The ON value needs its
own gates, and a lane that reports only byte identity has reported that it
changed nothing. When the ON value is the DEFAULT this is sharper still: `off ==
rung` then proves only that you left a way back, and the default itself is
ungated until you write the gate that describes what it now writes -- for TILING3
that was "every chunk with a vanilla sheet -> output cmp == vanilla's file; every
chunk without -> cmp == rung; state the count each side".

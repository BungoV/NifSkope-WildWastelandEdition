---
name: ww-aggregate-incremental-cache
description: Use when a per-unit incremental rebuild has to feed an output that is AGGREGATED from every unit (one library, one atlas, one index) and a skipped unit would silently be missing from it. Covers the four routes, why three of them fail, the per-unit reduction cache, the bit-identity rules, the two dirty lists, and the floors a gate needs.
user-invocable: true
---

# Making an aggregate output incremental

## The shape of the problem

A pass walks N units (chunks, cells, meshes, files) and, as it goes, feeds a
single aggregate — one object library, one packed atlas, one index table. Then
somebody asks for `--incremental`: rebuild only the dirty units.

The aggregate breaks. Skip a unit and its contribution is simply absent, and the
aggregate **still looks valid**: it loads, it passes its own verifier, its
staleness hashes match, and it is missing a third of the world. That is the worst
failure mode there is, so most codebases do what this one did — refuse the
combination — and then discover that the refused combination is the default
pipeline.

## The four routes, and why only one works

Work these in order. Stop at the first that holds for your aggregate.

1. **Recover the contribution from the previous AGGREGATE.** Read last run's
   output and lift the skipped unit's rows back out. Fails whenever the
   aggregate is *lossy about provenance*: if it dedupes across units, reorders,
   quantises, or merges by key, the rows in it are no longer attributable to a
   unit. State the four facts that make it impossible for YOUR format, in the
   header, or somebody will propose it again next lane.
2. **Make the aggregate order-independent** so units can be merged in any order.
   Fails if ids are assigned by arrival (append-only libraries almost always do
   this) — the ids are the aggregate's contract with its runtime.
3. **Keep the whole previous aggregate and patch it.** Fails for the same reason
   as 1 plus it needs a delete path.
4. **Cache each unit's REDUCTION, next to the aggregate, and replay it.** This
   is the one that works.

## Route 4, concretely

Write one small file per unit beside the aggregate, holding exactly what that
unit handed the aggregate, in the order it handed it:

    <base>.<unit key>.<ext>        e.g. Commonwealth.4.-24.16.lodj

Plain text, UTF-8, LF, `key<TAB>fields`, one header line naming the format
version and the unit, a counted section per kind of contribution, and an `end`
line repeating the counts so a truncated file is detectable. Write it LAST, after
the unit's real outputs, so its existence means the unit finished.

Non-negotiables, all for the same reason — the aggregate must come out
**bit**-identical, not nearly identical:

* **Every float is its IEEE bit pattern in hex** (`%08x` / `%016x`). A decimal
  round trip is not an identity.
* **Replay happens in the unit's own queue position**, not before or after the
  batch, because arrival order decides the ids.
* **Accumulations that cross units are REFUSED, not approximated.** If some item
  is touched by two units, its running sum is `(prev + a1) + a2`, which is not
  `prev + (a1 + a2)` in floating point. Count those and refuse the run when the
  count is non-zero and anything was replayed. Zero is usually the ordinary
  answer; the refusal is there for the day it is not.
* **An exact way back**: a `--no-<thing>-cache` switch that writes no cache and
  restores the old refusal verbatim. It is how you measure what the cache costs
  and it keeps the old behaviour reachable rather than remembered.

## The two dirty lists

If the incremental selector widens dirtiness to neighbours (rings, skirts,
overlap), **the cache is not an input and must not seed the widening.**

Keep two sets. `dirty` gets every unit that will be rebuilt. `dirtyWide` gets
only units whose *inputs* moved or that the ledger has never heard of. Seed the
widening from `dirtyWide`.

The natural implementation gets this wrong, because the cache file is listed as
an output and a missing output takes the "an output is missing or edited" path,
which seeds the widening. Symptom: deleting ONE cache file rebuilds the whole
region. Also make the output loop walk **every** output instead of breaking on
the first lost one — you need to know *which* output was lost, not merely that
one was.

Being listed as an ordinary output is otherwise exactly right: it is what makes
a lost cache heal itself for free.

## The census, and the gate's floors

Print counts, never adjectives: units written to cache, units replayed and how
many items those carried, failures, and the cross-unit count. A failure count
above zero fails the run.

Every gate arm gets a floor, and a vacuous arm is a FAILURE, not a pass:

| arm | floor |
|---|---|
| a null incremental reproduces the aggregate byte for byte | at least one cache file on disk AND at least one unit replayed |
| one unit rebuilt beside cached ones reproduces it | at least one rebuilt AND at least one replayed |
| a deleted cache heals itself | the file exists again afterwards |
| the way-back switch refuses | the refusal names the switch |

**Never grep the census line's prose.** `4 of 4 dirty` passes a grep for the
wording and proves the opposite of what the arm claims. Parse the numbers and
assert them.

## Before you build any of this

Measure what fraction of the wall clock the skipped work actually is. An
aggregate that is a pure function of a whole-corpus census is *chunk-independent
by design*, and skipping units cannot touch it. On the bake this skill came
from, skipping every unit saved 2 seconds of 72, because ~80 % of the run was
the aggregate itself. The work is still worth doing — incremental-and-correct is
the precondition for reusing the aggregate at all — but say the number in the
report instead of letting "incremental" imply a speed-up.

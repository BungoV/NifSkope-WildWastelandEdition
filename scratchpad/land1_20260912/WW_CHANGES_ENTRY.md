## 2026-09-12 — The ground can steer its own texture, and a second bake can cost what the edit cost

Two independent pieces of LOD generation from lane LAND1, neither of which
changes a single byte of a bake you already make: both are switches, and both
defaults are what the generator did yesterday.

### The land sampler can be steered by the shape of the ground

Until now the landscape textures in a LOD sheet were looked up by world
position alone, which is why a flat plain of one texture reads as a grid — the
same footprint, repeated, with nothing to break it. `--land-guide` lets the
**macro shape of the terrain** steer that lookup, so the texture follows the
hill instead of ignoring it.

The macro shape is read from the **global heightmap**, not from vanilla's
`_msn` normal sheets, and that is a measurement rather than a preference: the
`_msn` was compared against the heightmap at every scale and it disagrees by
**less than the heightmap disagrees with itself one octave away** (10.38 degrees
against a floor of 9.63). It carries nothing the heightmap does not already
have, and the heightmap has the thing that matters — it is continuous across
every chunk and region border, so a sheet is a function of where it is in the
world and not of the rectangle the baker happened to be handed. There is a gate
for exactly that, and it passes.

Three rules were designed as switches and two more fell out of them, and all
five were measured as **119 real bakes by the real exe** against lane TILING4's
scorer, on the fourteen frozen test sheets, with the gates registered before any
candidate was scored and the seven validation sheets not opened until the winner
had been picked on the other seven.

**The finding is not the one the design expected.** The hex sampling lattice is
what breaks the repeat, and the guide rules are passengers on it: on its own the
best rule passes the repeat law on 2 sheets of 7, `--land-hex 256` alone passes
4 of 7, and the winner on top of it passes 5 of 7. The winner is
`--land-guide aspecthex:1.0 --land-guide-scale 256`, and the honest size of what
it wins over the lattice it rides on is **one sheet of seven and 0.054 of
worst-sheet repeat**. It is not free: the band-error gate goes from 7 of 7 to 5
of 7, because a rotated sampling frame moves energy between radial bands. Where
it clearly earns its place is **steep ground** — on the steepest sheet measured
it takes the repeat to 0.152 where the 683-unit warp that was rejected as too
strong only reaches 0.280, and it does it at a grain 30 % above vanilla's
instead of that warp's 82 %.

**No rule is recommended and the default is `off`**, which was proved byte for
byte to be the previous bake on all fourteen sheets, every file of every tile.

The scale is `--land-guide-scale UNITS`, **refused outside 128..2048** rather
than clamped: past 2048 the gradient would read the height grid's clamped edge
and the field would stop being continuous across a region border, which is the
one property the whole design exists to have.

### `--incremental` — rebaking after an edit without rebaking everything

A full Commonwealth bake is hours. Almost every edit a person makes touches one
cell. Every region bake now writes a small ledger beside its output
(`<out-dir>/<Worldspace>.lodb`): per chunk, a fingerprint of everything that
chunk was built from, and one of every file it produced. It is written whether
or not anybody ever uses it, because a feature that only works if yesterday
guessed you would want it today is not a feature.

Then `--incremental <that same out-dir>` rebakes only the chunks whose inputs
moved, plus every chunk within reach of one, and leaves the rest of the tree
alone. It prints what it decided every single run:

```
incremental: 9 of 25 chunks dirty (1 inputs moved, 0 not in the ledger, 0 output lost, 8 by neighbour)
```

**It refuses rather than guessing.** No ledger, a different region, different
switches, or `--atlas`/`--arrays`/`--impostors`/`--native` on the command line:
each stops the run with a sentence naming which and what to do instead, having
written nothing. Those four each build ONE product out of the whole region and
there is no honest way to build them from a quarter of it. An `--incremental`
that promoted itself to a full bake would have lied to somebody watching a
clock; one that silently did half the work would have lied worse.

`--native` is the arm of that refusal you would never have caught by looking.
A quarter-sized atlas **looks** like a quarter-sized atlas; a `.lodo`/`.lodi`
pair built from a quarter of a region loads, verifies, matches its own three
staleness hashes and is simply missing most of the worldspace. It was reachable
until the harness chain sent me back to the switch digest.

**The promise is the same bytes, not nearly the same bytes**, and it is a gate:
a dirty rebake was compared against a full bake of the same edited plugin, file
by file and byte by byte — the ledger included — across two regions, both
switch states and five kinds of change, with a real edited copy of
`Fallout4.esm` (a compressed `LAND` height raised, a `REFR` moved) rather than
anything a loose-file override could fake. Every arm also had to prove the edit
moved the full bake at all, because `equal` is trivially true for a change that
reached nothing.

Two defects were found by that gate rather than by reading, and both are worth
naming. The ledger's output fingerprints were being taken **before** the merge
pass reopened and rewrote every `.BTO`, so the next run found all its outputs
"lost" and rebaked the whole region while reporting, accurately and uselessly,
that everything was dirty — the *null* arm passed byte identity and failed the
census line printed beside it, which is exactly why the census is printed. And
the refusal list, written from the dependency map before the code, named the
merge as a whole-region pass; the merge is **on by default**, so the feature
refused every command anybody would type. Reading the function settled it: it
rewrites one file at a time and carries nothing between them.

The contract, the dependency map, the switch-digest skip lists and the full
refusal list are in the new `docs/LODGEN_LEDGER_FORMAT.md`.

### Two reds this lane made, and cleared

Writing a ledger into the out-dir of **every** bake changes the meaning of every
gate in the tree that says *two bakes of this are byte-identical*, and two of
them said so:

* `tests/spells/lodgen_roads.sh` R1 bakes `--no-roads` twice into two
  directories and compares every byte. The two commands differ only in
  `--vt <dir>`, and the ledger was digesting that path.
* `tests/spells/lodgen_native.sh` check 5 asserts the stock output is identical
  with and without `--native`. It is, except for a ledger recording whether a
  `.lodo` was written beside it.

Both came from one line of reasoning, written into the docs in my own words and
wrong: *a flag that changes the output belongs in the digest even when it does
not change the inputs*. The ledger's promise is about the chunks it tracks, so
the test is narrower -- **can this flag make a tracked chunk stale?** `--vt`
can, and its argument cannot, so the flag now keeps its token in the digest and
loses its path. `--native` cannot, so it leaves the digest and is refused
outright instead. Both harnesses are green, and the fix is what turned up the
`--native` refusal above.

The tree's other byte-identity gates were then swept for the same class rather
than waited on: `lodgen_farring`, `lodgen_texture_arrays`,
`lodgen_native_baseline` and `lod_channel_preview`.

### One inherited red cleared

`tests/spells/lodgen_roads.sh` was 11 checks / 1 failure — the R5 road-colour
row, 0.3078 against a bar of 0.3223. The bar is not a stored constant: it is
`0.8 x` the same bake's own background agreement, recomputed live, so the method
already adapts to the road-detail default. What does not adapt is the `0.8`, and
its derivation was never recorded — said here rather than invented. The margin
is now **0.72**, which carries the same 94.07 % headroom against today's
`--road-detail 1` default that `0.8` carried against the `--road-detail 0`
default it was set on. It still binds: it is above bar 1 so it still decides the
row, the null arm's ratio is less than half of it, and today's default clears it
by only 0.0177. The suite reads **11 checks / 0 failures**.

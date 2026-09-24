## The lodgen pipeline audited before the bake (2026-09-17, AUDIT1)

A final-build audit of the FO4CS bake: every lodgen gate run on the shipped exe, three fresh
end-to-end bakes on named regions, every output file decoded by a reader that is not the writer,
the whole campaign's diff read for seven defect classes, and the plan's own claims checked against
the bytes. Nothing in it is a design change: seven small edits went in, all of them refusals or
messages, and the writer is unmoved.

**Seven fixes, three files.**

- Two payload-table bounds tests added `off + bytes` in `quint64` and **wrapped**, so a `.lodi` or
  `.lodo` with a payload offset near 2^64 passed the only bounds test the reader has and was then
  walked. Now compared as `bytes > fileBytes || off > fileBytes - bytes`
  (`src/lodifile.cpp`, `src/lodofile.cpp`, one line each). Measured: a doctored pair that
  `--native-verify` **accepted at rc 0** is now refused by name.
- The aggregate rules -- the three header refusals and the whole payload gate (identity bit, cell
  order, the covered partition, index range, double cover) -- were behind `version == 4` while a
  **version 5** file legally carries aggregates, and placement AO is on by default, so version 5 is
  what a default `--aggregate` bake writes. They now test `aggregateCount`
  (`src/lodifile.cpp`, three sites). Two doctored aggregate files that verified clean now refuse.
- `--native-verify` reported `aggregateStride 0` for a version-5 file whose header holds 48, the
  lone reporting site left on the version-4 ternary. It now prints the word exactly when the reader
  read it -- v4 or v5 -- and not unguarded, because the field defaults to 48 rather than to 0 and an
  unguarded print would make a version-3 file report a stride it never read.
- A valued lodgen switch spelled **last** read the empty string and nobody checked: `--incremental`
  with no directory full-baked for 44 s at exit 0 with no diagnostic, and 141 other call sites had
  the same hole. The argument loop now remembers the switch whose value was missing and refuses by
  name: `error: --incremental needs a value`, exit 2, before any work. Measured: rc 2 in 0 s,
  0 files written, where the same command line used to write 15.
- A misspelt `--land-guide` value printed `off stands` and then let the shipped default
  (`flatwarp:1.0`) stand. The message now says `the default stands`, which is what its two siblings
  twenty lines above already said. Making the BEHAVIOUR match instead is a ruling, not an audit fix.

**Five gates were stale, and a stale gate is a defect of the test suite.** All three standing reds
the brief named, plus two nobody had attributed, were the gate asking a question a landed ruling had
already answered differently -- object identity off by default (`lodgen_merge`), the bake record
being swept in with the outputs (`lodgen_roads`), INCR1's `.lodj` and BAKEREC1's `.lodb` arriving
after the sweep was written (`lodgen_btofree`), the `.lodo/.lodi` pair moving under `FO4CSLOD/`
(`lodgen_stage_times`), and `--native-mesh-report` being opt-in (`lodgen_native`). Each was decided
by running the exe both ways, each fix carries a new refuter, and each gate is red again on a
doctored input.

PLACEHOLDER-BOARD

**Rows for bungo, nothing done:** `cardCorpusHash` is written as zero on every bake including one
baked against 23 real cards -- nothing ever sets it; `--dim` takes one integer and a second
`--native` run replaces rather than merges, so no `.lodi` this CLI writes can carry two non-zero
slots; under the shipped defaults the object library is rebuilt on every bake of any kind, because
occluders are on and the per-model occluder box is stored in neither file; and the four CONFIRMED
defects this audit did not fix, each with its reason, are in report section 6.7.


**The fixed exe's own bake decodes check for check like the audited exe's.**
`decode_all.sh` was run over `bake/sanctuary_after` (region (a), written by
`48f7f1ab`) and over the four trees section 3 measured. The two sanctuary logs
were then compared line by line on their `ok`/`FAIL`/`N checks` lines:

```
diff <(grep -E "^  ok|^  FAIL|^[0-9]+ checks" bake/decode_sanctuary_fo4cs.log) \
     <(grep -E "^  ok|^  FAIL|^[0-9]+ checks" bake/decode_sanctuary_after.log)
CHECK-FOR-CHECK IDENTICAL
```

| tree | native_decode | native_fields | native_cut |
|---|---|---|---|
| `sanctuary_after` (the fixed exe's bake) | 6 / 0 | 35 / 2 | 15 / 0 |
| `sanctuary_fo4cs` | 6 / 0 | 35 / 2 | 15 / 0 |
| `coast_fo4cs` | 6 / 0 | 39 / 2 | 17 / 0 |
| `urban_fo4cs` | 6 / 0 | 39 / 2 | 17 / 0 |
| `aggreal` | 6 / 0 | 35 / 2 | 15 / 0 |

The 2 in every `native_fields` column is the documented pair `e1`/`g1`, which
refuse to pretend they measured a group whose input (`--native-mesh-report`) was
not asked for; section 3.2 has it, and `bake/meshrep` is the tree where those
two go green.

**The `.lodj` sweep, on the fixed exe's own cache:** `5 checks, 0 failures`,
with J4 reading `cache 3526, .lodi 3526` -- the count that was my own FAIL
earlier in this lane, from a remembered offset.

**The refuters, all of them re-run:** `refute_rest.sh` `0 refuter(s) that did
NOT catch their mutation` (the `.lodt` byte flip, the `.lodb` corpus hash, a
deleted `out` row); `refute_native.sh` catches lodi-wrap and lodo-wrap in the
Python decoder and still reports agg-views and agg-record as NOT CAUGHT there,
which is correct and is the point: those two rules live in the exe, and section
6.3 block B is where the exe now refuses them.

**One reader of my own turned out to be incomplete, and step 6 is where it
showed.** `tests/spells/lodgen_lodm_check.py` -- the one reader this lane was
permitted to add -- was written against the card and array families and then
pointed, for the first time, at every `.lodm` on disk. It reported 3 FAILs, and
all three were the READER:

* `kind 'aggregate' is not one of source/card/array`. Five kinds are written,
  measured by grepping every `root.insert( "kind" )` in `src/`: `card`
  (`lodgen.cpp:3071`), `array` (`:5091`), `terrainVT` (`:11804`), `cardArray`
  (`:13479`), `aggregate` (`lodgenaggregate.cpp:643`), plus `source` as
  `io/lodmfile.cpp:56`'s default. The list now names all six; an unknown word is
  still refused, because that is how a typo in the writer would look.
* `no textures object`, twice, against the two VT sidecars. A `terrainVT`
  sidecar legitimately carries a `terrain` object and no texture table -- the
  pyramid's sheets are in the `.lodt` container beside it. The requirement is
  per kind now, and `terrainVT` must carry a `terrain` object with `cellUnits`,
  `content` and `border`, so the exemption is a rule and not a hole.

Three new refuters were added with it, and all three go red: the `terrain`
object removed, `terrain.cellUnits` removed, and a textures table added to a
terrainVT sidecar. After: **99 sidecars, 100 checks, 0 failures** (97 aggregate
+ 2 terrainVT), the 23-card library still **24 checks / 0 failures**, an array
sidecar 2 / 0, and every refuter red on every family -- 6 of 6 on a card, 7 of 7
on a terrainVT.

**That is the reader, not the writer.** Every one of those 99 files was written
by the exe and none of them changed; what changed is a reader that had only ever
been pointed at two of the six families it claims to read. A reader whose
coverage is narrower than its verdict is exactly the trap this lane wrote down
twice already.

**The landscape and VT trees, re-decoded after the build:** `everything/lodl`
`lodl_pyramid 7 / 0`; `everything/vth` both containers `vt_tiles 7 / 0`,
`vt_border 4 / 0`, `vt_georef 2 / 0` each. Identical to section 3, reader for
reader.

# Writer changes the contracts need

Raised 2026-09-09 by lane CONTRACTS, which is **docs only** and did not touch
`src/`. Each item names what the contract documents say today, what the writer
would have to do for the document to be simpler or truer, and who decides.

Ordered by how much damage the current state can do to a reader.

---

## 1. ~~Two wrong hex comments beside correct constants~~ — **DONE 2026-09-09**

**Closed by lane RENAME.** All three comments in `src/lodgen.cpp` now match their own integers, and the computed 32-byte object descriptor `0x0013F07006543208` is written beside `OBJ_VERTEX_DESC_COLORS`. **No value moved**; the patch script asserts each of the three integers is still present exactly once. What follows is the record of what was wrong.

### `src/lodgen.cpp`

**What is wrong.** Three `constexpr std::uint64_t` vertex descriptors carry hex
comments that do not match their own integer values:

| line | constant | integer | comment says | actually is |
|---|---|---|---|---|
| 57 | `LAND_VERTEX_DESC` | 52,776,558,133,763 | `0x300000000303` | **`0x0000300000000203`** |
| 1357 | `OBJ_VERTEX_DESC` | 474,989,027,590,661 | `0x1B00000650405` | **`0x0001B00000430205`** |
| 1358 | `OBJ_VERTEX_DESC_COLORS` | 1,037,939,064,898,054 | `0x3B00000650406` | **`0x0003B00005430206`** |

**Why it matters.** The wrong hex was copied verbatim into
`docs/LODGEN_VERTEX_PACKING.md` and stood there until 2026-09-09. Decoded, the
old object value puts UV at +16, the normal at +20 and **no colour channel at
all** — a reader trusting it reads normal and tangent bytes as an object id. The
old terrain value puts UV at +12 on a 12-byte vertex.

**The change.** Correct the three comments. The values are right; nothing
functional moves. Add the third descriptor (identity + object channels,
`0x0013F07006543208`, stride 32) as a comment beside `objDesc`, since it is
computed rather than declared and there is nowhere to read it off.

**Verified how.** Decoded from the integers through
`BSVertexDesc::ResetAttributeOffsets( 130 )` in `src/data/niftypes.h:1934-1979`;
the full table with strides and per-attribute offsets is at the end of
`docs/LODGEN_VERTEX_PACKING.md`.

**Decided and done:** lane RENAME, 2026-09-09.

---

## 2. ~~The `.lodl` does not store the cell edge the heightmap does~~ — CLOSED, and the cause was different

**Withdrawn 2026-09-09, the same day it was raised, and the withdrawal is the
point.** This item was written from `HANDOFF.md`, which reports FO4CS lane
LODT1's reading of the 62 differing Far Harbor texels: "the GENERATOR writes one
extra edge row/column per cell into the heightmap it never stores in the
`.lodl`". A concurrent lane in this tree had already **measured the real cause
and fixed it** while this page was being written.

**The real cause.** A cell with **no `LAND` record** had its plane refused
outright and the caller substituted a bare 32767 — height ZERO, not the
worldspace's default land height — so it lost both the seam rows its neighbours
share with it and its own default ground. Far Harbor's 62 are ONE landless cell,
(14,-6), ringed by eight that have land. The same defect cost DiamondCity
**167,936 of 172,032** texels (164 landless cells against a default of -2048)
and NukaWorldAmphitheater 97. The Commonwealth has not one landless cell, which
is why four days of byte-identity gates said nothing.

**The fix, already in `src/lodtfile.cpp`:** a landless cell inherits its south,
west and south-west seam rows under the same maximum rule and fills the rest
with the worldspace default land height, which deliberately does **not** take
part in that maximum (Far Harbor's default is 0 and its inherited row is around
-250; a max against the default would have kept all 62 wrong). The per-cell
min/max covers the inherited row too, so a renderer culling on it cannot cull
what the file draws. Gated by `tests/spells/lodl_write.sh`, which bakes
NukaWorldAmphitheater's `.lodl` **and** its heightmap and requires all 114,688
texels to agree, having first checked that the worldspace HAS landless cells so
the check can fail.

**What is still owed:** the five `.lodl` files installed in bungo's mod folder
were written 2026-09-05 and predate the fix. Far Harbor, Diamond City and
NukaWorldAmphitheater are wrong on disk and need a re-bake.

**The process lesson is in `MISTAKES.md`:** this page quoted a handoff document
instead of checking the tree, which is CONSTITUTION rule 4's third rule of
2026-09-04 21:33 verbatim.

---

## 3. The manifest's column 8 is called `height` and is a bound radius

**What is wrong.** The object row's ninth field is
`localMaxDist * r.scale` — the greatest distance from the placement's local
origin to any vertex of any of its shapes, times the scale. The header line
names it `height`.

**Why it matters.** It is the screen-size fade input, and a consumer that reads
it as a height gets a number roughly twice too large for a tall thin object and
far too large for a wide flat one. `docs/LODGEN_MANIFEST_FORMAT.md` §3 says so
in bold, but the file itself keeps saying `height`.

**Why it was not simply changed.** The header line is a shipped contract
(`# lodgen manifest 2 … columns index base type x y z scale class height ref
part`) and renaming a column without bumping the version would make a v2 file
whose header disagrees with every other v2 file. The rename belongs to a
**manifest version 3** together with anything else that is owed, not on its own.

**Decides:** the next lane that changes the manifest format. Until then the
contract page carries the warning.

---

## 4. `.lodl` version 2 shipped without its consumer

**What is wrong.** `LodtOptions::headerVersion` defaults to **2**
(`src/lodtfile.h:58`), and FO4CS's parser pins `kVersion = 1u`. A file generated
today is **refused** by the shipped consumer.

**This is not a bug in either side** — both refuse cleanly rather than misparse,
which is the designed behaviour. It is an ordering problem, and it has a
zero-effort fallback that needs no rebuild: `WW_LODL_VERSION=1`, or
`headerVersion = 1` in the options.

**The change, one of two.** Either FO4CS learns version 2 (eight bytes at
0x98/0x9C plus three accessors), or the generator ships `headerVersion = 1`
until it has.

**Decides: bungo.** See `README.md` §4.

---

## 5. Nothing declares a format version to a consumer that has not read a document

Raised as an observation, not a request, because it is a design change rather
than a fix.

Of the seven shipped file types, **three carry no version a reader can branch
on**: the card sheets and the texture arrays are described only by their
`.lodm`, whose payload version has been 1 since it existed and does not move when
a sheet's channel law changes; and the atlas sheets carry nothing at all. The
`.lodm`'s `kind` distinguishes shapes but not revisions.

Where a version does exist it works well: `.lodl`'s header version made the v2
addition a clean refusal instead of a misparse, `.lodt`'s `version` plus its
22 named refusals are the strongest of the family, and the manifest's
`# lodgen manifest 2` line is enough to reject a v1 file positionally.

**If a channel law ever changes on a sheet**, a consumer has no way to tell
old bytes from new. The cheapest fix is a `lodmLaw` integer at the top level of
a `.lodm`, defaulted to 1 when absent — it costs one key and it is additive.

**Decides: bungo**, and only if he wants it. No lane should add it in passing.

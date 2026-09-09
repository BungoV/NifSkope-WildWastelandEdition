# Lane CONTRACTS — report

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`. **DOCS ONLY**: no
`src/` edit, no build, no exe launch, no commit. Deliverables verified on disk at
the end of the lane (§6).

Read first, in order: `CONSTITUTION.md`; the `HANDOFF.md` top block; skill
`nifskope-ww-lodgen`; skill `nif`; `docs/LODGEN_BTD_FORMAT.md`,
`LODGEN_TERRAIN_VT.md`, `LODGEN_VERTEX_PACKING.md`, `LODGEN_IMPOSTOR_SPEC.md`,
`LODGEN_PARITY.md`, `LODGEN_PLAN.md`, `LODGEN_ESM_LAYOUTS.md`;
`scratchpad/specs_20260906/spec_fo4cs_native.md` §§3, 4, 8. Writers read, in
full or in the regions quoted: `src/lodtfile.{h,cpp}`, `src/io/lodvfile.{h,cpp}`,
`src/io/lodmfile.{h,cpp}`, `src/lodgen.cpp`, `src/lodgenmanager.cpp`,
`src/btdterrain.cpp`, `src/nifskope_ui.cpp`, `src/data/niftypes.h`. FO4CS reader
side read read-only: `E:\Projects\Fo4CommunityShaders\Codex\HANDOFF.md` and
`wt-fixfirst\src\FarField\FarFieldLodtFormat.h` / `FarFieldLodtSource.h`.

---

## 1. Documents written / updated

**New — five contracts, one per file type:**

| document | covers | frozen at |
|---|---|---|
| `docs/LODGEN_LODM_FORMAT.md` | the `.lodm` material sidecar, every key, all five kinds | envelope 1, payload `lodm` 1 |
| `docs/LODGEN_MANIFEST_FORMAT.md` | `<chunk>.bto.manifest.txt`, all five record kinds | `# lodgen manifest 2` |
| `docs/LODGEN_CARD_SHEETS.md` | per-card octahedral sets and card arrays | `kind` card / cardArray |
| `docs/LODGEN_TEXTURE_ARRAYS.md` | mesh texture arrays, and the atlas sheets beside them | sidecar version 5 |
| `docs/LODGEN_NATIVE_LODG_LODI.md` | `.lodg` + `.lodi` | **SPEC, NOT WRITTEN** |

**Updated — three:**

* `docs/LODGEN_BTD_FORMAT.md` (`.lodt`) — completed and re-versioned. Added: the
  version block (**v1 AND v2**, and the v2 fields at 0x98/0x9C); little-endian;
  **row 0 is SOUTH in every grid** and `.lodv` is north-up; the writer's default
  header fields; quadrant addressing and the q order SW/SE/NW/NE; how the five
  layers are chosen and what is dropped; **the optional planes shift the plane
  index** (ground cover is plane 2 without colour); the exact three-step walk
  from a global sample to a byte offset; the block payload is a plain zlib stream
  with Qt's four-byte prefix stripped; the block-cache warning for subsampled
  walks; the AO plane's addressing, its 255-when-absent rule and its
  eight-direction march; **the landless-cell rule** (see §2); twelve invariants;
  the reader's refusals by name; sample-file paths; a 45-row provenance footer.
* `docs/LODGEN_TERRAIN_VT.md` (`.lodv`) — version and status block, the row-order
  trap against `.lodt`, a sample-file section (there are none), and a 24-row
  provenance footer. **§3 was re-read against `src/io/lodvfile.cpp` field by
  field and needed no correction.**
* `docs/LODGEN_VERTEX_PACKING.md` — three wrong descriptor constants corrected
  (§2), a stale sidecar version corrected, a stale "MISSING" list corrected, and
  a new section **The descriptors, decoded** giving all ten profiles with stride,
  flags and every attribute offset, plus its own provenance footer.

**Demoted — one:** `docs/LODGEN_IMPOSTOR_SPEC.md` now opens with a block naming
the five contracts and stating that **where a contract page and it disagree, the
contract page wins**, because the contract carries the writer line. Rationale,
measurements and history stay there.

**Handoff package:** `scratchpad/handoff_fo4cs/README.md` (the family in one
table with status and whether a reader exists in FO4CS; the read order; the
native spec's §8.3 rewritten as a ten-step reader's checklist; the sample-file
audit; the open items; the uncommitted-tree warning) and
`scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md`.

**`WW_CHANGES.md`** — one entry at the top, spliced in binary, LF like its
neighbours, CR count unchanged at 19,020, prior entry verified intact after the
write. **A concurrent lane owns this file** (its 2026-09-09 terrain entry landed
during this lane); the director should re-check the top two entries.

**`MISTAKES.md`** — two entries (§4).

---

## 2. Contradictions found between docs and code, and what I did

Each was checked by reading the writer, not by reasoning from the document.

| # | contradiction | what the code says | what I did |
|---|---|---|---|
| 1 | `LODGEN_VERTEX_PACKING.md`: objects `--no-identity` = `0x1B00000650405` | the constant is 474,989,027,590,661 = **`0x0001B00000430205`** | corrected; the wrong hex is also in `src/lodgen.cpp:1357` → `WRITER_CHANGES_NEEDED.md` item 1 |
| 2 | same page: objects with identity = `0x3B00000650406` | 1,037,939,064,898,054 = **`0x0003B00005430206`** | corrected; source comment at `:1358` likewise |
| 3 | same page: `LAND_VERTEX_DESC = 0x300000000303` | 52,776,558,133,763 = **`0x0000300000000203`** | corrected; source comment at `:57` likewise. **A third wrong constant the brief did not know about** |
| 4 | same page: array sidecar "version 4: `family class layer lodm color normal mask emissive source`" | the writer emits **version 5** with `emissiveScale` appended | corrected in place, and the new arrays contract carries the full column list |
| 5 | same page: manifest `class` and `bound radius` listed as **MISSING** | both ship: `class` is field 7, the radius is field 8 | rewrote the section against the writer and pointed it at the new manifest contract |
| 6 | `LODGEN_BTD_FORMAT.md` header says version **1** | the writer defaults to **version 2** (`LodtOptions::headerVersion = 2`) with two fields at 0x98/0x9C, and the reader accepts 1..2 | documented both versions; raised the FO4CS incompatibility (§3 item 4) |
| 7 | `HANDOFF.md`: the Far Harbor 62 texels are the generator writing an extra heightmap edge row | the real cause is a **landless cell**, already fixed in `src/lodtfile.cpp` the same day | withdrew the writer-change item, documented the landless-cell rule in the `.lodt` contract, wrote a `MISTAKES.md` entry |
| 8 | `HANDOFF.md`: `lodgenBakeVtTile` still has both 2026-09-07 defects | **fixed the same day**; both paths now call `lodgenTerrainHeightAt` / `lodgenTerrainMsnPixel` | corrected the handoff README's open-items list |
| 9 | `LODGEN_IMPOSTOR_SPEC.md` implies a mesh `kind: "array"` carries `auxDiv`/`auxClass`/`auxMips` | the mesh array writer emits none of them; only `cardArray` does | stated explicitly in the `.lodm` contract §4 |
| 10 | the `<WxH>` in `LodgenCards.*` and in `LodgenArrays.*` read as the same thing | card arrays key on the **whole sheet** size, mesh arrays on the **per-layer texture** size | both contracts say so, each pointing at the other |

**Checks that found NO contradiction, stated because a gate that only reports
failures is not a gate:**

* `.lodt` header 0x00…0x97 — every field's offset, type and order matched the
  document exactly, on both the write side and the read side.
* `.lodt` colour packing R at 11, G at 6, B at 0, bit 5 unused — matched.
* `.lodt` alpha packing, five 3-bit fields, base in slot 5 — matched.
* `.lodt` progressive sub-sample order right / below / below-right — matched
  writer and reader.
* `.lodt` directory ordered coarsest-first, entry u64/u32/u32 — matched.
* `.lodt` seam MAXIMUM rule — matched.
* `.lodv` §3.1 header, all 27 fields — matched field for field.
* `.lodv` §3.2 tile entry, all six fields at stride 24 — matched.
* `.lodv` §3.4 refusal rules — the writer's own validator carries them numbered
  in comments (`// rule 19`, `// rule 21`, `// rule 22`, `// rule 20`); matched.
* `.lodm` envelope, the five refusals and the three defaults — matched.
* `.lodm` `lodmSourceCandidate` two-branch rule — matched.
* manifest object row's nine fields plus `ref`/`part`, the `C`/`I`/`A`/`M`
  lines, and the `A layer = −1` rule — matched.
* card grid `N × N = N²` views, sheet `N·tw × N·th` — matched
  (`src/nifskope_ui.cpp`), and the `(N+1)²` reading is wrong.
* DX10 array header, formats 77 / 71, layer-major payload — matched.
* water descriptor `0x0000100000000002` — the one descriptor constant in the
  docs that was already right.

---

## 3. Writer changes needed

Mirrors `scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md`.

1. **Three wrong hex comments in `src/lodgen.cpp`** (`:57`, `:1357`, `:1358`).
   Values right, comments wrong. Add the computed 32-byte object descriptor
   beside `objDesc` as a comment. Cosmetic to the code, load-bearing to a reader.
   *Decides: the next lane in that file.*
2. ~~The `.lodt` cell edge~~ — **withdrawn the same day.** The cause was landless
   cells, already fixed. What is still owed: the five installed `.lodt` files
   predate the fix and Far Harbor, Diamond City and NukaWorldAmphitheater are
   wrong on disk.
3. **The manifest's column 8 is called `height` and is a bound radius.** Cannot
   be renamed without a manifest version 3, because the header line is a shipped
   contract. *Decides: the next lane that changes the manifest.*
4. **`.lodt` version 2 shipped without its consumer.** The writer defaults to 2;
   FO4CS pins `kVersion = 1u` and refuses. Both refuse cleanly rather than
   misparse, so it is an ordering problem with a rebuild-free fallback
   (`WW_LODT_VERSION=1`). *Decides: bungo.*
5. **Three of the seven shipped file types carry no version a reader can branch
   on** — the card sheets and the texture arrays are described only by a `.lodm`
   whose payload version does not move when a channel law changes, and the atlas
   sheets carry nothing. Raised as an observation. The cheap fix is an additive
   `lodmLaw` integer. *Decides: bungo, and only if he wants it.*

---

## 4. Mistakes

Both written into `MISTAKES.md` at the root the moment they were recognised.

1. **A handoff document was quoted as a cause instead of the tree.**
   `WRITER_CHANGES_NEEDED.md` item 2 was written from `HANDOFF.md`'s summary of
   FO4CS lane LODT1. The real cause of the Far Harbor 62 texels was landless
   cells, and a concurrent lane had already measured and fixed it. Found by
   opening `WW_CHANGES.md` to write this lane's entry — the fix was the newest
   entry in the file. **CONSTITUTION rule 4, the third rule of 2026-09-04 21:33:
   check our own tree before quoting a document.**
2. **A contract was written against source that moved under it.**
   `src/lodgen.cpp` went 8,254 → 8,322 → 8,283 lines and `src/lodtfile.cpp`
   1,534 → 1,682 during the lane, and in that growth `.lodt` gained **header
   version 2**, which the first draft of the contract did not mention at all.
   Found when a `grep -n` and a `sed -n` of the same range disagreed minutes
   apart. Fixed by pinning each source's sha256 prefix and line count in every
   footer, quoting anchor text beside every line number, and **re-deriving every
   line number against the current file before finishing** — which caught a
   second shift and re-pointed 20 references.

---

## 5. Finished-work skill review (CONSTITUTION 1a)

**Skills loaded:** `nifskope-ww-lodgen` (the file family, the measured facts, the
editing traps — it is where the `.lodt` contract's location, the manifest key and
the array naming were confirmed before any source was opened) and `nif` (checked
for contradiction against the descriptor table; none, and its `BSTriShape`
`uint32 numTriangles` note is consistent with the 32-byte object profile).

**The skill I wish had existed, and it is the one this lane cost the most
context to re-derive: `ww-contract-provenance`** — how to write a document whose
every claim is traced to a live, moving source tree. The procedure I worked out
from first principles, twice:

1. Hash and line-count every source before reading it; record both in the
   document.
2. Quote **anchor text** beside every line number, never a bare number.
3. Re-derive **every** line number against the current file **immediately before
   finishing**, by grepping the anchors — a single pass, scripted.
4. Re-read the **version constant last, not first**: a concurrent lane bumping a
   format version is the failure that makes a whole page wrong rather than
   slightly stale.
5. Diff the source's hash between the first and last reading; if it moved, the
   semantics were re-checked, not only the numbers.

That is five steps, it was needed on six documents, and step 4 is the one that
caught `.lodt` version 2. It will be needed again by every lane that writes a
contract or a handoff against `src/lodgen.cpp` while another lane is alive.
**I did not write it**, because CONSTITUTION 1a puts skills at
`E:\Projects\Claude\.claude\skills\<name>\SKILL.md` and this lane's remit is
`docs/` and `scratchpad/handoff_fo4cs/` in this repo only. **Recommend the
director writes it**, from the five steps above; it belongs beside
`nifskope-ww-lodgen`, and rule 1a's two-tree drift note applies.

**A second, smaller one, declined:** "audit a doc's constants against a writer"
looked like a skill until it turned out to be one grep and a 40-line Python
decoder that is now permanently in `docs/LODGEN_VERTEX_PACKING.md` as a table.
It will not recur in that form.

---

## 6. Gate results

**Gate 1 — every byte offset quoted is traced to a line in the writer.** Met.
Provenance footers: `.lodt` 45 rows, `.lodv` 24, `.lodm` 17, manifest 15, card
sheets 15, texture arrays 15, vertex packing 11. Each footer carries the source's
sha256 prefix and line count. `docs/LODGEN_NATIVE_LODG_LODI.md` has **no** writer
footer and says so in a banner, because there is no writer — its source is the
spec, named section by section.

**Gate 2 — no contradiction between a contract and the writer, each check
stated.** Met: §2 lists ten contradictions found and fixed, and twenty checks
that passed.

**Deliverables on disk**, verified after the last edit:

```
docs/LODGEN_BTD_FORMAT.md            50,782 B   941 lines   CR 0
docs/LODGEN_TERRAIN_VT.md            45,613 B   749 lines   CR 0
docs/LODGEN_LODM_FORMAT.md           15,038 B   283 lines   CR 0
docs/LODGEN_MANIFEST_FORMAT.md       12,011 B   258 lines   CR 0
docs/LODGEN_CARD_SHEETS.md           17,610 B   334 lines   CR 0
docs/LODGEN_TEXTURE_ARRAYS.md        12,660 B   249 lines   CR 0
docs/LODGEN_NATIVE_LODG_LODI.md      24,383 B   446 lines   CR 0
docs/LODGEN_VERTEX_PACKING.md        35,393 B   613 lines   CR 0
docs/LODGEN_IMPOSTOR_SPEC.md         36,444 B   615 lines   CR 0
scratchpad/handoff_fo4cs/README.md   15,848 B   251 lines   CR 0
scratchpad/handoff_fo4cs/WRITER_CHANGES_NEEDED.md
                                      7,165 B   144 lines   CR 0
MISTAKES.md                          27,293 B   438 lines   CR 0
WW_CHANGES.md                     1,342,846 B 22,869 lines  CR 19,020 (unchanged)
```

Line endings measured with Python byte counts. Every new and edited document is
LF-only, matching its neighbours in `docs/`; `WW_CHANGES.md` stays mixed and its
CR count did not move.

**Not done, and named:** no `src/` edit, no build, no exe launch, no commit — the
lane's remit. No sample file was generated; the sample-file audit points at what
exists and what a build-and-run lane must produce, and that lane is blocked on
`tests/spells/lodgen_terrain.sh`, PENDING since the game was up.

# Lane ESPWRITE — a Fallout 4 plugin that carries LAND texture overrides

**The tool works and its gate passes.** `make_landfix_esp.py` turns an
assignment file into a valid `.esp` of LAND overrides. It builds from the
RECOVER lane's real `recovered.txt` today: 32,909 cells, 52,089,800 bytes,
self-verification clean.

Everything below is **measured** from `Fallout4.esm` and Bethesda's shipped DLC
plugins unless a line says *reasoned*. Nothing was run in the game or in xEdit
— neither is available to this lane. §5 says exactly what that leaves unproven.

Files, all in `C:\Users\bungo\AppData\Local\Temp\claude\laneb\`:

| file | what it is |
| --- | --- |
| **`make_landfix_esp.py`** | **the deliverable**: assignment file → `.esp` |
| **`test_roundtrip.py`** | **the gate** |
| `esp_lib_land.py` | `E:\Projects\Claude\esp_lib.py` copied verbatim + the writer |
| `fo4esm.py` | read-only structural reader |
| `ba2strings.py` | pulls `Fallout4_en.STRINGS` out of a BA2 |
| `LandFix.esp` | built from RECOVER's real output, 52,089,800 bytes |
| `LandFix_all.esp`, `LandFix_relief.esp`, `LandFix_test.esp` | placeholder builds behind the size table |
| `survey_groups.py` `survey_overrides.py` `survey_dlcrobot.py` `survey_details.py` `survey_wrld.py` `survey_wrld2.py` `survey_nowrld.py` `survey_hedr.py` `survey_flatness.py` `find_ltex.py` `measure_sizes.py` | the measurements |
| `make_sample_recovered.py` | writes a `sample_recovered.txt` in the accepted format |

The original `esp_lib.py` was **not** modified.

---

## 1. Round-trip result — the gate

**PASS**, everything below over the whole Commonwealth rather than a sample.
`python test_roundtrip.py`, 10.8 s.

| check | scope | result |
| --- | --- | --- |
| **A. Group tree identity** | all 36 exterior blocks: 74,340 groups, 754,220 records | **215,518,075 bytes in, 215,518,075 out, all 36 blocks byte-identical** |
| **B. Subrecord round trip** | all 36,864 LAND payloads, 236,791 subrecords | all byte-identical; longest subrecord 9,196 bytes, so LAND needs no XXXX escape |
| **C. Compression wrapper** | all 36,864 LAND records (all are compressed) | `[u32 uncompressed size][zlib]` holds for every one, and every one survives our writer then our reader |
| **C2. XXXX oversize escape** | the Commonwealth WRLD payload, 7,133 subrecords | re-emitted **byte-identical**; OFST (148,996 B) and CLSZ exceed the u16 length field |
| **D. Identity splice** | all 36,864 LAND payloads | `splice_land_base_textures(payload, {})` returns the payload **byte-identical** — the brief's check, exactly |
| **D2. Replace in place** | 2,000 LANDs that already have a BTXT | changes **at most the 4 FormID bytes**, no length change, no other quadrant disturbed |
| **D3. Insert** | 2,000 untextured LANDs | **exactly +56 bytes** (4 × (6+8)); every original subrecord bit-identical and in order |

Test A is the strong one. It does not compare the master against itself: group
sizes are **recomputed bottom-up** from the children and record headers are
**re-packed** from their parsed fields, so a wrong size convention or a
mis-packed header cannot pass.

### The negative controls

Per the standing rule — an invariant that cannot fail on broken code is not a
test — each gate was made to fail on purpose, and each did:

| control | detected |
| --- | --- |
| E1 group size written excluding its own 24-byte header | yes |
| E2 block label byte order swapped to (x,y) | yes |
| E3 truncate-toward-zero instead of floor, at x = −40 | yes |
| E4 one payload byte flipped | yes |
| E5 compression wrapper's declared size corrupted | yes |
| E6 a writer that silently drops VCLR | yes |
| E7 a group whose children overrun its declared size | yes |
| C2 the plain u16 writer still refuses the 148,996-byte OFST | yes |

---

## 2. The group structure

### 2.1 The path

Measured by recording the GRUP-type stack above every one of the 36,864
Commonwealth LAND records. **All 36,864 take exactly one path, no variants:**

```
GRUP type 0  "Top",                  label = b'WRLD'
  WRLD record                        formid 0000003C, uncompressed
  GRUP type 1  "World Children",     label = u32 0x0000003C (the WRLD formid)
    GRUP type 4  "Exterior Block",     label = (int16 y, int16 x)
      GRUP type 5  "Exterior Sub-Block", label = (int16 y, int16 x)
        CELL record
        GRUP type 6  "Cell Children",  label = u32 CELL formid
          GRUP type 9  "Cell Temporary Children", label = u32 CELL formid
            LAND record
```

| type | name | count | distinct labels |
| --- | --- | --- | --- |
| 4 | ExteriorBlock | 36 | 36 |
| 5 | ExteriorSubBlock | 576 | 576 |
| 6 | CellChildren | 36,865 | 36,865 |
| 8 | CellPersistent | 1 | 1 |
| 9 | CellTemporary | 36,865 | 36,865 |

36 blocks = 6×6, 576 sub-blocks = 24×24, 36,864 cells = 192×192 over
x,y ∈ [−96, 95], exactly 64 cells per sub-block with no gaps anywhere. The one
extra CellChildren/CellPersistent pair is the worldspace's persistent cell,
which sits directly in the World Children group and has no LAND. **Every LAND
is in a type-9 group; none is in type 8 or type 10.**

Independent confirmation: the same path, and only that path, in DLCCoast
(2,432 LANDs), DLCNukaWorld (4,326), DLCRobot (2) and DLCworkshop03 (4).

### 2.2 Block and sub-block labels — the part most likely to be silently wrong

The label is **two int16s, Y FIRST then X**, little-endian, and the division is
**floor**, not truncation toward zero. Both decided by testing candidates
against all 36,864 cells, not assumed:

| candidate | block | sub-block |
| --- | --- | --- |
| **(int16 y, int16 x), floor** | **36,864 ok / 0 bad** | **36,864 ok / 0 bad** |
| (int16 x, int16 y), floor | 6,144 ok / 30,720 bad | 1,536 ok / 35,328 bad |
| (y, x), truncate toward zero | 9,801 ok / 27,063 bad | — |

```python
block_label    = struct.pack('<hh', floor(y / 32), floor(x / 32))
subblock_label = struct.pack('<hh', floor(y /  8), floor(x /  8))
```

The truncate row is the trap. It agrees on all 9,801 cells with x,y ≥ 0 and
disagrees on the other 27,063 — a tool developed against the positive quadrant
looks perfect and puts three quarters of the Commonwealth in the wrong group.

### 2.3 Ordering

- **Blocks** run `(x=0,y=0) (0,1) (0,2) (0,−3) (0,−2) (0,−1) (1,0) …`. That is
  neither signed `(x,y)` nor signed `(y,x)`. It **is** ascending order of the
  4-byte label read as a **little-endian u32**: 0, 1, 2, 0xFFFD, 0xFFFE,
  0xFFFF, 0x10000, …
- **Sub-blocks** within a block: the same u32-label rule.
- **Cells** within a sub-block: **ascending FormID — 576 of 576 sub-blocks, no
  exceptions.** Not coordinate order by either axis (0 of 576 for each).

### 2.4 Compression as found in the master

| | count | on-disk dataSize |
| --- | --- | --- |
| LAND | 36,864, **all compressed** | min 66, median 88, max 20,205; total 76,028,193 |
| CELL | 36,864, **6,819 compressed** (18%) | min 46, median 46, max 10,437 |

The median CELL payload is 46 bytes — DATA(2) + XCLC(12) + LTMP(4) + XCLW(4)
plus four 6-byte headers — and Bethesda leaves those uncompressed because zlib
inflates them. The writer does the same: `build_record_compressed` falls back
to uncompressed whenever compression does not actually save bytes.

### 2.5 Header tails are copied, never synthesised

The 8 bytes after the FormID (timestamp, VCS, form version, unknown) are not
constant: LAND uses at least 3 distinct tails across the Commonwealth, CELL at
least 5, and GRUP tails vary too. The writer copies each record's and each
group's own tail from the master. Nothing is invented.

### 2.6 How I convinced myself it is right

Four independent ways, because "it loads" is not available to me:

1. **The rebuild test.** 215,518,075 bytes of the master's own group tree
   re-emitted through the writer's `build_group`/`build_record_raw` with sizes
   recomputed, byte-identical. If any of the label encoding, size convention,
   nesting or ordering were wrong, those bytes would differ.
2. **Candidate elimination.** The label order and the floor/truncate question
   were each decided by a count over all 36,864 cells with the wrong answers
   scoring 30,720 and 27,063 failures — not by picking the plausible one.
3. **Retail corroboration.** Four shipped DLC plugins put their LAND overrides
   on exactly the same group path.
4. **Post-build verification.** `verify_plugin()` re-reads every emitted
   plugin with the same reader used on the master and asserts each LAND's
   block and sub-block membership against `block_coords`/`subblock_coords`
   recomputed from the cell's own XCLC coordinates — a second, independent
   derivation of the same fact.

---

## 3. Do the CELL records need overriding too?

**Yes.** Evidence from Bethesda's shipped plugins:

| plugin | LAND records | overriding Fallout4.esm formids | LANDs whose owning CELL is also in the plugin |
| --- | --- | --- | --- |
| DLCCoast.esm | 2,432 | 253 | **2,432 / 2,432** |
| DLCNukaWorld.esm | 4,326 | 43 | **4,326 / 4,326** |
| DLCRobot.esm | 2 | 2 | **2 / 2** |
| DLCworkshop03.esm | 4 | 4 | **4 / 4** |

6,764 LAND records across four retail plugins; in every case the owning CELL is
present in the same plugin, and for every Fallout4.esm-formid LAND that CELL is
itself an override of a Fallout4.esm CELL. No counterexample exists in the
shipped game. Separately, across **every** plugin in the Data folder there are
**0 orphan Cell Children groups and 0 orphan World Children groups** — a
children group never appears without its record.

The structural reason (*reasoned*): a LAND is reachable only through a type-6
Cell Children group whose label is the CELL's FormID and which the format
attaches to the CELL record immediately preceding it. There is nowhere to hang
the children group without the CELL.

**The writer copies the CELL's raw on-disk record bytes and never parses
them** — header, flags, compression and payload straight through. That is what
protects `XCRI`, the precombined-refs list present on 4,160 Commonwealth cells;
a CELL override that dropped it would kill precombines in those cells and cost
real frame rate.

### The localization trap, and why it does not bite on CELL

`Fallout4.esm` sets the Localized flag, so a localizable subrecord holds a u32
string ID into its `.STRINGS`, not text. Copied verbatim into a plugin that is
not itself flagged localized, the engine would read those 4 bytes as an inline
zstring. I counted it: **0 of 36,864 Commonwealth exterior CELLs carry FULL,
DESC, SHRT, RNAM or ITXT.** Full census — DATA 36864, XCLC 36864, LTMP 36864,
XCLW 36864, MHDT 6640, XCLR 4664, VISI 4552, RVIS 4552, PCMB 4486, XCRI 4160,
XPRI 1721, EDID 755, XLCN 665, XCWT 507, XWCN/XWCU/XEZN/XCMO 1 each. EDID is
never localized. So the plugin does **not** set the Localized flag and the CELL
bytes pass through untouched.

It *does* bite on WRLD. See §6.

### FormID remapping: none needed, and asserted

`Fallout4.esm` declares zero masters, so every FormID inside it is `00xxxxxx`.
With Fallout4.esm as this plugin's master #0, `00xxxxxx` still resolves to
Fallout4.esm, so a verbatim payload copy is correct with no remapping. The tool
asserts the master really has no MAST entries rather than trusting it, and
`verify_plugin` asserts Fallout4.esm is master #0 in the output.

---

## 4. Size table

Measured by **actually emitting each plugin**, not extrapolating. Placeholder
assignment = `LDirtGravel01` (`00021336`, the second most-used real Commonwealth
base texture) in all four quadrants of every selected untextured cell.

| selection | cells | compressed | uncompressed | compressed, no WRLD record |
| --- | ---: | ---: | ---: | ---: |
| ring ±16 | 0 | — | — | — |
| ring ±24 | 0 | — | — | — |
| ring ±32 | 496 | 2,488,535 | 3,376,639 | 1,539,345 |
| ring ±40 | 2,613 | 8,362,779 | 13,556,631 | 7,413,589 |
| ring ±48 | 5,454 | 15,328,458 | 26,704,538 | 14,379,268 |
| ring ±64 | 12,686 | 32,275,152 | 59,854,728 | 31,325,962 |
| **all 32,909 untextured** | **32,909** | **51,915,616** | **152,544,137** | **50,966,426** |
| all, but only cells with >64 units of relief | 13,993 | 47,454,115 | — | 46,504,925 |
| **from RECOVER's real `recovered.txt`** | **32,909** | **52,089,800** | — | — |
| from `recovered.txt`, `--min-conf 0.30` | 1,263 | 3,515,978 | — | — |

Rings ±16 and ±24 come out empty because every cell that close to the origin is
already painted; the painted region reaches x −36..32, y −41..32.

**Compression is worth it and is not close: 2.94× on the full plugin**
(51.9 MB against 145.5 MB). Ship compressed.

**The cost of the edit itself is tiny.** For the full run, the same 32,909 LAND
records weigh 45,686,553 bytes in the master and 46,519,636 in our plugin —
**+833,083 bytes for 131,636 inserted BTXTs, about 6.3 compressed bytes per
BTXT, +1.8%.** Our zlib output is within 2% of Bethesda's on the same data, so
nothing is being lost to a bad compression setting. Almost the entire 52 MB is
the VNML/VHGT/VCLR terrain data the override rule *obliges* us to copy.

### Ring or everything?

**Ship everything.** Measured reasons:

- 18,890 of the 32,909 untextured cells are **dead flat** (VHGT relief exactly
  0), and 13,993 have more than 64 units of relief. But dropping all 18,916
  flat ones saves only 51.9 MB → 47.5 MB, **8.6%**: the flat cells compress to
  ~236 bytes each and are effectively free.
- Every cell with real relief lies inside ring ±80 (x −77..76, y −77..76), and
  the counts are identical at ±80 and ±96 — so ±80 loses nothing geometrically,
  yet the saving over "all" is small for the same reason.
- The size is driven by the terrain payload per cell, not by the number of
  cells, so trimming the cell list is a poor lever. `--min-relief` and `--box`
  exist if you want it anyway.

### The ESL flag

- ESL is bit **0x0200**, confirmed against the 9 real `.esl` files in Data,
  which all carry flags `0x281` = ESM(0x1) | Localized(0x80) | ESL(0x200).
- **This plugin is eligible.** It creates no new records: measured, **0 records
  in the emitted plugin have a FormID outside master index 00.** ESL's
  restriction is on new FormIDs (0x800–0xFFF), which it therefore cannot
  violate. `--esl-flag` sets it and was verified to produce flags 0x00000200.
- What it buys: the plugin takes an `FE:xxx` slot instead of one of the 255
  regular load-order slots. At 52 MB it is a heavy thing to spend a full slot
  on, so this is worth having.
- *Reasoned, not measured:* an ESL that also sets the ESM flag sorts into the
  master block and would lose conflicts against ordinary landscape `.esp`s
  loaded later. If this plugin must win over other landscape mods, use
  `--esl-flag` **without** `--esm-flag`. I could not test load-order behaviour.

### The plugin header

`--esm-flag`/`--esl-flag` control TES4 flags; Localized is deliberately never
set. HEDR is `[float version][int numRecords][u32 nextObjectID]`, and
**numRecords = records + GRUPs, excluding the TES4 itself** — measured across
every shipped plugin, where **15 of 16 agree exactly** (only Fallout4.esm's own
header disagrees, by 80,196, which is the master's business). The tool counts
what it actually emitted rather than predicting it, and `verify_plugin` rechecks
the number by re-walking the finished file. `nextObjectID` is 0x800 and nothing
ever allocates from it, since no new records are created.

---

## 5. Open risks — what I could not verify

Neither xEdit nor the game is available to this lane. Everything here is a real
risk, not a formality.

1. **Nothing was loaded.** The plugin has never been opened by xEdit, xLODGen,
   DynDOLOD or Fallout 4. Every structural claim rests on byte-level agreement
   with `Fallout4.esm` and the shipped DLCs. **NEEDS-OVERSEER.**
2. **The WRLD override is the biggest single risk.** See §6 — I had to choose
   between three imperfect options and the choice is not verifiable from bytes.
3. **BTXT insertion position.** A new BTXT is inserted after the last of
   DATA/VNML/VHGT/VCLR and before the first ATXT. That matches where the master
   puts a quadrant's BTXT, and our own reader (`esmdata.cpp:305`) is
   order-insensitive — but whether the *engine* requires a BTXT to precede its
   quadrant's ATXT run is not something I can prove. It only matters for cells
   that already have alpha layers but lack a base, of which the master has
   438 (245 + 193 in the shape census); on the 32,909 target cells there are no
   ATXTs at all, so the question does not arise for the actual mod.
4. **The BTXT "unknown" byte** is written as 0. It is 0 in 56% of the master's
   real BTXTs and takes 95 distinct values otherwise. I believe it is
   uninitialised Creation Kit memory, but I cannot prove the engine ignores it.
5. **Compression level.** We use zlib level 9; Bethesda's output is within 2%,
   so the format is right, but I cannot confirm the engine has no expectation
   about the stream beyond "it inflates".
6. **`--min-conf` semantics.** I read RECOVER's `conf` column and threshold on
   it. Whether their scale means what I assume is theirs to confirm.
7. **Load order and conflicts.** A LAND override loses to any later plugin
   overriding the same LAND. Untested.
8. **The MPCD subrecord** appears on 2 Commonwealth LANDs and is undecoded
   (`LODGEN_ESM_LAYOUTS.md` flags it). We copy it through byte-for-byte, which
   is the right thing, but neither of those cells is in the target set anyway.

---

## 6. The WRLD record — a decision I could not close

This did not go the way I expected and it deserves its own section.

**A LAND override plugin appears to need the WRLD record.** All four DLCs that
override Commonwealth LANDs carry the Commonwealth WRLD record (940,271 bytes
in the master), and across every plugin in Data there are **0 orphan World
Children groups**.

**But copying the master's WRLD record is measurably harmful.** Our plugin
loads last, so its WRLD wins:

- RNAM on a FO4 WRLD is the **large-references table** — decoded here as
  `[int16 gridX][int16 gridY][u32 count]` + `count × [u32 formid][int16 cellX]
  [int16 cellY]`, an encoding that holds for every RNAM in every plugin. The
  master has 7,111 grids / 9,624 refs. **DLCCoast adds 167 refs across 65 grids
  the master does not have, and DLCNukaWorld adds 75.** A verbatim master copy
  loaded last would delete them, and large references are exactly the distant-
  object system this whole project is about.
- The WRLD's `FULL` is a **string ID** (0x0003613F), and unlike CELL this one
  is real. There is no loose `Data\Strings\Fallout4_en.STRINGS` on this
  machine, so I wrote `ba2strings.py` to pull it out of
  `Fallout4 - Interface.ba2` and resolved it: **"Commonwealth"**.

**What the tool does, default `--wrld merge-rnam`:** emit the master's WRLD
payload with two changes — FULL rewritten **inline** as `Commonwealth\0` (which
is what the Creation Kit does saving a non-localized esp, and is why the plugin
needs no Localized flag or strings table), and the RNAM large-reference table
**unioned** with every other plugin's, so nothing in the load order is lost. On
this machine that merges in 997 refs across 65 new grids from DLCCoast (739),
DLCNukaWorld (426), DLCRobot (53) and ccOTMFO4001-Remnants (36). A self-check
asserts the result is a superset of the master's own set, and the payload is
round-tripped through the XXXX escape and re-parsed before it is emitted.

`--wrld master` (verbatim, FULL inline, no merge) and `--wrld none` (omit it
entirely) are both available. The WRLD record costs 949,190 bytes, a one-off.

**What I could not settle:** whether `--wrld none` actually works. It is the
cleanest option — a landscape-texture mod ideally should not touch WRLD at all,
and omitting it removes every conflict above — but there is no shipping
precedent for a World Children group without its WRLD record, and this is
precisely the "loads without complaint but the engine never reads it" failure
the brief warned about. **NEEDS-OVERSEER.**

---

## 7. Defects found

### In `esp_lib.py` (reported, not silently worked around)

The original was not modified; `esp_lib_land.py` is a verbatim copy plus a
clearly separated additions block.

1. **`read_subrecords` does not implement the XXXX oversize escape.** A
   subrecord longer than 65535 bytes is preceded by an `XXXX` subrecord holding
   the real u32 length, and its own u16 length field is written as 0.
   `read_subrecords` would read that 0 and desynchronise, silently returning
   garbage for the rest of the record. Not merely theoretical — the
   Commonwealth WRLD's OFST is 148,996 bytes and CLSZ also exceeds the limit.
2. **`sub()` cannot write an oversize subrecord** — it raises
   `struct.error: ushort format requires 0 <= number <= 0xffff`. This killed
   the first attempt to emit a WRLD override. Fixed in the copy as `sub_any()`.
3. **`walk_all_records` has an off-by-one:** `while pos < len(data) - 24` stops
   24 bytes early and silently drops a record that ends exactly at EOF. Should
   be `<=`.
4. **`decompress_record_payload` / `compress_record_payload` carry an explicit
   "wrapper format NOT validated, no compressed records here to test against"
   caveat.** This lane validated it: all 36,864 Commonwealth LAND records are
   compressed, all 36,864 decompress to their declared size, and all 36,864
   survive our own write-then-read. **The caveat can be lifted.**

### In this project's own documentation

5. **The BTXT byte at offset 5 is not padding.** `src/esmdata.cpp` (~line 305)
   and `docs/LODGEN_ESM_LAYOUTS.md` both describe BTXT as
   `formID u32 | quadrant u8 | pad | layer s16`. DLCRobot's override of LAND
   `0000F124` changes **exactly that byte** (0xC7 → 0x4E) in one BTXT while
   leaving the FormID, quadrant and layer alone. Across the master it takes 95
   distinct values, 0 in 56% of cases and the rest spread thinly — the
   signature of uninitialised Creation Kit stack memory reaching disk. It is an
   "unknown", not a pad. Reading it as padding is harmless; *writing* a
   copied record while assuming it is padding would not be.
6. **`docs/LODGEN_ESM_LAYOUTS.md` line 18 has a typo** in the BTXT quadrant
   note: `(0 BL, 1 BR, 2 TL, 3 BR->TR)`. The intent is clear but it reads as
   two BRs.
7. Confirmed from the corpus, worth recording: **BTXT layer is −1 in all 12,471
   Commonwealth BTXTs**, no exceptions.

### My own mistake

8. **I overwrote the RECOVER lane's `recovered.txt`.** My
   `make_sample_recovered.py` defaulted to that filename in the shared laneb
   folder and wrote a placeholder over it. RECOVER rewrote the file about a
   minute later so nothing appears to have been lost, but it was luck, not
   design — that folder is shared and I took a name another lane owns. The
   script now defaults to `sample_recovered.txt` and carries a comment saying
   why.

---

## 8. Joining with RECOVER

Their file is already here and **the two lanes join today**:

```
python make_landfix_esp.py --recovered recovered.txt --out LandFix.esp
  assignments: 32909 cells
  32909 lines read (32909 in RECOVER format, 0 in the terse format)
  built: 52,089,800 bytes (32909 cells, 65819 records, 66391 GRUPs)
  BTXT inserted 131636, replaced 0
  verify: 32909 CELL records byte-identical to the master
  verify: 32909 LAND records checked against the expected splice
  VERIFY OK
```

The parser accepts their format —
`R <cx> <cy> q0=<ltex>[:w][;...] … conf=<0..1>` — taking the highest-weight
candidate per quadrant, as well as a terser `<x> <y> <q0> <q1> <q2> <q3>` form.
`--min-conf` thresholds on their confidence column.

**One thing to carry to whoever decides what ships.** RECOVER's own header says,
in their words, `*** DO NOT SHIP THIS AS A MATERIAL RECOVERY. READ
report_recover.md FIRST. ***`, and reports top-1 accuracy of **0.8% beyond 16
cells** from painted terrain against an 18.5% null model, while the median cell
in the file is 42.5 cells out. My lane can write those assignments into a
structurally perfect plugin — that is what it does — but a correct plugin
carrying wrong materials is still wrong on screen. `--min-conf 0.30` keeps 1,263
cells (3.5 MB), which is their own stated threshold for where the numbers hold
up. **That is a call for the overseer, not for either lane.**

---

## 9. NEEDS-OVERSEER

1. **Load `LandFix.esp` in xEdit.** The single highest-value check available.
   It settles the WRLD question, the group structure, and whether anything
   about the file is malformed in a way byte-comparison cannot see.
2. **Decide the WRLD policy** (§6): `merge-rnam` (default, lossless but
   conflicts with anything else touching the Commonwealth WRLD), `master`
   (deletes 242 DLC large references — do not use), or `none` (cleanest, no
   precedent, may not be read at all).
3. **Decide what ships**, given RECOVER's accuracy numbers (§8). Structurally
   sound and materially wrong is a real outcome.
4. **The xEdit definitions file** (`wbDefinitionsFO4.pas`) that memory
   mentions is gone from the old session scratchpad. I worked from
   `esmdata.cpp` and the real bytes instead and did not need it, but it would
   have shortened §2 considerably and would settle open risks 3 and 4. Worth
   re-fetching if it is cheap.
5. **Two documentation fixes** in the repo, if you want them (§7 items 5 and 6)
   — I did not touch the repo, per the lane rules.
6. **Run `test_roundtrip.py` again after any change to `esp_lib_land.py`.** It
   takes 11 seconds and it is the only thing standing between this tool and a
   silently corrupt plugin.

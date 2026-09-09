# Lane ESPWRITE — a Fallout 4 plugin that carries LAND texture overrides

Pure Python. **You cannot run `release/NifSkope.exe`** — the sandbox refuses it
from a B session. Do not retry it. Never build, never launch a GUI, never edit a
file inside the repo, and **never write into the Fallout 4 Data folder**. Writes
go only to `C:\Users\bungo\AppData\Local\Temp\claude\laneb\`.

Write `report_espwrite.md` **incrementally**, and commit code to disk as you go.
You may spawn your own subagents; they bill to this account.

---

## THE GOAL

A companion lane (RECOVER) is working out which LTEX materials belong in the
32,909 Commonwealth cells that have terrain but no painted material. Your job is
the other half: **a tool that writes those assignments into a valid Fallout 4
`.esp`** as LAND record overrides, so any LOD generator — xLODGen, DynDOLOD, ours
— can bake correct far terrain from it, and so a landscape texture replacer
reaches the distant mountains for the first time.

You do **not** need RECOVER's output to build and prove this. Develop against a
placeholder assignment (say, one known LTEX everywhere) and prove the plugin is
structurally correct. The two lanes join at the end.

**The deliverable is a script**, `make_landfix_esp.py`, that takes a `recovered.txt`
and emits the plugin. Not a one-off blob.

---

## WHAT MAKES THIS HARD, and the trap to respect

A Bethesda record override **replaces the whole record**. You cannot ship "just
the BTXT". Every LAND you touch must carry its VNML, VHGT, VCLR and everything
else copied verbatim from the master, with only the texture subrecords changed or
added. Get that wrong and the terrain out there goes flat or black in-game.

So the core of the tool is: read the master's LAND record, splice in the texture
subrecords, re-emit it under the plugin's own group structure, byte-identical in
every other respect.

**Prove that with a round trip before you write a single new byte of texture
data.** Read a vanilla LAND, re-emit it through your writer with no changes, and
assert the payload is byte-identical to what you read. If that does not hold,
nothing downstream is trustworthy. That check is the gate for this whole lane.

---

## RESOURCES

* **`E:\Projects\Claude\esp_lib.py`** — an existing, working binary FO4 plugin
  library from another project of bungo's. It already has
  `read_record_header`, `read_record_header_tail`, `read_group_header`,
  `read_subrecords`, `sub(tag, content)`, `build_record`, `walk_all_records`,
  `build_formid_index`, `find_record_by_edid`, `find_group`, `insert_records`,
  `delete_records`. Read it fully before writing anything of your own; reuse it
  rather than reimplementing, and extend it in a COPY under the laneb folder
  (never edit the original).
* **`E:\Projects\NifskopeWildWastelandEdition\src\esmdata.cpp`** — the
  authoritative reader for the subrecords you care about. BTXT parsing ~line 305,
  ATXT ~310, VTXT ~322, VCLR ~351, and a second parse site ~line 629. This is
  measured, working code against the real corpus; treat it as the layout spec.
* **`E:\Projects\NifskopeWildWastelandEdition\docs\LODGEN_ESM_LAYOUTS.md`** — the
  repo's own notes on these records, including that quadrants are 0 BL, 1 BR,
  2 TL, 3 TR.
* Master: `X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm`.
  **Read-only. Never write to that folder.**
* `layers.txt` in the laneb folder — every cell's current BTXT/ATXT state, so you
  can find real examples of each shape (no textures, base only, base plus layers).
* The Commonwealth worldspace is form id `0000003C`, cells x -96..95, y -96..95,
  36,864 of them with LAND.

The xEdit definitions file that memory mentions is **gone** (it lived in an old
session scratchpad). Work from `esmdata.cpp` and the real bytes instead. If you
want it, note that under NEEDS-OVERSEER.

---

## WHAT TO WORK OUT AND REPORT

1. **Group structure.** A LAND lives under WRLD -> a cell block group -> a cell
   sub-block group -> CELL -> its temporary-children group. Work out from the
   master exactly which group types and labels FO4 uses, how block and sub-block
   are derived from cell coordinates, and reproduce it. This is the part most
   likely to be silently wrong: a plugin that loads without complaint but whose
   records the engine never reads. Say how you convinced yourself it is right.
2. **The round trip**, as above. Report it as a pass/fail with byte counts.
3. **Do the CELL records need overriding too**, or only LAND? Find out from the
   master's structure and say which, with evidence.
4. **Compression.** FO4 records can be zlib-compressed individually. Measure both
   ways: what does the plugin weigh with and without? This decides whether the mod
   ships all 32,909 cells or a ring around the playable area, so give me real
   numbers, not an estimate. Measure a real LAND record's size from the master
   rather than computing it from field widths.
5. **Header.** TES4 with Fallout4.esm as its master, correct record count and
   next-object-id. Check whether the ESL flag is possible here — these are all
   overrides, no new form ids — and say what it would buy.
6. **Scale test.** Emit a plugin covering a few hundred real cells with
   placeholder textures and report its size, so the full-run size is measured
   rather than extrapolated.

---

## OUTPUT

`report_espwrite.md`:
1. **Round-trip result** — the gate. Pass or fail, with numbers.
2. **The group structure**, described precisely enough that someone could
   reimplement it, and how you verified it.
3. **Size table** — cells against plugin bytes, compressed and not.
4. **Open risks** — everything you could not verify without loading the plugin in
   the game or in xEdit. Be explicit; neither is available to you.
5. **NEEDS-OVERSEER** — anything you want me to run or check.

Files: `make_landfix_esp.py`, a copied and extended `esp_lib`, the test plugin,
and your round-trip harness.

Two standing rules from this project that apply here. Measure, do not eyeball: an
invariant that cannot fail on broken code is not a test, so make each check fail
on purpose once and say that it did. And a mistake found is written down — if you
find a defect in `esp_lib.py`, report it rather than silently working around it.

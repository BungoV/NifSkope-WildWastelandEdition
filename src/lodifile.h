/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODIFILE_H
#define LODIFILE_H

#include <QString>
#include <QStringList>

#include <cmath>
#include <vector>

/*! `.lodi` v1 -- the FO4CS-native INSTANCE TABLE: one 24-byte quantised record
 *  per placed object of a worldspace, binned into 4-cell (16,384-unit) chunks
 *  with a dense north-up chunk table, a per-cell range blob for near-field
 *  suppression, and a parallel 8-byte COLD record carrying the `(ref, part)`
 *  key a drawing consumer never loads. It points into `.lodo`
 *  (src/lodofile.h) by `baseId`.
 *
 *  The contract is docs/LODGEN_NATIVE_LODO_LODI.md; this header is the
 *  layout. The same rules as `.lodo` apply (little-endian, absolute 64-bit
 *  offsets, 4,096-aligned zero-padded payloads, CRC-32 zlib, reserved = 0 or
 *  a refusal, a 32-byte editor ID refused not truncated), plus:
 *
 *   * NORTH-UP row order in the chunk table and inside a chunk's cells, and
 *     the header bit that says so is a REFUSAL when clear (`.lodl` is
 *     row-0-SOUTH; the mirror trap has already cost a consumer once);
 *   * the two silent u16 ceilings are REFUSALS at write time: a `scale` above
 *     65535/8192 = 7.99988 names the ref, a `baseId` above 65,535 names the
 *     base. Never a clamp, never a drop;
 *   * a zero `lodoIdentity` without the NOLIB bit is a refusal, and a set
 *     NOLIB bit with a non-zero identity is one too.
 *
 *  THE ONE SORT LAW (v2, 2026-09-11, lane NATIVE1a). Instance order is part of
 *  the format, and there is exactly one law, checkable by the `.lodi` reader
 *  alone:
 *
 *      chunk index          north-up row-major over the dense table
 *      cell index           north-up row-major inside the chunk, (3 - ly)*4 + lx
 *      drawKey              the base's (mesh, material) draw rank -- see below
 *      refFormId
 *      scolPart
 *
 *  bungo 2026-09-11 08:1x asked for instances "pre-sorted by mesh then
 *  material" at bake time. Mesh and material could NOT become the outermost
 *  key: the cell-range blob states one (first, count) run per cell and a
 *  mesh-major order inside a chunk leaves a cell's instances in up to sixteen
 *  disjoint runs, which the 8-byte cell row cannot describe. The CELL stays
 *  outermost and the mesh/material rank sorts inside it. A cell is 4,096 units
 *  -- the granularity the near-field suppression the blob exists for works at
 *  anyway -- so a consumer building per-bucket lists still walks one
 *  contiguous run per (cell, mesh, material). Stated as a deviation in the
 *  contract page; the reverse (mesh outermost, cell ranges dropped) is
 *  bungo's to take.
 *
 *  `drawKey` is a u16 in the instance record's v2 growth slot (0x16, reserved
 *  in v1). It is the RANK of the base's (primary mesh id, that mesh's first
 *  material id) pair among every base in the `.lodo`, so it is a pure function
 *  of `baseId` -- redundant on purpose, because it is what lets the `.lodi`
 *  reader check the sort law without opening the `.lodo`, and what lets
 *  `--native-verify` and the independent decoder check the rank itself
 *  against the library. */

//! First four bytes, little-endian: `L`,`O`,`D`,`I`.
constexpr quint32 LODI_MAGIC = 0x49444F4CU;
/*! v3 (2026-09-11, lane NATIVE1b): PRECOMPUTED OCCLUDERS. Two new tables at the
 *  header room v2 reserved -- up to `LODI_OCCLUDERS_PER_CELL` oriented boxes a
 *  cell, each one fitted INSIDE a real placed object, and a per-cell range blob
 *  parallel to the cell ranges. bungo, 2026-09-11 08:2x, "2 sounds good":
 *  *"a few boxes per cell for buildings and hills, baked from the meshes"*.
 *
 *  v2 and v1 are REFUSED BY NAME. A v2 file has both table offsets at zero,
 *  which a v3 reader would take as "this worldspace occludes nothing" -- a
 *  silent, total loss of the chunk rejection the boxes exist for, and exactly
 *  the class of failure the version word is for. The pair must match: a v3
 *  `.lodi` beside a v2 `.lodo` is refused by the identity rule already. */
constexpr quint32 LODI_VERSION = 3;
/*! v4 (2026-09-11, lane CARDS-AGG): AGGREGATE RING-3 IMPOSTORS. bungo, 08:2x
 *  -> 08:3x, *"1 sounds good"*: one card set per forested cell, photographed
 *  from the horizon views, and *"the ring 3 instance list then holds one
 *  placement per cell instead of one per tree"*.
 *
 *  Two new tables in the room v3's section 12 named (header 0xB0..0xFF): an
 *  aggregate row a forested cell, and a flat u32 blob naming, per aggregate,
 *  exactly which INSTANCES that aggregate stands for. The instances themselves
 *  are NOT removed -- the `.lodi` has no rings (4.4), one table serves every
 *  distance -- so the aggregate SUPPRESSES them past its own projected-size
 *  threshold instead, and the covered blob is what says which.
 *
 *  THE VERSION IS CONDITIONAL, and that is a deviation stated rather than
 *  taken quietly (contract 11, Deviation 6). A bake with no aggregate writes
 *  **version 3, byte for byte what it wrote before**, because aggregation is a
 *  MODULE and CONSTITUTION 10 requires its off value to be the exact way back.
 *  A bake WITH aggregates writes version 4. The reader accepts 3 and 4 and
 *  refuses 1 and 2 by name: a v3 file read as v4 is unambiguous (zero
 *  aggregates, both offsets 0, and the header words after 0xB0 are the zero pad
 *  the v3 writer already wrote), which is exactly the case v3 could not make
 *  for v2. */
constexpr quint32 LODI_VERSION_AGGREGATE = 4;

/*! v5 (2026-09-16, lane NATIVE1c): THE PLACEMENT AO BLOB, and the four
 *  per-MNAM-slot instance totals.
 *
 *  bungo, 2026-09-11 15:3x: *"is vertex AO baked into impostors too on top of
 *  the texture AO they hold?"* -- it was not. A card carries the model's own
 *  vertex colour and a self-AO times the texture's AO, all of which are
 *  constant across every copy of that model. What it never carried is the
 *  PLACEMENT's own occlusion: the ground it sits on, the bridge over it, the
 *  building beside it -- which the chunk MESHES have had per vertex, ray-cast,
 *  in colour B, since the object channels shipped.
 *
 *  The 24-byte instance record has no room: bits 6..15 of `flags` are the only
 *  space and a set reserved bit is a refusal there (docs 4.1). So the byte goes
 *  in a blob of its OWN, one u8 an instance, parallel to the instance blob at
 *  the instance's own index -- the same join the cold blob uses. Neither of the
 *  two homes the brief named was taken and the reason is in docs 11: the cold
 *  record is exactly 8 bytes and growing it to 12 is a stride the reader must
 *  then refuse by name, for a blob the draw path never reads anyway; and a
 *  per-CHUNK table cannot say that THIS tree is under a bridge and that one is
 *  in a field, which is the entire measurement.
 *
 *  **0xFF is NOT AO 255, it is NOT MEASURED.** A measured value is clamped to
 *  0..254 so the refusal has a byte of its own instead of hiding inside a
 *  plausible answer; the census counts the refusals by name.
 *
 *  A v5 header is a SUPERSET of a v4 one: the aggregate words at 0xB0..0xD3
 *  are present and may be all zero, which means no aggregate. THE VERSION IS
 *  CONDITIONAL, on the same reasoning as v4's (contract 11, deviation 12):
 *  the placement AO is a MODULE and `--native-no-placement-ao` has to be the
 *  exact way back, which means byte-identical, which an unconditional bump
 *  would make impossible. The four slot totals ride with the module rather
 *  than taking a switch of their own, because the alternative is writing them
 *  into bytes a v4 file declares must be zero.
 *
 *  Versions 1 and 2 stay refused by name. A v5 file with no AO blob, and a v3
 *  or v4 file with a non-zero AO offset, are each refused by name too. */
constexpr quint32 LODI_VERSION_PLACEMENT_AO = 5;
constexpr quint8 LODI_PLACEMENT_AO_UNMEASURED = 0xFF;
constexpr quint8 LODI_PLACEMENT_AO_MAX = 0xFE;
/*! VERSION 6 (2026-09-18, bungo: "where is the vertex AO we had?" / "The AO on
 *  the objects was from the objects themselves, from objects amongst each
 *  other, and with the objects and terrain and with objects on nearby chunks
 *  too"): THE VERTEX-AO BLOB. One byte per LIBRARY VERTEX per INSTANCE, the
 *  scene occlusion the `.BTO` route writes into every vertex's colour B, cast
 *  at the placement's world position against the chunk's heightfield, every
 *  placement of the chunk and the apron of placements one cell into the
 *  neighbours (`src/lodgenao.h`, the one caster). The per-placement byte of
 *  v5 is that cast's MEAN; this blob is what it was averaged from.
 *
 *  Layout, at header 0xF4 (u64 offset) / 0xFC (u32 bytes): u32 first[n + 1]
 *  in INSTANCE order, then the bytes; instance i's vertices are
 *  bytes[first[i] .. first[i+1]), one per vertex of the mesh it draws
 *  (`LodoBase::rep[mnamSlot]`), in that mesh's vertex order -- the union of
 *  its clusters' vertex ranges, which the writer checks is contiguous -- and
 *  EMPTY (first[i] == first[i+1]) for a card-drawn instance or one whose
 *  chunk gave no heightfield. 255 = open; there is no "not measured" word,
 *  an unmeasured instance is an empty range. A v6 header is a SUPERSET of a
 *  v5 one; the version is conditional for the same reason as v4's and v5's
 *  (`--native-no-vertex-ao` is the exact way back). The pad is 0xF1..0xF3. */
constexpr quint32 LODI_VERSION_VERTEX_AO = 6;
/*! VERSION 7 (2026-09-18, bungo: "The houses should be one object each though,
 *  for identity" / "We need per vertex sky visbility too"): TWO additions.
 *
 *  (a) THE GROUP TABLE, header 0x100 (u64 offset) / 0x108 (u32 count) /
 *  0x10C (u16 stride = 2): one u16 an instance, parallel to the instance table
 *  and in the same order. `identity` stays what it always was -- unique over a
 *  chunk, one value a placement -- and the GROUP is the thing a viewer colours
 *  when it wants "one house, one colour". Ids are dense PER CHUNK from 0: a
 *  chunk holding C groups uses exactly {0 .. C-1} and uses every one. Per chunk
 *  and not per file because a whole Commonwealth would run past 65,536 groups;
 *  the price is that a house cut by a chunk line is two groups, one a side, and
 *  that is the honest answer rather than a fabricated join across a seam the
 *  bake never sees whole.
 *
 *  (b) THE VERTEX-SKY STREAM, header 0x110 (u64 offset) / 0x118 (u32 bytes):
 *  the SAME layout as v6's vertex-AO blob at 0xF4, byte for byte -- u32
 *  first[n + 1] in instance order, then one byte a library vertex in the mesh's
 *  vertex order, `first[0] == 0`, monotone, `first[n] == bytes - 4 (n + 1)`.
 *  The values are `LodgenAoScene::skyVisibility(p, 300)` against the same scene
 *  the v6 AO stream is cast in, in the same `place`/`perVertex` loop. The 0x11
 *  `sky` byte is the mean of a RELATED cast over a DIFFERENT vertex population
 *  (the stock .BTO chunk mesh's, see docs s4.10); the two agree to a quarter of
 *  a byte for a typical placement and diverge near a chunk line.
 *
 *  Version 7 is written when the file carries EITHER table. A v7 header is a
 *  SUPERSET of a v6 one and `--lodi-v6` is the exact way back. The header BLOCK
 *  grows 256 -> 512 bytes for v7 ONLY: 0x100..0xFFF was zero pad before the
 *  4,096-aligned first payload in every .lodi ever written, so no payload moves
 *  and no v3..v6 file changes by a byte -- `headerCrc32` covers
 *  0x10 .. lodiHeaderBytes(version) - 1, which is still exactly 256 there. */
constexpr quint32 LODI_VERSION_GROUP_SKY = 7;
/*! VERSION 8 (2026-09-18, lane HORIZON1, from bungo's "B sounds good" ruling on
 *  far LOD shadows): THE PER-VERTEX HORIZON STREAM, header 0x11C (u64 offset) /
 *  0x124 (u32 bytes) / 0x128 (u16 azimuths) / 0x12A (u16 march steps) /
 *  0x12C (f32 reach in world units).
 *
 *  THE LAYOUT MIRRORS s4.10's sky stream exactly, with one difference that is
 *  the whole point: `A` bytes a library vertex instead of one. `first[i]` is
 *  still a BYTE offset, so instance i owns `first[i] .. first[i+1]` and that run
 *  is `A` times the vertex count of the mesh it draws -- which the reader checks
 *  against the AO stream's slice, since the three streams are ONE vertex
 *  population.
 *
 *  A byte is the maximum ELEVATION of any occluder in that azimuth bin, above
 *  the vertex's own tangent plane, as `round( elevation_deg / 90 * 255 )`. 0 is
 *  "nothing blocks in that direction" and 255 is the zenith. Rays BELOW the
 *  tangent plane are the surface itself and are never cast: N.L already handles
 *  that side, and casting them would darken every back face twice.
 *
 *  Bin 0 is centred on NORTH (+Y) and the bins step CLOCKWISE seen from above,
 *  i.e. toward EAST (+X): bin k covers azimuth `k * 360/A` plus or minus
 *  `180/A` degrees. A runtime sun at azimuth `a` reads the two bins whose
 *  centres straddle it and lerps between them.
 *
 *  Version 8 is written when the stream is present. A v8 header is a SUPERSET
 *  of a v7 one and `--lodi-v7` is the exact way back, byte for byte: the stream
 *  is written LAST so no payload offset moves, the new header words live in the
 *  v7 block's own reserved pad (0x11C..0x1FF), and the header block stays 512
 *  bytes, so a v7 file's `headerCrc32` window and bytes are untouched. */
constexpr quint32 LODI_VERSION_HORIZON = 8;
/*! **VERSION 8 IS RETIRED AND NO WRITER IN THIS TREE PRODUCES IT** (lane
 *  HORIZONOUT, 2026-09-19, from bungo's "horizon goes bye bye now, we're back
 *  to identity"). The measured reason is in the version table of
 *  docs/LODGEN_NATIVE_LODO_LODI.md s3.7: baked per-vertex object horizons
 *  disagreed with a ray-cast sun on 50-58 per cent of object pixels at a low
 *  sun, where the identity far shadow map simulated at 64 units disagreed on
 *  about 9 per cent.
 *
 *  THE READER STAYS TOLERANT, deliberately: a v8 file baked by
 *  `release/NifSkope.before_horizonout.exe` or by any earlier build is still
 *  opened, its five header words are still parsed and still validated, and the
 *  stream's region is still named in the note line -- its PAYLOAD is skipped by
 *  its own length rather than copied into the table, because nothing in this
 *  tree consumes it any more. A file met in the wild must never crash a reader
 *  for carrying a retired stream. The constants below stay for the same
 *  reason: they are what the bytes of such a file MEAN. */
/*! v9, THE WORKSHOP-SCRAPPABLE BIT (lane HORIZON3, 2026-09-19; the layout
 *  settled by lane HORIZONOUT the same day).
 *
 *  **VERSION 9 IS THE v7 LAYOUT PLUS BIT 6 OF THE INSTANCE FLAGS. IT CARRIES NO
 *  HORIZON STREAM.** A default bake writes version 7; a bake with
 *  `--scrappable` writes version 9. Version 8 sits between the two numbers and
 *  outside the line of descent -- it is the retired baked-horizon stream above,
 *  and 9 is NOT a superset of it.
 *
 *  WHY THE NUMBER MOVES AT ALL, when the layout does not. The version word is
 *  the only field that tells a reader WHICH INSTANCE FLAG BITS MAY APPEAR.
 *  Below v9, bit 6 is reserved-zero and a reader is entitled to refuse it --
 *  which is exactly what this one does. Writing the bit into a file stamped
 *  version 7 would be a file that its own contract says cannot exist, so the
 *  number has to move even though not one byte of layout does.
 *
 *  WHY NOT REUSE 7. A v7 reader built before today -- ours on an older exe, and
 *  any consumer -- reads `LODI_INST_FLAGS_KNOWN` as 0x3F. A v7 file with bit 6
 *  set would be refused by it, or worse, silently accepted by a looser one. A
 *  new number is the only honest way to say "there is one more bit here".
 *
 *  A placement the player can walk up to and scrap is a placement that WILL
 *  NOT BE THERE -- and a far field that keeps drawing it, and keeps casting
 *  its baked shadow, is wrong about a settlement from the first hour of a save
 *  onwards. The bit says which placements those are, so a consumer can drop
 *  them from the far field the moment the workshop's own scrap list says they
 *  are gone, instead of re-deriving a three-clause plugin rule at runtime.
 *
 *  IT COSTS NOTHING. It is bit 6 of the instance flags word the 24-byte record
 *  has carried since v1, so a v9 file is a v7 file with one more bit
 *  meaningful.
 *
 *  The rule is `EsmScrapIndex` in `src/esmdata.h` and the count it produces on
 *  the measured urban region is 14 of 33,123. Leaving `--scrappable` off (the
 *  default) writes no bit, keeps the version at 7, and the file is byte for
 *  byte the v7 file. */
constexpr quint32 LODI_VERSION_SCRAPPABLE = 9;
/*! VERSION 10 IS THE v9 LAYOUT PLUS BIT 7 OF THE INSTANCE FLAGS, `SCALE_WIDE`
 *  (lane BAKE2, 2026-09-25; the director's ruling (a) the same day).
 *
 *  THE CEILING IT LIFTS. The instance record stores its scale as u16 / 8192, so
 *  nothing above 65535/8192 = 7.99988 fits, and the writer refused the whole
 *  file on such a ref. The engine and the CK allow a reference scale up to 10.0,
 *  and Nuka-World places four LOD-carrying cliffs above the old line
 *  (0604D45A 9.97, 0604D45D 8.33, 0604DDA1 9.23, 0604DDB9 8.33): its .lodi could
 *  not be written at all.
 *
 *  THE RULE. Bit 7 set: scale = 8 + v / 8192, range 8 .. 15.99988. Bit 7 clear:
 *  scale = v / 8192, exactly as before. The step is 1/8192 on both sides, so no
 *  instance anywhere reads coarser than it did, and 16 is 60 percent headroom
 *  over the engine's 10. A scale above 15.99988 is still REFUSED, not clamped.
 *
 *  WHAT DOES NOT MOVE. A placement at or below 7.99988 is written with bit 7
 *  clear and the very same u16, and the version rises to 10 ONLY when some
 *  instance carries the bit. A file whose scales all fit is therefore byte for
 *  byte the file this writer wrote before (v7, or v9 with `--scrappable`); the
 *  installed Commonwealth .lodi stays a v7 file every reader keeps accepting.
 *
 *  Like v9, the number moves because the version word is the only thing that
 *  tells a reader which flag bits may appear: below v10, bit 7 is reserved zero.
 *  10 implies v7's 512-byte header block, so a `--lodi-v6` bake that meets a
 *  wide scale is refused (no pre-v7 version can say it). The FO4CS reader owes
 *  the same decode. */
constexpr quint32 LODI_VERSION_WIDE_SCALE = 10;
/*! WHAT A VERSION-8 FILE'S BYTES MEAN (contract s4.11). No writer in this tree
 *  produces such a file any more (see LODI_VERSION_HORIZON above) and there is
 *  no longer a switch that moves these; they stay because a reader that meets
 *  a v8 file in the wild needs them, and needs no second file to have them.
 *
 *  16 azimuths = 22.5 degrees a bin. The reach is 127,561 units, which is the
 *  tallest thing standing in the measured Commonwealth region (11,160 u of
 *  GreebTower02 over the terrain under it) divided by tan(5 degrees) -- the sun
 *  elevation below which the shadow of the tallest object is longer than the
 *  march (lane HORIZON1 step 1b; the plugin stores sunrise/sunset TIMES and no
 *  angle at all, so the bar is a stated default and not a read value). */
constexpr quint16 LODI_HORIZON_AZIMUTHS = 16;
constexpr float LODI_HORIZON_REACH = 127561.0f;
//! The consumer's smoothstep half-width at the horizon edge, degrees (Sloan-Cohen softening).
constexpr float LODI_HORIZON_SOFT_DEG = 1.0f;
//! The u8 quantiser: `byte = round( elevation_deg / 90 * 255 )`. One step = 0.3529 deg.
constexpr float LODI_HORIZON_DEG_PER_STEP = 90.0f / 255.0f;
constexpr quint32 LODI_HEADER_BYTES = 256;
constexpr quint32 LODI_HEADER_BYTES_V7 = 512;
constexpr quint16 LODI_GROUP_STRIDE = 2;
//! The emitter's "this placement is its own group" key; never written to a file.
constexpr quint32 LODI_GROUP_ALONE = 0xFFFFFFFFu;
constexpr quint32 LODI_PAYLOAD_ALIGN = 4096;

//! The header BLOCK for a version word. The one place 256 and 512 meet.
inline quint32 lodiHeaderBytes( quint32 version )
{
	return version >= LODI_VERSION_GROUP_SKY ? LODI_HEADER_BYTES_V7 : LODI_HEADER_BYTES;
}

enum LodiHeaderFlags
{
	LODI_FLAG_ROW_ORDER_NORTH_UP = 1,   //!< clear = refusal
	LODI_FLAG_PARTIAL = 2,              //!< a region bake; merge by chunk key, last wins
	LODI_FLAG_NOLIB = 4                 //!< written before a .lodo exists; lodoIdentity must be 0
};
constexpr quint32 LODI_FLAGS_KNOWN = LODI_FLAG_ROW_ORDER_NORTH_UP | LODI_FLAG_PARTIAL | LODI_FLAG_NOLIB;

constexpr quint16 LODI_CHUNK_CELLS = 4;
constexpr quint16 LODI_INSTANCE_STRIDE = 24;
constexpr quint32 LODI_MAX_CHUNKS = 65536;
constexpr float LODI_CELL_UNITS = 4096.0f;
constexpr float LODI_CHUNK_UNITS = 16384.0f;        //!< LODI_CHUNK_CELLS x 4096
constexpr float LODI_SCALE_DIVISOR = 8192.0f;
constexpr float LODI_SCALE_MAX = 65535.0f / 8192.0f;  //!< 7.99988, the narrow range's top (v10: bit 7 above it)
constexpr float LODI_SCALE_WIDE_BASE = 8.0f;          //!< v10: bit 7 set adds this to v / 8192
constexpr float LODI_SCALE_MAX_WIDE = LODI_SCALE_WIDE_BASE + 65535.0f / 8192.0f;  //!< 15.99988, the refusal line
constexpr quint32 LODI_BASE_MAX = 65535;

/*! v3: at most this many occluder boxes a cell, the largest by world volume.
 *  Four is a deliberate start and it is in the header, so a reader sizes its
 *  per-cell array statically and a later bake may raise it without a format
 *  break. */
constexpr quint16 LODI_OCCLUDERS_PER_CELL = 4;
constexpr quint16 LODI_OCCLUDER_STRIDE = 40;

//! v4: the aggregate row's stride, and the reference threshold the band rests on.
constexpr quint16 LODI_AGGREGATE_STRIDE = 48;
/*! The projected WIDTH, in pixels of the contract's reference projection
 *  (`projectionScale` 1371.0, 4.4), at which a cell's aggregate takes over from
 *  its per-tree cards: three quarters of a 128-px frame, i.e. the point past
 *  which the sheet can no longer add detail. The per-tree cards cross-fade out
 *  over `switchPx .. bandRatio * switchPx`. Both travel in the header so a
 *  consumer never has to guess a band, and both are recomputed by the consumer
 *  against its LIVE projection -- the pixel number is the rule, the distance is
 *  not. */
constexpr float LODI_AGG_SWITCH_PX = 96.0f;
constexpr float LODI_AGG_BAND_RATIO = 1.2f;
//! The aggregate identity space: the top bit set, so it can never collide with
//! an instance index (contract 4.6, bungo's 08:4x far-shadow ruling).
constexpr quint32 LODI_AGG_IDENTITY_BIT = 0x80000000U;

//! v4: aggregate.flags
enum LodiAggregateFlags
{
	LODI_AGG_HEIGHT = 1,        //!< the sheet's normal B carries height (always, today)
	LODI_AGG_MIRRORED = 2       //!< at least one source tree was composited mirrored
};
constexpr quint16 LODI_AGG_FLAGS_KNOWN = 0x3;

//! instance.flags
enum LodiInstanceFlags
{
	LODI_INST_MIRRORED = 1,         //!< the tree's UV mirror, (treeHash >> 8) & 1
	LODI_INST_FORCE_CARD = 2,
	LODI_INST_ALPHA_TESTED = 4,
	LODI_INST_EMITS = 8,
	LODI_INST_SCOL_PART = 16,
	LODI_INST_BURIED_CANDIDATE = 32,
	//! v9: the player can scrap this placement at a workshop (LODI_VERSION_SCRAPPABLE)
	LODI_INST_SCRAPPABLE = 64,
	//! v10: scale = 8 + v / 8192 (LODI_VERSION_WIDE_SCALE); set by the writer, never by a caller
	LODI_INST_SCALE_WIDE = 128
};
constexpr quint16 LODI_INST_FLAGS_KNOWN = 0xFF;

/*! v10, the one encoder and the one decoder of the instance scale. At or below
 *  LODI_SCALE_MAX the word is `lround( s x 8192 )` clamped to u16 -- the exact
 *  arithmetic of every version before 10 -- and the bit is clear. */
inline bool lodiScaleIsWide( float s )
{
	return s > LODI_SCALE_MAX;
}
inline quint16 lodiScaleWord( float s )
{
	const float b = lodiScaleIsWide( s ) ? s - LODI_SCALE_WIDE_BASE : s;
	const long v = std::lround( b * LODI_SCALE_DIVISOR );
	return quint16( v < 0 ? 0 : ( v > 65535 ? 65535 : v ) );
}
inline float lodiScaleValue( quint16 word, quint16 flags )
{
	const float s = float( word ) / LODI_SCALE_DIVISOR;
	return ( flags & LODI_INST_SCALE_WIDE ) ? s + LODI_SCALE_WIDE_BASE : s;
}
//! the quantised scale a consumer reads back, for the writer's own bounds
inline float lodiScaleQuantised( float s )
{
	return lodiScaleValue( lodiScaleWord( s ), lodiScaleIsWide( s ) ? quint16( LODI_INST_SCALE_WIDE ) : quint16( 0 ) );
}

#pragma pack( push, 1 )

//! The instance record, 24 bytes (docs 4.1).
struct LodiInstance
{
	quint16 pos[3];     //!< u16 into the chunk box: X, Y over 16,384 units, Z over zMin..zMin+zExtent
	quint16 rot[3];     //!< 2-bit selector + 3 x 15-bit smallest-three quaternion, LSB-first over the three u16
	quint16 scale;      //!< scale = v / 8192; v10 with flags bit 7: 8 + v / 8192 (lodiScaleValue)
	quint16 baseId;     //!< index into the .lodo base table
	quint8 ao;
	quint8 sky;
	quint8 ground;
	quint8 seed;        //!< treeHash & 0xFF, the generator's position hash (0 for a non-tree)
	quint16 flags;      //!< LodiInstanceFlags; bits 6-15 reserved 0
	/*! v2: the base's (primary mesh, primary material) draw rank. A pure
	 *  function of `baseId`; it is here so the sort law is checkable without
	 *  the `.lodo`. v1 called this word `reserved` and wrote 0. */
	quint16 drawKey;
};

//! Chunk table entry, 32 bytes, dense, north-up row-major; absent = all zero.
struct LodiChunk
{
	quint32 instanceFirst;
	quint32 instanceCount;
	float zMin;
	float zExtent;
	float maxBoundRadius;   //!< max over the chunk of base.boundRadius x scale
	quint32 cellRangeOffset;    //!< index of this chunk's 16 cell ranges in the cell-range blob
	quint32 crc32;          //!< over this chunk's instance records then its cold records
	quint32 reserved;
};

struct LodiCellRange
{
	quint32 instanceFirst;
	quint32 instanceCount;
};

/*! Cold record, 8 bytes, parallel to the instance blob -- the JOIN row. A
 *  consumer that only draws never loads it; a consumer that must line our
 *  instances up with the engine's own objects reads it once at load and builds
 *  its map.
 *
 *  `refFormId` is the PLACED REFR's form ID in the load-order-mapped ID space
 *  (`src/esmdata.cpp:377`, `ref.formID = r->formID`, the same space
 *  `ESMFile::mapFormID` puts every other id in), which is what bungo's ruling
 *  of 2026-09-11 10:3x asks for: "the instance table must be joinable to the
 *  engine's placed REFR (formID in the .lodi record)". It is at the instance's
 *  OWN index, so the join is `cold[i].refFormId` with no search. The 24-byte
 *  hot record was NOT grown to 32 to duplicate it -- see the contract page. */
/*! v3: ONE PRECOMPUTED OCCLUDER, 40 bytes. An oriented box that lies entirely
 *  INSIDE a placed object's own LOD mesh, so a consumer may reject anything
 *  wholly behind it without ever hiding something the object does not cover.
 *
 *  bungo's constraint, and it is why the fit is conservative rather than tight:
 *  a box that sticks out of its object hides things WRONGLY, which is worse
 *  than no occluder at all. The writer therefore fits the box on the interior
 *  voxels of a WATERTIGHT mesh only, shrinks it by one voxel on every side, and
 *  then tests 100 points inside it against the mesh itself before it writes the
 *  row; a mesh that cannot pass that yields NO box and the census counts it.
 *
 *  The rotation is the same smallest-three codec as an instance record, so a
 *  consumer already has the decoder. The box's half extents are along the box's
 *  OWN axes, which are the object's, after the instance's scale. */
struct LodiOccluder
{
	float centre[3];        //!< world units
	float halfExtent[3];    //!< along the box's own axes, after scale
	quint16 rot[3];         //!< the instance's rotation, same codec as LodiInstance::rot
	quint16 flags;          //!< bit0 fitted inside a watertight mesh; bits 1-15 reserved 0
	quint32 instanceIndex;  //!< the .lodi instance this box was fitted inside
	/*! The `.lodo` mesh the box was fitted INSIDE. Stated rather than derived
	 *  from the base's `rep[]`, because a placement draws ONE slot and only
	 *  that slot's geometry is what the box was measured against -- a checker
	 *  that guessed the slot would be testing the wrong mesh. */
	quint16 meshId;
	quint16 reserved;       //!< 0
};

//! v3: one per cell, parallel to the cell-range blob.
struct LodiOccluderRange
{
	quint32 occluderFirst;
	quint32 occluderCount;
};

/*! v4: ONE AGGREGATE IMPOSTOR, i.e. one forested cell's whole tree cluster on a
 *  single card set (bungo 2026-09-11 08:3x). 48 bytes.
 *
 *  There is no `.lodm` path in the row and that is deliberate: the sheets sit
 *  at a path DERIVED from the worldspace editor ID in the header and this row's
 *  own cell, `Data\FO4CSLOD\<EDID>\Aggregate\<cellX>_<cellY>_agg.lodm`,
 *  so the table needs no string blob and a reader cannot be handed a path that
 *  disagrees with the cell (zero-authoring, CONSTITUTION 10).
 *
 *  `half` is ONE pair for every view, exactly as a card's is; where each view's
 *  quad SITS is the `.lodm`'s `frameOffset`, because that is a picture fact and
 *  belongs beside the picture. */
struct LodiAggregate
{
	float centre[3];        //!< world units: the aggregate quad's centre, all views
	float half[2];          //!< half extents along the view's own right and up, world units
	float depthSpan;        //!< the height channel's span: units = (B - 0.5) * depthSpan
	float boundRadius;      //!< the cell's tree-cloud radius, for the screen-size test
	qint16 cellX, cellY;    //!< the cell this aggregate stands for
	quint16 views;          //!< azimuths photographed, shared with the header's word
	quint16 flags;          //!< LodiAggregateFlags; a set reserved bit is a refusal
	/*! The far-shadow identity (bungo 08:4x). ONE identity per aggregate, never
	 *  the dominant tree's: once a cell's trees are one card they are one
	 *  caster, and sharing an identity with that tree's own per-tree instances
	 *  would make the shadow pass exclude the wrong pixels. Always
	 *  `LODI_AGG_IDENTITY_BIT | aggregateIndex`, so the space is disjoint from
	 *  the instance indices by construction and the top bit says which it is.
	 *  The top bit CLEAR is a refusal. */
	quint32 identity;
	quint32 coveredFirst;   //!< into the covered-instance blob
	quint32 coveredCount;   //!< how many instances this aggregate stands for
};

struct LodiCold
{
	quint32 refFormId;
	qint16 scolPart;    //!< -1 when not a SCOL part
	/*! v2: the STOCK bake's identity index for this placement (R + G*256 of
	 *  the `.bto` vertex colour, the manifest's `index` column), unique inside
	 *  the stock chunk it was first drawn in. bungo 2026-09-11 08:4x: the far
	 *  shadow pass keys on the colour id, so it must survive into the native
	 *  output. v1 called this word `flags` and wrote 0. */
	quint16 identity;
};

#pragma pack( pop )

static_assert( sizeof( LodiInstance ) == 24, "LodiInstance is 24 bytes" );
static_assert( sizeof( LodiChunk ) == 32, "LodiChunk is 32 bytes" );
static_assert( sizeof( LodiCellRange ) == 8, "LodiCellRange is 8 bytes" );
static_assert( sizeof( LodiCold ) == 8, "LodiCold is 8 bytes" );
static_assert( sizeof( LodiOccluder ) == LODI_OCCLUDER_STRIDE, "LodiOccluder is 40 bytes" );
static_assert( sizeof( LodiOccluderRange ) == 8, "LodiOccluderRange is 8 bytes" );
static_assert( sizeof( LodiAggregate ) == LODI_AGGREGATE_STRIDE, "LodiAggregate is 48 bytes" );

struct LodiHeader
{
	quint32 version = LODI_VERSION;
	quint32 flags = LODI_FLAG_ROW_ORDER_NORTH_UP;
	quint32 headerCrc32 = 0;
	quint64 pluginCorpusHash = 0;
	quint64 objectCorpusHash = 0;
	quint64 lodoIdentity = 0;
	QString worldspaceEdid;
	qint16 chunkWest = 0, chunkSouth = 0, chunkEast = 0, chunkNorth = 0;   //!< inclusive, chunk units
	quint16 chunkCells = LODI_CHUNK_CELLS;
	quint16 instanceStride = LODI_INSTANCE_STRIDE;
	quint32 chunkCount = 0;
	quint32 instanceCount = 0;
	quint32 presentChunks = 0;
	quint32 maxInstancesPerChunk = 0;
	quint32 indexCrc32 = 0;         //!< over the chunk table and the cell-range blob
	quint64 offChunks = 0, offCellRanges = 0, offInstances = 0, offCold = 0;
	quint64 fileBytes = 0;
	//! v2, file offset 0x90; must equal the `.lodo`'s (lodofile.h).
	quint64 loadOrderHash = 0;
	//! v3, file offsets 0x98 and 0xA0: the occluder boxes and their per-cell ranges.
	quint64 offOccluders = 0, offOccluderRanges = 0;
	quint32 occluderCount = 0;              //!< v3, 0xA8
	quint16 occluderStride = LODI_OCCLUDER_STRIDE;      //!< v3, 0xAC
	quint16 maxOccludersPerCell = LODI_OCCLUDERS_PER_CELL;  //!< v3, 0xAE
	//! v4, file offsets 0xB0 and 0xB8: the aggregate rows and the covered blob.
	quint64 offAggregates = 0, offCovered = 0;
	quint32 aggregateCount = 0;             //!< v4, 0xC0
	quint32 coveredCount = 0;               //!< v4, 0xC4 -- the blob's length in u32s
	quint16 aggregateStride = LODI_AGGREGATE_STRIDE;    //!< v4, 0xC8
	quint16 aggregateViews = 0;             //!< v4, 0xCA -- azimuths a sheet, 0 with no aggregate
	float aggSwitchPx = 0.0f;               //!< v4, 0xCC
	float aggBandRatio = 0.0f;              //!< v4, 0xD0
	/*! v5, file offset 0xD4: how many instances each MNAM slot drew. The index
	 *  is the slot the STOCK chunk builder chose for the placement, 0..3, and
	 *  the four sum to `instanceCount` exactly -- which is the check that keeps
	 *  them honest. A consumer budgets its per-slot draw from the header alone.
	 *  (plan 5 row 1, census 6.3 item 1.) */
	quint32 slotInstances[4] = { 0, 0, 0, 0 };
	//! v5, file offsets 0xE4/0xEC/0xF0: the placement-AO blob, one u8 an instance.
	quint64 offPlacementAo = 0;
	quint32 placementAoCount = 0;           //!< v5, 0xEC; equals instanceCount when present
	quint8 placementAoStride = 0;           //!< v5, 0xF0; 1 when present, 0 when absent
	quint64 offVertexAo = 0;                //!< v6, 0xF4: the vertex-AO blob (u32 first[n+1] then the bytes)
	quint32 vertexAoBytes = 0;              //!< v6, 0xFC: the whole blob's size, offsets included
	quint64 offGroup = 0;                   //!< v7, 0x100: the group table, one u16 an instance
	quint32 groupCount = 0;                 //!< v7, 0x108: distinct groups over the FILE (the chunks' counts summed)
	quint16 groupStride = 0;                //!< v7, 0x10C: 2 when present, 0 when absent
	quint64 offVertexSky = 0;               //!< v7, 0x110: the vertex-sky stream, s4.8's layout exactly
	quint32 vertexSkyBytes = 0;             //!< v7, 0x118: the whole stream's size, offsets included
	quint64 offVertexHorizon = 0;           //!< v8, 0x11C: the per-vertex horizon stream, s4.11
	quint32 vertexHorizonBytes = 0;         //!< v8, 0x124: the whole stream's size, offsets included
	quint16 horizonAzimuths = 0;            //!< v8, 0x128: bytes a vertex; 0 when the stream is absent
	quint16 horizonSteps = 0;               //!< v8, 0x12A: far-march steps a azimuth, as cast
	float horizonReach = 0.0f;              //!< v8, 0x12C: the march reach in WORLD units
};

//! What the writer takes: one unquantised placement.
struct LodiSrcInstance
{
	float pos[3] = { 0.0f, 0.0f, 0.0f };    //!< world units
	float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   //!< row-major 3x3, world = R * local (the DRAWN rotation)
	float scale = 1.0f;
	quint32 baseId = 0;
	quint32 refFormId = 0;
	qint16 scolPart = -1;
	quint8 ao = 255, sky = 255, ground = 0, seed = 0;
	/*! v5: this PLACEMENT's own occlusion, ray-cast at bake against the chunk
	 *  and the heightfield -- not the model's `ao`, which is the mean over the
	 *  model's own lit vertices and is the same for a tree in a field and a
	 *  tree under a bridge when neither is drawn as a mesh at all.
	 *  LODI_PLACEMENT_AO_UNMEASURED (0xFF) means the bake never cast a ray for
	 *  this placement; a measured value is clamped to 0..254. */
	quint8 placementAo = LODI_PLACEMENT_AO_UNMEASURED;
	//! v5: the MNAM slot the stock chunk builder drew this placement from, 0..3.
	quint8 mnamSlot = 0;
	/*! v6: one scene-AO byte per vertex of the mesh this instance draws, in
	 *  that mesh's vertex order; EMPTY when the bake had none for it. */
	std::vector<quint8> vertexAo;
	/*! v7: one sky-visibility byte per vertex of the mesh this instance draws,
	 *  the same population and order as `vertexAo`; EMPTY when the bake had
	 *  none. 255 = the whole upper hemisphere open. */
	std::vector<quint8> vertexSky;
	/*! v7: which GROUP this placement belongs to, as the EMITTER sees it -- a
	 *  global key, any u32, equal for two placements of one object.
	 *  LODI_GROUP_ALONE means "its own group". The writer turns these into ids
	 *  dense per chunk, because only the writer knows the sort and the chunk
	 *  partition (the same division of labour `LodiSrcAggregate::covered` uses). */
	quint32 groupKey = LODI_GROUP_ALONE;
	quint16 flags = 0;
	quint16 drawKey = 0;        //!< v2: the base's (mesh, material) draw rank; the sort's third key
	quint16 identity = 0;       //!< v2: the stock bake's identity index for this placement
	/*! The base's bound radius AT SCALE 1. The writer multiplies it by the
	 *  QUANTISED scale -- exactly the arithmetic the consumer does -- for the
	 *  chunk's `maxBoundRadius`, so that value is a true upper bound. Handing
	 *  in the product instead let four Sanctuary instances sit up to 0.055 u
	 *  outside their own chunk's expanded box (lane NATIVE1a). */
	float boundRadius = 0.0f;
	/*! v3: the occluder box the emitter fitted inside this placement's OWN mesh,
	 *  in the MESH's local frame at scale 1. The writer applies the quantised
	 *  scale and the rotation, bins it by cell and keeps the largest
	 *  `LODI_OCCLUDERS_PER_CELL` by world volume -- the selection is the
	 *  writer's because only the writer knows the cell partition. */
	bool hasOccluder = false;
	float occCentre[3] = { 0.0f, 0.0f, 0.0f };
	float occHalf[3] = { 0.0f, 0.0f, 0.0f };
	quint16 occMeshId = 0;      //!< the `.lodo` mesh the box was fitted inside
	QString baseName;           //!< what a refusal quotes for this record's base
};

/*! v4: one aggregate as the EMITTER hands it in. `covered` names instances by
 *  their index in `LodiSrcSet::instances`, i.e. BEFORE the writer's sort; the
 *  writer remaps them to written indices and sorts them ascending, because only
 *  the writer knows the sort. Handing in written indices would make the emitter
 *  depend on the sort law, which is the one thing 2.1 exists to keep in one
 *  place. */
struct LodiSrcAggregate
{
	float centre[3] = { 0.0f, 0.0f, 0.0f };
	float half[2] = { 0.0f, 0.0f };
	float depthSpan = 0.0f;
	float boundRadius = 0.0f;
	int cellX = 0, cellY = 0;
	quint16 views = 0;
	quint16 flags = LODI_AGG_HEIGHT;
	std::vector<quint32> covered;   //!< SOURCE instance indices
};

struct LodiSrcSet
{
	QString worldspaceEdid;
	quint32 flags = LODI_FLAG_ROW_ORDER_NORTH_UP;
	quint64 pluginCorpusHash = 0;
	quint64 objectCorpusHash = 0;
	quint64 lodoIdentity = 0;
	quint64 loadOrderHash = 0;      //!< v2
	std::vector<LodiSrcInstance> instances;
	/*! v4. EMPTY is the module's off value and it is exact: the file is written
	 *  at version 3 and is byte-identical to what the same set wrote before this
	 *  lane existed. */
	std::vector<LodiSrcAggregate> aggregates;
	float aggSwitchPx = LODI_AGG_SWITCH_PX;
	float aggBandRatio = LODI_AGG_BAND_RATIO;
	/*! v5. FALSE is the module's off value and it is exact: no AO blob, no slot
	 *  totals, the version word left to the aggregate rule, and the file
	 *  byte-identical to what the same set wrote before this lane existed
	 *  (`--native-no-placement-ao`). It is a flag and not "did any instance get
	 *  a value", because a region where every cast refused must still write the
	 *  blob -- of 0xFF bytes -- and say so, rather than silently vanishing. */
	bool placementAo = false;
	/*! v6. FALSE is the module's off value (`--native-no-vertex-ao`): no blob,
	 *  no 0xF4 words, the version word left to the v5 rule. Needs `placementAo`. */
	bool vertexAo = false;
	/*! v7. FALSE is the module's off value (`--lodi-v6`): no group table, no
	 *  0x100 words, a 256-byte header block, the version word left to the v6
	 *  rule, and the file byte-identical to what the same set wrote before this
	 *  lane existed. A flag and not "did any instance get a group", because a
	 *  region where every placement stands alone must still write the table --
	 *  of 2,446 distinct ids -- and say so. */
	bool group = false;
	//! v7. FALSE is the module's off value (`--lodi-v6`). Needs `vertexAo`.
	bool vertexSky = false;
};

//! The whole table in memory, as the reader gives it.
struct LodiTable
{
	std::vector<LodiChunk> chunks;
	std::vector<LodiCellRange> cellRanges;
	std::vector<LodiInstance> instances;
	std::vector<LodiCold> cold;
	std::vector<LodiOccluder> occluders;            //!< v3
	std::vector<LodiOccluderRange> occluderRanges;  //!< v3, parallel to cellRanges
	std::vector<LodiAggregate> aggregates;          //!< v4
	std::vector<quint32> covered;                   //!< v4, the aggregates' instance indices
	std::vector<quint8> placementAo;                //!< v5, parallel to `instances`; 0xFF = not measured
	std::vector<quint32> vertexAoFirst;             //!< v6, instanceCount + 1 entries into `vertexAo`
	std::vector<quint8> vertexAo;                   //!< v6, one byte a library vertex an instance
	std::vector<quint16> group;                     //!< v7, parallel to `instances`; dense per chunk from 0
	std::vector<quint32> vertexSkyFirst;            //!< v7, instanceCount + 1 entries into `vertexSky`
	std::vector<quint8> vertexSky;                  //!< v7, one byte a library vertex an instance
	/* v8's per-vertex horizon stream is NOT read into this table (lane
	 * HORIZONOUT, 2026-09-19). A v8 file still opens and its stream is still
	 * bounds-checked, but the payload is skipped by its length: nothing in this
	 * tree consumes it, and a vector nobody reads is a vector that goes stale. */
};

/* ---- the packing contract, shared by the writer, the reader and the tests ---- */

//! Rotation matrix (row-major, world = R * local) -> three u16 (2 + 3 x 15 bits, LSB-first).
void lodiPackRotation( const float m[9], quint16 out[3] );
//! The three u16 -> unit quaternion (w, x, y, z) and the matrix.
void lodiUnpackRotation( const quint16 in[3], float quat[4], float m[9] );
//! Which chunk (in chunk units) a world X or Y falls in: floor(v / 16384).
int lodiChunkOf( float v );
//! The cell index inside a chunk, north-up row-major: (3 - ly) * 4 + lx.
int lodiCellOf( float x, float y, int chunkX, int chunkY );

/*! One step of the instance position quantiser in X/Y: 3 x u16 over the
 *  16,384-unit chunk box (NATIVE 4.1), so 0.2500038 units. */
constexpr float LODI_POS_QUANT_STEP = LODI_CHUNK_UNITS / 65535.0f;
/*! THE CELL AMBIGUITY BAND. `lodoQuantU16` rounds to nearest, so a stored
 *  position is at most half a step from the float the WRITER sorted its cell
 *  on (`lodifile.cpp`, the sort law); the two ulp of the chunk box cover the
 *  float32 round trip on the way back out. A position this close to a cell
 *  line could have been on either side of it before quantisation. */
constexpr float LODI_CELL_QUANT_TOL = LODI_POS_QUANT_STEP / 2.0f + LODI_CHUNK_UNITS * 1.1920929e-7f * 2.0f;

/*! Does the cell the FILE stores an instance in agree with the one its stored
 *  position derives?
 *
 *  The writer sorts on the cell of the FLOAT position; a reader only ever sees
 *  the quantised one, which can have crossed a cell line by up to half a step.
 *  So the cell a reader USES is the one the cell-range table states, and this
 *  is the check on it: the two may differ by one on ONE axis, and only inside
 *  LODI_CELL_QUANT_TOL of that cell line. Anything else is a real disagreement
 *  and `why` says which of the two it is, with the numbers. */
bool lodiCellAgrees( int storedCell, float x, float y, int chunkX, int chunkY, QString * why = nullptr );
//! Dense table index of a chunk, north-up row-major over the header's extent.
quint32 lodiChunkIndex( const LodiHeader & h, int chunkX, int chunkY );
//! Chunk units of a table index.
void lodiChunkAt( const LodiHeader & h, quint32 index, int * chunkX, int * chunkY );
//! Decode an instance's world position from its chunk.
void lodiDecodePosition( const LodiHeader & h, quint32 chunkIndex, const LodiChunk & c,
	const LodiInstance & r, float xyz[3] );

/* ---- file I/O ---- */

struct LodiWriteStats
{
	quint32 instances = 0;
	quint32 chunks = 0;         //!< dense table length
	quint32 presentChunks = 0;
	quint32 maxInstancesPerChunk = 0;
	quint64 fileBytes = 0;
	float maxScale = 0.0f;
	quint32 maxBaseId = 0;
	/* v3, the occluders. Every one of these is WRITTEN and MOVES, and the
	 * fraction of cells with no box at all is the honest half of the story. */
	quint32 occluders = 0;              //!< boxes written
	quint32 occluderCandidates = 0;     //!< instances that offered one
	quint32 cellsWithOccluder = 0;      //!< cells holding at least one
	quint32 cellsPopulated = 0;         //!< cells holding at least one instance
	quint32 occludersDropped = 0;       //!< offered but beaten by bigger boxes in the same cell
	/* v4, the aggregates. Every one is WRITTEN and MOVES; `coveredInstances` is
	 * the count identity gate's own number and must equal what the bake
	 * photographed, aggregate by aggregate. */
	quint32 aggregates = 0;             //!< aggregate rows written
	quint32 coveredInstances = 0;       //!< instances the aggregates stand for, in total
	quint32 aggregateViews = 0;         //!< azimuths a sheet
	/* v7, the census words the brief names. Every one is WRITTEN and MOVES. */
	quint32 groups = 0;                 //!< distinct groups over the file (the chunks' counts summed)
	quint32 groupedPlacements = 0;      //!< placements in a group of two or more
	quint32 largestGroup = 0;           //!< members of the biggest group in the file
	quint32 singletonGroups = 0;        //!< groups of exactly one placement
	quint32 vertexSkyBytes = 0;         //!< the whole sky stream, offsets included
	quint32 vertexSkyPlacements = 0;    //!< placements with a non-empty sky slice
	quint32 version = LODI_VERSION;     //!< the version word actually written
	quint32 wideScaleInstances = 0;     //!< v10: instances written with bit 7 (scale above 7.99988)
};

/*! Write the table. Refuses BEFORE writing a byte on: a scale above
 *  LODI_SCALE_MAX (names the ref), a baseId above 65,535 (names the base), a
 *  dense chunk table past 65,536 (names the extreme chunk), a set reserved
 *  flag bit, a zero identity without NOLIB (or NOLIB with a non-zero one), an
 *  editor ID of 32 bytes or more. Two writes of one set are byte-identical. */
bool lodiWrite( const QString & path, const LodiSrcSet & set, LodiHeader * headerOut,
	LodiWriteStats * stats, QString * error );

/*! Read and validate; `error` NAMES the field. `payloadCheck` recomputes
 *  indexCrc32 and every present chunk's crc32 and walks the ranges (every
 *  chunk's cells partition it, every chunk's instances partition the blob,
 *  the sort order holds, reserved words are zero). */
bool lodiRead( const QString & path, LodiHeader * header, LodiTable * table,
	bool payloadCheck, QString * error );

QStringList lodiDescribe( const LodiHeader & h, const LodiTable * table );

/*! The KNOWN-ANSWER control: a synthetic three-instance worldspace whose
 *  records are written by hand here -- one tree (mirrored, rotated about all
 *  three axes, scale 1.5), one plain static, one SCOL part -- over two
 *  cluster-sized meshes, into `<dir>/Synthetic.lodo` + `Synthetic.lodi`. The
 *  expected answers are printed as `expect key value` lines so the Python
 *  decoder (tests/spells/lodgen_native_decode.py) checks the files against
 *  numbers written down BEFORE the run, never against the writer. Also
 *  writes `Synthetic.expect.txt` beside them. */
bool lodNativeFixtureWrite( const QString & dir, QStringList * report, QString * error );

#endif // LODIFILE_H

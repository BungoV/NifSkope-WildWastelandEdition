# F4FX provenance block — byte-exact specification

Source of truth. `FarFieldHeightmapFormat.h`, `FarFieldHeightmapBake.h` and
`FarFieldBakeOracle.h` are byte-identical (md5) across every FO4CS worktree except
the stale `fallout4-community-shaders/` clone and `wt-batch1`; `FarFieldHeightmapLoader.h`
has three variants but the provenance logic quoted below is identical in all of them.
Citations use **`E:\Projects\Fo4CommunityShaders\wt-solar\src\FarField\`**.

Real file checked against: `E:\Projects\Fallout 4 Mods\mods\FO4CS\Textures\Terrain\Commonwealth\Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds`
(75,497,620 bytes = 148 header + 6144*6144*2 payload).

---

## 1. Field table

The block lives in DDS `dwReserved1[11]`: **file offset 32, 44 bytes**
(`kDdsReserved1FileOffset = 32`, `kDdsReserved1Size = 44` —
FarFieldHeightmapFormat.h:535-536). The provenance struct occupies the first
**40** bytes (`kBakeProvenanceBytes = 40`, :627); the whole 44 are zeroed first
(`std::memset(reserved44, 0, kDdsReserved1Size)`, :634), so reserved word 10 is
always 0. Everything is little-endian (plain `memcpy` of native x86-64 values).

| file off | resv idx | type | name | value in the real file | source |
|---|---|---|---|---|---|
| 32 | 0 | u32 | `magic` | `0x58463446` = 'F4FX' LE | :572, :648 |
| 36 | 1 | u32 | `version` | `0x00000001` | :590, :649 |
| 40 | 2 (lo), 3 (hi) | u64 | `corpusHash` | `0x2f637f22`,`0xd8337d02` -> `0xD8337D022F637F22` | :611, :650 |
| 48 | 4 (lo), 5 (hi) | u64 | `pixelHash` | `0xeec51d7e`,`0x0b9232ea` -> `0x0B9232EAEEC51D7E` | :612, :651 |
| 56 | 6 lo16 | i16 | `south` | `0xffa0` = -96 | :652 |
| 58 | 6 hi16 | i16 | `west` | `0xffa0` = -96 | :653 |
| 60 | 7 lo16 | i16 | `north` | `0x005f` = 95 | :654 |
| 62 | 7 hi16 | i16 | `east` | `0x005f` = 95 | :655 |
| 64 | 8 lo16 | u16 | `encoding` | `0x0001` = `kBakeEncodingXLodGenFixed` | :599, :656 |
| 66 | 8 hi16 | u16 | `flags` | `0x0000` | :657 |
| 68 | 9 | u32 | `coveredTexels` | `0x02400000` = 37,748,736 = 6144^2 | :658 |
| 72 | 10 | u32 | (padding) | `0x00000000` | :634 (memset) |

Notes on the observed values:

- `south`/`west`/`north`/`east` are **cell indices**, inclusive, taken from
  `HeightmapPlacement::extent` (FarFieldHeightmapBake.h:655-658). -96..95 in both
  axes = 192x192 = 36,864 Commonwealth cells.
- `coveredTexels` = `BakeEncodeStats::texelsCovered`
  (FarFieldHeightmapBake.h:661), i.e. the number of texels backed by a real VHGT
  sample. 6144^2 means every texel of the 6144x6144 grid was covered, which the
  bake header comment predicts for the Commonwealth ("For Commonwealth the count
  is 0 [uncovered] — every one of the 36 864 cells carries a VHGT",
  FarFieldHeightmapBake.h:259-261). The loader **never reads this field**; it is
  census only.
- `flags` is written as literal 0 by both provenance builders
  (FarFieldHeightmapBake.h:660, :639) and is **never read** by the loader.
- `version` is the heightmap artifact version; it stands at 1 and the source
  explicitly explains why it must not be bumped (FarFieldHeightmapFormat.h:573-590).


---

## 2. Corpus hash — exact definition

`corpusHash` is **FNV-1a 64 over the raw VHGT subrecord payload bytes of every
LAND record in the target worldspace, in file order, of ONE plugin file.**

Hash primitive (FarFieldHeightmapFormat.h:63-85), verbatim:

```cpp
inline constexpr std::uint64_t kFnv1a64Offset = 0xCBF29CE484222325ull;
inline constexpr std::uint64_t kFnv1a64Prime = 0x100000001B3ull;

[[nodiscard]] inline std::uint64_t Fnv1a64Update(
    std::uint64_t hash, const void* data, std::size_t size) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnv1a64Prime;
    }
    return hash;
}
```

The accumulation (FarFieldHeightmapBake.h:717-742), verbatim:

```cpp
std::uint64_t corpusHash = kFnv1a64Offset;
...
if (!plugin.ForEachLand(
        target->formId,
        [&](int cellX, int cellY, const std::uint8_t* data, std::size_t size) {
            if (size != kVhgtSubrecordSize) {
                ++wrongSize;
            }
            corpusHash = Fnv1a64Update(corpusHash, data, size);
            ...
```

The hash is updated **before** any size or decode check, so a wrong-sized or
undecodable VHGT still contributes its bytes.

The shipped constant (FarFieldHeightmapFormat.h:87-91), verbatim:

```cpp
// MEASURED IN F0 (RE note 10.1): FNV-1a 64 over every Commonwealth VHGT
// payload of the retail Fallout4.esm, in file order. This is the bake's
// INPUT fingerprint; the bake writes it into its own header and the loader
// compares what it finds against it.
inline constexpr std::uint64_t kCommonwealthVhgtCorpusHash = 0xD8337D022F637F22ull;
```

### The exact traversal (FarFieldPluginReader.h)

1. `PluginFile::Open(options.pluginPath)` — **one file** (`HeightmapBakeOptions::pluginPath`,
   FarFieldHeightmapBake.h:568). No load order, no BSA, no data-folder state.
2. `CollectWorldspaces` walks top level and picks the WRLD whose `EDID` equals
   `options.worldspaceEditorId` (default `"Commonwealth"`,
   FarFieldHeightmapBake.h:569 / FarFieldHeightmapFormat.h:92). Its FormID is the
   traversal key (FarFieldHeightmapBake.h:688-699).
3. `ForEachLandAndCell(worldFormId, ...)` (FarFieldPluginReader.h:375-400): finds
   that WRLD record at top level, requires the immediately-following block to be
   a `GRUP` of `groupType == 1` (world children), then recurses
   `WalkGroup(next + 24, next + childHeader.dataSize)`.
4. `WalkGroup` (FarFieldPluginReader.h:523-638) recurses into every nested GRUP in
   file order. On a `CELL` it clears `haveCellCoords_`, decompresses the record if
   `flags & 0x00040000`, and takes `XCLC`'s first two int32 as (cellX, cellY). On a
   `LAND` it skips the record entirely if the last CELL had no XCLC
   (`landWithoutCellCoords`), otherwise decompresses and walks subrecords, calling
   the visitor with the **first `VHGT` only** and then stopping that record's
   subrecord walk (`return false`, :629).
5. Subrecord walk `ForEachSubrecord` (:254-288) honours the `XXXX` big-size
   override; the bytes handed to the visitor are the subrecord's payload, without
   its 6-byte header.
6. Decompression: `ReadRecordPayload` (:429-465) reads `dataSize` bytes, and when
   the compressed flag is set treats the first 4 bytes as the declared inflated
   size and zlib-inflates the rest; a size mismatch discards the record.

### Reproduced independently — MATCH

Reimplemented in Python against
`X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm` (330,776,576
bytes; script kept at `<scratchpad>/corpus.py`). Result:

```
WRLD Commonwealth formid=0000003C childGRUP=GRUP type=1 size=218483605
cells 36865, cellsWithCoords 36865, land 36864, landVhgt 36864,
landWithoutCellCoords 0, VHGT wrongSize 0
total vhgt bytes 40402944   (36864 x 1096)
corpusHash = 0xD8337D022F637F22   MATCH
```

### Can NifSkope compute it from the ESM alone?

**Yes.** The inputs are: one plugin file's bytes, the WRLD EditorID, the group
tree, zlib inflate, and the VHGT subrecord payloads. Nothing depends on the
runtime load order, the game's Data folder, BSAs, INI state, or any FO4CS build
artifact. What NifSkope must match exactly:

- **Only the named worldspace's world-children GRUP** (type 1) is walked; LAND
  records elsewhere in the file never contribute.
- **File order**, not sorted-by-coordinate order, and **no de-duplication** of
  repeated (x, y) cells.
- **First VHGT per LAND record only.**
- **Raw payload bytes** (post-inflate, post-XXXX), 1096 bytes each for retail
  FO4 — but hash whatever size is present, do not normalise.
- A LAND whose owning CELL carried no XCLC is **skipped entirely**, contributing
  nothing.

Caveat, not a blocker: for any worldspace other than `Commonwealth` the loader
never sets `haveCorpusHash` (FarFieldHeightmapRuntime.cpp:941-946), so the value
written is unchecked. For `Commonwealth` it must be exactly
`0xD8337D022F637F22`; if the user's `Fallout4.esm` differs from retail (it does
not for any shipped patch level that produced this constant), a hash computed
from their own ESM would be rejected. Writing the literal constant is therefore
the safer choice for Commonwealth; computing it is the correct choice for any
other worldspace.

---

## 3. Pixel hash — exact definition, VERIFIED

`pixelHash` = `Fnv1a64` over the **R16 payload exactly as written to the file**,
i.e. `width * height * sizeof(uint16_t)` bytes starting at the DDS data offset,
little-endian, row-major, row 0 = north.

Bake side (FarFieldHeightmapBake.h:309):

```cpp
stats.pixelHash = Fnv1a64(pixels.data(), pixels.size() * sizeof(std::uint16_t));
```

Loader side (FarFieldHeightmapLoader.h:218-225):

```cpp
if (expectation.verifyPixelHash) {
    evaluation.observedPixelHash =
        Fnv1a64(bytes + evaluation.payloadOffset, evaluation.payloadBytes);
    evaluation.pixelHashVerified = true;
    if (evaluation.observedPixelHash != evaluation.provenance.pixelHash) {
        evaluation.reason = HeightmapRejectReason::kPixelHashMismatch;
        return evaluation;
    }
}
```

with `payloadOffset = dds.dataOffset` (148 for a DX10 header, 128 for legacy) and
`payloadBytes = dds.dataBytesRequired = width * height * 2`
(FarFieldHeightmapLoader.h:189-190, FarFieldHeightmapFormat.h:756-758). Note it
hashes **exactly** `width*height*2` bytes — trailing bytes past that (mips, junk)
are excluded, so extra tail data does not change the hash, but the file must be at
least that long or it is rejected as `kTruncatedPayload` first.

**VERIFIED against the real file.** FNV-1a 64 over bytes [148, 148+75497472) of
`Commonwealth_fine.HeightMap.-96.-96.95.95.-8320.44872.dds`:

```
hashed 75497472  fnv1a64 = 0x0B9232EAEEC51D7E
header pixelHash        = 0x0B9232EAEEC51D7E   MATCH
```

(script `<scratchpad>/fnvpy.py`.)

---

## 4. What the loader does on each mismatch

`EvaluateHeightmapFile` (FarFieldHeightmapLoader.h:141-232) is a pure
accept/reject; there is **no warn-only path**. Every reject leaves the far-field
terrain half OFF with a named reason (`HeightmapLoadState::kRejected`,
FarFieldHeightmapRuntime.cpp:951-956). The function body is identical in all four
variants of the loader header found in the tree.

Gates in order (first failure wins, the function returns immediately):

| # | check | source | reject reason |
|---|---|---|---|
| 1 | filename parses | Loader.h:148-152 | `kNameUnparsable` |
| 2 | `fields[0]` equals the resolved worldspace EditorID, case-insensitive (skipped when the file came from `sHeightmapOverride`) | Loader.h:153-157, Runtime.cpp:936-940 | `kWorldspaceMismatch` |
| 3 | `'DDS '`, `dwSize == 124`, `ddspf.dwSize == 32` | Format.h:708-735 | `kNotDds` |
| 4 | R16_UNORM: DX10 `dxgiFormat == 56`, **or** legacy `DDPF_LUMINANCE` + 16 bits + red mask `0xFFFF` | Format.h:738-755 | `kUnsupportedFormat` |
| 5 | width, height nonzero | Loader.h:172-175 | `kDimensionsZero` |
| 6 | width, height <= 8192 (`kHeightmapMaxDimension`) | Loader.h:103, :176-180 | `kDimensionsTooLarge` |
| 7 | file has at least `dataOffset + width*height*2` bytes | Loader.h:181-184 | `kTruncatedPayload` |
| 8 | **provenance magic** — if `!= 0x58463446` the block is treated as ABSENT and gates 9-13 are skipped entirely; the file is **accepted unverified** | Format.h:695, Loader.h:192-193, tests:1683-1697 | (none) |
| 9 | `version == 1` | Loader.h:194-197 | `kProvenanceVersion` |
| 10 | `encoding == 1` | Loader.h:198-201 | `kProvenanceEncoding` |
| 11 | `Extent() == name.extent` (all four i16 equal the four filename cell fields) | Loader.h:202-208 | `kProvenanceExtent` |
| 12 | `corpusHash == expectation.corpusHash`, **only when the resolved worldspace EditorID is exactly `"Commonwealth"`** | Loader.h:209-217, Runtime.cpp:941-946 | `kCorpusHashMismatch` |
| 13 | `pixelHash` equals FNV-1a 64 of the payload — `verifyPixelHash` defaults true and nothing in the tree ever sets it false | Loader.h:119, :218-226 | `kPixelHashMismatch` |

Fields **never read** by the loader: `flags`, `coveredTexels`, reserved word 10.
They are census/telemetry only.

### Accepted `encoding` values

Exactly one, for a heightmap: **`1` = `kBakeEncodingXLodGenFixed`**
(FarFieldHeightmapFormat.h:599; the check is `!= kBakeEncodingXLodGenFixed`,
Loader.h:198). Value `2` = `kBakeEncodingWaterHeightF32` (:604) belongs to the
water sidecar and is a named refusal if it appears in a heightmap, by design
("A DISTINCT id and not a second meaning for 1", :601-603). No other value is
accepted; the tests poke `0x7FFF` and expect `kProvenanceEncoding`
(tests/far_field_shadows_tests.cpp:1785-1790).

Encoding 1 means: `pixel = lround(clamp(units / 8 + 32767, 0, 65535))` and
`units = (pixel - 32767) * 8` (FarFieldBakeOracle.h:530-545;
`kXLodGenHeightQuantum = 8.0f` at :53, zero-height pixel 32767).

### Status of the currently-deployed maps (measured just now)

| file | reserved[0..10] |
|---|---|
| `Commonwealth.HeightMap.-96.-96.95.95.-8320.44872.dds` (Sep 5 02:02, the NifSkope bake) | all zero — **no F4FX block**, loads as *unverified* |
| `Commonwealth_fine.HeightMap...dds` (Aug 19, FO4CS's own bake) | full F4FX block, verified above |
| `Commonwealth.WaterMap.-96.-96.95.95.dds` | F4FX, version 1, same corpusHash `0xD8337D022F637F22`, `encoding 2`, `coveredTexels 0x9000 = 36864` |
| `NukaWorld.HeightMap...dds` | all zero |

So today the NifSkope-baked Commonwealth map is accepted but unchecked; adding
the block arms gates 9-13.

Note in passing (not part of the block): the file named
`Commonwealth_fine.HeightMap...` parses its worldspace as `"Commonwealth_fine"`,
which fails gate 2 on the scanned path — it can only be loaded through
`sHeightmapOverride` (FarFieldShadowsSettings.h:562, Runtime.cpp:913-919). Gate 12
still applies to it, because `haveCorpusHash` keys off the *resolved worldspace*,
not the filename.

---

## 5. Minimal C++ for `lodgenWriteR16Dds`

Drop-in for `E:\Projects\NifskopeWildWastelandEdition\src\lodgen.cpp`, matching
the existing `put`/`hdr` style at lodgen.cpp:2779-2831. `hdr` is already
zero-filled, so reserved word 10 and the 4 unused tail bytes stay zero — which is
exactly what `WriteBakeProvenance`'s leading `memset` produces (Format.h:634).

```cpp
// FNV-1a 64, byte for byte the same walk as
// f4fx::farfield::Fnv1a64Update (FarFieldHeightmapFormat.h:68-85).
static const quint64 F4FX_FNV_OFFSET = Q_UINT64_C( 0xCBF29CE484222325 );
static const quint64 F4FX_FNV_PRIME  = Q_UINT64_C( 0x100000001B3 );

static quint64 f4fxFnv1a64Update( quint64 h, const void * data, size_t size )
{
    const quint8 * p = static_cast<const quint8 *>( data );
    for ( size_t i = 0; i < size; i++ ) {
        h ^= p[i];
        h *= F4FX_FNV_PRIME;
    }
    return h;
}

static bool lodgenWriteR16Dds( const QString & path, int w, int h,
    const std::vector<quint16> & texels,
    // Provenance inputs. havePro = false writes the plain xLODGen-shaped map
    // with zero reserved words, which the loader still accepts - unverified.
    bool havePro, quint64 corpusHash,
    int cellSouth, int cellWest, int cellNorth, int cellEast,
    quint32 coveredTexels )
{
    if ( int( texels.size() ) != w * h )
        return false;

    /* The pixel hash is over the payload EXACTLY AS WRITTEN: little-endian
     * uint16, the same row-major order the writer below emits, w*h*2 bytes and
     * nothing else. On a little-endian host that is the texel vector's own
     * bytes, but hash the emitted byte pairs rather than the vector so the two
     * can never drift apart. */
    quint64 pixelHash = F4FX_FNV_OFFSET;
    if ( havePro ) {
        for ( size_t i = 0; i < texels.size(); i++ ) {
            const quint16 v = texels[i];
            const quint8 le[2] = { quint8( v & 0xFF ), quint8( ( v >> 8 ) & 0xFF ) };
            pixelHash = f4fxFnv1a64Update( pixelHash, le, 2 );
        }
    }

    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly ) )
        return false;
    QByteArray hdr( 4 + 124 + 20, '\0' );
    auto put = [&hdr]( int off, quint32 v ) {
        hdr[off]     = char( v & 0xFF );
        hdr[off + 1] = char( ( v >> 8 ) & 0xFF );
        hdr[off + 2] = char( ( v >> 16 ) & 0xFF );
        hdr[off + 3] = char( ( v >> 24 ) & 0xFF );
    };
    put( 0, 0x20534444 );
    put( 4 + 0, 124 );
    put( 4 + 4, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 );
    put( 4 + 8, quint32( h ) );
    put( 4 + 12, quint32( w ) );
    put( 4 + 16, quint32( w * 2 ) );
    put( 4 + 24, 1 );

    /* ---- the F4FX provenance block, DDS dwReserved1[11] ----
     * File offset 32, 44 bytes, 40 of them used, every value little-endian.
     * Mirrors f4fx::farfield::WriteBakeProvenance field for field
     * (FarFieldHeightmapFormat.h:631-659). */
    if ( havePro ) {
        const quint16 s16 = quint16( qint16( cellSouth ) );
        const quint16 w16 = quint16( qint16( cellWest ) );
        const quint16 n16 = quint16( qint16( cellNorth ) );
        const quint16 e16 = quint16( qint16( cellEast ) );
        put( 32,      0x58463446u );                                    // [0] magic 'F4FX'
        put( 32 +  4, 1u );                                             // [1] version
        put( 32 +  8, quint32( corpusHash & 0xFFFFFFFFu ) );            // [2] corpusHash lo
        put( 32 + 12, quint32( ( corpusHash >> 32 ) & 0xFFFFFFFFu ) );  // [3] corpusHash hi
        put( 32 + 16, quint32( pixelHash & 0xFFFFFFFFu ) );             // [4] pixelHash lo
        put( 32 + 20, quint32( ( pixelHash >> 32 ) & 0xFFFFFFFFu ) );   // [5] pixelHash hi
        put( 32 + 24, quint32( s16 ) | ( quint32( w16 ) << 16 ) );      // [6] south, west
        put( 32 + 28, quint32( n16 ) | ( quint32( e16 ) << 16 ) );      // [7] north, east
        put( 32 + 32, 1u );                                             // [8] encoding=1, flags=0
        put( 32 + 36, coveredTexels );                                  // [9] census only
        // [10], file offset 72, stays zero.
    }

    put( 4 + 72, 32 );
    put( 4 + 76, 0x4 );
    put( 4 + 80, 0x30315844 );
    put( 4 + 104, 0x1000 );
    put( 4 + 124 + 0, 56 );
    put( 4 + 124 + 4, 3 );
    put( 4 + 124 + 12, 1 );
    if ( f.write( hdr ) != hdr.size() )
        return false;
    // ... existing row loop unchanged ...
}
```

### What the caller (`lodgenBakeHeightmap`) must supply

- `cellSouth/West/North/East` — the **same four integers the filename carries**,
  in the same S.W.N.E order (`FormatHeightmapFileName`, Format.h:495-520). Gate 11
  compares them for equality; a name/header disagreement is a hard reject.
  `lodgenBakeHeightmap` already has them as `minY, minX, maxY, maxX` from
  `world.cellBounds` (lodgen.cpp:2861-2866).
- `coveredTexels` — number of texels backed by real LAND data. Never read by the
  loader; write the honest count (`w*h` when every texel was covered).
- `corpusHash` — for the Commonwealth this MUST be `0xD8337D022F637F22`
  (`kCommonwealthVhgtCorpusHash`) or the map is rejected. Either write the literal
  or compute it with the traversal in section 2; the reproduction above shows the
  two agree on the retail ESM. For every other worldspace the value is unchecked,
  so computing it from that worldspace's own VHGT payloads is free and correct.

### Rollout note

Because gates 9-13 only arm when the magic is present, the block is
all-or-nothing per file: writing it wrong turns a currently-loading map into a
named refusal. Verify a written file by re-reading its own bytes and checking
`Fnv1a64(payload) == header pixelHash` before shipping it.

---

## 6. Not determinable / out of scope

- Nothing in the block was left unexplained: all 11 reserved words of the real
  file are accounted for by `WriteBakeProvenance` plus the leading `memset`.
- `flags` has no defined bits anywhere in the tree — both producers write literal
  0 and no consumer reads it. Write 0.
- The two z fields of the filename (`-8320`, `44872`) are **not** part of this
  block. They are the encode's min/max height in units divided by 8
  (Format.h:495-520) and feed only `minHeightUnits_`/`maxHeightUnits_`
  (Runtime.cpp:741-742, :965-966). The pixel decode is fixed by `encoding == 1`
  and ignores them (Format.h:308-310).

## Reproduction scripts (kept in this scratchpad)

- `hdr.py` — dumps the DDS header and decodes the 11 reserved words.
- `fnvpy.py <file> <offset> <len>` — FNV-1a 64 over a byte range; used for the
  pixel hash.
- `corpus.py` — walks Fallout4.esm exactly as `PluginFile::ForEachLandAndCell`
  does and reproduces `0xD8337D022F637F22`.

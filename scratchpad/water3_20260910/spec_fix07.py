# -*- coding: utf-8 -*-
"""Replace spec_water.md's provenance footer wholesale.

The old footer was written against `src/lodtfile.cpp` at 1,706 lines, BEFORE
lane WATER2 built version 3 into it (3,638 lines). Every line number in it is
wrong and several of its anchors no longer exist -- the header-size ternary in
particular, which WATER2 replaced with a table. Anchors are written here;
`anchors.py` re-derives every LINE NUMBER from them as the last step.
"""
import sys
P = '../specs_20260909/spec_water.md'
t = open(P, 'rb').read().decode('utf-8')

head = t.index('## Provenance')
FOOT = """## Provenance

Written by lane WATER1 (design), corrected by lane WATER3 against the code lane
WATER2 landed. Anchors are quoted beside every line number and every number was
re-derived from its anchor by `scratchpad/water3_20260910/anchors.py` as the last
step, per the `ww-contract-provenance` procedure.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodtfile.cpp` | `SHA_LODTFILE_CPP` | BYTES_LODTFILE_CPP | LINES_LODTFILE_CPP |
| `src/lodtfile.h` | `SHA_LODTFILE_H` | BYTES_LODTFILE_H | LINES_LODTFILE_H |
| `src/watermark.cpp` | `SHA_WATERMARK_CPP` | BYTES_WATERMARK_CPP | LINES_WATERMARK_CPP |
| `src/watermark.h` | `SHA_WATERMARK_H` | BYTES_WATERMARK_H | LINES_WATERMARK_H |
| `src/watermarkpanel.cpp` | `SHA_WATERMARKPANEL_CPP` | BYTES_WATERMARKPANEL_CPP | LINES_WATERMARKPANEL_CPP |
| `docs/LODGEN_BTD_FORMAT.md` | `SHA_BTDFORMAT` | BYTES_BTDFORMAT | LINES_BTDFORMAT |
| `tests/spells/lodl_open_authority.py` | `SHA_AUTHORITY` | BYTES_AUTHORITY | LINES_AUTHORITY |

The format, as WATER2 built it:

| claim | line | anchor |
|---|---|---|
| the version constants, now 1..3 | lodtfile.cpp ? | `constexpr quint32 LODL_VERSION = 3;` |
| the header size is a TABLE, and the version refusal runs before it | lodtfile.cpp ? | `static inline qsizetype lodtHeaderBytes( int version )` |
| version 3's header is 0xF8 | lodtfile.cpp ? | `constexpr qsizetype LODL_HEADER_V3 = 0xF8;` |
| the default water-type sentinel | lodtfile.cpp ? | `constexpr quint16 WATER_TYPE_DEFAULT = 0xFFFFU;` |
| refusal 1's text | lodtfile.cpp ? | `unsupported version %1 (this reader knows %2..%3)` |
| the writer's own version refusal and the `WW_LODL_VERSION` fallback | lodtfile.cpp ? | `if ( qEnvironmentVariableIsSet( "WW_LODL_VERSION" ) )` |
| section-flag bits 4..7, the water sections | lodtfile.h ? | `constexpr quint32 LODL_SECT_BODIES      = 1u << 4;` |
| the body record, 48 bytes | lodtfile.h ? | `struct LodtWaterBody` |
| the water module's own switches | lodtfile.h ? | `struct LodtWaterOptions` |
| the plane container's packer, and the uniform tile | lodtfile.cpp ? | `static QByteArray lodtPackPlane( int tilesX, int tilesY, int tileEdge,` |
| the flow word the writer puts on an unmarked body | lodtfile.cpp ? | `word = quint16( dir | ( 8 << 8 ) );` |
| the stroke store, written present and EMPTY | lodtfile.cpp ? | `/* The stroke store is written EMPTY and present: a count of zero.` |
| the reader's version-3 accessors | lodtfile.h ? | `bool waterBody( int id, LodtWaterBody & out ) const;` |
| the raw stroke store, as read | lodtfile.h ? | `QByteArray strokeStore() const { return strokes; }` |

The marking tool, as WATER3 built it:

| claim | line | anchor |
|---|---|---|
| the header fields this tool patches | watermark.cpp ? | `constexpr qsizetype kHdrV3      = 0xF8;` |
| the packer, the TWIN of lodtPackPlane, and why a twin is safe | watermark.cpp ? | `QByteArray packPlane( int tilesX, int tilesY, int tileEdge, int bytesPerSample,` |
| an absolute plane offset is rebased, never memcpy'd | watermark.cpp ? | `QByteArray rebasePlane( const QByteArray & bytes, quint64 oldBase, quint64 newBase )` |
| the section ORDER the tool refuses to rearrange | watermark.cpp ? | `"the water sections are ordered body 0x%1, stroke 0x%2, "` |
| the stroke store codec | watermark.cpp ? | `QByteArray WaterMarkDoc::encodeStrokes() const` |
| the body table encoder, byte for byte the writer's | watermark.cpp ? | `QByteArray WaterMarkDoc::encodeTable() const` |
| a stroke on dry land is refused in words | watermark.cpp ? | `"that stroke starts on dry land, so it names no body of water; "` |
| the constrained harmonic fill, red-black SOR | watermark.cpp ? | `bool WaterMarkDoc::solve( WaterMarkSolve * out, QString * error )` |
| confidence: the stated departure from 4.3 | watermark.cpp ? | `* spec asks for "a second harmonic fill with 15 at the constraints and` |
| the automatic word, reproduced from the table | watermark.cpp ? | `quint16 WaterMarkDoc::automaticWord( quint16 id ) const` |
| the whole-plane sweep the isolation gate measures on | watermark.cpp ? | `bool WaterMarkDoc::sweep( const std::function<void( int, int, quint16, quint16, quint16 )> & cb,` |
| the save: prefix verbatim, tail re-derived, original renamed aside | watermark.cpp ? | `bool WaterMarkDoc::save( QString * error )` |
| the two identity gates | watermark.cpp ? | `bool WaterMarkDoc::flowRepackMatches( qint64 * differingBytes, QString * error ) const` |
| the harness | watermark.cpp ? | `bool lodtWaterMarkSelfTest( const QString & path, QString * text, QString * error )` |
| the stroke kinds, 0..6 | watermark.h ? | `enum Kind { Stroke = 0, Pin = 1, Barrier = 2, Merge = 3, SourcePin = 4, OutletPin = 5,` |
| the still-water mark is a STROKE, not a table bit | watermark.h ? | `*  kind 6, a one-point mark saying this body is still -- a STROKE and not a bit` |
| the dock, and its one line in somebody else's file | watermarkpanel.cpp ? | `void waterMarkInstall( QMainWindow * mw )` |
| the panel's rows and its summary sentence | watermarkpanel.cpp ? | `void refreshSummary()` |
| the canvas, and the Blender divergences | watermarkpanel.cpp ? | `class WaterMarkCanvas final : public QWidget` |
| the dock's self-test counts | watermarkpanel.cpp ? | `void runSelfTest( QMainWindow * mw, QDockWidget * dock, WaterMarkPanel * panel )` |

Still true from the older documents:

| claim | line | anchor |
|---|---|---|
| the plane list a `--info` prints, and where `waterheight`/`watertype` come from | lodl_open_authority.py ? | `def plane_keys(self):` |
| depth is a runtime subtraction, shore was dropped for it | LODGEN_BTD_FORMAT.md ? | `**shore proximity**` |
| ROW 0 IS SOUTH in every grid | LODGEN_BTD_FORMAT.md ? | `**ROW 0 IS SOUTH**` |
| FO4CS pins `kVersion = 1u` | LODGEN_BTD_FORMAT.md ? | `A CONSUMER EXISTS, AND IT KNOWS VERSION 1 ONLY.` |

Measured inputs, with their own stamps:

| input | stamp |
|---|---|
| `Commonwealth.lodl`, version 2 (the writer's own bytes with the module off) | 35,953,294 bytes |
| `Commonwealth.lodl`, version 3 | 38,612,038 bytes, `scratchpad/water2_20260909/out/Terrain/`, mtime 2026-09-10 00:50, 346 bodies, body table at 0x2249AE6, stroke store 4 bytes (a count of zero), planes at 32/32/32 samples a cell |
| the body census every number in 1 and 2 was re-derived from | `scratchpad/water3_20260910/census_v3.txt` (`lodl --water-census`) and `audit_spec.py` through the INDEPENDENT decoder `scratchpad/water2_20260909/lodl_v3_authority.py` |
| `Fallout4.esm` | `X:\\Programs\\Steam\\steamapps\\common\\Fallout 4\\Data\\Fallout4.esm`, 42 WATR records, WRLD `0000003C` |
| `WATR DNAM` field order | xEdit `wbDefinitionsFO4.pas`, `wbRecord(WATR, 'Water'` — 201 bytes, and all 40 full records measured at exactly 201 (2 truncated at 188, the `SetOptionalFrom(4)` tail) |
"""

t = t[:head] + FOOT
open(P, 'wb').write(t.encode('utf-8'))
nb = open(P, 'rb').read()
print('ok: %d bytes, %d lines, CR=%d' % (len(nb), nb.count(b'\n'), nb.count(b'\r')))

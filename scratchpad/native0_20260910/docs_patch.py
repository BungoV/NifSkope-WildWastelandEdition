"""Lane NATIVE0b: docs/LODGEN_NATIVE_LODO_LODI.md SPEC -> AS BUILT, and the handoff README's
.lodo/.lodi rows. Every replacement is anchored and asserted unique; the provenance footer's
line numbers are DERIVED from anchor text in the sources at run time (ww-contract-provenance
step 3), never typed. LF-only files; CR asserted 0."""
import hashlib
import re
import sys

ROOT = '.'


def read(p):
    b = open(p, 'rb').read()
    assert b.count(b'\r') == 0, p
    return b.decode('utf-8')


def write(p, s):
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(p, 'wb').write(out)


def stamp(p):
    b = open(p, 'rb').read()
    return hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b'\n')


def line_of(path, anchor, src_cache={}):
    if path not in src_cache:
        src_cache[path] = read(path).split('\n')
    hits = [i + 1 for i, l in enumerate(src_cache[path]) if anchor in l]
    if len(hits) != 1:
        raise SystemExit('anchor %r in %s: %d hits' % (anchor, path, len(hits)))
    return hits[0]


def rep(s, old, new, count=1):
    n = s.count(old)
    if n != count:
        raise SystemExit('anchor %r: %d hits (want %d)' % (old[:60], n, count))
    return s.replace(old, new)


# ------------------------------------------------------------------ contract page
P = 'docs/LODGEN_NATIVE_LODO_LODI.md'
s = read(P)
if 'STATUS: AS BUILT' in s:
    print('contract page already patched')
else:
    s = rep(s, '''> **STATUS: SPEC. NOT YET WRITTEN.**
> No writer, no reader, no file. Nothing in `src/` emits either extension and
> nothing in FO4CS reads one. Every byte below comes from
> `scratchpad/specs_20260906/spec_fo4cs_native.md`, which is a design document,
> **not** from a writer — so this page has no provenance footer of writer lines
> and must not be treated as one. The first lane to write the container updates
> this page against its own code and adds that footer.
''', '''> **STATUS: AS BUILT, NOT YET BUILT INTO THE EXE (2026-09-10, lane NATIVE0/NATIVE0b).**
> Writers, readers and the emitter exist: `src/lodofile.{h,cpp}` (the library),
> `src/lodifile.{h,cpp}` (the instance table), `src/nativeemit.{h,cpp}` (the
> emitter + `--native-verify`), all three syntax-clean under the real build
> flags and listed in `NifSkope.pro`; the independent decoder is
> `tests/spells/lodgen_native_decode.py`. **What has run:** the writers linked
> standalone against Qt6Core wrote the synthetic known-answer pair
> (`scratchpad/native0_20260910/fixture/Synthetic.lodo` 28,903 B / `.lodi`
> 16,408 B), the decoder passed **46 checks, 0 failures** on answers written
> before the run, two writes were byte-identical, and 20 single-byte mutations
> (11 of them with the CRCs re-signed) were refused BY NAME by both readers.
> **What has not:** no real worldspace pair -- the one hook-up call into
> `lodgenBuildObjectChunk` and the `--native` switch are a patch in
> `scratchpad/native0_20260910/HOOKUP_CHANGE_NEEDED.md`, not applied (lodgen.cpp
> was held by another lane); nothing in FO4CS reads either file. Two deviations
> from the spec are ratified below (**Deviations**), each with its measurement.
''')
    s = rep(s, '''| `Data\\Terrain\\<WS>\\Objects\\<WS>.lodo` | the **geometry library**: base, mesh, cluster and material tables, a fixed-stride local-index blob, a vertex blob, a string blob. One row per distinct model, never per placement | ~7.0 MiB |
| `Data\\Terrain\\<WS>\\Objects\\<WS>.lodi` | the **instance tables**: a dense chunk table, a cell-range blob, a 24-byte instance record per placement, and a parallel cold record | ~5.1 MiB |''',
    '''| `Data\\Terrain\\<WS>\\Objects\\<WS>.lodo` | the **geometry library**: base, mesh, cluster and material tables, a fixed-stride local-index blob, a vertex blob, a string blob. One row per distinct model, never per placement | ~7.0 MiB planned; **unmeasured** (no worldspace bake yet) |
| `Data\\Terrain\\<WS>\\Objects\\<WS>.lodi` | the **instance tables**: a dense chunk table, a cell-range blob, a 24-byte instance record per placement, and a parallel cold record | ~5.1 MiB planned; **unmeasured** |''')
    s = rep(s, '''| instance | chunk index (north-up row-major), then cell index, then `refFormId`, then `scolPart` |''',
    '''| instance | chunk index (north-up row-major), then cell index, then `refFormId`, then `scolPart`. **AS BUILT, the cell index the spec left unstated:** north-up row-major inside the chunk, `cell = (3 − ly)·4 + lx` with `lx, ly = floor((x, y − chunkOrigin) / 4096)` clamped to 0..3 (`lodiCellOf`) |''')
    s = rep(s, '''**Mesh entry — 48 B:** `f32 aabbMin[3]`, `f32 aabbExtent[3]`, `f32 uvMin[2]`,
`f32 uvExtent[2]`, `u32 clusterFirst`, `u16 clusterCount`, `u16 flags`
(bit0 anyAlphaTested, bit1 anySway).''',
    '''**Mesh entry — 56 B (AS BUILT; the spec said 48):** `f32 aabbMin[3]`,
`f32 aabbExtent[3]`, `f32 uvMin[2]`, `f32 uvExtent[2]`, `u32 clusterFirst`,
`u16 clusterCount`, `u16 flags` (bit0 anyAlphaTested, bit1 anySway),
**`u32 modelStringOffset`** (the LOD model path — the table's own sort key, so
the reader can check it and a verifier can find the source `_lod.nif`),
**`u32 reserved`** (0). See Deviations 1.''')
    s = rep(s, '''`u16 layer` (**< 2048**, the D3D11 array-axis limit), `u8 family`''',
    '''`u16 layer` (**< 2048**, the D3D11 array-axis limit, **or 0xFFFF = no array layer
assigned** — v1 writes no texture arrays, so every row says 0xFFFF until lane OBJM), `u8 family`''')
    s = rep(s, '''| 0x06 | 6 | `rotation` | 2-bit selector + 3 × 15-bit smallest-three quaternion (48 bits, **LSB-first over the three u16**). Worst 0.0146° = 0.07 px on a 2,000-unit crown at D = 10,240 |''',
    '''| 0x06 | 6 | `rotation` | 2-bit selector (bits 0–1 = the dropped, largest component; w,x,y,z order) + 3 × 15-bit smallest-three quaternion over [−1/√2, 1/√2] (48 bits, **LSB-first over the three u16**). Measured worst **0.0073°**, mean 0.0026° (10⁵ random rotations; the spec's 0.0146° was a bound). **AS BUILT this is the DRAWN rotation — the ESM rotation × the generator's tree yaw** — see Deviations 2 |''')
    s = rep(s, '''| 0x13 | 1 | `seed` | u8, the generator's **existing** position-derived hash — see §4.3 |''',
    '''| 0x13 | 1 | `seed` | u8 = `treeHash & 0xFF`, the low byte of the generator's position hash (0 for a non-tree): sway phase / jitter only, **not** the yaw — see §4.3 and Deviations 2 |''')
    s = rep(s, '''### 4.3 `seed` is the existing position hash, and the order of operations is
part of the format

`seed` is the generator's own position-derived hash
(`treeHash = qRound(pos[0]) * 2654435761U + …`), which today drives a tree's yaw
**and** its UV mirror. The quaternion carries **the ESM rotation only**; the
consumer applies the yaw and the mirror from `seed`.

**Hash first, quantise second.** Hashing the quantised position would flip the
hash for some trees and swing their yaw by up to 360°.

Two ways to get this wrong, and a stock-to-stock gate observes neither: bake the
yaw into the quaternion and the ESM cross-check cannot run for trees (94.8% of
the far field); replace it with a `(refFormId, part)` hash and every tree in the
Commonwealth rotates differently from the stock bake with nothing to notice.''',
    '''### 4.3 The tree yaw is in the quaternion; `seed` is the hash's low byte (AS BUILT)

The generator's hash is
`treeHash = (quint32(qRound(pos[0])) * 2654435761U) ^ (quint32(qRound(pos[1])) * 40503U)`
(`src/lodgen.cpp`, the `treeHash =` line); the stock bake rotates a tree by
`treeHash % 360` degrees about Z and mirrors its U when `(treeHash >> 8) & 1`.

The spec wanted the ESM rotation alone in the record and the yaw re-derived by
the consumer from a u8 `seed`. **That cannot work**: `% 360` needs 9 bits and the
mirror a tenth, and the consumer holds only the QUANTISED position (0.25-u step),
from which `qRound` disagrees with the generator for any tree within 0.125 u of a
.5 boundary. So, as built:

* `rotation` = **the drawn rotation**, `ESM × Rz(treeHash % 360°)` for a tree,
  the ESM rotation for anything else — exactly the matrix the stock `.bto`
  vertices were transformed by, to 0.0073° worst;
* `flags` bit0 = the mirror, `(treeHash >> 8) & 1`;
* `seed` = `treeHash & 0xFF`: a per-instance phase for sway, colour jitter or
  light flicker, and nothing a consumer must reconstruct the transform from.

**The ESM cross-check still runs for trees.** The independent decoder
(`tests/spells/lodgen_native_decode.py`, `check_esm`) recomputes `treeHash` from
the plugin's own float `DATA` position — it has the pre-quantisation value the
consumer does not — composes the yaw and requires the record's rotation within
0.02° of it. The known-answer fixture's tree carries a (10°, 20°, 30°) ESM
rotation times a 123° yaw and passes that gate.''')
    s = rep(s, '''## 9. Sample files

**None, and none are possible**: there is no writer. See
`scratchpad/handoff_fo4cs/README.md` §5 for what a first lane must produce.''',
    '''## 9. Sample files

**The synthetic known-answer pair**, written by the writers themselves and
checked by the independent decoder against answers written down before the run:
`scratchpad/native0_20260910/fixture/Synthetic.lodo` (28,903 B: 3 bases, 2
meshes, 3 clusters, 2 materials, 32 vertices, 32 triangles),
`Synthetic.lodi` (16,408 B: 3 instances in 3 of 6 dense chunks — a tree at
(1000.5, 2000.25, 300) rotated (10°, 20°, 30°) × yaw 123° at scale 1.5, mirrored
and alpha-tested; a static in chunk (−1,1); a SCOL part on the exact east edge of
chunk (0,0), which is chunk (1,0)), and `Synthetic.expect.txt` (the answers).
Regenerate with `lodgen --native-fixture <dir>` once the hook-up is built, or
with `scratchpad/native0_20260910/fixture_tool.exe <dir>` (the writers linked
standalone). **No real worldspace pair exists yet** — that needs the hook-up
patch and a build (`HOOKUP_CHANGE_NEEDED.md` section C).

## 10. Deviations from the spec, as built

1. **Mesh row 56 B, not 48.** The spec's row carried no model path, so the mesh
   table's sort law ("model path ascending") could not be checked in the file
   and a reconstruction verifier could not name a mesh's source `_lod.nif` (the
   BASE row names the near MODL, not the slot meshes). `modelStringOffset` +
   a reserved word were added: +8 B × ~3,355 meshes = 26.8 KiB. The reader
   refuses an unsorted table by row number.
2. **The rotation is the drawn rotation; `seed` is the hash's low byte.**
   Measured cause: `treeHash % 360` needs 9 bits, the u8 holds 8
   (`scratchpad/native0_20260910/audit_out.txt`, section 3). §4.3 above.
3. **The cell index inside a chunk is defined** (north-up row-major,
   `(3 − ly)·4 + lx`); the spec named the sort key and not the numbering.
4. **`material.layer` may be 0xFFFF** ("unassigned") in v1; the spec's `< 2048`
   holds for every assigned layer and the reader refuses 2048..0xFFFE.
5. **v1 writes what the stock ring bakes and nothing more:** no card layer
   (`cardLayer` = 0xFFFF everywhere, `cardCorpusHash` = 0), no `crossPx16`
   (0), `selfAO` = 255, `arrayClass`/`arraySet` = 0 with `layer` unassigned.
   A base with no loadable LOD model in any slot is left OUT of the base table
   and its instances are counted (`dropped for a base outside the table` in the
   census line) rather than written with neither mesh nor card, which the
   reader would refuse. Lanes OBJM/OBJC/OBJP fill those fields.

**Refusals, by name, as built** (each seen to fire on the synthetic pair with
the CRCs re-signed so the row rule and not the CRC answers): base table not
sorted by formId; cluster reserved flag bits; material family neither legacy nor
pbr; reserved header bytes (`.lodo` 0xB8..0xFF, `.lodi` 0x90..0xFF); base
`boundRadius` not > 0; instance reserved word / reserved flag bits (bits 6–15);
`NOLIB` with a non-zero identity; `ROW_ORDER_NORTH_UP` clear; `lodoIdentity`
not naming the `.lodo` (pairing). Plain flips are caught first by `headerCrc32`,
`indexCrc32` or the chunk `crc32`. The `(cell, ref, part)` order rule inside a
chunk is checked by both readers but **not exercised** by the fixture (one
instance per chunk) — a two-instance chunk fixture is owed.''')
    # ---- source + provenance footer, anchor-derived
    footer_files = ['src/lodofile.h', 'src/lodofile.cpp', 'src/lodifile.h', 'src/lodifile.cpp',
                    'src/nativeemit.h', 'src/nativeemit.cpp', 'tests/spells/lodgen_native_decode.py',
                    'src/lodgen.cpp']
    rows = [
        ('`.lodo` magic, version, header size, 4,096 alignment', 'src/lodofile.h', 'constexpr quint32 LODO_MAGIC = 0x4F444F4CU;'),
        ('the 16-byte vertex', 'src/lodofile.h', 'struct LodoVertex'),
        ('the 56-byte mesh row (Deviation 1)', 'src/lodofile.h', 'struct LodoMesh'),
        ('the 16-byte cluster row', 'src/lodofile.h', 'struct LodoCluster'),
        ('the 16-byte material row', 'src/lodofile.h', 'struct LodoMaterial'),
        ('the 32-byte base row', 'src/lodofile.h', 'struct LodoBase'),
        ('the strides pinned at compile time', 'src/lodofile.h', 'static_assert( sizeof( LodoVertex ) == 16'),
        ('`.lodo` header field offsets', 'src/lodofile.cpp', 'constexpr int H_MAGIC = 0x00, H_VERSION = 0x04, H_FLAGS = 0x08, H_HCRC = 0x0C;'),
        ('octahedral 12:12 pack', 'src/lodofile.cpp', 'quint32 lodoPackOct12( const float n[3] )'),
        ('tangent as a roll about the normal, bit 7 handedness', 'src/lodofile.cpp', 'quint8 lodoPackTangent( const float n[3], const float t[3], bool flipHanded )'),
        ('`lodoIdentity` = FNV-1a 64 over headerCrc32, modelCorpusHash, objectCorpusHash', 'src/lodofile.cpp', 'quint64 lodoIdentityOf( quint32 headerCrc32, quint64 modelCorpusHash, quint64 objectCorpusHash )'),
        ('whichever cap binds first closes the cluster', 'src/lodofile.cpp', 'if ( idx.size() / 3 >= LODO_CLUSTER_MAX_TRIS || members.size() + size_t( fresh ) > LODO_CLUSTER_MAX_VERTS )'),
        ('local-index slots past triangleCount are 0xFF', 'src/lodofile.cpp', 'lib.localIndices.resize( at + LODO_LOCAL_INDEX_BYTES, LODO_LOCAL_INDEX_NONE );'),
        ('payloads 4,096-aligned, pad zeroed by hand (Qt 6 resize does not zero)', 'src/lodofile.cpp', 'const quint64 at = alignUp( start, LODO_PAYLOAD_ALIGN );'),
        ('indexCrc32 over the seven payloads in file order; headerCrc32 over 0x10..0xFF', 'src/lodofile.cpp', 'h.headerCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( file.constData() ) + H_PLUGIN,'),
        ('`.lodo` reader: reserved header bytes refused by offset', 'src/lodofile.cpp', 'return refuse( QString( "reserved header byte at 0x%1 is not zero" )'),
        ('`.lodo` reader: the base table sort law', 'src/lodofile.cpp', 'return refuse( QString( "base table is not sorted by formId ascending at row %1 (0x%2 after 0x%3)" )'),
        ('`.lodo` reader: no mesh and no card is a refusal', 'src/lodofile.cpp', 'return refuse( QString( "base 0x%1 has no mesh in any slot and no card" )'),
        ('`.lodi` magic and the three header flags', 'src/lodifile.h', 'constexpr quint32 LODI_MAGIC = 0x49444F4CU;'),
        ('the 24-byte instance record', 'src/lodifile.h', 'struct LodiInstance'),
        ('the 32-byte chunk row, the cell range, the 8-byte cold record', 'src/lodifile.h', 'struct LodiChunk'),
        ('the two u16 ceilings as constants', 'src/lodifile.h', 'constexpr float LODI_SCALE_MAX = 65535.0f / 8192.0f;'),
        ('`.lodi` header field offsets', 'src/lodifile.cpp', 'constexpr int H_WEST = 0x48, H_SOUTH = 0x4A, H_EAST = 0x4C, H_NORTH = 0x4E;'),
        ('smallest-three 2 + 3 x 15, LSB-first', 'src/lodifile.cpp', 'void lodiPackRotation( const float m[9], quint16 out[3] )'),
        ('the cell index inside a chunk (Deviation 3)', 'src/lodifile.cpp', 'return ( LODI_CHUNK_CELLS - 1 - ly ) * LODI_CHUNK_CELLS + lx;'),
        ('the scale refusal, naming the ref, before a byte is written', 'src/lodifile.cpp', 'return fail( QString( "ref 0x%1 part %2 (base %3): scale %4 is outside 0 .. %5 (the u16/8192 ceiling); refused, not clamped" )'),
        ('the baseId refusal, naming the base', 'src/lodifile.cpp', 'return fail( QString( "base %1 (ref 0x%2): baseId %3 is past the u16 base table (65,535); refused" )'),
        ('the chunkCount cap, naming the extreme chunk', 'src/lodifile.cpp', 'return fail( QString( "dense chunk table would be %1 x %2 = %3 chunks, past the 65,536 cap; the extreme chunk is (%4, %5), ref 0x%6" )'),
        ('instance order: chunk, cell, ref, part', 'src/lodifile.cpp', 'return std::make_tuple( chunkIdx[a], cellIdx[a], A.refFormId, A.scolPart )'),
        ('per-chunk crc32 over instance then cold records', 'src/lodifile.cpp', 'c.crc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( &inst[k] ), qsizetype( ( e - k ) * sizeof( LodiInstance ) ) );'),
        ('`.lodi` reader: ROW_ORDER_NORTH_UP clear is a refusal', 'src/lodifile.cpp', 'return refuse( QStringLiteral( "flags: ROW_ORDER_NORTH_UP is clear (the .lodl mirror trap)" ) );'),
        ('`.lodi` reader: NOLIB / identity pairing rules', 'src/lodifile.cpp', 'return refuse( QStringLiteral( "NOLIB is set but lodoIdentity is not 0" ) );'),
        ('`.lodi` reader: cells partition the chunk in order', 'src/lodifile.cpp', 'return refuse( QString( "chunk %1: cell ranges sum to %2, chunk holds %3" )'),
        ('the synthetic known-answer fixture', 'src/lodifile.cpp', 'bool lodNativeFixtureWrite( const QString & dir, QStringList * report, QString * error )'),
        ('the fixture composes ESM x yaw into the record (Deviation 2)', 'src/lodifile.cpp', 'fromEuler( 0.0f, 0.0f, 123.0f * 0.01745329f, yaw );                // treeHash % 360 = 123'),
        ('the base table is the FULL worldspace census, formId ascending', 'src/nativeemit.cpp', 'std::sort( baseIds.begin(), baseIds.end() );'),
        ('objectCorpusHash: every REFR field the walk reads', 'src/nativeemit.cpp', 'objHash = lodoFnv1a64( r.pos, sizeof( r.pos ), objHash );'),
        ('the base-count refusal (u16)', 'src/nativeemit.cpp', 'return fail( QString( "the worldspace census names %1 LOD-bearing bases; the u16 baseId holds 65,536 (the last is 0x%2)" )'),
        ('the sway law h^2 (0.35 + 0.65 r) per vertex, model space', 'src/nativeemit.cpp', 'sh.geom.sway[v] = quint8( std::clamp( int( std::lround( hF * hF * ( 0.35f + 0.65f * rF ) * 255.0f ) ), 0, 255 ) );'),
        ('`seed` = the low byte of treeHash', 'src/nativeemit.cpp', 'r.seed = quint8( p.treeHash & 0xFF );'),
        ('the mirror in flags bit0', 'src/nativeemit.cpp', 'r.flags = quint16( ( p.mirrorU ? LODI_INST_MIRRORED : 0 ) | ( p.hasAlpha ? LODI_INST_ALPHA_TESTED : 0 )'),
        ('the finest ring wins the lighting', 'src/nativeemit.cpp', 'return;                 // a coarser ring: the finer one already spoke'),
        ('the PARTIAL rule (a region bake)', 'src/nativeemit.cpp', 'set.flags |= LODI_FLAG_PARTIAL;'),
        ('the census line the writer prints', 'src/nativeemit.cpp', 'QString line = QString( "native: %1.lodo %2 bytes (bases %3 of %4 in the census, %5 without a loadable model; "'),
        ('`--native-verify`: instances with neither geometry nor a card == 0', 'src/nativeemit.cpp', 'return fail( QString( "%1 instances have neither geometry nor a card" ).arg( noGeometryNoCard ) );'),
        ('the decoder reads the 56-byte mesh row', 'tests/spells/lodgen_native_decode.py', "le('ffffffffffIHHII', b, h['offMeshes'] + i * 56)"),
        ('the decoder recomputes treeHash from the ESM position', 'tests/spells/lodgen_native_decode.py', 'def tree_hash(x, y):'),
        ('the decoder composes the yaw into the ESM rotation before comparing', 'tests/spells/lodgen_native_decode.py', 'm = mat_mul(m, euler_neg_matrix(0.0, 0.0, -yaw))'),
        ('the generator hash the record derives from', 'src/lodgen.cpp', 'treeHash = ( quint32( qRound( r.pos[0] ) ) * 2654435761U )'),
        ('the yaw multiplied into the drawn rotation', 'src/lodgen.cpp', 'xf.rotation = xf.rotation * rz;'),
        ('the mirror bit', 'src/lodgen.cpp', 'const bool mirrorU = isTree && ( ( treeHash >> 8 ) & 1 );'),
        ('the silent drop this format deletes (`continue; // bucket full`)', 'src/lodgen.cpp', 'continue;   // bucket full; a second shape would need splitting'),
    ]
    foot = ['## Source, as built', '',
            'Written 2026-09-10 by lane NATIVE0b against the tree below; every line number is',
            'DERIVED from the quoted anchor by `scratchpad/native0_20260910/docs_patch.py` at',
            'the moment this page was written (ww-contract-provenance step 3), and `src/lodgen.cpp`',
            'was under edit by lane CLAMP2b while it ran. The design record is',
            '`scratchpad/specs_20260906/spec_fo4cs_native.md` (118,557 bytes); the audit of its',
            'numbers against the tree is `scratchpad/native0_20260910/audit_out.txt`; the hook-up',
            'that is still owed is `scratchpad/native0_20260910/HOOKUP_CHANGE_NEEDED.md`.', '',
            '| file | sha256 (16) | bytes | lines |', '|---|---|---|---|']
    for f in footer_files:
        h, n, l = stamp(f)
        foot.append('| `%s` | `%s` | %s | %s |' % (f, h, format(n, ','), format(l, ',')))
    foot += ['', '| claim | line | anchor |', '|---|---|---|']
    for claim, f, anchor in rows:
        ln = line_of(f, anchor)
        foot.append('| %s | `%s:%d` | `%s` |' % (claim, f.split('/')[-1], ln, anchor.replace('|', '\\|')))
    foot += ['',
             'bungo\'s `.lodo`/`.lodi` naming ruling and the *Improved LOD* module ruling are',
             'in `HANDOFF.md` (2026-09-09) and in',
             '`E:\\Projects\\Fo4CommunityShaders\\Codex\\HANDOFF.md` (15:34 2026-09-09).', '']
    i = s.index('## Source\n')
    s = s[:i] + '\n'.join(foot)
    write(P, s)
    print('%s: patched, %d anchor rows derived' % (P, len(rows)))

# ------------------------------------------------------------------ handoff README
P = 'scratchpad/handoff_fo4cs/README.md'
s = read(P)
if 'WRITTEN, gated on a synthetic pair' in s:
    print('README already patched')
else:
    s = rep(s, '''| `<WS>.lodo` | the FO4CS-native **geometry library**: base / mesh / cluster / material tables, index and vertex blobs | **SPEC ONLY, NOT WRITTEN** | no | `docs/LODGEN_NATIVE_LODO_LODI.md` |
| `<WS>.lodi` | the FO4CS-native **instance tables**: chunk table, cell ranges, 24-byte instance records, cold records | **SPEC ONLY, NOT WRITTEN** | no | `docs/LODGEN_NATIVE_LODO_LODI.md` |''',
    '''| `<WS>.lodo` | the FO4CS-native **geometry library**: base / mesh / cluster / material tables, index and vertex blobs | **WRITTEN, gated on a synthetic pair (46/46, decoder independent), NOT YET BUILT INTO THE EXE** — `src/lodofile.{h,cpp}`; the hook-up is a pending patch | no | `docs/LODGEN_NATIVE_LODO_LODI.md` (AS BUILT, two deviations) |
| `<WS>.lodi` | the FO4CS-native **instance tables**: chunk table, cell ranges, 24-byte instance records, cold records | **WRITTEN, same gate** — `src/lodifile.{h,cpp}`, emitter `src/nativeemit.{h,cpp}`, decoder `tests/spells/lodgen_native_decode.py` | no | `docs/LODGEN_NATIVE_LODO_LODI.md` |''')
    s = rep(s, '''| `.lodo` / `.lodi` | **still absent -- there is no writer** | section 6 |''',
    '''| `.lodo` / `.lodi` | the SYNTHETIC pair only: `scratchpad/native0_20260910/fixture/Synthetic.{lodo,lodi,expect.txt}`; no Commonwealth pair until the hook-up builds | section 6 |''')
    s = rep(s, '''1. **`.lodo` / `.lodi` are unwritten.** No writer, no reader, no file. The spec
   is `scratchpad/specs_20260906/spec_fo4cs_native.md` and the contract page
   `docs/LODGEN_NATIVE_LODO_LODI.md` carries a **SPEC / NOT YET WRITTEN** banner.
   Nothing on that page has been checked against a writer, because there is no
   writer. The spec's own provenance tags (`[par]`, `[pri]`, `[cei]`, `[nat]`,
   `[arith]`, `[con]`, `[inv]`) must be consulted before quoting any of its
   numbers as measured.''',
    '''1. **`.lodo` / `.lodi` are WRITTEN but not yet baked for a real worldspace**
   (2026-09-10, lanes NATIVE0/NATIVE0b). Writers, readers, the emitter and an
   independent Python decoder exist and are gated on a hand-written synthetic
   pair (46 checks, 0 failures; two writes byte-identical; 20 mutations refused
   by name). The contract page is **AS BUILT** with two deviations from the
   spec a reader must know: the mesh row is **56 B** (a model-path offset was
   added), and the instance `rotation` is the **DRAWN** rotation (ESM × the
   tree yaw) with `seed` = the hash's low byte. The one hook-up call into the
   chunk builder and the `--native` switch are a pending patch
   (`scratchpad/native0_20260910/HOOKUP_CHANGE_NEEDED.md`); lane 0's baseline
   (`tests/spells/lodgen_native_baseline.sh`) is written and not yet baked. The
   spec's own provenance tags (`[par]`, `[pri]`, `[cei]`, `[nat]`, `[arith]`,
   `[con]`, `[inv]`) still apply to every size and draw count on that page.''')
    write(P, s)
    print('%s: patched' % P)

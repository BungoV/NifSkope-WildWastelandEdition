#!/usr/bin/env python
"""`ww-contract-provenance` step 3 for docs/LODGEN_NATIVE_LODO_LODI.md v2: every
line number in the page's footer is FOUND AGAIN from its own anchor text against
the sources as they stand now, never shifted by a delta. Prints the markdown
rows and exits 1 if any anchor is not found exactly once."""
import io
import sys

ANCHORS = [
    ('`.lodo` magic, version 2 and the header size', 'src/lodofile.h',
     'constexpr quint32 LODO_MAGIC = 0x4F444F4CU;'),
    ('the cache-order flag', 'src/lodofile.h', '\tLODO_FLAG_CACHE_ORDER = 4'),
    ('the 16-byte vertex', 'src/lodofile.h', 'struct LodoVertex'),
    ('the 56-byte mesh row (Deviation 1)', 'src/lodofile.h',
     'quint32 modelStringOffset;  //!< the LOD model path this row was built from'),
    ('the 16-byte cluster row', 'src/lodofile.h', 'struct LodoCluster'),
    ('the 16-byte material row', 'src/lodofile.h', 'struct LodoMaterial'),
    ('the 32-byte base row', 'src/lodofile.h', 'struct LodoBase'),
    ('`loadOrderHash` at .lodo header 0xB8, and its law', 'src/lodofile.h',
     'quint64 loadOrderHash = 0;          //!< v2, header 0xB8'),
    ('the per-mesh statistics the gate reads', 'src/lodofile.h', 'struct LodoMeshStats'),
    ('the strides pinned at compile time', 'src/lodofile.h',
     'static_assert( sizeof( LodoVertex ) == 16'),
    ('`.lodo` header field offsets, and 0xB8 / 0xC0', 'src/lodofile.cpp',
     'constexpr int H_LOADORDER = 0xB8, H_RESERVED_C0 = 0xC0;'),
    ('octahedral 12:12 pack', 'src/lodofile.cpp', 'quint32 lodoPackOct12( const float n[3] )'),
    ('the boundary-edge count, welded by quantised position', 'src/lodofile.cpp',
     'quint32 lodoBoundaryEdges( const std::vector<quint32> & tris, KeyFn key )'),
    ('the GPU cache order, and KEEPING THE BETTER of the two', 'src/lodofile.cpp',
     'if ( cacheOrder && ca.acmr > cb.acmr ) {'),
    ('meshopt_optimizeVertexCache then the fetch remap', 'src/lodofile.cpp',
     'meshopt_optimizeVertexFetchRemap( remap.data(), tmp.data(), tmp.size(), nv );'),
    ('whichever cap binds first closes the cluster', 'src/lodofile.cpp',
     'if ( idx.size() / 3 >= LODO_CLUSTER_MAX_TRIS'),
    ('payloads 4,096-aligned, pad zeroed by hand', 'src/lodofile.cpp',
     'const quint64 at = alignUp( start, LODO_PAYLOAD_ALIGN );'),
    ('`.lodo` reader: version 1 refused by name', 'src/lodofile.cpp',
     'return refuse( QStringLiteral( "version 1: the v1 vertex blob is in SOURCE order'),
    ('`.lodo` reader: reserved header bytes refused by offset', 'src/lodofile.cpp',
     'return refuse( QString( "reserved header byte at 0x%1 is not zero" )'),
    ('`.lodo` reader: the base table sort law', 'src/lodofile.cpp',
     'return refuse( QString( "base table is not sorted by formId ascending at row %1'),
    ('`.lodi` magic and version 2', 'src/lodifile.h',
     'constexpr quint32 LODI_MAGIC = 0x49444F4CU;'),
    ('THE ONE SORT LAW, stated in the header', 'src/lodifile.h',
     ' *  THE ONE SORT LAW (v2, 2026-09-11, lane NATIVE1a)'),
    ('the 24-byte instance record, 0x16 = drawKey', 'src/lodifile.h', '\tquint16 drawKey;'),
    ('the cold record: the placed REFR and the stock identity', 'src/lodifile.h',
     '\tquint16 identity;'),
    ('the bound radius is the base radius at scale 1', 'src/lodifile.h',
     '\tfloat boundRadius = 0.0f;'),
    ('`.lodi` header field offsets, and 0x90 / 0x98', 'src/lodifile.cpp',
     'constexpr int H_LOADORDER = 0x90, H_RESERVED_98 = 0x98;'),
    ('smallest-three 2 + 3 x 15, LSB-first', 'src/lodifile.cpp',
     'void lodiPackRotation( const float m[9], quint16 out[3] )'),
    ('the cell index inside a chunk (Deviation 3)', 'src/lodifile.cpp',
     'return ( LODI_CHUNK_CELLS - 1 - ly ) * LODI_CHUNK_CELLS + lx;'),
    ('the sort: chunk, cell, drawKey, ref, part', 'src/lodifile.cpp',
     'return std::make_tuple( chunkIdx[a], cellIdx[a], A.drawKey, A.refFormId, A.scolPart )'),
    ('maxBoundRadius from the QUANTISED scale', 'src/lodifile.cpp',
     'maxR = std::max( maxR, r.boundRadius * qs );'),
    ('the scale refusal, naming the ref, before a byte is written', 'src/lodifile.cpp',
     'return fail( QString( "ref 0x%1 part %2 (base %3): scale %4 is outside 0 ..'),
    ('`.lodi` reader: version 1 refused by name', 'src/lodifile.cpp',
     'return refuse( QStringLiteral( "version 1: the v1 record\'s word at 0x16'),
    ('`.lodi` reader: a scale of 0 is a refusal', 'src/lodifile.cpp',
     'if ( r.scale == 0 )'),
    ('`.lodi` reader: the (cell, drawKey, ref, part) order', 'src/lodifile.cpp',
     'return refuse( QString( "instance %1: out of (cell, drawKey, ref, part) order'),
    ('the synthetic known-answer fixture', 'src/lodifile.cpp',
     'bool lodNativeFixtureWrite( const QString & dir, QStringList * report, QString * error )'),
    ('the fixture\'s two extra instances that EXERCISE the order rule', 'src/lodifile.cpp',
     'LodiSrcInstance keyLow;      // baseId 1 -> mesh B, material 0 -> drawKey 1'),
    ('the object census and its hash law, in one function', 'src/nativeemit.cpp',
     'bool nativeObjectCensus( const EsmWorld & world, std::vector<quint32> * baseIdsOut,'),
    ('the base table is the FULL worldspace census, formId ascending', 'src/nativeemit.cpp',
     'std::sort( baseIds.begin(), baseIds.end() );'),
    ('the cache-order flag is set on every emitted library', 'src/nativeemit.cpp',
     'lib.flags |= LODO_FLAG_CACHE_ORDER;'),
    ('the load-order hash carried into both files', 'src/nativeemit.cpp',
     'lib.loadOrderHash = world.loadOrderHash();'),
    ('THE DRAW RANK: one rank a distinct (mesh, material) pair', 'src/nativeemit.cpp',
     'std::vector<quint16> baseDrawKey( lib.bases.size(), 0 );'),
    ('the shadow-caster refusal at emit time', 'src/nativeemit.cpp',
     'the emit opened the silhouette and this object is a shadow caster'),
    ('the stock identity carried into the cold record', 'src/nativeemit.cpp',
     'r.identity = quint16( p.objectIndex );'),
    ('the per-mesh report file', 'src/nativeemit.cpp',
     '# lodgen native mesh report 1 ws '),
    ('the census line the writer prints', 'src/nativeemit.cpp',
     'QString line = QString( "native: %1.lodo %2 bytes'),
    ('`--native-verify`: the three staleness hashes recomputed', 'src/nativeemit.cpp',
     'if ( world ) {'),
    ('`--native-verify`: the drawKey rank checked against the library', 'src/nativeemit.cpp',
     'std::vector<quint16> wantKey( lib.bases.size(), 0 );'),
    ('the load-order hash law', 'src/esmdata.cpp', 'quint64 EsmWorld::loadOrderHash() const'),
    ('the REFR form id is the load-order-mapped one', 'src/esmdata.cpp', 'ref.formID = r->formID;'),
    ('the CLI: --native, --native-verify, --native-fixture', 'src/nifcli.cpp',
     'else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();'),
    ('the CLI: --native-mesh-report and --native-verify-corpus', 'src/nifcli.cpp',
     'else if ( t == QLatin1String( "--native-mesh-report" ) ) lgNativeMeshReport = next();'),
    ('the emitter armed from the region driver', 'src/nifcli.cpp',
     'lodgenNativeBegin( &world, nativeDir, lodgenNativeLoadModel,'),
    ('the decoder reads the 56-byte mesh row', 'tests/spells/lodgen_native_decode.py',
     "le('ffffffffffIHHII', b, h['offMeshes'] + i * 56)"),
    ('the decoder reproduces the load-order hash', 'tests/spells/lodgen_native_decode.py',
     'def load_order_hash(plugin_list):'),
    ('the decoder recomputes the (mesh, material) rank', 'tests/spells/lodgen_native_decode.py',
     'def draw_key_ranks(L):'),
    ('the decoder budgets the manifest\'s own print step', 'tests/spells/lodgen_native_decode.py',
     'def print_step(token):'),
    ('the generator hash the record derives from', 'src/lodgen.cpp',
     'treeHash = ( quint32( qRound( r.pos[0] ) ) * 2654435761U )'),
    ('the yaw multiplied into the drawn rotation', 'src/lodgen.cpp', 'xf.rotation = xf.rotation * rz;'),
    ('the placement handed to the emitter, one a ring', 'src/lodgen.cpp',
     'lodgenNativeAddPlacement( np );'),
    ('the identity index read before AO overwrites B', 'src/lodgen.cpp',
     'lodgenNativeLighting( chunkX, chunkY, dim,'),
]


def main():
    root = 'E:/Projects/NifskopeWildWastelandEdition/'
    rows = []
    bad = 0
    for claim, f, anchor in ANCHORS:
        text = io.open(root + f, encoding='utf-8').read()
        n = text.count(anchor)
        if n != 1:
            print('ANCHOR %s in %s: found %d times' % (anchor[:60], f, n), file=sys.stderr)
            bad += 1
            continue
        line = text[:text.index(anchor)].count('\n') + 1
        esc = anchor.strip().replace('|', '\\|').replace('\t', '')
        if len(esc) > 96:
            esc = esc[:93] + '...'
        rows.append('| %s | `%s:%d` | `%s` |' % (claim, f, line, esc))
    print('| claim | line | anchor |')
    print('|---|---|---|')
    for r in rows:
        print(r)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())

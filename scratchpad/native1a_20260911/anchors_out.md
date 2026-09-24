| claim | line | anchor |
|---|---|---|
| `.lodo` magic, version 2 and the header size | `src/lodofile.h:51` | `constexpr quint32 LODO_MAGIC = 0x4F444F4CU;` |
| the cache-order flag | `src/lodofile.h:67` | `LODO_FLAG_CACHE_ORDER = 4` |
| the 16-byte vertex | `src/lodofile.h:93` | `struct LodoVertex` |
| the 56-byte mesh row (Deviation 1) | `src/lodofile.h:116` | `quint32 modelStringOffset;  //!< the LOD model path this row was built from` |
| the 16-byte cluster row | `src/lodofile.h:121` | `struct LodoCluster` |
| the 16-byte material row | `src/lodofile.h:134` | `struct LodoMaterial` |
| the 32-byte base row | `src/lodofile.h:148` | `struct LodoBase` |
| `loadOrderHash` at .lodo header 0xB8, and its law | `src/lodofile.h:203` | `quint64 loadOrderHash = 0;          //!< v2, header 0xB8` |
| the per-mesh statistics the gate reads | `src/lodofile.h:255` | `struct LodoMeshStats` |
| the strides pinned at compile time | `src/lodofile.h:161` | `static_assert( sizeof( LodoVertex ) == 16` |
| `.lodo` header field offsets, and 0xB8 / 0xC0 | `src/lodofile.cpp:40` | `constexpr int H_LOADORDER = 0xB8, H_RESERVED_C0 = 0xC0;` |
| octahedral 12:12 pack | `src/lodofile.cpp:115` | `quint32 lodoPackOct12( const float n[3] )` |
| the boundary-edge count, welded by quantised position | `src/lodofile.cpp:234` | `quint32 lodoBoundaryEdges( const std::vector<quint32> & tris, KeyFn key )` |
| the GPU cache order, and KEEPING THE BETTER of the two | `src/lodofile.cpp:400` | `if ( cacheOrder && ca.acmr > cb.acmr ) {` |
| meshopt_optimizeVertexCache then the fetch remap | `src/lodofile.cpp:368` | `meshopt_optimizeVertexFetchRemap( remap.data(), tmp.data(), tmp.size(), nv );` |
| whichever cap binds first closes the cluster | `src/lodofile.cpp:526` | `if ( idx.size() / 3 >= LODO_CLUSTER_MAX_TRIS` |
| payloads 4,096-aligned, pad zeroed by hand | `src/lodofile.cpp:602` | `const quint64 at = alignUp( start, LODO_PAYLOAD_ALIGN );` |
| `.lodo` reader: version 1 refused by name | `src/lodofile.cpp:716` | `return refuse( QStringLiteral( "version 1: the v1 vertex blob is in SOURCE order` |
| `.lodo` reader: reserved header bytes refused by offset | `src/lodofile.cpp:768` | `return refuse( QString( "reserved header byte at 0x%1 is not zero" )` |
| `.lodo` reader: the base table sort law | `src/lodofile.cpp:906` | `return refuse( QString( "base table is not sorted by formId ascending at row %1` |
| `.lodi` magic and version 2 | `src/lodifile.h:67` | `constexpr quint32 LODI_MAGIC = 0x49444F4CU;` |
| THE ONE SORT LAW, stated in the header | `src/lodifile.h:36` | `*  THE ONE SORT LAW (v2, 2026-09-11, lane NATIVE1a)` |
| the 24-byte instance record, 0x16 = drawKey | `src/lodifile.h:123` | `quint16 drawKey;` |
| the cold record: the placed REFR and the stock identity | `src/lodifile.h:166` | `quint16 identity;` |
| the bound radius is the base radius at scale 1 | `src/lodifile.h:217` | `float boundRadius = 0.0f;` |
| `.lodi` header field offsets, and 0x90 / 0x98 | `src/lodifile.cpp:35` | `constexpr int H_LOADORDER = 0x90, H_RESERVED_98 = 0x98;` |
| smallest-three 2 + 3 x 15, LSB-first | `src/lodifile.cpp:108` | `void lodiPackRotation( const float m[9], quint16 out[3] )` |
| the cell index inside a chunk (Deviation 3) | `src/lodifile.cpp:166` | `return ( LODI_CHUNK_CELLS - 1 - ly ) * LODI_CHUNK_CELLS + lx;` |
| the sort: chunk, cell, drawKey, ref, part | `src/lodifile.cpp:290` | `return std::make_tuple( chunkIdx[a], cellIdx[a], A.drawKey, A.refFormId, A.scolPart )` |
| maxBoundRadius from the QUANTISED scale | `src/lodifile.cpp:312` | `maxR = std::max( maxR, r.boundRadius * qs );` |
| the scale refusal, naming the ref, before a byte is written | `src/lodifile.cpp:226` | `return fail( QString( "ref 0x%1 part %2 (base %3): scale %4 is outside 0 ..` |
| `.lodi` reader: version 1 refused by name | `src/lodifile.cpp:447` | `return refuse( QStringLiteral( "version 1: the v1 record's word at 0x16` |
| `.lodi` reader: a scale of 0 is a refusal | `src/lodifile.cpp:606` | `if ( r.scale == 0 )` |
| `.lodi` reader: the (cell, drawKey, ref, part) order | `src/lodifile.cpp:618` | `return refuse( QString( "instance %1: out of (cell, drawKey, ref, part) order` |
| the synthetic known-answer fixture | `src/lodifile.cpp:702` | `bool lodNativeFixtureWrite( const QString & dir, QStringList * report, QString * error )` |
| the fixture's two extra instances that EXERCISE the order rule | `src/lodifile.cpp:864` | `LodiSrcInstance keyLow;      // baseId 1 -> mesh B, material 0 -> drawKey 1` |
| the object census and its hash law, in one function | `src/nativeemit.cpp:98` | `bool nativeObjectCensus( const EsmWorld & world, std::vector<quint32> * baseIdsOut,` |
| the base table is the FULL worldspace census, formId ascending | `src/nativeemit.cpp:141` | `std::sort( baseIds.begin(), baseIds.end() );` |
| the cache-order flag is set on every emitted library | `src/nativeemit.cpp:369` | `lib.flags \|= LODO_FLAG_CACHE_ORDER;` |
| the load-order hash carried into both files | `src/nativeemit.cpp:371` | `lib.loadOrderHash = world.loadOrderHash();` |
| THE DRAW RANK: one rank a distinct (mesh, material) pair | `src/nativeemit.cpp:471` | `std::vector<quint16> baseDrawKey( lib.bases.size(), 0 );` |
| the shadow-caster refusal at emit time | `src/nativeemit.cpp:418` | `the emit opened the silhouette and this object is a shadow caster` |
| the stock identity carried into the cold record | `src/nativeemit.cpp:547` | `r.identity = quint16( p.objectIndex );` |
| the per-mesh report file | `src/nativeemit.cpp:599` | `# lodgen native mesh report 1 ws` |
| the census line the writer prints | `src/nativeemit.cpp:620` | `QString line = QString( "native: %1.lodo %2 bytes` |
| `--native-verify`: the three staleness hashes recomputed | `src/nativeemit.cpp:687` | `if ( world ) {` |
| `--native-verify`: the drawKey rank checked against the library | `src/nativeemit.cpp:716` | `std::vector<quint16> wantKey( lib.bases.size(), 0 );` |
| the load-order hash law | `src/esmdata.cpp:873` | `quint64 EsmWorld::loadOrderHash() const` |
| the REFR form id is the load-order-mapped one | `src/esmdata.cpp:379` | `ref.formID = r->formID;` |
| the CLI: --native, --native-verify, --native-fixture | `src/nifcli.cpp:5782` | `else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();` |
| the CLI: --native-mesh-report and --native-verify-corpus | `src/nifcli.cpp:5785` | `else if ( t == QLatin1String( "--native-mesh-report" ) ) lgNativeMeshReport = next();` |
| the emitter armed from the region driver | `src/nifcli.cpp:3430` | `lodgenNativeBegin( &world, nativeDir, lodgenNativeLoadModel,` |
| the decoder reads the 56-byte mesh row | `tests/spells/lodgen_native_decode.py:153` | `le('ffffffffffIHHII', b, h['offMeshes'] + i * 56)` |
| the decoder reproduces the load-order hash | `tests/spells/lodgen_native_decode.py:227` | `def load_order_hash(plugin_list):` |
| the decoder recomputes the (mesh, material) rank | `tests/spells/lodgen_native_decode.py:241` | `def draw_key_ranks(L):` |
| the decoder budgets the manifest's own print step | `tests/spells/lodgen_native_decode.py:261` | `def print_step(token):` |
| the generator hash the record derives from | `src/lodgen.cpp:3324` | `treeHash = ( quint32( qRound( r.pos[0] ) ) * 2654435761U )` |
| the yaw multiplied into the drawn rotation | `src/lodgen.cpp:3329` | `xf.rotation = xf.rotation * rz;` |
| the placement handed to the emitter, one a ring | `src/lodgen.cpp:3438` | `lodgenNativeAddPlacement( np );` |
| the identity index read before AO overwrites B | `src/lodgen.cpp:3702` | `lodgenNativeLighting( chunkX, chunkY, dim,` |

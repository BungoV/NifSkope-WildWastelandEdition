| `.lodo` magic, version 3 and the header size | `src/lodofile.h:51` | `constexpr quint32 LODO_MAGIC = 0x4F444F4CU;` |
| the version constant itself | `src/lodofile.h:64` | `constexpr quint32 LODO_VERSION = 3;` |
| the LADDER flag (v3) and the way back it names | `src/lodofile.h:82` | `LODO_FLAG_LADDER = 8` |
| CONE_OPEN, the cones refusal in a bit | `src/lodofile.h:105` | `constexpr quint16 LODO_CLUSTER_CONE_OPEN = 4;` |
| the ladders constants: group, floor, depth | `src/lodofile.h:118` | `constexpr int LODO_LADDER_GROUP = 4;` |
| the exact-error budget | `src/lodofile.h:127` | `constexpr int LODO_ERROR_EXACT_TRIS = 512;` |
| the root marker | `src/lodofile.h:129` | `constexpr float LODO_ERROR_ROOT = 3.4028235e38f;` |
| the 16-byte vertex | `src/lodofile.h:140` | `struct LodoVertex` |
| the 56-byte mesh row, and the v3 words in v2s reserved slot | `src/lodofile.h:165` | `quint16 clusterCountL0;` |
| the 16-byte cluster row | `src/lodofile.h:170` | `//! Cluster entry, 16 bytes. Unchanged from v2 except flags bit 2.` |
| THE 48-BYTE LADDER ROW | `src/lodofile.h:202` | `struct LodoClusterLod` |
| the 16-byte material row | `src/lodofile.h:223` | `struct LodoMaterial` |
| the 32-byte base row | `src/lodofile.h:237` | `struct LodoBase` |
| the strides pinned at compile time | `src/lodofile.h:253` | `static_assert( sizeof( LodoClusterLod ) == 48` |
| `loadOrderHash` at .lodo header 0xB8 | `src/lodofile.h:298` | `quint64 loadOrderHash = 0;          //!< v2, header 0xB8` |
| the ladder tables header words | `src/lodofile.h:276` | `quint64 offClusterLods = 0;` |
| the per-mesh statistics the gate reads | `src/lodofile.h:358` | `struct LodoMeshStats` |
| the ladder's silhouette refusal, declared | `src/lodofile.h:394` | `quint32 groupsRefusedSilhouette = 0;` |
| `.lodo` header field offsets, and 0xC0 / 0xCE | `src/lodofile.cpp:44` | `constexpr int H_OFF_CLUSTERLODS = 0xC0, H_CLUSTERLOD_STRIDE = 0xC8;` |
| octahedral 12:12 pack | `src/lodofile.cpp:120` | `quint32 lodoPackOct12( const float n[3] )` |
| octahedral 16:16, the cone axis | `src/lodofile.cpp:152` | `void lodoPackOct16( const float n[3], quint16 out[2] )` |
| the boundary-edge count, welded by quantised position | `src/lodofile.cpp:270` | `quint32 lodoBoundaryEdges( const std::vector<quint32> & tris, KeyFn key )` |
| point-to-triangle distance | `src/lodofile.cpp:310` | `float lodoPointTriDist2( const float p[3], const float a[3], const float b[3], const float c[3] )` |
| the two-sided vertex-sampled deviation | `src/lodofile.cpp:398` | `float lodoSoupDeviation( const std::vector<quint32> & a, const std::vector<quint32> & b,` |
| ONE emitter for every level: sphere, cone, size class | `src/lodofile.cpp:412` | `quint32 lodoEmitCluster( LodoLibrary & lib, const LodoMesh & mesh, float meshRadius, quint16 meshId,` |
| the sphere and the cone describe the STORED geometry | `src/lodofile.cpp:461` | `qp[v * 3 + k] = lodoDequantU16( lodoQuantU16( verts[v].pos[k], mesh.aabbMin[k], mesh.aabbExtent[k] ),` |
| the cone axis is AREA-WEIGHTED, the cosine measured on the DECODED axis | `src/lodofile.cpp:551` | `cosMin = std::min( cosMin, dec[0] * faceN[t] + dec[1] * faceN[t + 1] + dec[2] * faceN[t + 2] );` |
| the weld the ladder simplifies on, and the UV conflicts it counts | `src/lodofile.cpp:835` | `uvConflicts++;` |
| the group partition | `src/lodofile.cpp:997` | `nParts = meshopt_partitionClusters( part.data(), flat.data(), flat.size(),` |
| the group border LOCKED so a replaced group cannot crack | `src/lodofile.cpp:1033` | `lockv[v] = 1;` |
| the simplification itself | `src/lodofile.cpp:1039` | `const size_t n = meshopt_simplifyWithAttributes( dst.data(), gTris.data(), gTris.size(),` |
| THE LADDERS SILHOUETTE REFUSAL | `src/lodofile.cpp:1059` | `if ( lodoBoundaryEdges( outSoup, identityKey ) > lodoBoundaryEdges( gTris, identityKey ) ) {` |
| the error: exact under the budget, chain-bounded above it | `src/lodofile.cpp:1076` | `E = lodoSoupDeviation( fullSoup, outSoup, wpos );` |
| the error must GROW or the group is not formed | `src/lodofile.cpp:1083` | `if ( !( E > childErr ) ) {` |
| the coverage split that makes sourceTriangles a partition | `src/lodofile.cpp:1124` | `outCover[best].push_back( t );` |
| whichever cap binds first closes the cluster | `src/lodofile.cpp:916` | `if ( idx.size() / 3 >= LODO_CLUSTER_MAX_TRIS \|\| members.size() + size_t( fresh ) > LODO_CLUSTER_MAX_VERTS )` |
| payloads 4,096-aligned, pad zeroed by hand | `src/lodofile.cpp:1251` | `const quint64 at = alignUp( start, LODO_PAYLOAD_ALIGN );` |
| the ladder table is inside indexCrc32 | `src/lodofile.cpp:1296` | `h.offClusterLods = payload( lib.clusterLods.data(), quint64( lib.clusterLods.size() ) * sizeof( LodoClusterLod ) );` |
| `.lodo` reader: version 2 refused by name | `src/lodofile.cpp:1383` | `return refuse( QStringLiteral( "version 2: a v2 library has NO cluster ladder table` |
| `.lodo` reader: reserved header bytes refused by offset | `src/lodofile.cpp:1451` | `return refuse( QString( "reserved header byte at 0x%1 is not zero" )` |
| `.lodo` reader: the base table sort law | `src/lodofile.cpp:1688` | `return refuse( QString( "base table is not sorted by formId ascending at row %1` |
| `.lodo` reader: the cluster sort law gained LEVEL | `src/lodofile.cpp:1596` | `return refuse( QString( "cluster table is not sorted by (meshId, materialId, level) at row %1" ).arg( i ) );` |
| `.lodo` reader: MONOTONICITY is a refusal | `src/lodofile.cpp:1625` | `return refuse( QString( "cluster %1: geometricError %2 is larger than its parentError %3; the "` |
| `.lodo` reader: the cone and its flag must agree | `src/lodofile.cpp:1651` | `return refuse( QString( "cluster %1 is CONE_OPEN but carries an axis (%2, %3) and cosine %4" )` |
| `.lodi` magic and version 3 | `src/lodifile.h:67` | `constexpr quint32 LODI_MAGIC = 0x49444F4CU;` |
| the version constant itself | `src/lodifile.h:79` | `constexpr quint32 LODI_VERSION = 3;` |
| THE ONE SORT LAW, stated in the header | `src/lodifile.h:36` | `*  THE ONE SORT LAW (v2, 2026-09-11, lane NATIVE1a)` |
| the 24-byte instance record, 0x16 = drawKey | `src/lodifile.h:136` | `quint16 drawKey;` |
| the cold record: the placed REFR and the stock identity | `src/lodifile.h:215` | `quint16 identity;` |
| THE 40-BYTE OCCLUDER ROW | `src/lodifile.h:170` | `/*! v3: ONE PRECOMPUTED OCCLUDER, 40 bytes.` |
| the per-cell occluder range | `src/lodifile.h:200` | `struct LodiOccluderRange` |
| the per-cell cap and the stride, as constants | `src/lodifile.h:104` | `constexpr quint16 LODI_OCCLUDERS_PER_CELL = 4;` |
| the box the emitter offers per instance | `src/lodifile.h:279` | `bool hasOccluder = false;` |
| `.lodi` header field offsets, and 0x98 / 0xB0 | `src/lodifile.cpp:39` | `constexpr int H_OFF_OCC = 0x98, H_OFF_OCCRANGE = 0xA0, H_OCCCOUNT = 0xA8;` |
| smallest-three 2 + 3 x 15, LSB-first | `src/lodifile.cpp:113` | `void lodiPackRotation( const float m[9], quint16 out[3] )` |
| the cell index inside a chunk (Deviation 3) | `src/lodifile.cpp:171` | `return ( LODI_CHUNK_CELLS - 1 - ly ) * LODI_CHUNK_CELLS + lx;` |
| the sort: chunk, cell, drawKey, ref, part | `src/lodifile.cpp:295` | `return std::make_tuple( chunkIdx[a], cellIdx[a], A.drawKey, A.refFormId, A.scolPart )` |
| maxBoundRadius from the QUANTISED scale | `src/lodifile.cpp:318` | `maxR = std::max( maxR, r.boundRadius * qs );` |
| THE OCCLUDER SELECTION: volume descending, index as the tie-break | `src/lodifile.cpp:390` | `std::sort( v.begin(), v.end(), []( const std::pair<double, quint32> & a, const std::pair<double, quint32> & b ) {` |
| the 0.999 pull-in for the quantised rotation | `src/lodifile.cpp:412` | `b.halfExtent[k] = r.occHalf[k] * qs * 0.999f;` |
| the occluders join indexCrc32 | `src/lodifile.cpp:451` | `// v3: the occluder table and its ranges join indexCrc32 (contract 4.5)` |
| `.lodi` reader: version 2 refused by name | `src/lodifile.cpp:541` | `return refuse( QStringLiteral( "version 2: a v2 instance table has NO occluder tables` |
| `.lodi` reader: a scale of 0 is a refusal | `src/lodifile.cpp:767` | `if ( r.scale == 0 )` |
| `.lodi` reader: a box must name an instance of its OWN cell | `src/lodifile.cpp:749` | `return refuse( QString( "occluder %1 is listed in chunk %2 cell %3 but names instance %4, "` |
| the synthetic known-answer fixture | `src/lodifile.cpp:884` | `bool lodNativeFixtureWrite( const QString & dir, QStringList * report, QString * error )` |
| the fixtures CONE THAT MUST OPEN | `src/lodifile.cpp:1018` | `E( QStringLiteral( "lodo.mesh0.l0cluster0.coneOpen" ), QStringLiteral( "1" ) );` |
| the fixtures one hand-derived occluder | `src/lodifile.cpp:1093` | `keyHigh.hasOccluder = true;` |
| the object census and its hash law, in one function | `src/nativeemit.cpp:301` | `bool nativeObjectCensus( const EsmWorld & world, std::vector<quint32> * baseIdsOut,` |
| THE OCCLUDER FITTER, and the rule that shapes its constants | `src/nativeemit.cpp:162` | `NativeOccRefusal fitOccluderBox( const std::vector<float> & pos, const std::vector<quint32> & tris,` |
| ray parity along +X | `src/nativeemit.cpp:147` | `bool pointInSoup( const float pt[3], const std::vector<float> & pos, const std::vector<quint32> & tris )` |
| the whole voxel shaved off every side | `src/nativeemit.cpp:259` | `lo[k] = mn[k] + float( i0[k] ) * d[k] + d[k];` |
| the writers own hundred-point gate | `src/nativeemit.cpp:280` | `if ( !pointInSoup( pt, pos, tris ) )` |
| the LADDER flag set from the CLI switch | `src/nativeemit.cpp:582` | `lib.flags \|= LODO_FLAG_LADDER;` |
| the box offered per placement, for the mesh it DREW | `src/nativeemit.cpp:797` | `r.occMeshId = mi.value().meshId;` |
| the shadow-caster refusal at emit time | `src/nativeemit.cpp:631` | `the emit opened the silhouette and this object is a shadow caster` |
| the per-mesh report, version 3 | `src/nativeemit.cpp:910` | `"# lodgen native mesh report 3 ws "` |
| the ladder census line | `src/nativeemit.cpp:964` | `QString ladderLine = QString( "native-ladder: %1` |
| the occluder census line | `src/nativeemit.cpp:981` | `QString occLine = QString( "native-occluders: %1` |
| `--native-verify`: the ladder is a partition of its own surface | `src/nativeemit.cpp:1131` | `return fail( QString( "mesh %1 (%2): its root clusters account for %3 full-detail triangles but the mesh "` |
| the load-order hash law | `src/esmdata.cpp:873` | `quint64 EsmWorld::loadOrderHash() const` |
| the REFR form id is the load-order-mapped one | `src/esmdata.cpp:379` | `ref.formID = r->formID;` |
| the CLI: --native, --native-verify, --native-fixture | `src/nifcli.cpp:5791` | `else if ( t == QLatin1String( "--native" ) ) lgNativeDir = next();` |
| the CLI: the two ways back | `src/nifcli.cpp:5796` | `else if ( t == QLatin1String( "--native-no-ladder" ) ) lgNativeLadder = false;` |
| the emitter armed from the region driver | `src/nifcli.cpp:3430` | `lodgenNativeBegin( &world, nativeDir, lodgenNativeLoadModel,` |
| the partitioner joined the build | `NifSkope.pro:489` | `lib/meshoptimizer/src/partition.cpp` |
| the decoder reads the 48-byte ladder row | `tests/spells/lodgen_native_decode.py:179` | `L['clusterLods'] = [dict(zip(('cx', 'cy', 'cz', 'radius', 'geometricError', 'parentError',` |
| the decoder refuses a non-monotone ladder | `tests/spells/lodgen_native_decode.py:258` | `raise Refusal('cluster %d: geometricError %r > parentError %r -- the ladder is not monotone'` |
| the decoder walks the occluder ranges by cell | `tests/spells/lodgen_native_decode.py:522` | `raise Refusal('chunk %d cell %d: occluders start at %d, expected %d' % (ci, k, of, occCursor))` |
| the reference selector: the cut and its partition | `tests/spells/lodgen_native_cut.py:295` | `def partition_ok(L, mi, sel):` |
| the reference projection constant | `tests/spells/lodgen_native_cut.py:54` | `PROJECTION_SCALE = 960.0 / math.tan(math.radians(35.0))` |
| the cone floor that CAN fire | `tests/spells/lodgen_native_cut.py:264` | `coneWorstNarrow = max(coneWorstNarrow, (worstDot + 1.0e-3) - worstDot)` |
| the occluder box floor, grown until it leaks | `tests/spells/lodgen_native_cut.py:450` | `GROW = (1.1, 1.25, 1.5, 2.0)` |
| the mutation set: one per new v3 field | `tests/spells/lodgen_native_mutate.py:226` | `add('v3 lodo a child deviates more than its parent', 'lodo',` |
| the field gate reads the 25-column report | `tests/spells/lodgen_native_fields.py:126` | `t = line.split(None, 24)` |
| the spell: thirteen legs | `tests/spells/lodgen_native.sh:6` | `# Thirteen legs, in the order a failure is cheapest to read:` |
| the spell: the second region, because Sanctuary has no watertight mesh | `tests/spells/lodgen_native.sh:50` | `OCCREGION="${OCCREGION:-0 -12 11 -1}"` |

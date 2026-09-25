/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODOFILE_H
#define LODOFILE_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

/*! `.lodo` v1 -- the FO4CS-native OBJECT LIBRARY: one deduplicated copy of
 *  every LOD mesh a worldspace's placements reach, as fixed-stride tables a
 *  consumer uploads into StructuredBuffers and never parses on the render
 *  thread. Its twin is `.lodi` (src/lodifile.h), the instance table that
 *  points into it by `baseId`.
 *
 *  The contract is docs/LODGEN_NATIVE_LODO_LODI.md; this header is the layout
 *  and nothing else. bungo's FINAL NAMES ruling of 2026-09-09: `.lodo` is the
 *  object library (was to be `.lodg`), `.lodi` the instances, `.lodl` land,
 *  `.lodt` terrain textures, `.lodm` materials. `.bto`/`.btr` stay the stock
 *  bake the engine reads; these files are the parallel native path.
 *
 *  Rules inherited from `.lodt`/`.lodl` and load-bearing (a change is a
 *  format break):
 *
 *   * little-endian throughout, ABSOLUTE 64-bit offsets, fixed table strides;
 *   * payloads in table-index order at 4,096-aligned offsets with ZERO-filled
 *     pad, an absent row all-zero -- that is what makes two runs of one bake
 *     byte-identical;
 *   * CRC-32, zlib polynomial 0xEDB88320 (lodvCrc32, src/io/lodvfile.h), over
 *     the header and over the tables;
 *   * every reserved field zero, and a non-zero reserved field is a REFUSAL
 *     that names it, never a warning;
 *   * the worldspace editor ID at 32 bytes, refused, never truncated;
 *   * a magic that is neither `DDS ` nor `LODT` nor `LDTX` nor `LODI` nor
 *     `LODM`, and a reader that names each of those when it sees one.
 *
 *  Table ORDER is part of the format (two-bake byte identity cannot see a
 *  self-consistent bake that is merely ordered differently), so the reader
 *  checks it: bases by formId ascending over the FULL worldspace census;
 *  meshes by model path ascending (case-folded); clusters by meshId, then
 *  materialId, then LEVEL (v3), then first triangle; materials by family,
 *  arrayClass, arraySet, layer, then the material's string. */

//! First four bytes, little-endian: `L`,`O`,`D`,`O`.
constexpr quint32 LODO_MAGIC = 0x4F444F4CU;
/*! v3 (2026-09-11, lane NATIVE1b): the CLUSTER LADDER. A parallel
 *  `LodoClusterLod` row per cluster at header 0xC0 carries the bounding sphere,
 *  the normal cone, the geometric error measured against FULL detail, the
 *  parent link and the level; the cluster table now holds every level of every
 *  mesh, sorted (meshId, materialId, level, first triangle); the mesh row's
 *  reserved word became `clusterCountL0` + `levelCount`.
 *
 *  v2 is REFUSED BY NAME, and not merely version-checked: a v2 file has no
 *  ladder table at all, so a v3 consumer reading one would see error 0 and
 *  parentError 0 for every cluster and draw the whole library at full detail at
 *  every distance -- the exact opposite of what the screen-error selection is
 *  for. v1 is refused for v2's reasons as well. */
/*! v4 (2026-09-16, lane NATIVE1c): the four header words, and the library
 *  built from each base's NEAR model. `LodoBase`'s eight bytes of `crossPx16`
 *  become a u32 `fullTriangles` -- the base's FULL-DETAIL triangle count,
 *  summed over the DISTINCT meshes its `rep` slots name -- plus two remaining
 *  screen-size steps; the `.lodo` header gains `cardCount` at 0xD0; the mesh
 *  row's flags gain LODO_MESH_WATERTIGHT at bit 2.
 *
 *  v3 is REFUSED BY NAME, and not merely version-checked, because the base row
 *  is REINTERPRETED and not extended: a v3 file's `crossPx16[0..1]` are two
 *  screen-size steps in 1/16 px, and a v4 reader taking those same four bytes
 *  as a little-endian triangle count would read a base with steps (16, 0) as a
 *  base with 16 full-detail triangles -- a plausible number, silently wrong,
 *  and the worst kind of format error. v1 and v2 stay refused for their own
 *  reasons. */
/*! v5 (2026-09-25, lane SEAM1, W4 -- bungo's ruling): the OPTIONAL per-vertex
 *  COLOUR STREAM. A mesh whose source carries BOTH a vertex-colour channel and
 *  the shader's Vertex_Colors flag (SLSF2 bit 5), which is exactly when the
 *  game applies it, gets LODO_MESH_VERTEX_COLOUR and one RGBA8 row per library
 *  vertex of its range in a blob written LAST, after the strings; header 0xD4
 *  counts the rows and 0xD8 is the blob's offset. RGB and A are stored as the
 *  source has them, never premultiplied, and they stay two channels: RGB tints
 *  the diffuse, A is an opacity factor ONLY where the source shader also sets
 *  Vertex_Alpha (LODO_MESH_VERTEX_ALPHA) -- on every Fallout4.esm LOD shape the
 *  census reached, it does not (28 of 28, scratchpad/seam1_20260925/w4_alpha.py).
 *
 *  A library with no coloured mesh writes 0 at 0xD4 and 0xD8 and no blob, so its
 *  bytes are a v4 file's with the version word changed -- the version is outside
 *  headerCrc32 (which starts at 0x10), so nothing else moves, not even the
 *  `.lodi`'s lodoIdentity. For the same reason a v4 file is READ as a v5 file
 *  without colour: v5 EXTENDS the layout into v4's reserved-zero pad and
 *  reinterprets nothing, unlike v3 -> v4.
 *
 *  VERSION 5 was once the subdivided library (lane HORIZON3, 2026-09-19), dropped
 *  the same day (lane HORIZONOUT); no exe ever wrote that file, so the number is
 *  free. The history paragraph is in docs/LODGEN_NATIVE_LODO_LODI.md 3.7. */
constexpr quint32 LODO_VERSION = 5;
//! The one earlier version this reader accepts: v5's layout with no colour stream.
constexpr quint32 LODO_VERSION_NO_COLOUR = 4;
//! v5: a colour row, RGBA8 in byte order R, G, B, A.
constexpr quint32 LODO_COLOUR_STRIDE = 4;
//! v5: the in-memory colour of a vertex no coloured shape gave one -- white, opaque.
constexpr quint32 LODO_COLOUR_NONE = 0xFFFFFFFFU;
constexpr quint32 LODO_HEADER_BYTES = 256;
constexpr quint32 LODO_PAYLOAD_ALIGN = 4096;

enum LodoHeaderFlags
{
	LODO_FLAG_VERTEX_V1 = 1,    //!< must be set: vertex layout v1, stride 16
	LODO_FLAG_PARTIAL = 2,      //!< rows present for a subset; indices worldspace-stable
	/*! v2: every mesh's triangles were run through meshopt_optimizeVertexCache
	 *  and its vertices through meshopt_optimizeVertexFetchRemap before
	 *  clustering. Clear = the emitter was asked for source order. */
	LODO_FLAG_CACHE_ORDER = 4,
	/*! v3: the ladder was BUILT -- levels above 0 exist wherever a mesh could
	 *  be simplified. Clear = the exact way back (`--native-no-ladder`): every
	 *  mesh is one level, every `geometricError` is 0 and every `parentError`
	 *  is the root marker, the sphere and the cone are still written, and
	 *  `levelMax` is 0. A consumer then selects full detail everywhere, which
	 *  is what v2 did. The output NAMES its serving arm (CONSTITUTION 10). */
	LODO_FLAG_LADDER = 8
};
constexpr quint32 LODO_FLAGS_KNOWN = LODO_FLAG_VERTEX_V1 | LODO_FLAG_PARTIAL | LODO_FLAG_CACHE_ORDER
	| LODO_FLAG_LADDER;

constexpr quint16 LODO_CLUSTER_MAX_TRIS = 16;
constexpr quint16 LODO_CLUSTER_MAX_VERTS = 48;
constexpr quint16 LODO_VERTEX_STRIDE = 16;
constexpr quint32 LODO_LOCAL_INDEX_BYTES = 48;    //!< per cluster: 16 triangles x 3 u8
constexpr quint8 LODO_LOCAL_INDEX_NONE = 0xFF;    //!< a slot past triangleCount
constexpr quint16 LODO_NO_MESH = 0xFFFF;          //!< base.rep[k]: no mesh in that slot
constexpr quint16 LODO_NO_CARD = 0xFFFF;          //!< base.cardLayer: no card
constexpr quint16 LODO_NO_LAYER = 0xFFFF;         //!< material.layer: no array layer assigned (v1 emit)
constexpr quint16 LODO_LAYER_CAP = 2048;          //!< D3D11 array-axis limit; a layer is < this or NO_LAYER

//! cluster.flags bits 0-1: the draw size class (vertexCountPerInstance 12 / 24 / 48)
enum LodoClusterSizeClass { LODO_SIZE_4 = 0, LODO_SIZE_8 = 1, LODO_SIZE_16 = 2 };
/*! v3, cluster.flags bit 2 -- the room the v2 contract named for the normal
 *  cone. Set when the cluster's face normals span MORE than a hemisphere, so no
 *  cone can bound them: a leaf card, a crossed quad, a two-sided shape. The
 *  `LodoClusterLod` row then carries axis (0,0) and `coneCos` = -1, and a
 *  consumer must never backface-cull the cluster. It is the cone's REFUSAL, in
 *  the bit rather than as a magic cosine. */
constexpr quint16 LODO_CLUSTER_CONE_OPEN = 4;
constexpr quint16 LODO_CLUSTER_FLAGS_KNOWN = 0x7;

/*! v3, the ladder's constants. A group is up to LODO_LADDER_GROUP clusters
 *  (meshopt_partitionClusters' target), never crosses a material -- two
 *  materials are two textures and a collapse across them is a seam -- and is
 *  simplified with every vertex on its border with the rest of the level LOCKED,
 *  so a group can be replaced by its parent without cracking against a
 *  neighbour that was not. A group is not formed at all below
 *  LODO_LADDER_MIN_TRIS triangles, or when the simplifier cannot remove one
 *  triangle, or when the error would not GROW -- which is the brief's "until one
 *  cluster remains or the error stops growing" and is what makes the errors
 *  strictly increasing up every chain. */
constexpr int LODO_LADDER_GROUP = 4;
constexpr int LODO_LADDER_MIN_TRIS = 4;
constexpr int LODO_LADDER_MAX_LEVEL = 15;
/*! Up to this many FULL-DETAIL triangles under a group, the error is measured
 *  DIRECTLY against full detail (two-sided, vertex-sampled). Above it the cost
 *  is quadratic in the level, so the error becomes the CONSERVATIVE chain bound
 *  `child error + this step's deviation`, which the triangle inequality makes an
 *  upper bound on the true deviation. Which rule served is counted per mesh and
 *  printed, never assumed. */
constexpr int LODO_ERROR_EXACT_TRIS = 512;

/*! v4, THE FOLIAGE REFUSAL (lane NATIVE1c, 2026-09-16). A material that is
 *  ALPHA-TESTED and belongs to a TREE is leaf cards: crossed quads whose
 *  silhouette is carried entirely by the texture's alpha, not by the geometry.
 *  An edge collapse across them does not coarsen a crown, it deletes quads --
 *  the crown becomes fragments and then nothing, which is the stump bungo saw
 *  in `ladder.png` on 2026-09-11 16:1x. A tree's far representation is its
 *  CARD, by his ruling, so the ladder refuses the material outright and its
 *  level-0 clusters stay roots. TRUNK and every other opaque material of the
 *  same model still ladders: the refusal is per MATERIAL, not per mesh.
 *
 *  `--native-ladder-foliage` is the exact way back and ladders them again. */
constexpr bool LODO_LADDER_FOLIAGE_DEFAULT = false;

/*! v4, THE SILHOUETTE GATE (lane NATIVE1c). Every level a mesh earns must keep
 *  at least this fraction of LEVEL 0's silhouette area, measured from
 *  LODO_SILHOUETTE_VIEWS horizon azimuths by an orthographic software
 *  rasteriser, or the LEVEL is refused and the material stops laddering there.
 *
 *  The surface compared is the CUT a consumer would actually draw at that
 *  level -- the level's own clusters PLUS every cluster that stayed a root
 *  below it -- and not the level's clusters alone. A ladder is PARTIAL wherever
 *  a group refuses (docs 11, deviation 11), so a level's own clusters are a
 *  FRAGMENT of the mesh and comparing a fragment's outline against the whole
 *  is the apples-to-oranges error that deviation 11 records.
 *
 *  The default is stated with its reason in docs 3.5.5 and is a switch
 *  (`--native-silhouette <f>`); 0 turns the gate off, which is the exact way
 *  back to the v3 ladder. */
constexpr float LODO_SILHOUETTE_MIN_DEFAULT = 0.70f;
constexpr int LODO_SILHOUETTE_VIEWS = 8;     //!< azimuths around Z, at the horizon
constexpr int LODO_SILHOUETTE_GRID = 96;     //!< coverage grid per view, per axis
//! `LodoClusterLod::parentError` at a root: no parent, so no tolerance is ever above it.
constexpr float LODO_ERROR_ROOT = 3.4028235e38f;
constexpr quint32 LODO_NO_PARENT = 0xFFFFFFFFU;

/*! v4, mesh flags bit 2: the mesh's LEVEL-0 surface is WATERTIGHT -- zero
 *  boundary edges over the position weld, which is the same measurement
 *  `LodoMeshStats::boundarySource == 0` already made for the occluder-box fit.
 *  It is written so a consumer can pick shadow casters and occluders from the
 *  `.lodo` alone, without re-deriving the topology of every mesh at load. */
/*! v5, mesh flags bit 3: the mesh has rows in the colour stream (a source
 *  shape carried a colour channel AND Vertex_Colors). Bit 4: a coloured shape's
 *  shader also sets Vertex_Alpha (SLSF1 bit 3), so the row's A is an opacity
 *  factor; without it A is carried but a consumer does not apply it. Bit 4 is
 *  never set without bit 3. */
enum LodoMeshFlags { LODO_MESH_ANY_ALPHA = 1, LODO_MESH_ANY_SWAY = 2, LODO_MESH_WATERTIGHT = 4,
	LODO_MESH_VERTEX_COLOUR = 8, LODO_MESH_VERTEX_ALPHA = 16 };
enum LodoMaterialFlags { LODO_MAT_TWO_SIDED = 1, LODO_MAT_EMITS = 2, LODO_MAT_TREE = 4 };
enum LodoBaseFlags { LODO_BASE_TREE = 1, LODO_BASE_ANY_ALPHA = 2, LODO_BASE_ANY_MESH = 4 };
enum LodoFamily { LODO_FAMILY_LEGACY = 0, LODO_FAMILY_PBR = 1 };

#pragma pack( push, 1 )

//! Library vertex, 16 bytes (docs/LODGEN_NATIVE_LODO_LODI.md 3.1).
struct LodoVertex
{
	quint16 pos[3];     //!< u16 into the mesh's own AABB
	quint16 uv[2];      //!< u16 unorm into the mesh's own UV rect
	quint8 nrm[3];      //!< octahedral 12:12, n = b0 | b1<<8 | b2<<16, octX = n & 0xFFF, octY = n >> 12
	quint8 tangent;     //!< bits 0-6 roll about the normal (2.8125 deg step), bit 7 handedness
	quint8 sway;        //!< per-vertex sway weight, 0 = rigid
	quint8 selfAO;      //!< the model's own self-occlusion (255 = not baked, v1)
};

/*! Mesh entry, 56 bytes. The spec's row was 48 and carried NO path, so a
 *  verifier could not name the `_lod.nif` a mesh came from and a reader could
 *  not check the table's own sort order; AS BUILT the row carries the LOD
 *  model path (the mesh table's sort key) and a reserved word. */
struct LodoMesh
{
	float aabbMin[3];
	float aabbExtent[3];
	float uvMin[2];
	float uvExtent[2];
	quint32 clusterFirst;
	quint16 clusterCount;       //!< EVERY level of this mesh (v3), contiguous
	quint16 flags;              //!< LodoMeshFlags
	quint32 modelStringOffset;  //!< the LOD model path this row was built from
	//! v3, the v2 reserved word: how many of the range's clusters are level 0...
	quint16 clusterCountL0;
	quint8 levelCount;          //!< ...and how many levels the range holds (>= 1)
	quint8 reserved;            //!< 0
};

//! Cluster entry, 16 bytes. Unchanged from v2 except flags bit 2.
struct LodoCluster
{
	quint32 vertexBase;
	quint8 vertexCount;     //!< <= 48
	quint8 triangleCount;   //!< <= 16
	quint16 materialId;
	quint8 boundCentre[3];  //!< u8 into the mesh AABB
	quint8 boundRadius;     //!< u8/255 of the mesh radius (half the AABB diagonal)
	quint16 meshId;
	quint16 flags;          //!< bits 0-1 size class, bit 2 CONE_OPEN (v3), bits 3-15 reserved 0
};

/*! v3: the cluster's LADDER row, 48 bytes, PARALLEL to the cluster table (row
 *  i describes cluster i) at header 0xC0. It is a second table rather than a
 *  wider cluster row for two reasons, both measured: the 16-byte cluster row is
 *  what a consumer's index-fetch path reads per DRAWN cluster, while these
 *  fields are read per CANDIDATE cluster by the cull dispatch, and the v2 row
 *  has four bytes free against the 32 this needs.
 *
 *  `geometricError` is the maximum deviation of THIS cluster's surface from the
 *  FULL-DETAIL surface -- never from its parent -- in the mesh's own units at
 *  scale 1, the same space as `LodoBase::boundRadius`. Measuring against full
 *  detail is what lets a consumer compare ONE stored number against ONE
 *  tolerance without walking the chain, and it is what makes the errors monotone
 *  up the ladder.
 *
 *  `parentError` is the error of the group this cluster was merged INTO, or
 *  LODO_ERROR_ROOT when nothing consumed it. THE CUT: draw the cluster when
 *  `geometricError x k <= tolerance` and `parentError x k > tolerance`. Exactly
 *  one cluster of every leaf's ancestry satisfies that, which is the partition
 *  the reference selector checks (tests/spells/lodgen_native_cut.py). */
struct LodoClusterLod
{
	float centre[3];        //!< bounding-sphere centre, mesh-local
	float radius;           //!< bounding-sphere radius, mesh-local, > 0
	float geometricError;   //!< deviation from FULL detail; 0 at level 0
	float parentError;      //!< the consuming group's error; LODO_ERROR_ROOT at a root
	quint32 parentFirst;    //!< first cluster of the parent group's output range; LODO_NO_PARENT at a root
	quint16 parentCount;    //!< how many clusters that range holds; 0 at a root
	quint8 level;           //!< 0 = full detail
	quint8 reserved0;       //!< 0
	quint16 coneAxis[2];    //!< octahedral 16:16 of the normal-cone axis; (0,0) when CONE_OPEN
	float coneCos;          //!< cosine of the cone's half angle; -1 when CONE_OPEN
	/*! How many FULL-DETAIL triangles this cluster's subtree covers (at level 0,
	 *  its own `triangleCount`). The cut's rows must sum to the mesh's level-0
	 *  triangle count exactly -- a partition invariant a reader can check from
	 *  the file alone, without reconstructing geometry. */
	quint32 sourceTriangles;
	quint32 reserved1;      //!< 0
};

//! Material entry, 16 bytes.
struct LodoMaterial
{
	quint8 arrayClass;      //!< 0 = 256^2, 1 = 128^2
	quint8 arraySet;
	quint16 layer;          //!< < 2048, or LODO_NO_LAYER
	quint8 family;          //!< LodoFamily
	quint8 alphaThreshold;  //!< 0 = opaque
	quint8 flags;           //!< LodoMaterialFlags
	quint8 reserved;
	float emissiveScale;
	quint32 lodmStringOffset;   //!< into the string blob
};

//! Base entry, 32 bytes.
struct LodoBase
{
	quint32 formId;
	quint32 modelStringOffset;  //!< the base's own near MODL (what a card bake photographs)
	quint16 rep[4];             //!< per MNAM slot: a mesh index, or LODO_NO_MESH
	quint16 cardLayer;          //!< low 11 bits layer, high 5 bits set; LODO_NO_CARD
	quint16 flags;              //!< LodoBaseFlags
	float boundRadius;          //!< at scale 1, never 0
	/*! v4: the base's FULL-DETAIL triangle count -- the sum of the level-0
	 *  triangle counts of the DISTINCT meshes its `rep` slots name, so a base
	 *  whose four slots all point at one mesh counts that mesh once. It is the
	 *  number a consumer needs to budget a draw before it has opened a single
	 *  cluster row, and it is the number the near-vs-MNAM library ratio is
	 *  measured on. Never 0 for a base in the table: a base with no mesh at all
	 *  is not written (docs 11, deviation 5). */
	quint32 fullTriangles;
	//! v4: what is LEFT of the v3 `crossPx16[4]` -- two screen-size radius steps, 1/16 px (0 = unset).
	quint16 crossPx16[2];
};

#pragma pack( pop )

static_assert( sizeof( LodoVertex ) == 16, "LodoVertex is 16 bytes" );
static_assert( sizeof( LodoMesh ) == 56, "LodoMesh is 56 bytes" );
static_assert( sizeof( LodoCluster ) == 16, "LodoCluster is 16 bytes" );
static_assert( sizeof( LodoClusterLod ) == 48, "LodoClusterLod is 48 bytes" );
static_assert( sizeof( LodoMaterial ) == 16, "LodoMaterial is 16 bytes" );
static_assert( sizeof( LodoBase ) == 32, "LodoBase is 32 bytes" );

//! The 256-byte header, decoded. Offsets are the file's (docs 3).
struct LodoHeader
{
	quint32 version = LODO_VERSION;
	quint32 flags = LODO_FLAG_VERTEX_V1;
	quint32 headerCrc32 = 0;
	quint64 pluginCorpusHash = 0;
	quint64 objectCorpusHash = 0;
	quint64 modelCorpusHash = 0;
	quint64 cardCorpusHash = 0;
	QString worldspaceEdid;
	quint32 baseCount = 0, meshCount = 0, clusterCount = 0, materialCount = 0, vertexCount = 0;
	quint32 maxClustersPerMesh = 0;
	quint16 clusterMaxTris = LODO_CLUSTER_MAX_TRIS;
	quint16 vertexStride = LODO_VERTEX_STRIDE;
	quint32 stringBytes = 0;
	quint64 offBases = 0, offMeshes = 0, offClusters = 0, offMaterials = 0;
	quint64 offLocalIndices = 0, offVertices = 0, offStrings = 0;
	//! v3, file offset 0xC0: the parallel ladder table, `clusterCount` rows of 48 B.
	quint64 offClusterLods = 0;
	quint32 clusterLodStride = 48;      //!< v3, 0xC8; a reader that knows 48 refuses anything else
	quint8 levelMax = 0;                //!< v3, 0xCC: the deepest `level` in the file
	quint8 ladderGroup = 0;             //!< v3, 0xCD: the grouping target the ladder was built at
	/*! v4, 0xD0: how many bases in this library carry a CARD (`cardLayer` is
	 *  not LODO_NO_CARD). The `.lodi` reader pairs it with the aggregate table
	 *  and a consumer sizes its card draw list from the header alone. It is
	 *  0 on a bake that links no card arrays; a `--native --impostors --arrays`
	 *  bake links them (lane CARDLINK1, `lodgenNativeLinkCards`). The reader
	 *  RECOUNTS it from the base rows and refuses a mismatch by name. */
	quint32 cardCount = 0;
	/*! v5, 0xD4: rows in the colour stream -- the vertex ranges of the meshes
	 *  flagged LODO_MESH_VERTEX_COLOUR, in mesh-table order. 0 = no stream. */
	quint32 colourVertexCount = 0;
	//! v5, 0xD8: offset of the colour stream, the LAST payload; 0 exactly when the count is 0.
	quint64 offColours = 0;
	quint32 indexCrc32 = 0;
	quint64 fileBytes = 0;
	/*! v2, file offset 0xB8. FNV-1a 64 over the LOAD ORDER that produced this
	 *  file: for each plugin of `EsmWorld`'s comma-separated list, IN THAT
	 *  ORDER, the lower-cased base file name's UTF-8 bytes, then its byte size
	 *  as a little-endian u64. A reader gets it from the header alone, without
	 *  parsing a table (docs 3, 8). */
	quint64 loadOrderHash = 0;
};

//! The whole library in memory: what the writer takes and the reader gives.
struct LodoLibrary
{
	quint32 flags = LODO_FLAG_VERTEX_V1;
	quint64 pluginCorpusHash = 0;
	quint64 objectCorpusHash = 0;
	quint64 modelCorpusHash = 0;
	quint64 cardCorpusHash = 0;
	quint64 loadOrderHash = 0;          //!< v2, header 0xB8
	/*! v4, the two ladder knobs. They are BUILD settings, not file fields: the
	 *  file records their consequences (the refusal counts, `levelMax`) and not
	 *  the knobs themselves, so a reader never has to trust them. */
	bool ladderFoliage = LODO_LADDER_FOLIAGE_DEFAULT;
	float silhouetteMin = LODO_SILHOUETTE_MIN_DEFAULT;
	QString worldspaceEdid;
	std::vector<LodoBase> bases;
	std::vector<LodoMesh> meshes;
	std::vector<LodoCluster> clusters;
	std::vector<LodoClusterLod> clusterLods;    //!< v3, exactly one row per cluster
	std::vector<LodoMaterial> materials;
	std::vector<quint8> localIndices;   //!< 48 per cluster
	std::vector<LodoVertex> vertices;
	/*! v5: one RGBA8 per library vertex, PARALLEL to `vertices` (R in the low
	 *  byte), LODO_COLOUR_NONE where no coloured shape gave one. Only the rows of
	 *  meshes flagged LODO_MESH_VERTEX_COLOUR reach the file; the reader fills
	 *  the rest with LODO_COLOUR_NONE. Empty is accepted by the writer as "all
	 *  none" for libraries built by hand. */
	std::vector<quint32> colours;
	QByteArray strings;                 //!< NUL-terminated UTF-8, offset 0 = ""

	//! Append a string, returning its offset; "" is always offset 0.
	quint32 addString( const QString & s );
	//! The string at an offset (empty when out of range).
	QString stringAt( quint32 off ) const;
};

/* ---- the packing contract, shared by the writer, the reader and the tests ---- */

//! Octahedral 12:12 normal -> 24 bits; `n` must be unit length.
quint32 lodoPackOct12( const float n[3] );
void lodoUnpackOct12( quint32 packed, float n[3] );
/*! v3: the same octahedral mapping at 16 bits an axis, for the normal cone. The
 *  cone's half-angle is stored as an exact f32 cosine, so the only quantisation
 *  the cone carries is the axis, and the writer WIDENS the stored cosine by the
 *  axis's own round-trip error -- the stored cone therefore contains every face
 *  normal of its cluster as DECODED, which is what the gate asserts. */
void lodoPackOct16( const float n[3], quint16 out[2] );
void lodoUnpackOct16( const quint16 in[2], float n[3] );
/*! Tangent as a roll angle about the normal: bits 0-6 the angle in 128 steps
 *  from a reference direction derived from the normal alone (so the decoder
 *  needs nothing but the unpacked normal), bit 7 handedness (0 when the
 *  bitangent is cross(normal, tangent)). */
quint8 lodoPackTangent( const float n[3], const float t[3], bool flipHanded );
void lodoUnpackTangent( const float n[3], quint8 packed, float t[3], bool * flipHanded );
//! u16 into [lo, lo + extent]; extent 0 packs to 0 and unpacks to lo.
quint16 lodoQuantU16( float v, float lo, float extent );
float lodoDequantU16( quint16 q, float lo, float extent );
//! FNV-1a 64, the hash every corpus field in both headers uses.
quint64 lodoFnv1a64( const void * p, size_t n, quint64 h = Q_UINT64_C( 0xCBF29CE484222325 ) );
//! `lodoIdentity`: FNV-1a 64 over headerCrc32, modelCorpusHash, objectCorpusHash (never an XOR).
quint64 lodoIdentityOf( quint32 headerCrc32, quint64 modelCorpusHash, quint64 objectCorpusHash );

/* ---- building a mesh row ---- */

//! One shape of a source LOD model, in MODEL space, plain arrays.
struct LodoSrcShape
{
	std::vector<float> pos;     //!< 3 per vertex
	std::vector<float> nrm;     //!< 3 per vertex, unit
	std::vector<float> tan;     //!< 3 per vertex, unit
	std::vector<float> uv;      //!< 2 per vertex
	std::vector<quint8> sway;   //!< 1 per vertex (0 when rigid); may be empty = all 0
	/*! 1 per vertex, the model's own self-occlusion, 255 = open. EMPTY means
	 *  "cast it": lodoAppendMesh casts it over the model's own triangles with
	 *  the chunk bake's rays (src/lodgenao.h) and writes it to `selfAO`, which
	 *  was 255 everywhere from v1 to 2026-09-18 (bungo: "we needed vertex AO
	 *  bakes for the lod objects"). A caller that has its own fills it. */
	std::vector<quint8> ao;
	/*! v5: 4 per vertex, R G B A as the source stores them. EMPTY unless the
	 *  source shape has BOTH a vertex-colour channel and the Vertex_Colors shader
	 *  flag -- the game's own condition for applying it (bungo, W4 ruling). */
	std::vector<quint8> rgba;
	//! v5: the source shader also sets Vertex_Alpha, so A is an opacity factor (meaningful with `rgba` only).
	bool vertexAlpha = false;
	std::vector<quint32> tris;  //!< 3 per triangle
	quint16 materialId = 0;
};

/*! What one `lodoAppendMesh` call measured about the mesh it just wrote. Every
 *  field is written on every call (the written-AND-moves rule); the gate is
 *  `tests/spells/lodgen_native_fields.py` over the per-mesh report file. */
struct LodoMeshStats
{
	quint32 triangles = 0;
	quint32 srcVertices = 0;        //!< vertices the source shapes carried
	float selfAoMean = 1.0f;        //!< mean of the written selfAO bytes / 255 (1.0 = nothing occluded)
	quint32 selfAoDark = 0;         //!< emitted vertices whose selfAO < 128
	quint32 emittedVertices = 0;    //!< library vertices this mesh added (cluster copies included)
	float acmrBefore = 0.0f;        //!< meshopt_analyzeVertexCache ACMR, source order, triangle-weighted
	float acmrAfter = 0.0f;         //!< the same after meshopt_optimizeVertexCache
	float atvrBefore = 0.0f;        //!< meshopt_analyzeVertexFetch overfetch, source order
	float atvrAfter = 0.0f;         //!< the same after meshopt_optimizeVertexFetchRemap
	/*! The SHADOW-CASTER invariant (bungo 2026-09-11 08:3x, "a far shadow cast
	 *  by a LOD tower behind me will cover the area I'm at"): an edge used by
	 *  exactly one triangle, counted over the WELDED topology (vertices keyed
	 *  by their quantised library position, so the count is comparable across
	 *  the shape split and the cluster copies). `boundaryEmitted` must never
	 *  exceed `boundarySource`: a decimation or a lost triangle opens the
	 *  silhouette and the shadow leaks. */
	quint32 boundarySource = 0;
	quint32 boundaryEmitted = 0;

	/* ---- v3, the ladder (lane NATIVE1b) ---- */
	quint32 clustersL0 = 0;         //!< level-0 clusters this mesh emitted
	quint32 clustersLadder = 0;     //!< clusters at level >= 1
	quint8 levelCount = 1;          //!< 1 = no level above 0
	quint32 groupsFormed = 0;       //!< simplification groups that produced a level
	/*! Groups the ladder REFUSED to form, by reason, so a mesh with no ladder
	 *  says why instead of being silently absent (CONSTITUTION 10: a refusal
	 *  states its reason in words; here, in counters the report prints). */
	quint32 groupsRefusedSmall = 0;     //!< at or below LODO_LADDER_MIN_TRIS triangles
	quint32 groupsRefusedNoCut = 0;     //!< the simplifier removed no triangle
	quint32 groupsRefusedFlatErr = 0;   //!< the error did not grow above the children's
	/*! The SHADOW-CASTER refusal, at the ladder's own level (bungo 2026-09-11
	 *  08:3x). A simplified group whose boundary-edge count EXCEEDS its input's
	 *  has opened a hole -- measured on the nine-chunk region, two of fourteen
	 *  such meshes went from a WATERTIGHT level 0 to four and eight boundary
	 *  edges, which is a closed building with a hole in it and a far shadow
	 *  leaking through. The group is refused and its clusters stay roots. */
	quint32 groupsRefusedSilhouette = 0;
	/*! v4 (lane NATIVE1c): the two refusals this lane added, counted BY NAME
	 *  beside the four v3 ones so a mesh with no ladder always says which rule
	 *  stopped it.
	 *
	 *  `groupsRefusedFoliage` counts the level-0 clusters of an alpha-tested
	 *  TREE material that the ladder never tried to group at all (the census
	 *  word `ladder-refused: foliage N`); a region with no tree reads 0, which
	 *  is the floor that proves the counter MOVES.
	 *
	 *  `levelsRefusedSilhouette` counts LEVELS -- not groups -- thrown away
	 *  because the cut at that level kept less than `silhouetteMin` of level
	 *  0's outline. `silhouetteWorst` is the smallest kept ratio in this mesh,
	 *  1.0 when the mesh has no level above 0. */
	quint32 groupsRefusedFoliage = 0;
	quint32 levelsRefusedSilhouette = 0;
	float silhouetteWorst = 1.0f;
	quint32 errorsExact = 0;        //!< groups whose error was measured against FULL detail
	quint32 errorsBounded = 0;      //!< groups that took the conservative chain bound
	float maxError = 0.0f;          //!< the largest `geometricError` in the mesh
	quint32 weldedVertices = 0;     //!< vertices after the position weld the ladder simplifies on
	quint32 weldUvConflicts = 0;    //!< welds that merged vertices whose UVs differ by > 1/256
	//! The boundary-edge count of the COARSEST level, against `boundarySource`.
	quint32 boundaryCoarsest = 0;
};

/*! Append one model as a mesh row: the AABB and UV rect from every shape,
 *  the shapes partitioned into clusters of <= 16 triangles and <= 48 vertices
 *  (whichever cap binds first closes the cluster; cluster-local vertices are
 *  copied, so a vertex on a cluster boundary is duplicated), clusters in
 *  (materialId, first triangle) order. Refuses, naming the shape, on a
 *  triangle index past the shape's vertices, an empty shape, or a library
 *  past the u16 mesh index. `name` is what the refusal quotes.
 *
 *  v2: when `lib.flags` carries LODO_FLAG_CACHE_ORDER each shape's triangles
 *  go through `meshopt_optimizeVertexCache` and its vertices through
 *  `meshopt_optimizeVertexFetchRemap` BEFORE the cluster walk, so a cluster's
 *  16 triangles touch as few distinct vertices as possible (fewer boundary
 *  copies = a smaller vertex blob) and the blob is in first-use order. It is a
 *  PERMUTATION: no vertex is created, none is removed, and the welded topology
 *  is unchanged -- which is what `LodoMeshStats::boundary*` proves.
 *
 *  v3: when `lib.flags` carries LODO_FLAG_LADDER the level-0 clusters of each
 *  MATERIAL are then grouped (meshopt_partitionClusters, target
 *  LODO_LADDER_GROUP) and each group is simplified as one mesh with its border
 *  vertices locked, re-split into clusters of the same caps, and appended as
 *  level 1; the same again over level 1, and so on. Level 0 is emitted first
 *  and UNCHANGED, so the clusters come out in (materialId, level, first
 *  triangle) order, which is the v3 sort law. Every cluster, at every level,
 *  gets its `LodoClusterLod` row. */
bool lodoAppendMesh( LodoLibrary & lib, const std::vector<LodoSrcShape> & shapes,
	const QString & name, quint16 * meshId, QString * error, LodoMeshStats * stats = nullptr );

/*! THE SAME APPEND, SPLIT IN TWO SO IT CAN RUN ON A WORKER (lane PERF1,
 *  2026-09-17).
 *
 *  `lodoAppendMesh` is a pure function of (shapes, name, the library's FLAGS,
 *  LADDER KNOBS and MATERIAL TABLE) -- it reads nothing else out of `lib` -- but
 *  it WRITES into six of the library's tables, so a fan-out over models cannot
 *  call it directly without the workers racing on those tables and on the
 *  indices they hand out.
 *
 *  `lodoStageMesh` runs the identical code against a library of its OWN, seeded
 *  from `like` with exactly the five things the append reads (`flags`,
 *  `ladderFoliage`, `silhouetteMin`, `materials`, `worldspaceEdid`). Nothing it
 *  touches is shared, so any number of workers may run it at once.
 *
 *  `lodoMergeStagedMesh` then appends that one-mesh library into the real one on
 *  ONE thread and fixes up exactly the four fields that point inside it:
 *  `LodoMesh::clusterFirst`, `LodoMesh::modelStringOffset`,
 *  `LodoCluster::vertexBase` + `::meshId`, and `LodoClusterLod::parentFirst`
 *  (left alone at LODO_NO_PARENT). Every other byte is carried across
 *  unchanged.
 *
 *  **Merged in ASCENDING job order, the result is byte for byte what the serial
 *  `lodoAppendMesh` loop produced**, because the append's only dependence on
 *  what came before it is those four offsets. That is not an argument, it is
 *  gate (a) of `tests/spells/lodgen_perf.sh`: the whole `.lodo` of a real
 *  region, hashed, at one thread and at eight.
 *
 *  The way back is the serial loop, which is still there and still the code
 *  that runs at `--threads 1`. */
bool lodoStageMesh( LodoLibrary & staged, const LodoLibrary & like,
	const std::vector<LodoSrcShape> & shapes, const QString & name,
	QString * error, LodoMeshStats * stats = nullptr );
bool lodoMergeStagedMesh( LodoLibrary & lib, const LodoLibrary & staged,
	const QString & name, quint16 * meshId, QString * error );


/* ---- file I/O ---- */

//! Write the library. Two writes of one library are byte-identical.
bool lodoWrite( const QString & path, const LodoLibrary & lib, LodoHeader * headerOut, QString * error );

/*! Read and validate by every rule of the contract; `error` NAMES the field
 *  that failed. `payloadCheck` also recomputes indexCrc32 and walks every
 *  row's ranges, which an opening consumer may skip. `lib` may be null when
 *  only the header is wanted. */
bool lodoRead( const QString & path, LodoHeader * header, LodoLibrary * lib,
	bool payloadCheck, QString * error );

//! Human-readable, one `key value` token per line, for the CLI and the harness.
QStringList lodoDescribe( const LodoHeader & h, const LodoLibrary * lib );

#endif // LODOFILE_H

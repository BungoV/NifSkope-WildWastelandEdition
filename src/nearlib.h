/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef NEARLIB_H
#define NEARLIB_H

#include <QString>
#include <QStringList>

class EsmWorld;

/*! THE NEAR LIBRARY, rung N1 of campaign NEAR (lane NEAR1, 2026-09-26).
 *
 *  An OFFLINE bake of the FULL-DETAIL models of the static placements a
 *  worldspace draws up close -- the base's own MODL, never a LOD model -- into
 *  the far field's own formats: `<ws>.near.lodo` (v7, header flag NEAR) and
 *  `<ws>.near.lodi` (v7 layout; v11 when a placement is Initially Disabled).
 *  Nothing is installed; the output is a library for a renderer to read.
 *
 *  It is a SEPARATE path from `lodgenNativeWrite`: the far writer's code is not
 *  entered, so a far-field bake is byte-identical by construction (and gate G4
 *  measures it anyway).
 *
 *  ELIGIBILITY, per REFR, the first failing rule named (each its own census
 *  count): deleted; no base record; a base type other than STAT or SCOL (doors,
 *  furniture, activators, containers, lights, MSTT, trees ... all by type);
 *  STAT flag Is Marker; a DEST/DSTD subrecord; no MODL; for a SCOL, no parts,
 *  and each part placement judged as a STAT; the model is not in the stack
 *  (`model-missing`) or holds no BSTriShape with vertices and triangles
 *  (`no-geometry`: marker and DummyLOD stand-ins); the model carries a
 *  controller or sequence block (`animated`); no shape survives the shape rules
 *  (`no-drawable-shape`); scale above 15.99988. A SCOL REFR is eligible when one
 *  of its part placements is (else `scol-no-eligible-part`).
 *
 *  SHAPES, excluded by reason: effect shader (BGEM or BSEffectShaderProperty),
 *  alpha BLEND, decal (BGSM or SLSF1), tree / wind animation (SLSF2 Tree_Anim or
 *  BGSM bTree). Everything else is drawn, with a bucket tag on its material:
 *  alpha test (the threshold), two-sided (flag), parallax / env map /
 *  greyscale-to-palette / vertex colour / model-space normals (the v7
 *  `features` byte), legacy vs PBR (family). A BSMeshLODTriShape keeps its
 *  first `LOD0 Size` triangles and the vertices they use: its array also holds
 *  the LOD1 and LOD2 copies, which the engine never draws up close.
 *
 *  Per placement the `.lodi` keeps the REFR form id (+ SCOL part ordinal), the
 *  Initially-Disabled bit (v11) and the Scrappable bit (v9).
 *
 *  PLACEMENT IDENTITY (lane NEAR2, 2026-09-26). `refFormId` is in the bake's
 *  own id space (`ESMFile::mapFormID`: top byte = the plugin's position in the
 *  merged list, a light plugin included), the same space the far `.lodi` uses.
 *  The game numbers light plugins differently (0xFE, then a 12-bit light
 *  slot), so that id is NOT the running game's id once a light plugin sits in
 *  the load order. The cold record's `u16 identity` therefore carries the
 *  plugin's LOAD-ORDER-RESOLVED identity: a full plugin's game index (0..0xFD),
 *  or NEAR_IDENTITY_LIGHT | its light slot (0..0xFFF). The game's form id is
 *  nearGameFormId( refFormId, identity ). The far `.lodi` keeps the stock
 *  bake's chunk index in that word; a near library has no stock bake.
 *  `<ws>.near.plugins.txt` lists every plugin with its identity.
 *
 *  TEXTURE ARRAYS (lane NEAR2). Every texture a material names, in any of its
 *  NEAR_TEX_SLOTS slots, is one layer of one Texture2DArray POOL keyed by
 *  (kind 2d|cube, DXGI format, width, height, mips) -- the five things every
 *  slice of a D3D11 array must share, so a renderer copies each source DDS's
 *  mips into its slice as they are: nothing is decoded or re-encoded (BC data
 *  is GPU-native; re-encoding would lose quality and ship derived game data).
 *  A pool splits every LODO_LAYER_CAP layers (cube: LODO_LAYER_CAP / 6). Pools
 *  holding a diffuse come first, so the `.lodo` material's u8 `arraySet` /
 *  u16 `layer` are its DIFFUSE's pool and layer; the other slots are in
 *  `<ws>.near.texarrays.bin` (little-endian):
 *    header 64 B: 'NTXA', u32 version 1, u32 headerBytes 64, u32 poolCount,
 *      u32 textureCount, u32 materialCount (the .lodo's material order),
 *      u32 slotsPerMaterial, u32 stringBytes, u64 lodoIdentity (the .lodi's),
 *      u32 poolsOffset, u32 texturesOffset, u32 slotsOffset, u32 stringsOffset,
 *      u64 FNV-1a 64 of every byte after the header
 *    pool 16 B: u8 kind (0 2d, 1 cube), u8 dxgi, u8 mips, u8 0, u16 w, u16 h,
 *      u32 firstTexture, u16 layerCount, u16 part (the split index)
 *    texture 8 B: u32 path offset (NUL-terminated `textures\...`, folded),
 *      u16 pool, u16 layer -- pool by pool, layer order
 *    slots: materialCount x slotsPerMaterial u32 = pool << 16 | layer, or
 *      NEAR_SLOT_NONE | a reason (NEAR_SLOT_EMPTY, _MISSING, _UNUSABLE)
 *  Order is a function of the path set alone, so two bakes are byte-identical.
 *
 *  Beside the pair: `<ws>.near.textures.txt` (v2: the pools, every layer's
 *  path, and per material each slot's canonical path and `p<pool>:<layer>`),
 *  `<ws>.near.shapes.txt` (one row per source shape: block, name, triangles,
 *  model-space bounds, kept or the exclusion reason -- the input of gate G2)
 *  and `<ws>.near.refs.txt` (one row per REFR read and per placement -- gate G1),
 *  `<ws>.near.plugins.txt` and `<ws>.near.resources.txt` (lane NEAR2: the
 *  resource stack the bake resolved every model, material and texture from,
 *  last wins -- a viewer given it sees what the bake saw).
 *
 *  `region` (cells x0 y0 x1 y1, inclusive) limits the walk; null = the whole
 *  worldspace (`cellBounds`). Persistent references are taken inside the
 *  region's world rectangle. */
struct NearLibraryOptions
{
	QString outDir;
	QString dataRoot;
	bool haveRegion = false;
	int region[4] = { 0, 0, 0, 0 };
	//! lane NEAR2: the plugins EsmWorld merged, in load order (full paths) -- the identity column's input
	QStringList plugins;
};

//! lane NEAR2: `identity` bit 15 = a light plugin; the low 12 bits are then its light slot
constexpr quint16 NEAR_IDENTITY_LIGHT = 0x8000;

//! lane NEAR2: `<ws>.near.texarrays.bin` (layout in the TEXTURE ARRAYS note above)
constexpr quint32 NEAR_TEXARRAYS_VERSION = 1;
constexpr quint32 NEAR_TEXARRAYS_HEADER = 64;
constexpr quint32 NEAR_TEX_SLOTS = 10;              // BGSM v17+ / BSShaderTextureSet
constexpr quint32 NEAR_SLOT_NONE = 0xFFFF0000U;     // pool 0xFFFF: no layer; the low half says why
constexpr quint32 NEAR_SLOT_EMPTY = 0;              // the slot names no texture
constexpr quint32 NEAR_SLOT_MISSING = 1;            // it names one no stack entry holds
constexpr quint32 NEAR_SLOT_UNUSABLE = 2;           // it resolves, but its DXGI format is unknown (0)

//! lane NEAR2: the running game's form id of a near placement's REFR
inline quint32 nearGameFormId( quint32 refFormId, quint16 identity )
{
	if ( identity & NEAR_IDENTITY_LIGHT )
		return 0xFE000000U | ( quint32( identity & 0x0FFFU ) << 12 ) | ( refFormId & 0x0FFFU );
	return ( quint32( identity & 0xFFU ) << 24 ) | ( refFormId & 0x00FFFFFFU );
}

bool nearLibraryBake( const EsmWorld & world, const NearLibraryOptions & opts, QStringList * report, QString * error );

#endif // NEARLIB_H

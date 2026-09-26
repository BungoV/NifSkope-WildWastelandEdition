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
 *  and each part placement judged as a STAT; the model does not load; the
 *  model carries a controller or sequence block; no shape survives the shape
 *  rules. A SCOL REFR is eligible when one of its part placements is.
 *
 *  SHAPES, excluded by reason: effect shader (BGEM or BSEffectShaderProperty),
 *  alpha BLEND, decal (BGSM or SLSF1), tree / wind animation (SLSF2 Tree_Anim or
 *  BGSM bTree). Everything else is drawn, with a bucket tag on its material:
 *  alpha test (the threshold), two-sided (flag), parallax / env map /
 *  greyscale-to-palette / vertex colour / model-space normals (the v7
 *  `features` byte), legacy vs PBR (family).
 *
 *  Per placement the `.lodi` keeps the REFR form id (+ SCOL part ordinal), the
 *  Initially-Disabled bit (v11) and the Scrappable bit (v9).
 *
 *  Beside the pair: `<ws>.near.textures.txt` (every material's textures by
 *  (DXGI format, width, height) bucket; a material's `arraySet`/`layer` index
 *  its DIFFUSE's bucket part and layer -- nothing is re-encoded),
 *  `<ws>.near.shapes.txt` (one row per source shape: block, name, triangles,
 *  model-space bounds, kept or the exclusion reason -- the input of gate G2)
 *  and `<ws>.near.refs.txt` (one row per REFR read and per placement -- gate G1).
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
};

bool nearLibraryBake( const EsmWorld & world, const NearLibraryOptions & opts, QStringList * report, QString * error );

#endif // NEARLIB_H

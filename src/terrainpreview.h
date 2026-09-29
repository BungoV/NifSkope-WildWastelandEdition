#ifndef TERRAINPREVIEW_H
#define TERRAINPREVIEW_H

#include <QString>

class EsmWorld;

/*! Lane TERRLIVE1: the offscreen preview of the three LOD terrain options
 *  (`lodgen --terrain-preview <spec.json>`).
 *
 *  One program draws every option, so two pictures differ only by option:
 *   - the `.lodl` heights as a mesh;
 *   - the LIVE SPLAT: the `.lodl`'s LTEX slots and 3-bit weights over the
 *     game's landscape textures (LTEX -> TXST through the loaded plugins),
 *     times the `.lodl`'s vertex colour;
 *   - the PROJECTED DECALS: the `.lodg` placements as boxes, each projecting
 *     its `.lodd` picture along the box's local -Z onto the ground's depth;
 *   - the BAKED LEVELS: a `.lodt`'s colour sheet, decoded once to one texture;
 *   - the CROSS-FADE between live and baked, by distance from the eye.
 *
 *  Options: full (the finest FULL level everywhere, no decals), hybrid (live +
 *  decals near, the hybrid's baked level far, cross-faded), dynamic (live +
 *  decals everywhere), live (live splat only), baked (the hybrid's baked level
 *  everywhere, no decals).
 *
 *  GL timer queries around the terrain, decal and lighting passes; a box-layer
 *  count pass; the live/baked crossover by distance. Every number is printed
 *  one `preview:` line each. Returns the process exit code. */
int terrainPreviewRun( const EsmWorld & world, const QString & dataRoot, const QString & specPath );

#endif // TERRAINPREVIEW_H

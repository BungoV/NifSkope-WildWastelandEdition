#ifndef PROBESKY_H
#define PROBESKY_H

/*! THE SKY AND THE SUN IN THE BOUNCE (lane SKY1, 2026-10-02; docs/PRTP_PLAN.md, the SKY1 section).
 *
 *  Outdoors the bake's relight (src/probegi.h) had the placed lights only. This adds, in an
 *  exterior the weather is lighting (Scene mode Lookdev: the one place the viewer has a weather):
 *    sky  per probe and ambient axis: the weather's ambient for a surface facing that axis x the
 *         mean of (sky visibility x glass tint) over the four octants on that axis's side. Every
 *         octant holds pi / 4 of an axis's cosine lobe, so the four weigh the same; in the open
 *         the term IS the weather's ambient. The renderer then lets the grid stand in for the
 *         weather's unshadowed ambient where the grid is valid (cell_lights.glsl, cellGiSky).
 *    sun  per surfel: the sun's color x max(N . toSun, 0) when one ray from the surfel toward the
 *         sun through the bake's soup meets nothing; it leaves through the links like a placed
 *         light's. (The visibility grid knows probe-to-voxel sight lines only.)
 *  Not in it: the sky's own bounce off surfaces, clouds, glass on the sun's way, soft sun edges,
 *  and a closed interior (its octants are all 0 and its relight is unchanged). Lane SKYINT1: an interior
 *  whose cell shows the sky (CELL DATA bit 7) bakes its misses as sky and takes the sky term here,
 *  beside its own ambient (the grid never replaces it); the sun only with bits 8 + 11 (no vanilla cell).
 *
 *  The weather and the hour are the view's own. When they change the bake is relit again
 *  (probeSkyTick, one call a frame; the relight runs once the light has stood still 0.4 s). */

#include <QString>

class QWindow;
struct ProbeSoup;
struct ProbeGiSpec;

struct ProbeSkyLight
{
	bool on = false;                   //!< false: the view is not weather-lit, no sky and no sun
	float amb[6][3] = {};              //!< what a surface facing +X -X +Y -Y +Z -Z takes (linear, the viewport's units)
	float sunTo[3] = { 0, 0, 1 };      //!< unit, toward the sun (or the moon at night)
	float sun[3] = { 0, 0, 0 };        //!< its color (linear)
	QString label;                     //!< weather and hour, for the notes
	bool same( const ProbeSkyLight & o ) const;
};

//! The weather light the view is using now. `on` only in Scene mode Lookdev.
ProbeSkyLight probeSkyLightNow();

/*! A probe's sky irradiance on the six axes, added to E (rgb each).
 *  `tint` may be null (a v3 bake: clear). Reds (WW_CELL_SKY_RED): "novis" every octant sees the
 *  sky, "notint" the glass tint is ignored. */
void probeSkyCube( const float vis[8], const unsigned char ( *tint )[3], const ProbeSkyLight & sky, bool redNoVis,
	bool redNoTint, double E[6][3] );

//! The cell view hands over the exterior it just relit, so a later change of weather relights it.
void probeSkyKeep( const void * nif, const ProbeSoup & soup, const QString & bakeDir, const ProbeGiSpec & spec );
//! One call a frame: relights the kept bake when the view's weather light has changed.
void probeSkyTick( const void * nif, QWindow * view );

#endif // PROBESKY_H

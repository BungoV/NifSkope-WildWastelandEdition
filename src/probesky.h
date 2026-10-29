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
class Scene;
struct ProbeSoup;
struct ProbeGiSpec;

struct ProbeSkyLight
{
	bool on = false;                   //!< false: the view is not weather-lit, no sky and no sun
	float amb[6][3] = {};              //!< what a surface facing +X -X +Y -Y +Z -Z takes (linear, the viewport's units)
	float sunTo[3] = { 0, 0, 1 };      //!< unit, toward the sun (or the moon at night)
	float sun[3] = { 0, 0, 0 };        //!< its color (linear)
	QString label;                     //!< weather and hour, for the notes
	/* lane SKYSTR1: the Sky visibility row (wwCellSkyVis, 0..1): each octant's visibility v counts as 1 + vis (v - 1),
	 * FO4CS's Skylighting strength; 1 leaves v as stored, 0 is the open sky (the red "novis") */
	float vis = 1.0f;
	/* lane SKYPIC1 (WW_CELL_SKYPIC=1, ships off): the sky per direction from the sky the view DRAWS (the dome, the
	 * stars, the clouds; probeSkyPictureQ). picQ[axis][octant] = the integral over that octant of the drawn
	 * radiance x max(axis . dir, 0), so a probe's sky on an axis = sum over octants of visibility x tint x picQ.
	 * A sky of one colour C gives C x pi / 4 on the four octants of the axis's side: SKY1's flat result. */
	bool pic = false;
	double picQ[6][8][3] = {};
	QString picNote;                   //!< what the probes' sky came from, for the notes
	bool same( const ProbeSkyLight & o ) const;
};

//! The weather light the view is using now. `on` only in Scene mode Lookdev.
ProbeSkyLight probeSkyLightNow();

/*! A probe's sky irradiance on the six axes, added to E (rgb each).
 *  `tint` may be null (a v3 bake: clear). Reds (WW_CELL_SKY_RED): "novis" every octant sees the
 *  sky, "notint" the glass tint is ignored. */
void probeSkyCube( const float vis[8], const unsigned char ( *tint )[3], const ProbeSkyLight & sky, bool redNoVis,
	bool redNoTint, double E[6][3] );

//! The cell view hands over the cell it just relit, so a later change of weather relights it (sky = the cell is
//! sky-lit) and a Probe sharing tick relights any cell (lane GPURELIGHT1).
//! Lane LIGHTS5: skyKept false = kept only so the emittance row can flip it live (an interior without sky).
void probeSkyKeep( const void * nif, const ProbeSoup & soup, const QString & bakeDir, const ProbeGiSpec & spec, bool skyKept = true );
/*! lane RTXGI1 M2 (src/rtddgicell.h): the kept cell's soup when it is `nif`'s (else nullptr), the sky its last relight
 *  took (`on` false for an interior that shows none) and the lights that relight takes (the .nifx bulbs in): what the
 *  ray-traced GI volume traces and shades. The pointer lives until the next probeSkyKeep / probeSkyForget. */
struct WwCellLighting;
const ProbeSoup * probeSkyKeptSoup( const void * nif, ProbeSkyLight * sky, WwCellLighting * lights );
//! One call a frame: relights the kept bake when the view's weather light has changed. Lane SKYPIC1: with
//! WW_CELL_SKYPIC=1 it also captures the drawn sky (the view's GL context is current here) when it has changed,
//! and the first capture relights at once (the cell opened before any frame was drawn).
void probeSkyTick( Scene * scene, const void * nif, QWindow * view );
//! fix 10-22: a relight waits on its 0.4 s timer; RelightNow runs it at once (the live driver, before a picture)
bool probeSkyRelightPending();
void probeSkyRelightNow();
/* fix TDFIX2 10-23: forget the drawn-sky capture and the kept relight, so a document opened again in this process opens
 * as at launch (relit from the weather, then the first frame's capture). Kept, the reopen relit its GI from the old
 * capture and the AO decal relight ran on that: 81 px off its own launch on every row after a live reopen. */
void probeSkyForget();

//! lane SKYPIC1: per axis and octant, the integral of the captured sky's radiance x max(axis . dir, 0) d(solid
//! angle) (exact texel solid angles, the cosine at the texel's centre). Reds: "flipz" reads the picture upside
//! down, "nocos" drops the cosine.
struct WwSkyPicture;
void probeSkyPictureQ( const WwSkyPicture & pic, const QString & red, double Q[6][8][3] );

#endif // PROBESKY_H

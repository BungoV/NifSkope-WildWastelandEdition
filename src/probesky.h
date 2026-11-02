#ifndef PROBESKY_H
#define PROBESKY_H

//! THE SKY AND THE SUN IN THE BOUNCE:
//! Outdoors the bake's relight (src/probegi.h) had the placed lights only.

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
	/* the Sky visibility row (wwCellSkyVis, 0..1): each octant's visibility v counts as 1 + vis (v - 1),
	 * FO4CS's Skylighting strength; 1 leaves v as stored, 0 is the open sky (the red "novis") */
	float vis = 1.0f;
	// the sky per direction from the sky the view DRAWS (the dome, the stars, the clouds; probeSkyPictureQ).
	// picQ[axis][octant] = the integral over that octant of the drawn radiance x max(axis .
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
//! sky-lit) and a Probe sharing tick relights any cell.
//! skyKept false = kept only so the emittance row can flip it live (an interior without sky).
void probeSkyKeep( const void * nif, const ProbeSoup & soup, const QString & bakeDir, const ProbeGiSpec & spec, bool skyKept = true );
/*! the kept cell's soup when it is `nif`'s (else nullptr), the sky its last relight
 *  took (`on` false for an interior that shows none) and the lights that relight takes (the .nifx bulbs in): what the
 *  ray-traced GI volume traces and shades. The pointer lives until the next probeSkyKeep / probeSkyForget. */
struct WwCellLighting;
const ProbeSoup * probeSkyKeptSoup( const void * nif, ProbeSkyLight * sky, WwCellLighting * lights );
//! One call a frame: relights the kept bake when the view's weather light has changed. Lane SKYPIC1: with
//! WW_CELL_SKYPIC=1 it also captures the drawn sky (the view's GL context is current here) when it has changed,
//! and the first capture relights at once (the cell opened before any frame was drawn).
void probeSkyTick( Scene * scene, const void * nif, QWindow * view );
//! a relight waits on its 0.4 s timer; RelightNow runs it at once (the live driver, before a picture)
bool probeSkyRelightPending();
void probeSkyRelightNow();
/* fix TDFIX2 10-23: forget the drawn-sky capture and the kept relight, so a document opened again in this process opens
 * as at launch (relit from the weather, then the first frame's capture). Kept, the reopen relit its GI from the old
 * capture and the AO decal relight ran on that: 81 px off its own launch on every row after a live reopen. */
void probeSkyForget();

//! per axis and octant, the integral of the captured sky's radiance x max(axis . dir, 0) d(solid
//! angle) (exact texel solid angles, the cosine at the texel's centre). Reds: "flipz" reads the picture upside
//! down, "nocos" drops the cosine.
struct WwSkyPicture;
void probeSkyPictureQ( const WwSkyPicture & pic, const QString & red, double Q[6][8][3] );

#endif // PROBESKY_H

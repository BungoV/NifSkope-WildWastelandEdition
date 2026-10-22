/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTORDRAW_H
#define IMPOSTORDRAW_H

#include "impostoroct.h"

#include <QString>
#include <QStringList>

/*! Vanilla Fallout 4's LOD alpha test as a coverage fraction: NiAlphaProperty
 *  threshold 128 (flags 0x12EC) on every alpha-tested shape of every vanilla
 *  .BTO chunk. The impostor draw's default cut (lane IMPOSTORFIN1). */
constexpr float kImpostorVanillaCut = 128.0f / 255.0f;

class Scene;
struct ImpostorCardSet;
class Vector3;

/* ---------------------------------------------------------------------------
 * Drawing one baked octahedral impostor card.
 *
 * The pass is modelled on `Renderer::drawSkyBox` (src/gl/renderer.cpp:1760),
 * which is the precedent in this tree for "a program selected BY NAME, four
 * vertices of its own, no shape in the scene graph". A card is that: it is not
 * a NiTriShape, it has no NIF behind it, and it must be able to be drawn for a
 * `.lodm` that was opened on its own with no model in the view at all.
 *
 * WHAT THIS DOES NOT DO, and the single seam where the missing half goes.
 * bungo, 2026-09-19: "we can't display the impostors fully though with all the
 * textures, not until we recreate lighting in nifskope from [Fallout 4] and
 * fo4cs pbrm". So: the colour sheet and the baked normal sheet are LIT, by the
 * viewer's existing lighting and nothing else, and the baked AO multiplies the
 * viewer's ambient term. The mask sheet (GSAOS or RMAOS) and the emissive sheet
 * are LOADED, BOUND and VIEWABLE as debug channels, and not one of their
 * channels reaches the lit result. The seam is one block in
 * `res/shaders/impostor_oct.frag` marked `IMPOSTOR_MATERIAL_SEAM`; there is no
 * stub shading anywhere and no invented specular.
 * ------------------------------------------------------------------------- */

namespace ImpostorDraw
{

//! What the viewer asks of one card. Every field is a thing a person can see
//! change, and each is owed a menu row or an env switch, not a hidden default.
struct Options
{
	//! The spec's pixel depth offset (spec 284..293): `gl_FragDepth` from the
	//! height channel, so the card intersects terrain and meshes as a VOLUME
	//! rather than as a plane. OFF draws the flat-sticker card, which is what
	//! the gate's before/after compares against.
	bool depthOffset = true;
	//! The ghost-free frame blend: each of the three frames is sampled at the
	//! point its OWN height channel puts the surface at, not at the card
	//! plane, so the three do not disagree by a parallax and smear.
	bool heightBlend = true;
	//! THE CUT'S COVERAGE (lane IMPOSTORTEAR1, 2026-09-23), the shader's
	//! `cutRule`. 0 = the STIPPLED cut that stops the tear bungo called "like
	//! somebody ripped out a piece of paper" (a frame's own silhouette counts
	//! where a card-texel noise is below min(1, 2 x its weight), the 3-frame
	//! mean as the floor); 1 = the way back, the 3-frame weighted mean the cut
	//! read before; 2 = the strongest frame alone, which pops and exists only
	//! as impostor_draw.sh row 18's red control. `WW_IMPOSTOR_CUT=mean|strong`
	//! forces 1 or 2 for every draw.
	//! THIS IS THE BLENDED SLIDER'S CUT. At the crisp end (the default, one
	//! frame) the cut is rule 2, the strongest frame, BY NAME -- bungo's R5
	//! (2026-09-24 21:1x: "crisp cards -- N8, crisp cut, slider crisp end");
	//! see Resolved::cutRule.
	int cutRule = 0;
	/*! THE SLIDER, crisp (0) to smooth (1) -- docs/FO4CS_IMPROVED_LOD_PLAN.md's
	 *  slider contract, bungo's rulings of 2026-09-23 (lane IMPOSTORDEPTH2):
	 *    * 0, the CRISP end and THE DEFAULT, is FLAT SNAP (ruled 2026-09-23
	 *      13:1x): the nearest baked frame alone at full weight, no blend,
	 *      NOT moved by its depth -- the same picture as `heightBlend =
	 *      false`. It still writes its depth (`depthOffset`). It jumps when
	 *      the nearest frame changes and fails the tear bar; both are ruled
	 *      acceptable. Moving a lone frame by its depth thinned the trunk
	 *      (T1 203 of 360 views at el 20): it uncovers what that frame never
	 *      photographed and no second frame fills it.
	 *    * 1, the SMOOTH end, is the three-frame blend under the STIPPLED cut
	 *      (`cutRule` 0) with a 16-step depth search (`kSmoothSearchSteps`).
	 *    * between, the three frames with their weights SHARPENED, w^(1/s)
	 *      renormalised, under the same stipple and search. The middle is
	 *      wired, not measured.
	 *  `WW_IMPOSTOR_SLIDER=<0..1>` forces it for every draw. There is no menu
	 *  row for it yet: no impostor option has one (env switches are the
	 *  viewer's convention for the card drawer). */
	float slider = 0.0f;
	//! THE DEPTH SEARCH (lane IMPOSTORDEPTH1), the shader's
	//! `depthSearchSteps`: each frame marches the view ray through the card's
	//! depth in N steps and refines once onto the first surface its height
	//! channel puts there, so the frames agree where a thin trunk is. Costs up
	//! to 2N + 1 sheet reads per frame. Needs `heightBlend`. -1 (the default)
	//! = the SLIDER's own number (see `slider`); N >= 0 forces N.
	//! `WW_IMPOSTOR_SEARCH=N` forces N for every draw -- a HARNESS override,
	//! not a user setting.
	int depthSearchSteps = -1;
	//! THE DEPTH-MOVED SNAP (IMPOSTORDEPTH1), a HARNESS override since the
	//! crisp end became the flat snap (IMPOSTORDEPTH2): the nearest frame
	//! alone at full weight, MOVED by its height parallax and the search --
	//! unlike the crisp end, which draws that frame flat on the card plane.
	//! `WW_IMPOSTOR_SNAP=1` forces it for every draw. Not a user setting: it
	//! fails the trunk bar (tests/spells/impostor_trunk.sh).
	bool snap = false;
	/* The coverage is ALWAYS decoded per texel, then filtered (IMPOSTORDEPTH1's
	 * fix, made the only path by bungo's ruling, lane IMPOSTORDEPTH2): there is
	 * no field and no switch; `WW_IMPOSTOR_COVFILTER` is retired. */
	//! Baked AO into the viewer's AMBIENT term only. Never into the direct
	//! term -- an AO that darkens sunlight is a different, wrong picture.
	bool bakedAo = true;

	//! 0 = the lit card. 1..13 = one channel, unlit and unfiltered, per the
	//! table in `res/shaders/impostor_oct.frag` (13 = the normal the light is
	//! dotted with, in view space -- lane IMPOSTORLIGHT1).
	int debugChannel = 0;

	/*! The coverage FRACTION below which a texel is not drawn. Negative means
	 *  THE DEFAULT, which is VANILLA'S LOD ALPHA TEST: `kImpostorVanillaCut`,
	 *  128/255 (lane IMPOSTORFIN1, 2026-09-22, on bungo's word "what vanilla
	 *  game had worked pretty well"). Census of the whole vanilla corpus
	 *  (scratchpad/impostorfin1_20260922/cutoff_census.txt): every
	 *  alpha-tested shape in every Fallout 4 .BTO chunk -- 461 of 980, the rest
	 *  opaque, no other value anywhere -- carries NiAlphaProperty flags 0x12EC
	 *  (test ON, GREATER, NO blend) at threshold 128. The per-model tree LOD
	 *  sources vary (105, 80, 82, 127, 90, 100, 65, 110, 45 beside 128); the
	 *  drawn chunks do not. Vanilla's alpha is the leaf texture's coverage, so
	 *  its 128 is a cut on the coverage FRACTION at one half, which is the
	 *  domain this number is in.
	 *
	 *  The text below is the older rule this default replaced, kept because
	 *  its consequence still holds: the set's extents were measured at
	 *  `floor`, so a card cut at one half draws INSIDE its own `halfW` /
	 *  `halfH` (IMPOSTORFIX4 measured 0.50 below 0.063 on all five subjects).
	 *
	 *  The set's `coverage.test` (128) is an alpha in the ENCODED domain, and
	 *  the bake puts every covered texel at `base` (160) or above, so testing
	 *  the encoded alpha at 128 admits exactly the texels the bake counted as
	 *  covered when it measured `halfW`, `halfH` and every `frameOffset` --
	 *  that is, everything at or above `floor`. Decoded, that same cut is
	 *  `floor / 255` = 0.063, NOT 0.5: a card tested at 0.5 of the fraction
	 *  draws a silhouette narrower than the one its own extents describe, and
	 *  the tree changes size at the LOD transition (lodgen.cpp:3036..3041).
	 *
	 *  A set that declares no contract falls back to spec 311..316's 0.5 for a
	 *  full crown. Either way the viewer can override for a picture, which is
	 *  what `WW_IMPOSTOR_ALPHA` is for. */
	float alphaThreshold = -1.0f;

	/*! The placement's scale. A chunk's manifest gives each reference a `scale`
	 *  on its object row and the `C` line's extents are the UNSCALED ones off
	 *  the `.lodm` (measured on the Commonwealth chunk -32 16: every `C` line
	 *  for 000531b3 carries 135.314 x 360.837 while the references carry
	 *  1.1543, 0.9118, 0.7857 ...), so the two have to be multiplied here or
	 *  every tree in a chunk is drawn the same size.
	 *
	 *  It scales the quad, the centre offset and the depth span together --
	 *  the depth span is a distance in the same units and a card scaled in two
	 *  of the three would be a wedge. 1 is a set drawn at its own size, which
	 *  is what the `.lodm` preview and every gate row uses. */
	float worldScale = 1.0f;

	//! Sway. A card is four vertices, so there is nothing to displace: this is
	//! a UV shear weighted by the baked sway weight, which is an APPROXIMATION
	//! and is named one. Every gate runs at 0 and no picture taken with it on
	//! may be quoted as "the sway bake, drawn".
	float swayAmplitude = 0.0f;
	float swayPhase = 0.0f;

	//! THE RED CONTROL. Frame indices are permuted before the lookup, so the
	//! card draws the wrong view of itself. The silhouette IoU must collapse;
	//! if it does not, the IoU was never measuring frame selection and step 5
	//! of `tests/spells/impostor_draw.sh` proves nothing.
	bool shuffleFrames = false;

	/*! Which way round the grid is read. `SpecLiteral` (the default) is the
	 *  spec's own mapping and what the repaired bake produces. `AsBaked` reads
	 *  a LEGACY set, from before the azimuth repair of 2026-09-19, whose frames
	 *  sit 180 degrees of azimuth away from where the spec puts them.
	 *
	 *  A caller that has a set in hand should not choose: pass
	 *  `set.legacyBake() ? AsBaked : SpecLiteral` and let the set's own token
	 *  say. `drawCard` does exactly that unless `forceConvention` overrides it,
	 *  which is for the gate's red control and for a person inspecting a set by
	 *  hand -- nothing else. */
	ImpostorOct::Convention convention = ImpostorOct::Convention::SpecLiteral;

	//! False (the default) = `convention` is ignored and the SET's own token
	//! decides. True = `convention` wins, and the log says it was forced.
	bool forceConvention = false;
};

/*! Draw one card, at `worldOffset` from the scene's origin.
 *
 *  Returns false, without drawing and without leaving a program bound, when
 *  the set is not ok, the program is missing, or the colour sheet did not
 *  bind. `why` (when given) says which, by name: a card that silently does not
 *  appear is the failure mode this whole lane exists to end.
 *
 *  Call it between the scene's own passes with the depth buffer live. It
 *  leaves the GL state it found: blend off, depth test on, depth writes on,
 *  cull face off (a card is two-sided by construction -- the frame nearest the
 *  camera can be the far side of the grid at a grazing angle).
 */
bool drawCard( Scene * scene, const ImpostorCardSet & set,
				const Vector3 & worldOffset, const Options & opt, QString * why = nullptr );

//! The slider's two ends, in depth-search steps (lane IMPOSTORDEPTH2).
constexpr int kCrispSearchSteps  = 0;
constexpr int kSmoothSearchSteps = 16;

/*! What `drawCard` will actually do with `opt` once the environment's
 *  overrides are applied: ONE function, so the harness's log line and the
 *  draw cannot disagree. */
struct Resolved
{
	float slider = 0.0f;      //!< 0 crisp .. 1 smooth, after WW_IMPOSTOR_SLIDER
	bool  snap = true;        //!< one frame: the crisp end (or WW_IMPOSTOR_SNAP / Options::snap)
	bool  parallax = false;   //!< the frames are moved by their depth (shader useHeightBlend);
	                          //!< false at the crisp end = the FLAT snap
	int   frameCount = 1;     //!< 1 snap or flat, 3 blended
	int   searchSteps = 0;    //!< after WW_IMPOSTOR_SEARCH / Options::depthSearchSteps
	float sharpen = 1.0f;     //!< the weights' exponent, 1/slider between the ends
	int   cutRule = 2;        //!< the cut drawn: 2 (strongest frame) at the crisp end,
	                          //!< Options::cutRule once blended; WW_IMPOSTOR_CUT still wins
	bool  sliderForced = false, snapForced = false, searchForced = false;
};
Resolved resolve( const Options & opt );

/*! What `drawCard` chose for the camera the scene currently has, without
 *  drawing anything: the direction in card space, the grid cell, the three
 *  frames and their weights. This is what the harness's `map` mode prints and
 *  what `tests/spells/impostor_oct_ref.py` is compared against -- the answer
 *  the REAL draw path computes, not a second copy of the arithmetic.
 */
QStringList describeSelection( Scene * scene, const ImpostorCardSet & set,
								const Vector3 & worldOffset, const Options & opt );

/*! Make the sheets of a loosely-baked set reachable by the texture cache.
 *
 *  `TexCache` resolves every texture through the archive/loose-folder stack
 *  (`GameManager::get_file`); it has NO absolute-path fallback, so a set baked
 *  into a scratch folder outside the game data binds nothing and draws a blank
 *  card. This walks up from the `.lodm` looking for the folder that has the
 *  `textures\` subtree the sheets' own game paths are relative to, registers it
 *  for `scene->nifModel` and returns what it registered.
 *
 *  Returns an empty string when there is no such ancestor, which is a thing the
 *  caller must PRINT rather than swallow: the sheets are somewhere the viewer
 *  cannot reach and the fix is to bake under a data folder.
 */
QString registerLooseSheets( Scene * scene, const ImpostorCardSet & set );

} // namespace ImpostorDraw

#endif // IMPOSTORDRAW_H

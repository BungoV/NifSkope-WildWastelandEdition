/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTORCARD_H
#define IMPOSTORCARD_H

#include <QString>
#include <QStringList>
#include <QVector>

/* ---------------------------------------------------------------------------
 * A baked impostor set, as a DRAWER needs it.
 *
 * `src/io/lodmfile.h` reads the `.lodm` envelope and hands back the payload as
 * JSON. This file turns the `card` (and `cardArray`) block of that payload into
 * the handful of numbers a draw pass actually uses, refuses a set it cannot
 * draw BY NAME rather than drawing a wrong one, and resolves the four sheets to
 * something openable.
 *
 * There is no second `.lodm` parser here, and there must never be one: the
 * envelope, the keys and the family spelling belong to `lodmfile.h` and
 * `docs/LODGEN_LODM_FORMAT.md`.
 *
 * WHY THE SHEET PATHS NEED WORK AT ALL. A `.lodm` names its textures as GAME
 * paths (`Data\FO4CSLOD\Cards\<id>_oct_d.DDS`, spec 187..191), which is right
 * for FO4CS and wrong for a person who has just baked a set into a folder and
 * wants to look at it. So each sheet gets TWO candidates -- the game path,
 * for the resource stack, and the sibling beside the `.lodm` on disk -- and the
 * loader says WHICH ONE it opened. A picture taken off textures nobody can
 * name is not evidence.
 * ------------------------------------------------------------------------- */

//! One sheet of a set: what the material named, where it was actually found.
struct ImpostorSheet
{
	QString gamePath;		//!< as the `.lodm` names it, backslashes and all
	QString localPath;		//!< the sibling beside the `.lodm`, when one exists
	//! What the loader chose. Empty when the sheet is absent, which is legal
	//! for the emissive (spec 350: a set baked before it existed) and for the
	//! mask, and is NOT legal for the colour or the normal.
	QString resolved;
	bool fromLocal = false;	//!< true when `resolved` is the loose sibling
};

//! A card set: one base, one grid, four sheets.
struct ImpostorCardSet
{
	bool ok = false;
	QString error;			//!< why not, when !ok -- always names the file

	QString lodmPath;		//!< what was opened
	bool pbr = false;		//!< family: false legacy (GSAOS), true pbr (RMAOS)
	QString kind;			//!< "card" or "cardArray"

	int oct = 0;			//!< N, the grid side (spec 225); 0 on a ring set
	/*! THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1; bungo 2026-09-23:
	 *  "22.5 degrees per take"). > 0 = the set's `views` V: V frames in ONE row,
	 *  frame v photographed from azimuth 360*v/V at elevation 0 (the aggregate's
	 *  layout, docs/LODGEN_LODM_FORMAT.md 3a). `oct` is then 0, so nothing that
	 *  lays out an N x N grid can take a ring for one; cols()/rows() are the
	 *  sheet's layout either way, and frame (i, j) is at index j*cols() + i. */
	int views = 0;
	int cols() const { return views > 0 ? views : oct; }
	int rows() const { return views > 0 ? 1 : oct; }
	bool ring() const { return views > 0; }
	int frameW = 0, frameH = 0;	//!< one frame's pixels; the sheet is N x this
	int mips = 0;			//!< spec 243: the cap, which a consumer must obey
	float halfW = 0.0f, halfH = 0.0f;	//!< the quad IS the frame (spec 239)
	float center[3] = { 0.0f, 0.0f, 0.0f };
	float depthSpan = 0.0f;	//!< spec 285: 3 x max(bound radius, 1024)
	float emissiveScale = 1.0f;	//!< spec 88..95: the multiple, never in the sheet
	int auxDiv = 1;			//!< spec 617..630: aux sheets at half of each side

	/*! THE COVERAGE CONTRACT, as the set declares it (`card.coverage`, or the
	 *  layer's; written by `lodgen.cpp:3031..3050`, encoded by
	 *  `lodgenaggregate.cpp:122..129`). All three together or none.
	 *
	 *  The colour sheet's alpha is NOT the coverage fraction. The bake remaps a
	 *  covered texel into `[base .. 255]`, so a texel with ANY coverage comes
	 *  back at 160/255 or more, and the fraction is recovered by
	 *      cov = floor + (a - base) x (255 - floor) / (255 - base)
	 *  which is `lodgen.cpp:3033` verbatim. A consumer that averages the RAW
	 *  alpha of three frames and then tests the average is testing an encoded
	 *  number: no single frame's blend weight can carry 160/255 past a 0.5
	 *  test, so the blend discards texels every frame agrees are solid.
	 *  MEASURED 2026-09-19: card coverage collapsed 4 to 16 times at exactly
	 *  the diagonal azimuths, where the weight is spread over three frames, and
	 *  stood up at the cardinals, where one frame carries nearly all of it.
	 *  Decode first, blend the fractions, test the fraction.
	 *
	 *  ABSENT (all zero) is the older contract and is not an error: the alpha
	 *  IS the fraction, floored at 16/255 (`lodgen.cpp:3036..3041`, and the
	 *  same fallback as `lodgenaggregate.cpp:117`). `covOk()` says which. */
	int covFloor = 0, covTest = 0, covBase = 0;
	bool covOk() const { return covBase > covTest && covTest > covFloor && covFloor > 0; }

	/*! THE VIEW CONVENTION TOKEN, verbatim as the set declares it (`card.conv`
	 *  or the layer's `conv`; the bake writes it last on the sidecar's `oct`
	 *  line). `spec1` = frame (i, j) is the view from direction (i, j), the
	 *  spec's own law, which is what the bake produces from 2026-09-19.
	 *
	 *  EMPTY is not "unknown": the token did not exist before that day, so
	 *  every set without one came from the bake whose `rz = 90 - azim` turned
	 *  the azimuth by 180 degrees. Such a set MUST BE RE-BAKED; the preview
	 *  will still draw it, under `Convention::AsBaked`, and says so in its
	 *  notes rather than quietly showing the back of a tree at the front. */
	QString conv;

	//! What `conv` means for a drawer: `SpecLiteral` for `spec1`, `AsBaked` for
	//! a set with no token (or one this build does not recognise, which is the
	//! same refusal to guess).
	bool legacyBake() const { return conv != QLatin1String( "spec1" ); }

	/*! PER-FRAME POSITIONING: two numbers per frame, in MODEL UNITS, along
	 *  THAT frame's own right and up axes (`card.frameOffset`, written by
	 *  `lodgen.cpp:2999..3005`). Frame (i, j) is at index `j*oct + i`, so
	 *  element `2*(j*oct+i)` is its right offset and the next its up offset --
	 *  `frameOffsetOf()` does that arithmetic once so no caller repeats it.
	 *
	 *  WHAT IT IS FOR. `center` is the card's ONE centre and `half` its ONE
	 *  size, because the scale must not change between views. But a tree leans
	 *  one way from the front and the other from the side, so a frame that
	 *  centred every view on `center` would have to pay for the UNION of every
	 *  view's box. The bake instead slides each view's silhouette to its own
	 *  frame's centre and records the slide here, and a drawer slides it back:
	 *  the quad for frame k sits at `center + right*off.x + up*off.y`.
	 *
	 *  IGNORING IT IS NOT NEUTRAL, which is what this lane learned the
	 *  expensive way. lodgen's own note says a reader that skips the key gets
	 *  "a tree that steps sideways at the transition by the offset it skipped",
	 *  and that is the MILD case -- one frame, rigidly displaced. Under the
	 *  three-frame blend the three offsets differ, so the three samples taken
	 *  for one fragment come from three different places on the tree and the
	 *  card disperses into a wide sparse spray. Measured on TreeMapleblasted05
	 *  N=4: offsets reaching 36.6 right and 55.3 up against a `halfW` of 135.3,
	 *  i.e. 13% of a frame, and a silhouette IoU of 0.22 where the single
	 *  nearest frame alone scored 0.31.
	 *
	 *  EMPTY is legal and is the older law: a set baked before per-frame
	 *  positioning had every frame centred on `center`, and zero offsets
	 *  reproduce that exactly. */
	QVector<float> frameOffset;

	/*! Frame (i, j)'s offset along its own right and up axes, or (0, 0) when
	 *  the set declares none or the index is out of range. Out of range
	 *  answers with the older law rather than refusing: a truncated array
	 *  should draw the frames it can describe, not sink the whole set. */
	void frameOffsetOf( int i, int j, float * right, float * up ) const;

	//! cardArray only: which layer of the arrays this set is. -1 for a `card`.
	int layer = -1;

	ImpostorSheet colour, normal, mask, emissive;

	//! Everything the loader wants to be able to say afterwards, one line each:
	//! the family, the grid, the frame, which sheets resolved and from where.
	//! A harness prints this instead of asserting on private state.
	QStringList notes() const;
};

/*! Read a `<id>_oct.lodm` (or a `cardArray` one, with `layer` naming which
 *  layer) into a drawable set.
 *
 *  `layer` is ignored for `kind: "card"`. For `kind: "cardArray"` it selects
 *  the entry of `array.layers`; -1 takes layer 0 and says so in `notes()`.
 *
 *  REFUSES, with the file named, when: the envelope does not parse; `kind` is
 *  neither `card` nor `cardArray`; `oct` is outside the bake's own 2..16; the
 *  frame or the extents are absent or non-positive; `depthSpan` is absent
 *  (without it the height channel has no unit and the card cannot carry depth
 *  at all); or the colour or normal sheet resolves to nothing. It does NOT
 *  refuse a missing mask or emissive -- those are legal absences and the debug
 *  channels say so.
 */
ImpostorCardSet impostorCardLoad( const QString & lodmPath, int layer = -1 );

//! One manifest `C` line (spec 329..333):
//! `C index cx cy cz halfW halfH N depthspan lodm [arrayLodm layer]`.
//! A reader that stops at the tenth token is unaffected by the two extras,
//! which is why they are at the end -- this one reads them when they are there.
struct ImpostorPlacement
{
	bool ok = false;
	QString error;
	int index = -1;				//!< the placement's object index in the chunk
	float center[3] = { 0.0f, 0.0f, 0.0f };
	float halfW = 0.0f, halfH = 0.0f;
	int oct = 0;
	float depthSpan = 0.0f;
	QString lodm;				//!< the set's own `.lodm`, game path
	QString arrayLodm;			//!< the array's, when the card sits in one
	int layer = -1;				//!< its layer there
};

//! Parse one `C` line. `line` may or may not still carry the leading "C ".
ImpostorPlacement impostorParseCLine( const QString & line );

//! Every `C` line of a `<chunk>.bto.manifest.txt`, in file order. Lines that
//! do not parse are RETURNED with `ok` false and their reason, not dropped: a
//! manifest that half-parses is a thing the viewer has to be able to say.
QVector<ImpostorPlacement> impostorReadManifest( const QString & manifestPath, QString * error );

#endif // IMPOSTORCARD_H

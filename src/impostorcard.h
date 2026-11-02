/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef IMPOSTORCARD_H
#define IMPOSTORCARD_H

#include <QString>
#include <QStringList>
#include <QVector>

// A baked impostor set, as a DRAWER needs it:
// `src/io/lodmfile.h` reads the `.lodm` envelope and hands back the payload as JSON.

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
	/*! THE HORIZON RING (bungo 2026-09-23:
	 *  "22.5 degrees per take"). > 0 = the set's `views` V: V frames in ONE row,
	 *  frame v photographed from azimuth 360*v/V at elevation 0 (the aggregate's
	 *  layout, docs/LODGEN_LODM_FORMAT.md 3a). `oct` is then 0, so nothing that
	 *  lays out an N x N grid can take a ring for one; cols()/rows() are the
	 *  sheet's layout either way, and frame (i, j) is at index j*cols + i. */
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

	//! THE COVERAGE CONTRACT, as the set declares it (`card.coverage`, or the layer's; written by
	//! `lodgen.cpp:3031..3050`, encoded by `lodgenaggregate.cpp:122..129`). All three together or none.
	int covFloor = 0, covTest = 0, covBase = 0;
	bool covOk() const { return covBase > covTest && covTest > covFloor && covFloor > 0; }

	//! THE VIEW CONVENTION TOKEN, verbatim as the set declares it (`card.conv` or the layer's `conv`; the bake
	//! writes it last on the sidecar's `oct` line).
	QString conv;

	//! What `conv` means for a drawer: `SpecLiteral` for `spec1`, `AsBaked` for
	//! a set with no token (or one this build does not recognise, which is the
	//! same refusal to guess).
	bool legacyBake() const { return conv != QLatin1String( "spec1" ); }

	//! PER-FRAME POSITIONING: two numbers per frame, in MODEL UNITS, along THAT frame's own right and up axes
	//! (`card.frameOffset`, written by `lodgen.cpp:2999..3005`).
	QVector<float> frameOffset;

	//! Frame (i, j)'s offset along its own right and up axes, or (0, 0) when the set declares none or the index
	//! is out of range.
	void frameOffsetOf( int i, int j, float * right, float * up ) const;

	//! cardArray only: which layer of the arrays this set is. -1 for a `card`.
	//! On a lodm 3 array it is the slice of the set's FAMILY sheets (colour, mask, emissive).
	int layer = -1;
	//! lodm 3 only: the slice of the shared `_n` sheet (the layer's index in the .lodm); -1 = `layer`
	int normalLayer = -1;
	int nLayer() const { return normalLayer >= 0 ? normalLayer : layer; }

	ImpostorSheet colour, normal, mask, emissive;

	//! Everything the loader wants to be able to say afterwards, one line each:
	//! the family, the grid, the frame, which sheets resolved and from where.
	//! A harness prints this instead of asserting on private state.
	QStringList notes() const;
};

//! Read a `<id>_oct.lodm` (or a `cardArray` one, with `layer` naming which layer) into a drawable set.
ImpostorCardSet impostorCardLoad( const QString & lodmPath, int layer = -1 );

//! One manifest `C` line (spec 329..333): `C index cx cy cz halfW halfH N depthspan lodm [arrayLodm layer]`.
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

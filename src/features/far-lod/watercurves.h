/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WATERCURVES_H
#define WATERCURVES_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include <functional>

class WaterMarkDoc;

/*! @file watercurves.h  The EDITABLE model behind the water window (lane
 *  WATER5): curves with any number of points, pins, dye pins, per-body
 *  overrides and imported flow-map layers -- and the two files they live in.
 *
 *  bungo, 2026-09-10, verbatim: *"curves you can draw in nifskope, that can
 *  have as many connection points as you want. Then you solve the rest with a
 *  button to fill in the gaps, something like a simulation"*; *"allow me to
 *  save the curves as some type of a file"* / *"so you can load them again
 *  and edit in nifskope"*.
 *
 *  TWO HOMES, ONE SOURCE.  The curves are saved as `<Worldspace>.water.json`
 *  beside the land file (versioned, world coordinates, human-editable,
 *  shareable between mods and regenerations), and they are MIRRORED into the
 *  `.lodl`'s own stroke store when the land file is saved, which stays the
 *  baked copy the generator's consumers read.  Loading the json onto any land
 *  file of the same worldspace re-creates the curves editable, and Solve
 *  re-derives the planes from them -- the json never carries a plane.
 *
 *  This file knows nothing about widgets: the window (waterwindow.cpp) edits
 *  a `WaterCurveDoc`, and everything here is what the harness measures.
 */

//! One point of a curve, WORLD units, with its speed weight (1 = the curve's speed).
struct WaterCurvePoint
{
	float x = 0.0f;
	float y = 0.0f;
	float w = 1.0f;
};

/*! One curve.  `kind` uses the stroke store's own numbers (WaterStroke::Kind)
 *  so the mirror is a copy and not a translation: 0 a curve (its tangent is
 *  the flow direction; ONE point makes it a pin, stored as kind 1), 4 a source
 *  pin, 5 an outlet pin, 7 a dye pin (colour + strength in `speed`). */
struct WaterCurve
{
	enum Kind { Curve = 0, Pin = 1, SourcePin = 4, OutletPin = 5, DyePin = 7 };
	quint8 kind = Curve;
	quint16 body = 0;              //!< 0 = resolve from the first wet point on mirror
	bool enabled = true;
	float speed = 0.25f;           //!< world units a second; a dye pin's strength 0..1
	float width = 4096.0f;         //!< world units, the influence radius
	quint8 colour[4] = { 0, 0, 0, 0 };   //!< DyePin only
	QVector<WaterCurvePoint> pts;

	bool isPin() const { return kind == Pin || ( kind == Curve && pts.size() == 1 ); }
	bool onePoint() const { return kind == SourcePin || kind == OutletPin || kind == DyePin; }
	//! Blender's "Switch Direction": the same curve, walked the other way.
	void reverse();
	//! Length along the polyline, world units.
	double length() const;
};

//! What a person changed about one body: the rows under "Selected body".
struct WaterBodyOverride
{
	int id = 0;
	QString name;
	int cls = -1;                  //!< -1 automatic, 0 sea, 1 river, 2 lake
	quint32 form = 0;              //!< 0 = the file's own
	quint8 colour[4] = { 0, 0, 0, 0 };   //!< A = 0 none
	bool still = false;            //!< bungo's "lakes have no flow" (a ZeroFlow mark)
	bool dyeMouth = false;
	float dyeStrength = 1.0f;

	bool isEmpty() const
	{
		return name.isEmpty() && cls < 0 && form == 0 && colour[3] == 0 && !still && !dyeMouth;
	}
};

/*! An imported flow map: a RASTER source layer, authority where painted.
 *  Texel coordinates are the BODY plane's (row 0 south, as the file), the
 *  words are the flow plane's own encoding (dir8 | speed4 << 8 | conf4 << 12)
 *  and `painted` is the PNG's alpha > 0.  The stroke store carries it as a
 *  kind-10 record (section 5 of the lane report, hook-up H2) whose payload is
 *  `x0 y0 w h` as int32 then the words zlib-deflated. */
struct WaterRasterLayer
{
	int px0 = 0, py0 = 0, w = 0, h = 0;
	QVector<quint16> words;
	QVector<quint8> painted;
	QString sourceFile;            //!< the PNG it came from, for the json and a person
	QByteArray sha256;             //!< of that PNG's bytes, hex

	qint64 paintedCount() const;
	//! The word at a body-plane texel, and whether it is painted.
	bool wordAt( int px, int py, quint16 & word ) const;
	QByteArray payload() const;    //!< the kind-10 record's bytes after the header
	bool fromPayload( const QByteArray & p, QString * error );
};

/*! The whole editable document.  Save/load as json, mirror to and from a
 *  `WaterMarkDoc` (the `.lodl`), export/import PNG. */
class WaterCurveDoc
{
public:
	static constexpr int kJsonVersion = 1;
	static constexpr quint8 kRasterKind = 10;   //!< the stroke-store kind of a raster layer

	// ---- provenance: which land file these curves were drawn on ----------
	QString worldspace;            //!< "Commonwealth": the land file's base name
	QString landFile;              //!< the .lodl's file name (no directory)
	int cellMinX = 0, cellMinY = 0, cellMaxX = 0, cellMaxY = 0;
	int bodySamples = 0;           //!< body-ID plane samples a cell when saved
	double dyeHalfDistance = 8192.0;

	QVector<WaterCurve> curves;
	QVector<WaterBodyOverride> bodies;
	QVector<WaterRasterLayer> rasters;

	void clear();
	bool isEmpty() const { return curves.isEmpty() && bodies.isEmpty() && rasters.isEmpty(); }
	//! The override record of `id`, made on demand; nullptr for id 0.
	WaterBodyOverride * overrideFor( int id, bool create );
	const WaterBodyOverride * overrideOf( int id ) const;
	void dropEmptyOverrides();

	// ---- the json file ----------------------------------------------------
	//! `<dir>/<Worldspace>.water.json` for `<dir>/<Worldspace>.lodl`.
	static QString jsonPathFor( const QString & lodlPath );
	/*! Deterministic: the same document always gives the same bytes (fixed key
	 *  order, floats at 9 significant digits, LF), which is what makes gate W2
	 *  a byte comparison. */
	QByteArray toJson() const;
	//! Refuses BY NAME an unknown format, a newer version, or a malformed curve.
	bool fromJson( const QByteArray & bytes, QString * error );
	bool saveJson( const QString & path, QString * error ) const;
	bool loadJson( const QString & path, QString * error );

	// ---- the .lodl (through WaterMarkDoc) ---------------------------------
	/*! Read the land file's stroke store and body table into this document:
	 *  kinds 0/1/4/5/7 become curves (per-point weights come from the record's
	 *  trailing floats when the store carries them, 1 otherwise), kind 10
	 *  becomes a raster layer, and every body with a name, a hand-set class, a
	 *  colour, a still mark or a dye mouth becomes an override. */
	bool readFrom( const WaterMarkDoc & doc, QString * error );
	/*! Mirror this document into `doc`: the strokes of the kinds this model
	 *  owns are replaced (ZeroFlow, DyeKnob and DyeMouth marks are the doc's
	 *  own and are set through its API), the overrides applied.  Curves the
	 *  land file refuses (dry land) are counted in `refused` and kept here.
	 *  Nothing is solved and nothing is written to disk. */
	bool writeTo( WaterMarkDoc & doc, int * refused, QString * error ) const;
	//! Curves equal point for point (and weight for weight): the gate's comparison.
	static bool sameCurves( const WaterCurveDoc & a, const WaterCurveDoc & b, QString * why );

	// ---- PNG ----------------------------------------------------------------
	/*! The flow map at the file's body-plane grid: R = (cos + 1) / 2, G = (sin +
	 *  1) / 2 with +G NORTH (image row 0 is the northernmost texel row), B =
	 *  speed step x 17, A = confidence x 17 -- every field exactly invertible,
	 *  which is what makes W5 a byte comparison.  Dry texels are 0,0,0,0.  The
	 *  body mask beside it is 16-bit grey, the body id. */
	static bool exportFlowPng( const WaterMarkDoc & doc, const QString & flowPng,
		const QString & maskPng, qint64 * wetTexels, QString * error );
	/*! Read a flow PNG into a raster layer.  Refuses BY NAME a size that is not
	 *  the file's grid, and A FLIPPED GREEN CHANNEL: the mean cosine between
	 *  the PNG's direction and the document's own over the painted wet texels
	 *  is computed as-is and with G mirrored; when the mirrored reading agrees
	 *  better and the as-is reading is below 0.9, the import is refused with
	 *  both numbers.  `agreeAsIs` / `agreeFlipped` come back either way. */
	static bool importFlowPng( const WaterMarkDoc & doc, const QString & flowPng,
		WaterRasterLayer & out, double * agreeAsIs, double * agreeFlipped, QString * error );
	static quint16 wordFromRgba( quint8 r, quint8 g, quint8 b, quint8 a );
	static void rgbaFromWord( quint16 word, quint8 & r, quint8 & g, quint8 & b, quint8 & a );
	//! Mirror the green channel of a flow PNG in place -- the W6 control's input.
	static bool flipGreen( const QString & inPng, const QString & outPng, QString * error );

	/*! The flow words this document would put in the plane, over the raster
	 *  layers only: the raster's word where painted, else `docWord`.  The
	 *  consumption INSIDE the solve is hook-up H3 (CHANGE_NEEDED.md); until it
	 *  lands the window paints the layer and the file carries it. */
	bool rasterWordAt( int px, int py, quint16 & word ) const;

	/*! Per-point weights out of the RAW stroke store, index-aligned with
	 *  `WaterMarkDoc::strokes()`: a kind-0/1 record longer than 20 + 8n by
	 *  4n bytes carries one float a point after the points.  Empty when the
	 *  store carries none.  Reads the bytes itself so it works before hook-up
	 *  H2 teaches the store's own codec to keep them. */
	static QVector<QVector<float>> weightsFromStore( const QByteArray & rawStore );
	//! The kind-10 payloads of the raw store, in record order.
	static QVector<QByteArray> rastersFromStore( const QByteArray & rawStore );
};

#endif // WATERCURVES_H

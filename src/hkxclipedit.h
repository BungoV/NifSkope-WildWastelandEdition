/* The EDITABLE animation clip: sparse keys over a dense per-frame clip.
   Lane HKXEDIT2, 2026-09-10. bungo, verbatim: "Just make hkx fully editable in
   our nifskope".

   THE KEY MODEL, stated once. A Fallout 4 clip on disk is DENSE: one transform
   per track per frame (the spline's degree-1 control points, or the
   interleaved array). What the user edits is SPARSE: keys. This document holds
   both -- `clip.frames` is what plays and what is written, `keys` is what the
   dope sheet shows and moves -- and the rule between them is one sentence:
   a track's dense frames are REGENERATED from its keys, a key's frame reading
   the key's transform VERBATIM and every frame between two keys interpolating
   them (linear translation and scale, shortest-arc nlerp with an exact
   normalise on rotation -- the same law HkxPlayback::sampleTrack uses between
   frames, so what the dope sheet says and what the viewport shows agree).
   Before the first key a track holds its first key, after the last its last.

   A clip loaded from a file starts with A KEY AT EVERY FRAME, so its dense
   data is reproduced bit for bit and nothing is lost until the user asks;
   `reduce()` is the action that thins them by a tolerance (bungo's choice of
   the default is still open, so every-frame keys ship as the default and the
   Reduce row carries the tolerance). Regeneration touches ONLY the track an
   edit named: every other track's floats are untouched bytes, which is what
   gate (b) of the brief measures.

   Annotations (hkaAnnotationTrack::Annotation) stay per track as the file
   keeps them, in SECONDS; the dope sheet shows them on one marker row. Float
   tracks are rows of this document only: lane HKX1's reader does not decode
   the four shipped float tracks and lane HKX5's writer refuses them by name,
   so a document that carries one cannot be saved yet -- save() refuses in
   words, and CHANGE_NEEDED.md beside the lane names the two edits.

   Root motion: `bakeRootMotion(track)` moves a track's TRANSLATION travel into
   the clip's extracted motion (hkaDefaultAnimatedReferenceFrame samples) and
   leaves the track at its frame-0 translation; `unbakeRootMotion()` puts the
   very bytes back when nothing else has moved that track since, and otherwise
   adds the motion back arithmetically and says which it did.

   QtCore-only (plus data/niftypes.h through hkxanim.h): the standalone gate
   binary tests/hkxclipedit_gate.cpp links it without NifSkope. */

#ifndef HKXCLIPEDIT_H
#define HKXCLIPEDIT_H

#include "hkxanim.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct HkxWriteReport;

//! One key of a transform track: the frame and the bone-local transform at it.
struct HkxKey
{
	int frame = 0;
	HkxTransform xf;
};

//! One key of a float track.
struct HkxFloatKey
{
	int frame = 0;
	float value = 0.0f;
};

//! A float track of this document: keys plus the dense values they regenerate.
struct HkxFloatTrackDoc
{
	QString name;
	QVector<HkxFloatKey> keys;   //!< sorted by frame, unique frames
	QVector<float> values;       //!< one per frame, regenerated from keys
};

//! The address of a key on the dope sheet: which track, which frame.
struct HkxKeyRef
{
	int track = -1;
	int frame = -1;
	bool operator==( const HkxKeyRef & o ) const { return track == o.track && frame == o.frame; }
};

/* WHICH COMPONENTS OF A TRANSFORM AN AXIS STRIP TOUCHES -- bungo's ruling 3 of
   2026-09-12, verbatim: "Add an option here, under remove track, to remove all
   transforms in specific directions, so x, y, z or a combination of them. Same
   goes for rotation direction. That way, I can make it so that the slide stays
   in the center, but the COM still moves downward when the player crouches."

   The rotation axes are the Euler triple of NifSkope's own Matrix::toEuler /
   Matrix::fromEuler -- the same three numbers the Blocks tab shows in a
   rotation row -- taken about the bone's LOCAL axes, because that is what a
   track's transform is. The dialog says so where the hand can read it. */
struct HkxAxisMask
{
	bool tx = false, ty = false, tz = false;
	bool rx = false, ry = false, rz = false;
	bool any() const { return tx || ty || tz || rx || ry || rz; }
};

//! What an edit did, or the one reason it did not, in words.
struct HkxEditResult
{
	bool ok = false;
	QString message;
	static HkxEditResult done( const QString & what ) { return { true, what }; }
	static HkxEditResult refused( const QString & why ) { return { false, why }; }
};

class HkxClipDocument
{
public:
	HkxAnimClip clip;                  //!< the dense clip: frames, annotations, root motion, binding
	QVector<QVector<HkxKey>> keys;     //!< per track, sorted by frame, unique frames
	QStringList trackNames;            //!< per track, the bone name ("" when no skeleton names it)
	QVector<HkxFloatTrackDoc> floatTracks;
	QString sourcePath;                //!< the file the clip came from ("" when built in memory)
	/* THE PLAY RANGE, bungo's rulings 8 and 9 of 2026-09-12: "Allow me to drag
	   the starting and ending frame" and "Areas inside of an animation should
	   be as they are, outside like in Blender, darkened".

	   It is a range OVER the clip, not a cut of it: the frames outside stay in
	   the document, so a drag is reversible and undoable, the sheet darkens
	   them, the viewport plays only the range -- and a SAVE writes the range
	   alone, so the .hkx duration is what the grips show. "Trim to range" is
	   the same cut made immediately and destructively.

	   rangeLast < 0 means the clip's last frame, so a document nobody has
	   touched has the whole clip in range and nothing is darkened. */
	int rangeFirst = 0;
	int rangeLast = -1;

	//! Build from a decoded clip: a key at every frame, dense data untouched.
	static HkxClipDocument fromClip( const HkxAnimClip & c, const QStringList & names = QStringList() );

	int numFrames() const { return clip.numFrames; }
	int numTracks() const { return clip.numTracks; }
	//! frames per second, 1 / frameDuration (60 for the Mixamo fixture, 30 vanilla)
	float fps() const { return clip.frameDuration > 0.0f ? 1.0f / clip.frameDuration : 0.0f; }
	//! the frame an annotation time falls on, at the clip's rate
	int frameOfTime( float seconds ) const;
	float timeOfFrame( int frame ) const { return float( frame ) * clip.frameDuration; }
	//! the track named `name`, case-insensitively, or -1
	int findTrack( const QString & name ) const;
	int keyCount( int track ) const { return track >= 0 && track < keys.count() ? keys.at( track ).count() : 0; }
	const HkxKey * keyAt( int track, int frame ) const;

	// ---- keys (each regenerates only the tracks it touched)
	HkxEditResult insertKey( int track, int frame, const HkxTransform & xf );
	//! Refuses to delete a track's LAST key: a track with no key has no pose.
	HkxEditResult deleteKeys( const QVector<HkxKeyRef> & which );
	//! Move (or copy, `copy` = true) keys by `deltaFrames`, clamped to the clip.
	//! A moved key landing on a key that did not move replaces it (Blender's
	//! rule: the moved selection wins).
	HkxEditResult moveKeys( const QVector<HkxKeyRef> & which, int deltaFrames, bool copy );
	//! Thin every track's keys to the fewest that regenerate its dense frames
	//! within the tolerances (translation and scale in units, rotation in
	//! degrees, the 4*asin metric). Keeps the first and last key of each track.
	HkxEditResult reduce( float tolTranslation, float tolDegrees, float tolScale, int * removed = nullptr );

	// ---- the law between keys and frames
	static HkxTransform interpolate( const HkxTransform & a, const HkxTransform & b, float t );
	//! the 4*asin(|q1 -+ q2|/2) angle between two rotations, in degrees
	static float angleDeg( const Quat & a, const Quat & b );
	void regenerateTrack( int track );
	void regenerateAll();
	//! Does every track's dense data equal what its keys regenerate? (the
	//! invariant; unbake is the one operation allowed to break it, and says so)
	bool consistent( int * firstTrack = nullptr, int * firstFrame = nullptr ) const;

	// ---- annotations (per track, in seconds, as the file keeps them)
	HkxEditResult addAnnotation( int track, float time, const QString & text, int * index = nullptr );
	HkxEditResult renameAnnotation( int track, int index, const QString & text );
	HkxEditResult moveAnnotation( int track, int index, float time );
	HkxEditResult deleteAnnotation( int track, int index );
	int annotationCount() const;

	// ---- float tracks (document rows only until the reader and writer carry them)
	HkxEditResult addFloatTrack( const QString & name, int * index = nullptr );
	HkxEditResult removeFloatTrack( int index );
	HkxEditResult setFloatKey( int index, int frame, float value );
	HkxEditResult deleteFloatKey( int index, int frame );
	void regenerateFloatTrack( int index );

	// ---- the clip as a whole
	//! Keep frames first..last inclusive; keys re-based; a boundary frame that
	//! was not a key becomes one (verbatim), so the kept frames stay exact.
	HkxEditResult trim( int first, int last );
	//! The play range, clamped to the clip; rangeLast < 0 reads as the last frame.
	int rangeFirstFrame() const;
	int rangeLastFrame() const;
	//! Is the range narrower than the whole clip? (nothing is darkened when not)
	bool rangeIsPartial() const;
	//! Set the play range. Refuses a range that would keep fewer than two frames.
	HkxEditResult setPlayRange( int first, int last );
	//! Resample to a new rate: round(duration * fps) + 1 frames; a new frame
	//! coincident with an old one copies it bit for bit, the others interpolate.
	//! Keys are rebuilt at every frame (a reduction does not survive a retime).
	HkxEditResult retime( float newFps );
	HkxEditResult removeTrack( int track );
	/* Hold the ticked components of one track STILL, at their frame-0 value,
	   on every key of that track (ruling 3). Not zero: frame 0, so the bone
	   does not jump to the origin -- a COM stripped of X and Y keeps standing
	   where it stood and still dips in Z. Untricked components are not
	   touched at all: when no rotation axis is ticked the quaternions are not
	   even round-tripped through Euler, so they stay byte-identical. */
	HkxEditResult removeTransformAxes( int track, const HkxAxisMask & mask );
	//! Rename a track (its bone). With a skeleton the binding index follows;
	//! without one the index is kept and the message says so.
	HkxEditResult renameTrack( int track, const QString & name, const HkxSkeleton * skeleton = nullptr );
	HkxEditResult bakeRootMotion( int track );
	HkxEditResult unbakeRootMotion();
	bool rootMotionBaked() const { return bakedTrack >= 0; }
	int rootMotionTrack() const { return bakedTrack; }
	//! |last - first| of the extracted motion's translation, 0 when none
	float rootMotionTravel() const;
	//! |frame f - frame 0| translation, max over frames, of one track
	float trackTravel( int track ) const;

	// ---- out
	//! The interleaved packfile (src/hkxwrite). Empty + a sentence on refusal.
	QByteArray toPackfile( QString & error ) const;
	bool save( const QString & path, HkxWriteReport & report, QString & error ) const;
	//! One line for the summary band.
	QString summary() const;

	// ---- gates
	//! Bit-for-bit equality of two clips' dense frames; names the first difference.
	static bool framesEqual( const HkxAnimClip & a, const HkxAnimClip & b, int * frame = nullptr, int * track = nullptr );
	static bool transformsEqual( const HkxTransform & a, const HkxTransform & b );

private:
	void sortKeys( int track );
	void addBoundaryKeys( int track );
	int bakedTrack = -1;
	QVector<HkxRootMotion> rootMotionBeforeBake;
	QVector<Vector3> bakedTranslations;   //!< the track's translations before the bake, per frame
};

#endif // HKXCLIPEDIT_H

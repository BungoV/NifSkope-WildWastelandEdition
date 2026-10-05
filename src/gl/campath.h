#ifndef CAMPATH_H
#define CAMPATH_H

#include "data/niftypes.h"

#include <QString>
#include <QVector>

/*! Lane MOTION1 (2026-10-04): A SCRIPTED CAMERA PATH. Keyframed eye and look-at, sampled at a fixed timestep,
 *  in double precision, so frame k is the same camera on every run (and so the same picture).
 *
 *  The file (text, '#' starts a comment):
 *    fps <f>                                   the timestep is 1/f s (default 30)
 *    frames <n>                                how many frames (default: the keys' span x fps, plus one)
 *    key <t> <eye x y z> <look-at x y z> [fov]  a key at t seconds; fov in degrees (vertical), default 70
 *  Between keys the eye and the look-at follow a cubic Hermite curve whose tangents are Catmull-Rom's
 *  (the neighbours' difference over their time span; one-sided at the ends), the fov a straight line.
 *  Up is the scene's up axis; the camera never rolls. Frame k is at t = first key + k / fps.
 *
 *  The render hook draws it: WW_RENDER_PATH=<file> (or --path <file>) with WW_RENDER_SHOT=<out.png> writes
 *  out_000.png .. out_<n-1>.png and out_path.txt (per frame: the camera, the temporal AA, the sun cascades). */

struct WwCamPathKey
{
	double t = 0.0;
	double eye[3] = { 0, 0, 0 };
	double at[3] = { 0, 0, 0 };
	double fov = 70.0;
};

class WwCamPath
{
public:
	//! Parse a path file; false with the reason in *error.
	bool load( const QString & path, QString * error );
	//! How many frames the path renders.
	int frameCount() const;
	double fps() const { return m_fps; }
	//! The camera at frame k.
	void sample( int frame, Vector3 & eye, Vector3 & at, float & fov ) const;
	//! The same, at t seconds after the first key (doubles throughout, rounded to float once).
	void sampleAt( double t, double eye[3], double at[3], double & fov ) const;

private:
	QVector<WwCamPathKey> m_keys;
	double m_fps = 30.0;
	int m_frames = -1;
};

#endif

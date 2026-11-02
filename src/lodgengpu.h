/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENGPU_H
#define LODGENGPU_H

//! THE LOD BAKE'S GPU PATH:
//! One offscreen OpenGL 4.3 context on a thread of its own, made once per process by lodgenGpuConfigure().

#include <QString>
#include <QtGlobal>

//! The setting's key, under the GUI's QSettings scope (Settings > NIF page).
extern const char * const kLodgenGpuSettingKey;

//! The Settings row's value as the GUI would read it (default on).
bool lodgenGpuSettingEnabled();

/*! Whether the headless app should be a QGuiApplication: `argv` asks for
 *  lodgen, has no --no-gpu, the setting is on and the platform plugin is
 *  present. Called from main BEFORE any app object exists. */
bool lodgenGpuWantedForArgs( int argc, char ** argv );

/*! Once per process, on the main thread, after the app object exists: reads
 *  the setting, honours `forceCpu` (--no-gpu), makes the context and runs the
 *  self-check. Safe to call again (no-op). */
void lodgenGpuConfigure( bool forceCpu );

//! True when the GPU path is live.
bool lodgenGpuOn();

//! One line for the bake log: which path runs and why.
QString lodgenGpuReport();

//! The word the incremental chunk digest takes for the path: "|bc7gpu" when the GPU path is live, empty on the
//! CPU path (so a CPU bake's digest is the one it always had).
QString lodgenGpuDigestWord();

//! Counters for the end-of-bake line ("gpu: bc7 N images, M blocks, T ms, F fell back").
QString lodgenGpuSummary();

//! BC7-encode one image (0xAARRGGBB, `w` x `h`) under the per-channel error weights `wt` (R G B A) into `out`
//! (((w+3)/4) * ((h+3)/4) blocks of 16 bytes, row-major), a BC7 encoding under LodgenBc7::encodeBlock's error
//! measure (see THE RULE above: not its bytes).
bool lodgenGpuEncodeBc7( const quint32 * px, int w, int h, const int wt[4], quint8 * out );

#endif

/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENGPU_H
#define LODGENGPU_H

/*! THE LOD BAKE'S GPU PATH (lane GPU1, 2026-09-26).
 *
 *  One offscreen OpenGL 4.3 context on a thread of its own, made once per
 *  process by lodgenGpuConfigure(). No window is ever shown: the context draws
 *  into a QOffscreenSurface, and nothing is drawn -- the work is compute shaders.
 *
 *  THE RULE: a GPU stage is deterministic (two runs, same bytes) and no worse
 *  than its CPU stage on the CPU stage's own error measure. The BC7 block
 *  encoder runs the search of src/lodgenbc7.h in single precision, so its
 *  bytes differ from the CPU's on some blocks; the context is refused at
 *  start-up unless a self-check shows every GPU block decodes to its reported
 *  error and the GPU's total weighted error is no more than the CPU's. A job
 *  that fails later returns false and its caller runs the CPU loop. With the
 *  GPU path off, the bake writes exactly the CPU bytes it always wrote.
 *
 *  ON BY DEFAULT. Off by NifSkope's Settings > NIF > LOD bake > Use GPU (the
 *  headless `-no-gui lodgen` reads the same key), or for one run by --no-gpu,
 *  which wins over the setting. The bake log states the path and why. */

#include <QString>
#include <QtGlobal>

//! The setting's key, under the GUI's QSettings scope (Settings > NIF page).
extern const char * const kLodgenGpuSettingKey;

//! The Settings row's value as the GUI would read it (default on).
bool lodgenGpuSettingEnabled();

/*! Whether the headless app should be a QGuiApplication: `argv` asks for
 *  lodgen, has no --no-gpu, the setting is on and the platform plugin is
 *  present. Called from main() BEFORE any app object exists. */
bool lodgenGpuWantedForArgs( int argc, char ** argv );

/*! Once per process, on the main thread, after the app object exists: reads
 *  the setting, honours `forceCpu` (--no-gpu), makes the context and runs the
 *  self-check. Safe to call again (no-op). */
void lodgenGpuConfigure( bool forceCpu );

//! True when the GPU path is live.
bool lodgenGpuOn();

//! One line for the bake log: which path runs and why.
QString lodgenGpuReport();

/*! The word the incremental chunk digest takes for the path: "|bc7gpu" when
 *  the GPU path is live, empty on the CPU path (so a CPU bake's digest is the
 *  one it always had). The GPU's BC7 bytes are not the CPU's, so a cache
 *  written on one path is never reused by the other. */
QString lodgenGpuDigestWord();

//! Counters for the end-of-bake line ("gpu: bc7 N images, M blocks, T ms, F fell back").
QString lodgenGpuSummary();

/*! BC7-encode one image (0xAARRGGBB, `w` x `h`) under the per-channel error
 *  weights `wt` (R G B A) into `out` (((w+3)/4) * ((h+3)/4) blocks of 16 bytes,
 *  row-major), a BC7 encoding under LodgenBc7::encodeBlock's error measure (see
 *  THE RULE above: not its bytes). Returns false when the
 *  GPU path is off, the image is too small to be worth a dispatch, or the job
 *  failed: the caller then runs its CPU loop. Thread-safe (jobs are serialised). */
bool lodgenGpuEncodeBc7( const quint32 * px, int w, int h, const int wt[4], quint8 * out );

#endif

#ifndef RTDDGICELL_H
#define RTDDGICELL_H

/*! RAY-TRACED GI IN THE CELL VIEW (lane RTXGI1 M2, 2026-10-29; notes/rtxgi1/PLAN.md "New" 2-4, GUIDE.md 3).
 *
 *  Switch WW_CELL_RTDDGI (1 on, 0 off; unset: the Show menu's "Ray-traced GI" row, default off). On, and with the
 *  probe GI's soup kept for the open cell (probeSkyKeptSoup: the Precomputed Radiance Transfer relight), a DDGI volume
 *  (src/rtddgi.h RtDdgiLive) is built on a worker over that soup with the relight's own lights and weather sky, in the
 *  probe GI's grid box round the camera, and then updated on the bake device a fixed number of times a frame. Each
 *  update set copies the tiles back and into one GL texture buffer; the surfaces read it in place of the baked grid
 *  (res/shaders/cell_ddgi.glsl, called from cell_lights.glsl's cellGiVolume and cellSpecSky).
 *
 *  Off is today's frame: the read is compiled only under the loader's #define WW_CELL_RTDDGI (rtddgiCellShaderDefine),
 *  so an off program holds none of it, and nothing here binds, uploads or draws. A set of the switch reloads the
 *  shaders (its survey stage) and the next frame builds or frees the volume: live, no reopen.
 *
 *  Harness keys (no rows): WW_CELL_RTDDGI_RED=trilinear|nocheb|nobias (the read's refuters, live; trilinear = no
 *  backface, no Chebyshev: the leak gate's red), WW_CELL_RTDDGI_SPEC=spacing=140,probes=4096,reach=2048,rays=128,
 *  updates=4,max=0,zscale=1 (live: a changed value rebuilds; max > 0 stops after that many updates, for pictures that
 *  repeat), WW_CELL_RTDDGI_DUMP=<dir> (rtddgiDump of the volume at the stop, or at update set 64 without a max),
 *  WW_CELL_RTDDGI_VIEW=0|states|irradiance (pins the debug view row). Census lines start "rtddgi cell:".
 *
 *  M3 (2026-10-29): the volume is a window of 2 x reach a side round the camera (kept inside the probe GI's grid) that
 *  scrolls with it by whole probe cells: the kept probes keep their data, the new planes reset and are relocated and
 *  classified on the device, and the spec's max counts again from each scroll. GL reads the device's atlas in place
 *  (GL_EXT_memory_object_win32 + GL_EXT_semaphore_win32, the same GPU and driver by UUID), no copy back and no upload.
 *  Path reds (in WW_CELL_RTDDGI_RED's list, live, each rebuilds): readback (M2's copy back + upload), rebuild (a camera
 *  move builds a new volume, no scroll). A camera path (nifskope_ui.cpp wwRenderPathRun) waits at its first key until
 *  rtddgiCellPending() is false, so its pictures repeat. */

class Scene;
class Transform;

//! the switch: the WW_CELL_RTDDGI pin, else the row (a harness settings scope starts it off)
bool rtddgiCellOn();
bool rtddgiCellPinned();
void rtddgiCellSetOn( bool on );
//! what Shader::load asks: the read compiled in (on, and the GI's volumes path: the march red has no cellGiVolume)
bool rtddgiCellShaderDefine();

//! the debug view row ("Ray-traced GI probes"): 0 off, 1 probe states, 2 probe irradiance; WW_CELL_RTDDGI_VIEW pins it
int rtddgiCellView();
bool rtddgiCellViewPinned();
void rtddgiCellSetView( int mode );

//! one call a frame (glview.cpp, after probeSkyTick; the view's GL context is current): build, update, upload
void rtddgiCellFrame( Scene * scene );
//! the current cell program's read uniforms and its texture buffer (from wwCellLightsUniforms; a program without the
//! read, every program while the switch is off, returns at once)
void rtddgiCellUniforms( Scene * scene );
//! the debug view over the frame (glview.cpp, beside the PRTP views): since RTXFIX1 only with WW_CELL_RTDDGI_VIEW_FOG=0,
//! or on a frame the view did not draw inside (no linear frame)
void rtddgiCellViewDraw( Scene * scene, const Transform & viewTrans );
//! lane RTXFIX1: the debug view inside the linear frame (glview.cpp, after the opaque scene, before the tone map): rounder
//! instanced spheres, the irradiance read on the GPU, the fog and the exposure as on any surface
void rtddgiCellViewDrawLinear( Scene * scene, const Transform & viewTrans );
//! the live driver's settle and a path's first key wait on this (nifskope_ui.cpp): a build in flight, or a spec max not
//! reached since the build or the last scroll
bool rtddgiCellPending();

#endif // RTDDGICELL_H

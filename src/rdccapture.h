/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef RDCCAPTURE_H
#define RDCCAPTURE_H

#include <QString>

/*! \file rdccapture.h
 * \brief RenderDoc capture driven from inside the app.
 *
 * Attaching RenderDoc to NifSkope from outside does not work, and the reason is
 * structural rather than a settings problem (WW_CHANGES 2026-07-27, "Round 3").
 * The viewport is a native child window with its own GL context — which is also
 * why grabs must go through `ogl->grabFramebuffer()` rather than `skope->grab()`,
 * and why the scene reports `drawFbo=0`, FBO 0 *of its own context*. RenderDoc's
 * frame boundary follows the presenting surface, so every capture came back
 * holding one 2-triangle draw with a single `textureSampler` bound: Qt
 * compositing the finished viewport as a textured quad. The 3D work is outside
 * that frame and was never in the capture, at any frame number, on any file.
 *
 * The in-application API removes the guesswork: `StartFrameCapture` /
 * `EndFrameCapture` with a null device and window capture whatever context is
 * current, so calling them around the scene render defines the frame boundary
 * from inside the right context. No injection, no target-control handshake — the
 * handshake is the other half of why this never worked, since the
 * `WW_RENDER_SHOT` harness quits faster than `rdc capture` can attach.
 *
 * Everything here is a no-op unless WW_RDC_FRAMES is set, and on non-Windows.
 */

/*! Load renderdoc.dll and fetch the API. Call EARLY from main().
 *
 * Order matters: RenderDoc has to be in the process before the GL context is
 * created or it cannot hook the driver, and Qt creates that context when GLView
 * is constructed. Loading it lazily from the first paintGL would be too late.
 *
 * WW_RDC_FRAMES=<n>  arms the next n rdcBeginFrame()/rdcEndFrame() pairs; unset
 *                    or 0 means this whole file does nothing
 * WW_RDC_DLL=<path>  overrides the renderdoc.dll location
 * WW_RDC_OUT=<path>  capture file template (default: alongside the exe)
 *
 * Returns false when disarmed or when the DLL could not be loaded.
 */
bool rdcInit();

//! True when a capture is armed and the API is live.
bool rdcArmed();

//! Begin a capture, if any frames are still armed. \a label names the file.
void rdcBeginFrame( const QString & label );

//! End the capture begun by rdcBeginFrame(). Safe to call unpaired.
void rdcEndFrame();

//! How many armed frames are left.
int rdcFramesRemaining();

#endif // RDCCAPTURE_H

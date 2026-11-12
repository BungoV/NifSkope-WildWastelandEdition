#pragma once

/* Lane VKFRAME1 (2026-11-12, PRESENT FIRST): the finished GL frame goes to the screen through a Vulkan swapchain
 * (flip model: MAILBOX, else IMMEDIATE; FIFO only when vsync is on) instead of SwapBuffers, which blocked 1.5 ms of
 * build 82's 4.4 ms frame at the empty spot (windowed GL goes through the compositor's copy).
 * How: a child window over the view's client area (mouse and keys pass through to the view), its own Vulkan
 * device on GL's GPU (the ray-tracing device has no swapchain extension), a ring of 3 RGBA8 images GL imported.
 * GL resolves its back buffer into the ring image (one blit) and signals; Vulkan waits, blits into the swapchain
 * image (the y flip) and presents. Off: WW_VKPRESENT_RED=1, or Settings > Ray tracing device > Vulkan Present
 * unticked: GL's SwapBuffers. Log line `vk present:`. */

#include <cstdint>
#include <string>
#include <vector>

namespace WwVkPresent
{

using GlProc = void * (*)( const char * name );

struct Stat
{
	double ms = 0.0;           //!< the last present() call on the drawing thread, CPU (the Vulkan half: the worker, Split)
	double waitMs = 0.0;       //!< of it, the wait for the ring slot's last use (the worker's submit, then its fence)
	double stateMs = 0.0;      //!< of it, the GL state asked (scissor; the bindings when no named blit)
	double blitMs = 0.0;       //!< of it, the resolve blit
	double glMs = 0.0;         //!< of it, the fence for the worker's GL signal (sigWorker), else GL's signal to Vulkan
	double flushMs = 0.0;      //!< of it, glFlush
	bool sigWorker = false;    //!< build 88: GL's signal to Vulkan went out from the worker's own GL context
	std::string sigWhy;        //!< why not (the worker's context was not made)
	bool sigWait = false;      //!< build 91: no GL signal: the worker waited GL's fence on the CPU (signal route 2)
	bool dsa = false;          //!< the resolve went through glBlitNamedFramebuffer
	bool busy = false;         //!< the slot's last Vulkan read was still running: the drawing thread waited its fence
	bool worker = false;       //!< the Vulkan half went to the worker (false: ran here, the harness's capture)
	bool view = false;         //!< round 10: GL drew into the view on Vulkan; no GL blit, the worker resolved it
	const char * mode = "none";
	int images = 0;
	int w = 0, h = 0;
	std::uint64_t builds = 0;  //!< swapchain (re)builds this run
};

/* The GL context current, its back buffer (FB 0) holding the finished frame, hwnd the window GL draws in, w x h its
 * size in pixels. true: on the screen through the swapchain. false: why says why; the caller swaps through GL. */
bool present( void * hwnd, int w, int h, bool vsync, GlProc gl, std::string & why );
const Stat & last();
//! harness: the next present() also copies the swapchain image it presents into out (BGRA8, top row first)
void captureNext( std::vector<std::uint8_t> * out );
/* the present worker's Vulkan half, summed since the last call (then reset): fence, acquire, record, submit, present
 * and its whole ms, n frames, failed ones (a failed frame is not shown; why: the last reason) */
struct Split
{
	int n = 0, failed = 0;
	double signalMs = 0.0;  //!< build 88: the worker's GL fence wait (a GPU wait, queued) and GL signal
	double fenceMs = 0.0, acquireMs = 0.0, recordMs = 0.0, submitMs = 0.0, presentMs = 0.0, totalMs = 0.0, maxMs = 0.0;
	std::string why;
};
Split takeSplit();
//! the child window, the swapchain and the ring go (GL current: its textures and semaphores go too); the device stays
void shutdown();

/* Round 10 (VKFRAME1, the view on Vulkan, WW_VKVIEW=1; red WW_VKVIEW_RED=1 = the old route): before the paint, GL binds
 * (GL_FRAMEBUFFER) a view the present device made, w x h at the back buffer's samples (RGBA8 + D24S8), and draws the
 * frame into it; present() then hands it to the worker, which resolves it on Vulkan into the ring slot (no GL blit).
 * false: why; nothing bound (the caller draws into its back buffer). A ring of 3 views; GL waits a view's last present. */
bool viewBegin( int w, int h, int samples, GlProc gl, std::string & why );
//! the FBO the view is drawn into this frame (0: the back buffer)
unsigned viewFbo();
//! a view drawn but not presented (present() failed early): GL resolves it into the back buffer for GL's swap
bool viewRescue();
struct ViewStat
{
	bool on = false;
	int w = 0, h = 0, samples = 0, views = 0;  // not `slots`: a Qt macro
	double mb = 0.0, makeMs = 0.0;  //!< the ring's device memory, the last make's ms
	int busy = 0;                   //!< since the last take: views still read by Vulkan when GL wanted them
	double waitMs = 0.0;            //!< since the last take: the drawing thread's wait for them, summed
	std::string why;                //!< the last make that failed
};
ViewStat takeViewStat();
//! the views go (the route went off)
void viewDrop();
//! the route this process presents through, for the zone lines: "vk view" / "vk present" / "gl swap"
const char * routeName();

}

#ifndef WW_VKPRESENT_NO_QT
class QOpenGLWindow;
class QPaintDevice;
#include <functional>
namespace WwVkPresent
{
/* round 10: what the view's QPainter draws through: the window (its back buffer) or, while the view is on Vulkan, a
 * paint device that binds the view's FBO */
QPaintDevice * painterTarget( QOpenGLWindow * w );
/* GLView::event's UpdateRequest: false = not taken (the red, the row off, a new size: QOpenGLWindow's own paint runs,
 * which also sizes its paint device for QPainter). true = painted (paint()) and on the screen, through the swapchain
 * or, when the present failed, GL's swap; the caller emits frameSwapped. */
bool frame( QOpenGLWindow * w, const std::function<void()> & paint );
}
#endif

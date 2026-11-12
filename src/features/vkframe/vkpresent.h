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

/* Round 11 (VKFRAME1, stage 1 of the post on the present device): the cell tone map (cell_hdr.frag modes 0, 2 and 1) as
 * a Vulkan fragment pass into the view (res/shaders/hlsl/post/tone_view_*.hlsl over tonemap_raster.hlsl). GL draws the
 * linear frame into images the present device made (toneFrame), blits its depth into the view as before, then toneRun:
 * GL signals, the worker records and submits the pass (it waits GL's signal, signals back), the drawing thread waits the
 * submit and GL waits Vulkan's signal; GL's later passes (clouds in front, P7, lens flare, TAA, overlays) draw on the
 * result. Red WW_VKVIEW_TONE_RED=1 (or WW_VKVIEW off): GL's tone quads. */
struct ToneJob  // 96 bytes: ToneBindingsVk.hlsli's ToneJob (push constants); ints[0]'s mode / test / ref are toneRun's
{
	float cineE[4] = {}, tint[4] = {}, wbExposure[4] = {}, bloomRect[4] = {}, fo[4] = {};
	std::int32_t ints[4] = {};
};
static_assert( sizeof( ToneJob ) == 96, "ToneJob is the shader's push constant block" );
/* the linear frame (RGBA16F + D24S8, samples at the view's) GL draws the cell into, made on the present device; GL names
 * out (multisample textures; ds already in GL_STENCIL_INDEX mode). GL current. */
bool toneFrame( int w, int h, int samples, unsigned & color, unsigned & ds, std::string & why );
//! color is the live shared frame's colour name (a shutdown or a remake drops it)
bool toneFrameIs( unsigned color );
//! the shared frame goes (GL current)
void toneFrameDrop();
struct ToneRun
{
	ToneJob job;
	bool splitSky = false;   //!< mode 0 where the depth is nearer than the far plane, mode 2 (premultiplied) where it is not
	bool sorted = true;      //!< the stencil sorts the pixels: mode 0 takes stencil 1, mode 1 stencil 2 after it
	const std::uint8_t * lut = nullptr;  //!< the grading LUT, 16^3 RGB8 (null: none); uploaded when lutGen changes
	std::uint64_t lutGen = 0;
	unsigned measure = 0;    //!< the measure's bloom, a GL 2D float texture (0: none), measureW x measureH
	int measureW = 0, measureH = 0;
	std::uint64_t measureGen = 0;  //!< copied into the shared image when it changes
	int vp[4] = {};                //!< GL's viewport (x, y, w, h; w 0: the whole frame): the pass's viewport and scissor
	unsigned drawFb = 0, readFb = 0;  //!< bound again after the measure's copy (no glGet)
};
/* GL current, the view (viewBegin) bound and the frame's depth already in it: the tone draws on Vulkan into the view.
 * false: why; nothing drawn (GL draws its quads). */
bool toneRun( const ToneRun & t, std::string & why );
//! a frame whose tone map stayed on GL while the stage was wanted, and why (the telemetry)
void toneOnGl( const char * why );
struct ToneStat
{
	int runs = 0, gl = 0, fails = 0;   //!< since the last take: on Vulkan, on GL (toneOnGl), failed toneRun calls
	double cpuMs = 0.0, maxMs = 0.0;   //!< the drawing thread's toneRun ms, summed and max
	double mb = 0.0;                   //!< the shared frame + measure + LUT memory
	std::string glWhy, failWhy;        //!< the last reasons
	int pipelines = 0;                 //!< built for the frame's samples
	double buildMs = 0.0;
};
ToneStat takeToneStat();
//! the pipeline cache file (empty: none); the Qt part sets it before the first toneFrame
void toneCachePath( const std::string & path );

/* Lane VKSHARE1: single-sample targets another pass owns (the clouds' march and history, TAA's), made on the present
 * device and imported into GL, keyed by the owner. type 0 = 2D, 1 = 3D (d deep), 2 = 2D array (d layers); glFmt one of
 * GL_RGBA16F / RGBA32F / RGBA8 / RG32F / RG16F / R32F / R8 / DEPTH24_STENCIL8. GL current. Same key and shape: the same
 * name; another shape: the old one goes first (a full drain). false: why; tex 0 (the owner makes GL's own). The GL
 * texture has NEAREST filters; the owner sets its own. */
bool sharedTarget( std::uintptr_t key, int type, int w, int h, int d, unsigned glFmt, unsigned & tex, std::string & why );
//! tex is key's live shared name in the current GL context (a shutdown drops every one: the owner makes GL's own again)
bool sharedTargetIs( std::uintptr_t key, unsigned tex );
//! key's target goes (GL current: its GL names too)
void sharedTargetDrop( std::uintptr_t key );
struct SharedStat
{
	int made = 0, dropped = 0, fails = 0;  //!< since the last take
	int live = 0;                          //!< targets now
	double mb = 0.0;                       //!< their memory
	std::string failWhy;
};
SharedStat takeSharedStat();

/* Lane VKSHARE1: the Modern temporal AA (gametaa.cpp, tier High) on the present device. GL blits the frame's colour and
 * depth into shared targets (sharedTarget) as before; taaRun: GL signals, the worker submits the camera vectors, the
 * resolve and the RCAS copy-back into the view (one crossing), GL waits Vulkan's signal. Red WW_VKVIEW_TAA_RED=1: GL. */
struct TaaJob  // 128 bytes: TaaBindingsVk.hlsli's TaaJob
{
	float sizeJitter[4] = {};       //!< 1/W, 1/H, jitter px x, y
	std::int32_t ints[4] = {};      //!< tier, red, no history, inputs
	std::int32_t ints2[4] = {};     //!< linear, dither on, dither seed, spare
	float f[4] = {};                //!< expose
	float reproj[16] = {};          //!< GL's mat4, column-major
};
static_assert( sizeof( TaaJob ) == 128, "TaaJob is the shader's push constant block" );
struct PfxJob  // 128 bytes: PostFxBindingsVk.hlsli's PfxJob
{
	float c[7][4] = {};
	std::int32_t i[4] = {};
};
static_assert( sizeof( PfxJob ) == 128, "PfxJob is the shader's push constant block" );
struct TaaRun
{
	TaaJob job;                     //!< the resolve
	PfxJob mvJob, rcasJob;          //!< the camera vectors (game_taa_mv), the copy-back (taa2_rcas)
	int w = 0, h = 0, wr = 0;       //!< the frame; the history written this frame (0 / 1)
	std::uintptr_t cur = 0, depth = 0, mv = 0, pic = 0, hist[2] = {}, lock[2] = {};  //!< sharedTarget keys
	unsigned vel = 0, mask = 0;     //!< TAAINPUTS1's GL textures (RG16F / RGBA8; 0: none), copied into shared twins
	std::uintptr_t velKey = 0, maskKey = 0;
};
/* GL current, the view bound, the frame's colour and depth already blitted into the shared cur / depth: the vectors,
 * resolve and copy-back run on Vulkan. false: why; nothing drawn (GL runs its passes). */
bool taaRun( const TaaRun & t, std::string & why );
//! a frame whose TAA stayed on GL while the stage was wanted, and why (the telemetry)
void taaOnGl( const char * why );
struct TaaStat
{
	int runs = 0, gl = 0, fails = 0;   //!< since the last take
	double cpuMs = 0.0, maxMs = 0.0;   //!< the drawing thread's taaRun ms
	std::string glWhy, failWhy;
	int pipelines = 0;
	double buildMs = 0.0;
};
TaaStat takeTaaStat();

/* Lane VKSKY1 (row G, the sky first): the cell sky (atmo_sky.frag, step 3 of wwAtmoDrawSky) as a Vulkan fragment pass
 * into the tone frame (toneFrame's colour), res/shaders/hlsl/sky/atmo_sky_ps.hlsl. GL builds the LUTs as before; skyRun
 * copies the four into shared twins (sharedTarget), GL signals, the worker draws one full-screen triangle at the frame's
 * samples (no depth or stencil attachment: GL's quad ran with both off), GL waits Vulkan's signal and draws the rest of
 * the backdrop and the objects on it. Red WW_VKVIEW_SKY_RED=1 (or WW_VKVIEW off): GL's quad. */
struct SkyField
{
	const char * name;
	int offset, words, count;  //!< bytes into LooseUniforms; floats (or ints) a member; array length (1: plain)
	bool isInt;
};
//! atmo_sky_ps.hlsl's LooseUniforms (b31), offsets from spirv-cross --reflect of sky_view_ps.spv.h (block 564 B)
constexpr SkyField kSkyFields[] = {
	{ "atHaze", 0, 3, 1, false }, { "atRay", 16, 3, 1, false }, { "atMieS", 28, 1, 1, false }, { "atHazeSpec", 32, 3, 1, false },
	{ "atHazeAlbedo", 48, 3, 1, false }, { "atMieE", 60, 1, 1, false }, { "atOzone", 64, 3, 1, false }, { "atMsOld", 76, 1, 1, false },
	{ "atG", 80, 1, 1, false }, { "sceneExposure", 84, 1, 1, false }, { "viewTransform", 88, 1, 1, true }, { "atCamR", 92, 1, 1, false },
	{ "atViewSize", 96, 2, 1, false }, { "atTint", 112, 3, 17, false }, { "atRect", 384, 4, 1, false }, { "atPerPixel", 400, 1, 1, false },
	{ "atSunK", 404, 1, 1, false }, { "atMsSize", 408, 2, 1, false }, { "atMsOn", 416, 1, 1, false }, { "atGround", 420, 1, 1, false },
	{ "atSunW", 432, 3, 1, false }, { "atMoonK", 444, 1, 1, false }, { "atMoonW", 448, 3, 1, false }, { "atFloor", 464, 3, 1, false },
	{ "atRecNight", 480, 2, 1, false }, { "atTintOn", 488, 1, 1, false }, { "atInscatter", 496, 3, 1, false }, { "atDirIns", 512, 3, 1, false },
	{ "atDirL", 528, 3, 1, false }, { "atDirExp", 540, 1, 1, false }, { "atSunSize", 544, 1, 1, false }, { "atLimb", 548, 1, 1, false },
	{ "atDiscAir", 552, 1, 1, false }, { "atDiscHue", 556, 1, 1, false }, { "atDiscCap", 560, 1, 1, false },
};
constexpr int kSkyLooseBytes = 576;   //!< the block (564) rounded to 16
constexpr int kSkyGlobalBytes = 336;  //!< NifSkopeOpenGLContext::GlobalUniforms (b0), copied as is
struct SkyRun
{
	int w = 0, h = 0;              //!< the tone frame
	int vp[4] = {};                //!< GL's viewport (x, y, w, h): the pass's viewport and scissor
	unsigned color = 0;            //!< the tone frame's colour name (toneFrameIs)
	unsigned lut[4] = {};          //!< GL's RGBA16F 2D LUTs: transmittance, multiple scattering, sun view, moon view
	int lutW[4] = {}, lutH[4] = {};
	std::uintptr_t lutKey[4] = {}; //!< their shared twins' keys (sharedTarget)
	std::uint8_t global[kSkyGlobalBytes] = {};
	std::uint8_t loose[kSkyLooseBytes] = {};
};
/* GL current, the tone frame bound as GL's draw target, the LUTs drawn: the sky's pixels drawn on Vulkan. false: why;
 * nothing drawn (GL draws its quad). */
bool skyRun( const SkyRun & t, std::string & why );
//! a frame whose sky stayed on GL while the stage was wanted, and why (the telemetry)
void skyOnGl( const char * why );
struct SkyStat
{
	int runs = 0, gl = 0, fails = 0;   //!< since the last take
	double cpuMs = 0.0, maxMs = 0.0;   //!< the drawing thread's skyRun ms
	std::string glWhy, failWhy;
	int pipelines = 0;
	double buildMs = 0.0;
};
SkyStat takeSkyStat();

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
//! round 11: the tone map on the present device is wanted this frame (the view on Vulkan, WW_VKVIEW_TONE_RED unset)
bool toneWanted();
/* lane VKSHARE1: a pass's targets go on the present device this frame (sharedTarget): the view on Vulkan with its size
 * settled (viewFbo), and the pass's red (redEnv=1) unset */
bool shareWanted( const char * redEnv );
}
#endif

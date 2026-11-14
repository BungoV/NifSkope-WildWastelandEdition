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
struct ToneJob  // 112 bytes: ToneBindingsVk.hlsli's ToneJob (push constants); ints[0]'s mode / test / ref are toneRun's
{
	float cineE[4] = {}, tint[4] = {}, wbExposure[4] = {}, bloomRect[4] = {}, fo[4] = {};
	std::int32_t ints[4] = {};
	float tent[4] = {};  //!< lane VKFAR2: toneRun fills it (and ints[3], flag 8) from ToneRun::bloom when its lod is on
};
static_assert( sizeof( ToneJob ) == 112, "ToneJob is the shader's push constant block" );
/* the linear frame (RGBA16F + D24S8, samples at the view's) GL draws the cell into, made on the present device; GL names
 * out (multisample textures; ds already in GL_STENCIL_INDEX mode). GL current. */
bool toneFrame( int w, int h, int samples, unsigned & color, unsigned & ds, std::string & why );
//! color is the live shared frame's colour name (a shutdown or a remake drops it)
bool toneFrameIs( unsigned color );
//! the shared frame goes (GL current)
void toneFrameDrop();
/* Lane VKBLOOM1: the Fusion Shaders bloom (cellbloom.cpp, mode 3) on the present device, in the tone pass's command
 * buffer before its draws: the source from the shared frame, down / hold / up, the per-pixel combine (vkframe.cpp's
 * kernels, the same SPIR-V), then mode 3 over the backdrop the view holds after modes 0-2. cellbloom.cpp fills it. */
constexpr int kToneBloomMips = 12;
struct ToneBloom
{
	int mips = 0;
	int dims[kToneBloomMips][2] = {};
	float thr = 1.0f, knee = 0.5f;
	int red = 0;
	bool hold = false;
	int holdK = 0, holdN = 1;
	float radius = 1.0f, dispersion = 0.0f;
	float srcW[kToneBloomMips] = {}, accW[kToneBloomMips] = {};
	float exposure = 1.0f;  //!< cell_bloom_src.frag's uniforms
	bool on = false;
	float impulseX = 0.0f, impulseY = 0.0f, impulse = 0.0f;
	/* lane VKFAR2: Bloom Mip under the fog bloom (cell_hdr.frag's lod tent): no combine; the top's mips (levels 0-3, as
	 * glGenerateMipmap) read per sample by perPixel x log2 of the fog's gain (cellIsFoBloomParams.w) */
	bool lod = false;
	float perPixel = 0.0f;
};
struct ToneRun
{
	ToneJob job;
	const ToneBloom * bloom = nullptr;  //!< lane VKBLOOM1: flag 7 (and 11) on: the bloom runs in this pass
	int measQw = 0, measQh = 0;         //!< lane VKBLOOM1: the exposure measure's quarter size (0: not asked)
	bool splitSky = false;   //!< mode 0 where the depth is nearer than the far plane, mode 2 (premultiplied) where it is not
	bool sorted = true;      //!< the stencil sorts the pixels: mode 0 takes stencil 1, mode 1 stencil 2 after it
	const std::uint8_t * lut = nullptr;  //!< the grading LUT, 16^3 RGB8 (null: none); uploaded when lutGen changes
	std::uint64_t lutGen = 0;
	unsigned measure = 0;    //!< the measure's bloom, a GL 2D float texture (0: none), measureW x measureH
	int measureW = 0, measureH = 0;
	std::uint64_t measureGen = 0;  //!< copied into the shared image when it changes
	int vp[4] = {};                //!< GL's viewport (x, y, w, h; w 0: the whole frame): the pass's viewport and scissor
	unsigned drawFb = 0, readFb = 0;  //!< bound again after the measure's copy (no glGet)
	/* lane VKFAR2: flag 9 (cellFogBloomOn): the fog bloom's shared R8 target (fogBloomRun's sharedTarget key and GL name,
	 * the frame's size and samples), read per sample at binding 5; key 0 refuses (GL drew its own target) */
	std::uintptr_t fogKey = 0;
	unsigned fogTex = 0;
	int fogRed = 0;  //!< WW_VKVIEW_TONE_FOGBLOOM_RED=1: the pass ignores the target (gain 1; must FAIL the view gate)
};
/* GL current, the view (viewBegin) bound and the frame's depth already in it: the tone draws on Vulkan into the view.
 * false: why; nothing drawn (GL draws its quads). */
bool toneRun( const ToneRun & t, std::string & why );
//! lane VKBLOOM1: the bloom can run in the tone pass (the stage made, not broken; the frame multisample)
bool toneBloomReady();
//! lane VKBLOOM1: toneRun calls that took a bloom (cellbloom.cpp resets its hold when one was lost)
std::uint64_t toneBloomRuns();
//! lane VKBLOOM1: the last toneRun recorded the measure it was asked for
bool toneMeasureRan();
/* lane VKBLOOM1: the measure's quarter values (RGBA32F, qw x qh) read back in the pass's command buffer. 1: px valid until
 * the next toneRun, tex its GL name (the shared target; GL may blur it); 0: none waits; -1: its pass is not done yet. */
int toneMeasureTake( const float *& px, int & qw, int & qh, unsigned & tex );
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
	int deferred = 0, flushed = 0;     //!< lane VKPOST2: runs left for the TAA's crossing; of those, crossed alone (toneFlush)
	int bloom = 0, measure = 0;        //!< lane VKBLOOM1: runs that carried the Fusion Shaders bloom / the exposure measure
};
ToneStat takeToneStat();
/* Lane VKPOST2: the merged crossing. toneRun may DEFER its crossing (state recorded, no GL signal) when the last TAA ran
 * on Vulkan right after it: the TAA's crossing then carries the tone pass too (TaaRun::colorFromView). Any GL pass that
 * reads or draws the view between the two calls toneFlush() first (GL current): the deferred pass crosses alone.
 * tonePending: a deferred pass waits. Red WW_VKVIEW_MERGE_RED=1: never deferred (two crossings). */
bool tonePending();
bool toneFlush( std::string * why = nullptr );
//! the pipeline cache file (empty: none); the Qt part sets it before the first toneFrame
void toneCachePath( const std::string & path );

/* Lane VKSHARE1: single-sample targets another pass owns (the clouds' march and history, TAA's), made on the present
 * device and imported into GL, keyed by the owner. type 0 = 2D, 1 = 3D (d deep), 2 = 2D array (d layers); glFmt one of
 * GL_RGBA16F / RGBA32F / RGBA8 / RG32F / RG16F / R32F / R8 / DEPTH24_STENCIL8. GL current. Same key and shape: the same
 * name; another shape: the old one goes first (a full drain). false: why; tex 0 (the owner makes GL's own). The GL
 * texture has NEAREST filters; the owner sets its own. levels (lane VKAIR1): a mip chain (colour formats; the clouds'
 * noise volumes), the owner fills each level. samples (lane VKOBJ1): a multisample 2D (type 0, one level; the objects'
 * depth prepass in GL_DEPTH_COMPONENT24 / 32F), GL_TEXTURE_2D_MULTISAMPLE with fixed sample locations. */
bool sharedTarget( std::uintptr_t key, int type, int w, int h, int d, unsigned glFmt, unsigned & tex, std::string & why,
	int levels = 1, int samples = 1 );
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
	bool colorFromView = false;     //!< lane VKPOST2: GL skipped its colour blit; Vulkan resolves the view into cur behind
	                                //!< the deferred tone pass (one crossing for both). Refused without a deferred pass.
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
	int merged = 0;                    //!< lane VKPOST2: runs that carried the deferred tone pass (one crossing)
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
	//! lane VKFAR2 G2 (vkskybody.h): stars / moon draws in the sky's pass, bodies sent to GL, held crossings that failed
	int stars = 0, moons = 0, bodyGl = 0, flushFails = 0;
	std::string bodyWhy;
};
SkyStat takeSkyStat();

/* Lane VKLIGHT1 (plan rows L1-L3): AO (ao_vk.hlsl: gbuf, 4 mips, raw, across, fin), the contact shadow lines
 * (contact_vk.hlsl) and the SSGI cast (ssgi_vk.hlsl) as Vulkan compute on the present device. The caller lists GL's own
 * textures and the dispatches its GL stages would draw; lightRun copies the `in` textures into shared twins, GL signals,
 * the worker records the dispatches (LightBindingsVk.hlsli's set, a 128-byte push block each), GL waits and copies the
 * `out` twins back into GL's textures, so every consumer reads what it read before. false: why; nothing changed (GL draws
 * its stages). Reds WW_VKVIEW_AO_RED / _CONTACT_RED / _SSGI_RED = 1 (or WW_VKVIEW off): GL. */
enum LightPass { LightAo = 0, LightContact = 1, LightSsgi = 2, LightPasses = 3 };
enum LightKernel { LightAoGbuf = 0, LightAoMip, LightAoRaw, LightAoBlur, LightAoBlurF, LightContactK, LightSsgiK, LightKernels };
struct LightImage
{
	unsigned tex = 0;              //!< GL's texture (GL_TEXTURE_2D)
	unsigned glFmt = 0;            //!< its internal format: RGBA32F / RGBA16F / RG32F / R32F / R8 / RG8
	int w = 0, h = 0, levels = 1;  //!< levels > 1: every level is copied in (an input only)
	bool in = false, out = false;  //!< copied into the twin before / back into GL's texture after
};
struct LightDispatch
{
	int kernel = 0;
	std::uint8_t pc[128] = {};     //!< LpxJob
	int out0 = -1, out1 = -1;      //!< image indices (storage targets)
	int t[5] = { -1, -1, -1, -1, -1 };  //!< sampled images (-1: unbound)
	int gx = 0, gy = 0;            //!< gx > 0: these groups; else 8x8 groups over out0
};
struct LightRun
{
	int pass = 0;
	std::vector<LightImage> img;
	std::vector<LightDispatch> d;
	std::vector<std::uint8_t> buf; //!< binding 9 (the contact bands); empty: 256 zero bytes
};
//! GL current, its inputs drawn: the dispatches run on Vulkan and the outputs are back in GL's textures
bool lightRun( const LightRun & t, std::string & why );
//! a frame whose pass stayed on GL while it was wanted, and why (the telemetry)
void lightOnGl( int pass, const char * why );
struct LightStat
{
	int runs = 0, gl = 0, fails = 0;   //!< since the last take
	double cpuMs = 0.0, maxMs = 0.0;   //!< the drawing thread's lightRun ms
	std::string glWhy, failWhy;
	int pipelines = 0;
	double buildMs = 0.0;
};
LightStat takeLightStat( int pass );

/* Lane VKLIGHT2 (row L4): the reflections' full-screen stages (cell_ssr.frag 5, 1-4) as Vulkan compute,
 * res/shaders/hlsl/vklight/ssr_vk.hlsl (SSR_LIVE). GL keeps stage 0, the scene pass and the water's rays; the pass's own
 * targets (scene, water ray, ray, raw, across, fin) are shared (sharedTarget); the g-buffer, the depth mips (one R32F atlas,
 * side by side) and the previous frame are copied into twins. One crossing. Red WW_VKVIEW_SSR_RED=1: GL's quads. */
struct SsrRun
{
	int W = 0, H = 0, hw = 0, hh = 0;
	unsigned gbuf = 0;                      //!< RGBA32F W x H (the obscurance pass's)
	unsigned mip[5] = {};                   //!< 1..4: R32F min-of-2x2 depth
	int mipW[5] = {}, mipH[5] = {};
	std::uintptr_t gbufKey = 0, atlasKey = 0, prevKey = 0;  //!< the twins' keys
	unsigned prev = 0, prevFmt = 0;         //!< the previous frame (RGBA16F or RGBA32F), when prevOn
	unsigned scene = 0, wray = 0, ray = 0, raw = 0, across = 0, fin = 0;  //!< the shared targets' GL names
	std::uintptr_t sceneKey = 0, wrayKey = 0, rayKey = 0, rawKey = 0, acrossKey = 0, finKey = 0;
	bool haveWater = false, waterLook = false, prevOn = false, stage5 = false, march = false;
	float c[6][4] = {};                     //!< ssr_vk.hlsl's push c[0..5] (ssrRun adds c[6], the atlas offsets)
	int i[4] = {};
	float prevM[9] = {}, prevT[3] = {}, prevProj[2] = {};  //!< M row-major (GL's GL_TRUE upload)
	int red = 0;  //!< gates only: 1 atlas offsets off by one, 2 M untransposed, 4 water look on the base pipelines, 8 no stage 5
};
//! GL current, the scene pass and the water's rays drawn: stage 5 (stage5) and 1-4 (march) on Vulkan. false: why; GL runs them
bool ssrRun( const SsrRun & t, std::string & why );
void ssrOnGl( const char * why );
struct SsrStat
{
	int runs = 0, gl = 0, fails = 0;
	double cpuMs = 0.0, maxMs = 0.0;
	std::string glWhy, failWhy;
	int pipelines = 0;
	double buildMs = 0.0;
};
SsrStat takeSsrStat();

/* Lane VKSHADOW1 (plan row G6): the sun cascades' queued solid casters (one-sided and two-sided) as depth-only draws on the
 * present device into the shared D16 2D arrays GL samples (sharedTarget type 2, GL_DEPTH_COMPONENT16). GL keeps the clears,
 * copies, remaps, the alpha-tested queue and the per-draw casters; a depth-only LEQUAL pass keeps the nearest depth in any
 * order, so the queued draws run once, after GL's pass, in one crossing. The pool (vec3 float positions, u16 indices) is
 * copied each run into buffers GL imports; matrices and commands are host-visible. Red WW_VKVIEW_CSM_RED=1: GL's draws. */
struct CsmDraw
{
	int target = 0, layer = 0;      //!< tex[target], its layer
	int vw = 0, vh = 0;             //!< the cascade's viewport (0, 0, vw, vh)
	int sc[4] = {};                 //!< scissor x0, y0, x1, y1 (GL window rows from the bottom); x1 <= x0: none
	float clip[16] = {};            //!< csmClipFromView, GL column-major
	float unit = 0.0f, slope = 0.0f, clamp = 0.0f;  //!< glPolygonOffset( slope, unit ), its clamp (0: none)
	bool cull = true;               //!< back faces culled (GL CCW front); false: two-sided
	std::uint32_t first = 0, n = 0; //!< commands [first, first + n)
};
struct CsmRun
{
	int map = 0, layers = 0;        //!< the targets' size and layer count
	unsigned tex[2] = {};           //!< the shared targets' GL names (0: unused)
	std::uintptr_t key[2] = {};     //!< their sharedTarget keys
	unsigned pos = 0, idx = 0;      //!< GL's pool buffers
	std::size_t posBytes = 0, idxBytes = 0;
	const float * mats = nullptr;   //!< 16 floats per caster, GL column-major
	std::size_t nMats = 0;
	const std::uint32_t * cmds = nullptr;  //!< 5 words per command (VkDrawIndexedIndirectCommand)
	std::size_t nCmds = 0;
	std::vector<CsmDraw> d;
};
//! GL current, its own cascade draws issued: the queued draws run on Vulkan into the shared targets. false: why (GL draws them)
bool csmRun( const CsmRun & t, std::string & why );
void csmOnGl( const char * why );
struct CsmStat
{
	int runs = 0, gl = 0, fails = 0;
	double cpuMs = 0.0, maxMs = 0.0;
	std::string glWhy, failWhy;
	int pipelines = 0, draws = 0, cmds = 0;
	double buildMs = 0.0;
};
CsmStat takeCsmStat();
//! what csmRun cannot take on this device (empty: none); asked before GL skips its queued draws
std::string csmRefusal();
/* Lane VKAIR1 (rows L5, L6): the air's sky pass (cell_volsky.frag, WW_CELL_FOGLEAK's twin) and the clouds' march + resolve
 * (cloud_march / _a, cloud_resolve / _a) as Vulkan fragment passes, res/shaders/hlsl/{air,clouds}. Each GL program's
 * uniforms go over as its LooseUniforms block (b31) packed at the offsets below (spirv-cross --reflect of the shipped
 * SPIR-V, notes/vkair1/tools/reflect_table.py); the air also takes b0 (the global buffer's bytes). Inputs GL owns are
 * copied into shared twins (sharedTarget, glCopyImageSubData a level), or read as shared targets with no copy (the air's
 * packed volume, the clouds' march and history). One crossing a pass. Reds WW_VKVIEW_AIR_RED=1, WW_VKVIEW_CLOUD_RED=1: GL. */
struct FxField
{
	const char * name;
	int offset, words, count, isInt, cols;  //!< bytes into b31; floats (ints) a member, rows of a matrix; array length; cols > 0: a matrix
};
// kT_volsky: 53 fields, 848 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_volsky[] = {
	{ "fogNightHue", 0, 1, 1, 0, 0 }, { "fogNight", 16, 4, 1, 0, 0 }, { "atAp", 32, 4, 1, 0, 0 },
	{ "atApProj", 48, 4, 1, 0, 0 }, { "fogRed", 64, 1, 1, 1, 0 }, { "fogK", 80, 4, 6, 0, 0 },
	{ "fogView", 176, 4, 1, 0, 0 }, { "volAirTerrU", 192, 4, 1, 0, 0 }, { "volAirTerrV", 208, 4, 1, 0, 0 },
	{ "volAirTerrFU", 224, 4, 1, 0, 0 }, { "volAirTerrFV", 240, 4, 1, 0, 0 }, { "volAirFlags", 256, 1, 1, 1, 0 },
	{ "volAirGround", 272, 4, 1, 0, 0 }, { "fogDistScale", 288, 1, 1, 0, 0 }, { "volAirTerrRef", 292, 1, 1, 0, 0 },
	{ "volAirMed", 304, 4, 1, 0, 0 }, { "volAirLayer", 320, 4, 1, 0, 0 }, { "volAirRain", 336, 4, 1, 0, 0 },
	{ "volAirRainXf", 352, 4, 1, 0, 4 }, { "volAirRainFar", 416, 4, 1, 0, 0 }, { "volPackDims", 432, 4, 1, 0, 0 },
	{ "volFar", 448, 4, 1, 0, 0 }, { "volSlices", 464, 4, 1, 0, 0 }, { "volPack", 480, 4, 1, 0, 0 },
	{ "volFarClamp", 496, 1, 1, 1, 0 }, { "volRect", 512, 4, 1, 0, 0 }, { "fogOn", 528, 1, 1, 1, 0 },
	{ "volOn", 532, 1, 1, 1, 0 }, { "atApDouble", 536, 1, 1, 0, 0 }, { "volFogBlend", 540, 1, 1, 1, 0 },
	{ "fogSun", 544, 4, 1, 0, 0 }, { "fogSunColour", 560, 4, 1, 0, 0 }, { "fogUnit", 576, 1, 1, 0, 0 },
	{ "volSkyMode", 580, 1, 1, 1, 0 }, { "fogProbe", 592, 4, 1, 0, 0 }, { "volSkyOn", 608, 1, 1, 1, 0 },
	{ "atSkyCamR", 612, 1, 1, 0, 0 }, { "atApSkyRule", 616, 1, 1, 1, 0 }, { "volSkySea", 620, 1, 1, 1, 0 },
	{ "volSkyDome", 624, 1, 1, 0, 0 }, { "volProbe", 628, 1, 1, 1, 0 }, { "volAirDrift", 640, 4, 1, 0, 0 },
	{ "volAirDriftUv", 656, 4, 6, 0, 0 }, { "volAirDriftRed", 752, 1, 1, 1, 0 }, { "atRay", 756, 3, 1, 0, 0 },
	{ "atMieS", 768, 1, 1, 0, 0 }, { "atMieE", 772, 1, 1, 0, 0 }, { "atHaze", 784, 3, 1, 0, 0 },
	{ "atOzone", 800, 3, 1, 0, 0 }, { "atG", 812, 1, 1, 0, 0 }, { "atHazeAlbedo", 816, 3, 1, 0, 0 },
	{ "atHazeSpec", 832, 3, 1, 0, 0 }, { "atMsOld", 844, 1, 1, 0, 0 },
};
inline constexpr int kT_volskyBytes = 848;
// kT_march: 56 fields, 1152 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_march[] = {
	{ "cvCal", 0, 1, 34, 0, 0 }, { "cvBands.baseKm", 544, 3, 1, 0, 0 }, { "cvBands.topKm", 560, 3, 1, 0, 0 },
	{ "cvBands.density", 576, 3, 1, 0, 0 }, { "cvBands.coverage", 592, 3, 1, 0, 0 }, { "cvBands.sheet", 604, 1, 1, 0, 0 },
	{ "cvBands.mapRect", 608, 4, 1, 0, 0 }, { "cvBands.windKm", 624, 3, 1, 0, 0 }, { "cvSlab", 640, 1, 1, 0, 0 },
	{ "cvSunW", 644, 3, 1, 0, 0 }, { "cvMoonW", 656, 3, 1, 0, 0 }, { "cvSunK", 668, 1, 1, 0, 0 },
	{ "cvMoonK", 672, 1, 1, 0, 0 }, { "cvCamR", 676, 1, 1, 0, 0 }, { "cvViewSize", 680, 2, 1, 0, 0 },
	{ "cvShaft", 688, 4, 1, 0, 0 }, { "cvOctaves", 704, 1, 1, 1, 0 }, { "cvProbe", 708, 1, 1, 1, 0 },
	{ "cvProbeRect", 720, 4, 1, 0, 0 }, { "cvProbeBand", 736, 1, 1, 1, 0 }, { "cvStride", 740, 1, 1, 0, 0 },
	{ "cvPhase", 744, 2, 1, 0, 0 }, { "cvPhasePrm", 752, 4, 1, 0, 0 }, { "cvProj", 768, 4, 1, 0, 0 },
	{ "cvHistSize", 784, 2, 1, 0, 0 }, { "cvCam", 800, 3, 1, 0, 3 }, { "cvCamKm", 848, 3, 1, 0, 0 },
	{ "cvMaxKm", 860, 1, 1, 0, 0 }, { "cvSceneZ", 864, 4, 1, 0, 0 }, { "cvKeyW", 880, 3, 1, 0, 0 },
	{ "cvSteps", 892, 1, 1, 1, 0 }, { "cvFrame", 896, 1, 1, 0, 0 }, { "cvIce", 900, 3, 1, 0, 0 },
	{ "cvTaps", 912, 1, 1, 1, 0 }, { "cvLightKm", 916, 1, 1, 0, 0 }, { "cvKeyK", 920, 1, 1, 0, 0 },
	{ "cvAlbedoD", 928, 3, 1, 0, 0 }, { "cvMsOn", 940, 1, 1, 0, 0 }, { "cvMsG", 944, 1, 1, 0, 0 },
	{ "cvTopOn", 948, 1, 1, 0, 0 }, { "cvTint", 960, 3, 3, 0, 0 }, { "cvAmbDark", 1008, 1, 1, 0, 0 },
	{ "cvWaterK", 1012, 3, 1, 0, 0 }, { "cvAmbOn", 1024, 1, 1, 0, 0 }, { "cvAmb", 1028, 3, 1, 0, 0 },
	{ "cvApOn", 1040, 1, 1, 0, 0 }, { "cvDeckAp", 1056, 4, 1, 0, 0 }, { "atRay", 1072, 3, 1, 0, 0 },
	{ "atMieS", 1084, 1, 1, 0, 0 }, { "atMieE", 1088, 1, 1, 0, 0 }, { "atHaze", 1092, 3, 1, 0, 0 },
	{ "atOzone", 1104, 3, 1, 0, 0 }, { "atG", 1116, 1, 1, 0, 0 }, { "atHazeAlbedo", 1120, 3, 1, 0, 0 },
	{ "atHazeSpec", 1136, 3, 1, 0, 0 }, { "atMsOld", 1148, 1, 1, 0, 0 },
};
inline constexpr int kT_marchBytes = 1152;
// kT_march_a: 57 fields, 1168 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_march_a[] = {
	{ "cvCal", 0, 1, 34, 0, 0 }, { "cvBands.baseKm", 544, 3, 1, 0, 0 }, { "cvBands.topKm", 560, 3, 1, 0, 0 },
	{ "cvBands.density", 576, 3, 1, 0, 0 }, { "cvBands.coverage", 592, 3, 1, 0, 0 }, { "cvBands.sheet", 604, 1, 1, 0, 0 },
	{ "cvBands.mapRect", 608, 4, 1, 0, 0 }, { "cvBands.windKm", 624, 3, 1, 0, 0 }, { "cvSlab", 640, 1, 1, 0, 0 },
	{ "cvBlueSlice", 644, 1, 1, 1, 0 }, { "cvFrame", 648, 1, 1, 0, 0 }, { "cvSunW", 656, 3, 1, 0, 0 },
	{ "cvMoonW", 672, 3, 1, 0, 0 }, { "cvSunK", 684, 1, 1, 0, 0 }, { "cvMoonK", 688, 1, 1, 0, 0 },
	{ "cvCamR", 692, 1, 1, 0, 0 }, { "cvViewSize", 696, 2, 1, 0, 0 }, { "cvShaft", 704, 4, 1, 0, 0 },
	{ "cvOctaves", 720, 1, 1, 1, 0 }, { "cvProbe", 724, 1, 1, 1, 0 }, { "cvProbeRect", 736, 4, 1, 0, 0 },
	{ "cvProbeBand", 752, 1, 1, 1, 0 }, { "cvStride", 756, 1, 1, 0, 0 }, { "cvPhase", 760, 2, 1, 0, 0 },
	{ "cvPhasePrm", 768, 4, 1, 0, 0 }, { "cvProj", 784, 4, 1, 0, 0 }, { "cvHistSize", 800, 2, 1, 0, 0 },
	{ "cvCam", 816, 3, 1, 0, 3 }, { "cvCamKm", 864, 3, 1, 0, 0 }, { "cvMaxKm", 876, 1, 1, 0, 0 },
	{ "cvSceneZ", 880, 4, 1, 0, 0 }, { "cvKeyW", 896, 3, 1, 0, 0 }, { "cvSteps", 908, 1, 1, 1, 0 },
	{ "cvIce", 912, 3, 1, 0, 0 }, { "cvTaps", 924, 1, 1, 1, 0 }, { "cvLightKm", 928, 1, 1, 0, 0 },
	{ "cvKeyK", 932, 1, 1, 0, 0 }, { "cvAlbedoD", 944, 3, 1, 0, 0 }, { "cvMsOn", 956, 1, 1, 0, 0 },
	{ "cvMsG", 960, 1, 1, 0, 0 }, { "cvTopOn", 964, 1, 1, 0, 0 }, { "cvTint", 976, 3, 3, 0, 0 },
	{ "cvAmbDark", 1024, 1, 1, 0, 0 }, { "cvWaterK", 1028, 3, 1, 0, 0 }, { "cvAmbOn", 1040, 1, 1, 0, 0 },
	{ "cvAmb", 1044, 3, 1, 0, 0 }, { "cvApOn", 1056, 1, 1, 0, 0 }, { "cvDeckAp", 1072, 4, 1, 0, 0 },
	{ "atRay", 1088, 3, 1, 0, 0 }, { "atMieS", 1100, 1, 1, 0, 0 }, { "atMieE", 1104, 1, 1, 0, 0 },
	{ "atHaze", 1108, 3, 1, 0, 0 }, { "atOzone", 1120, 3, 1, 0, 0 }, { "atG", 1132, 1, 1, 0, 0 },
	{ "atHazeAlbedo", 1136, 3, 1, 0, 0 }, { "atHazeSpec", 1152, 3, 1, 0, 0 }, { "atMsOld", 1164, 1, 1, 0, 0 },
};
inline constexpr int kT_march_aBytes = 1168;
// kT_resolve: 12 fields, 192 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_resolve[] = {
	{ "cvMoveClamp", 0, 1, 1, 0, 0 }, { "cvHistSize", 4, 2, 1, 0, 0 }, { "cvStride", 12, 1, 1, 0, 0 },
	{ "cvPhase", 16, 2, 1, 0, 0 }, { "cvHistOn", 24, 1, 1, 0, 0 }, { "cvReproj", 28, 1, 1, 0, 0 },
	{ "cvCam", 32, 3, 1, 0, 3 }, { "cvProj", 80, 4, 1, 0, 0 }, { "cvPrevCam", 96, 3, 1, 0, 3 },
	{ "cvCamKm", 144, 3, 1, 0, 0 }, { "cvPrevCamKm", 160, 3, 1, 0, 0 }, { "cvPrevProj", 176, 4, 1, 0, 0 },
};
inline constexpr int kT_resolveBytes = 192;
// kT_resolve_a: 20 fields, 236 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_resolve_a[] = {
	{ "cvMoveClamp", 0, 1, 1, 0, 0 }, { "cvHistSize", 4, 2, 1, 0, 0 }, { "cvStride", 12, 1, 1, 0, 0 },
	{ "cvReproj", 16, 1, 1, 0, 0 }, { "cvCam", 32, 3, 1, 0, 3 }, { "cvProj", 80, 4, 1, 0, 0 },
	{ "cvPrevCam", 96, 3, 1, 0, 3 }, { "cvCamKm", 144, 3, 1, 0, 0 }, { "cvPrevCamKm", 160, 3, 1, 0, 0 },
	{ "cvPrevProj", 176, 4, 1, 0, 0 }, { "cvPhase", 192, 2, 1, 0, 0 }, { "cvHistAge", 200, 1, 1, 0, 0 },
	{ "cvHistOn", 204, 1, 1, 0, 0 }, { "cvAccReset", 208, 1, 1, 0, 0 }, { "cvAgeK", 212, 1, 1, 0, 0 },
	{ "cvAccResetSd", 216, 1, 1, 0, 0 }, { "cvAccAlpha", 220, 1, 1, 0, 0 }, { "cvAccClamp", 224, 1, 1, 0, 0 },
	{ "cvLitReset", 228, 1, 1, 0, 0 }, { "cvLitFloor", 232, 1, 1, 0, 0 },
};
inline constexpr int kT_resolve_aBytes = 240;
// lane VKWATER2: kFxMipRepeatAniso = the mip repeat at GL's aniso 16 (the water's moments / noise), plain without the feature
enum FxSampler { kFxLinearClamp, kFxNearestClamp, kFxLinearRepeat, kFxMipRepeat, kFxNearestRepeat, kFxMipRepeatAniso, kFxSamplers };
//! one texture slot of a pass (HLSL t<i>): a GL texture copied into its twin, or a shared target read as is
struct FxTex
{
	unsigned tex = 0;               //!< GL's name (0 and no shared: a zero texture, as GL's unbound unit reads)
	int type = 0;                   //!< 0 2D, 1 3D, 2 2D array (sharedTarget's)
	int w = 0, h = 0, d = 1, levels = 1;
	unsigned glFmt = 0;
	std::uintptr_t twin = 0;        //!< the twin's sharedTarget key
	std::uintptr_t shared = 0;      //!< non-zero: tex IS this shared target (no copy)
	bool still = false;             //!< the source never changes once made (noise): copied when the twin or name is new
	int sampler = kFxLinearClamp;
};
constexpr int kFxGlobalBytes = 336;  //!< b0, as the sky's
struct VolSkyRun
{
	int w = 0, h = 0, vp[4] = {};
	unsigned color = 0;             //!< the tone frame's colour (toneFrameIs)
	bool overSky = false;           //!< blend ONE / ONE_MINUS_SRC_ALPHA, alpha ZERO / ONE (else none)
	FxTex tex[7];                   //!< t0 atApTex, t1 volAirTerr, t2 volAirRainMap, t3 volTex, t4 volTile, t5 atSkyTransTex, t6 volAirNoise
	std::uint8_t global[kFxGlobalBytes] = {};
	std::uint8_t loose[kT_volskyBytes] = {};
	int red = 0;                    //!< gates only: 1 stencil ref 2, 2 no blend over the sky
};
/* GL current, the tone frame bound, the uniforms set: the air over the sky drawn on Vulkan (stencil ALWAYS ref 1 REPLACE,
 * mask 0x03, no depth test, as GL's). false: why; nothing drawn (GL draws its quad). */
bool volSkyRun( const VolSkyRun & t, std::string & why );
void volSkyOnGl( const char * why );
struct FxStat
{
	int runs = 0, gl = 0, fails = 0;
	double cpuMs = 0.0, maxMs = 0.0;
	std::string glWhy, failWhy;
	int pipelines = 0;
	double buildMs = 0.0;
	int copies = 0;                 //!< texture copies into twins since the last take
};
FxStat takeVolSkyStat();
struct CloudRun
{
	bool accum = false;             //!< the _a twins (Cloud Accumulation)
	int mw = 0, mh = 0, hw = 0, hh = 0, cur = 0;
	std::uintptr_t marchKey = 0, histKey[2] = {};  //!< shared targets: key = RGBA16F colour, key + 1 = RG32F depth
	FxTex tex[8];                   //!< the march's t0.. (march: map shape detail sun moon scene trans; _a: + blue at t3)
	std::uint8_t march[kT_march_aBytes] = {};
	std::uint8_t resolve[kT_resolve_aBytes] = {};
	int red = 0;                    //!< gates only: 1 the resolve's march / history inputs swapped, 2 the march's t1 / t2
	                                //!< swapped, 4 no march (the caller filled the march targets)
};
//! GL current: the march (into the shared march targets) and the resolve (into hist[cur]) on Vulkan, one crossing
bool cloudRun( const CloudRun & t, std::string & why );
void cloudOnGl( const char * why );
FxStat takeCloudStat();

// lane VKFAR1 (row G3): the far bulb dots (cell_fardots_ps__WW_CELL_FOGLEAK's b31, reflected)
inline constexpr FxField kT_fardots[] = {
	{ "fogNightHue", 0, 1, 1, 0, 0 }, { "fogNight", 16, 4, 1, 0, 0 }, { "atAp", 32, 4, 1, 0, 0 },
	{ "atApProj", 48, 4, 1, 0, 0 }, { "fogRed", 64, 1, 1, 1, 0 }, { "fogK", 80, 4, 6, 0, 0 },
	{ "volAirTerrU", 176, 4, 1, 0, 0 }, { "volAirTerrV", 192, 4, 1, 0, 0 }, { "volAirTerrFU", 208, 4, 1, 0, 0 },
	{ "volAirTerrFV", 224, 4, 1, 0, 0 }, { "volAirFlags", 240, 1, 1, 1, 0 }, { "volAirGround", 256, 4, 1, 0, 0 },
	{ "fogDistScale", 272, 1, 1, 0, 0 }, { "volAirTerrRef", 276, 1, 1, 0, 0 }, { "volAirMed", 288, 4, 1, 0, 0 },
	{ "volAirLayer", 304, 4, 1, 0, 0 }, { "volAirRain", 320, 4, 1, 0, 0 }, { "volAirRainXf", 336, 4, 1, 0, 4 },
	{ "volAirRainFar", 400, 4, 1, 0, 0 }, { "volPackDims", 416, 4, 1, 0, 0 }, { "fogView", 432, 4, 1, 0, 0 },
	{ "volFar", 448, 4, 1, 0, 0 }, { "volSlices", 464, 4, 1, 0, 0 }, { "volPack", 480, 4, 1, 0, 0 },
	{ "volFarClamp", 496, 1, 1, 1, 0 }, { "volRect", 512, 4, 1, 0, 0 }, { "fogOn", 528, 1, 1, 1, 0 },
	{ "volOn", 532, 1, 1, 1, 0 }, { "atApDouble", 536, 1, 1, 0, 0 }, { "volFogBlend", 540, 1, 1, 1, 0 },
	{ "fogSun", 544, 4, 1, 0, 0 }, { "fogSunColour", 560, 4, 1, 0, 0 }, { "fogUnit", 576, 1, 1, 0, 0 },
	{ "fogProbe", 592, 4, 1, 0, 0 }, { "volProbe", 608, 1, 1, 1, 0 }, { "volAirDrift", 624, 4, 1, 0, 0 },
	{ "volAirDriftUv", 640, 4, 6, 0, 0 }, { "volAirDriftRed", 736, 1, 1, 1, 0 },
};
inline constexpr int kT_fardotsBytes = 752;
struct FarDotsRun
{
	int w = 0, h = 0, vp[4] = {};    //!< the tone frame's size, GL's viewport (the point size is in its pixels)
	unsigned color = 0;             //!< the tone frame's colour (toneFrameIs)
	int n = 0;                      //!< dots
	const float * pos = nullptr;    //!< n x 3, world
	const float * value = nullptr;  //!< n x 3, the bulb's intensity
	float dotRow[12] = {}, dotProj[16] = {}, dotCam[3] = {}, dotBand[2] = {};  //!< cell_fardots.vert's uniforms (dotProj GL's columns)
	float dotFocal = 0.0f, dotBulb = 4.0f;
	int dotRed = 0;
	FxTex tex[5];                   //!< t0 atApTex, t1 volAirTerr, t2 volAirRainMap, t3 volTex, t4 volTile
	std::uint8_t loose[kT_fardotsBytes] = {};
	int red = 0;                    //!< gates only: 1 stencil write mask 0, 2 no blend, 4 quads twice the point size
};
/* GL current, the tone frame bound, cell_fardots' uniforms set: the dots as instanced quads on Vulkan (depth LEQUAL no
 * write, blend ONE / ONE, stencil ALWAYS ref 1 REPLACE mask 0x03, as GL's). false: why; nothing drawn (GL draws its points) */
bool farDotsRun( const FarDotsRun & t, std::string & why );
void farDotsOnGl( const char * why );
FxStat takeFarDotsStat();

// lane VKFAR2 (row P4b): the fog bloom's own pass (fogbloom_view_ps's b31, spirv-cross --reflect of the shipped SPIR-V)
inline constexpr FxField kT_fogbloom[] = {
	{ "fogOn", 0, 1, 1, 1, 0 }, { "fogK", 16, 4, 6, 0, 0 }, { "fogView", 112, 4, 1, 0, 0 },
	{ "fogDistScale", 128, 1, 1, 0, 0 }, { "fogSun", 144, 4, 1, 0, 0 }, { "fogSunColour", 160, 4, 1, 0, 0 },
	{ "fogUnit", 176, 1, 1, 0, 0 }, { "fogProbe", 192, 4, 1, 0, 0 }, { "fogRed", 208, 1, 1, 1, 0 },
	{ "fogNight", 224, 4, 1, 0, 0 }, { "fogNightHue", 240, 1, 1, 0, 0 }, { "atAp", 256, 4, 1, 0, 0 },
	{ "atApProj", 272, 4, 1, 0, 0 }, { "atApDouble", 288, 1, 1, 0, 0 }, { "volOn", 292, 1, 1, 1, 0 },
	{ "volRect", 304, 4, 1, 0, 0 }, { "volFarClamp", 320, 1, 1, 1, 0 }, { "volSlices", 336, 4, 1, 0, 0 },
	{ "volProbe", 352, 1, 1, 1, 0 }, { "volFogBlend", 356, 1, 1, 1, 0 }, { "volFar", 368, 4, 1, 0, 0 },
	{ "volPack", 384, 4, 1, 0, 0 }, { "volPackDims", 400, 4, 1, 0, 0 }, { "volAirMed", 416, 4, 1, 0, 0 },
	{ "volAirLayer", 432, 4, 1, 0, 0 }, { "volAirGround", 448, 4, 1, 0, 0 }, { "volAirFlags", 464, 1, 1, 1, 0 },
	{ "volAirTerrU", 480, 4, 1, 0, 0 }, { "volAirTerrV", 496, 4, 1, 0, 0 }, { "volAirTerrFU", 512, 4, 1, 0, 0 },
	{ "volAirTerrFV", 528, 4, 1, 0, 0 }, { "volAirTerrRef", 544, 1, 1, 0, 0 }, { "volAirRain", 560, 4, 1, 0, 0 },
	{ "volAirRainFar", 576, 4, 1, 0, 0 }, { "volAirRainXf", 592, 4, 1, 0, 4 }, { "volAirDrift", 656, 4, 1, 0, 0 },
	{ "volAirDriftUv", 672, 4, 6, 0, 0 }, { "volAirDriftRed", 768, 1, 1, 1, 0 }, { "fbProj", 772, 2, 1, 0, 0 },
	{ "fbProjZ", 784, 2, 1, 0, 0 }, { "fbParams", 800, 4, 1, 0, 0 }, { "fbTranslucent", 816, 1, 1, 1, 0 },
};
inline constexpr int kT_fogbloomBytes = 832;
inline constexpr int kT_fogbloomTranslucentAt = 816;  //!< fbTranslucent: 0 the first draw's set, 1 the second's
struct FogBloomRun
{
	int w = 0, h = 0, samples = 0, vp[4] = {};  //!< the tone frame's size and samples, GL's viewport at the pass
	unsigned color = 0;             //!< the tone frame's colour (toneFrameIs)
	std::uintptr_t key = 0;         //!< the target's sharedTarget key (R8, the frame's samples)
	FxTex tex[4];                   //!< volTex, volTile, volAirTerr, volAirRainMap (fogbloom_view_ps's t26..t29)
	std::uint8_t loose[kT_fogbloomBytes] = {};
	int red = 0;                    //!< gates only: 1 no second draw, 2 the first draw with fbTranslucent 1, 4 fbProj.y doubled
};
/* GL current, cell_fogbloom's uniforms set: the bloom-intensity target drawn on Vulkan (cleared to 0; draw 1 every
 * sample, draw 2 the samples whose stencil has 0x04, fbTranslucent 1), reading the tone frame's depth (sample 0, as GL's
 * NEAREST blit keeps) and stencil. target: the shared R8 GL name the tone map binds. false: why; nothing drawn (GL's pass) */
bool fogBloomRun( const FogBloomRun & t, unsigned & target, std::string & why );
void fogBloomOnGl( const char * why );
FxStat takeFogBloomStat();

// lane VKFAR2 (row P8): the clouds in front of the scene (cloud_composite_view_ps's b31, spirv-cross --reflect of the shipped SPIR-V)
inline constexpr FxField kT_cloudfront[] = {
	{ "cvRect", 0, 4, 1, 0, 0 }, { "sceneExposure", 16, 1, 1, 0, 0 }, { "viewTransform", 20, 1, 1, 1, 0 },
	{ "cvFront", 24, 1, 1, 1, 0 }, { "cvSceneZ", 32, 4, 1, 0, 0 }, { "cvProj", 48, 4, 1, 0, 0 },
	{ "cvLtOn", 64, 1, 1, 1, 0 }, { "cvLtSrc", 80, 4, 1, 0, 0 }, { "cvLtPrm", 96, 4, 1, 0, 0 },
	{ "cvLtCol", 112, 3, 1, 0, 0 }, { "cvCam", 128, 3, 1, 0, 3 }, { "cvCamKm", 176, 3, 1, 0, 0 },
};
inline constexpr int kT_cloudfrontBytes = 192;
inline constexpr int kT_cloudfrontFrontAt = 24;
inline constexpr int kT_cloudfrontFlashAt = 64;  // cvLtOn: a flash frame stays GL's (cloudfrontview's flash job)  //!< cvFront (the pass runs mode 2 only)
struct CloudFrontRun
{
	int w = 0, h = 0, samples = 0, vp[4] = {};  //!< the tone frame's size and samples, GL's viewport at the pass
	unsigned color = 0;             //!< the tone frame's colour (toneFrameIs)
	FxTex tex[3];                   //!< cvHist, cvHistDepth (the clouds' shared history), cvScene (GL's depth max, copied)
	std::uint8_t loose[kT_cloudfrontBytes] = {};
	int red = 0;                    //!< gates only: 1 stencil test off, 2 no blend (written over), 4 cvFront 1 (the display mode)
};
/* GL current, cloud_composite's uniforms set with cvFront 2: the history over the tone frame's stencil-1 samples on Vulkan
 * (blend ONE / ONE_MINUS_SRC_ALPHA, alpha ZERO / ONE; stencil EQUAL 1 mask 0x03, no write; no depth test), as cloudvol.cpp's
 * frontPass( 2 ). false: why; nothing drawn (GL draws its quad) */
bool cloudFrontRun( const CloudFrontRun & t, std::string & why );
void cloudFrontOnGl( const char * why );
FxStat takeCloudFrontStat();

/* Lane VKFAR2 P9: the lens-flare sprites (celllensflare.cpp, cell_lensflare.vert/.frag) recorded in the DEFERRED tone pass,
 * after its modes, before the crossing (res/shaders/hlsl/post/lens_flare.hlsl; vkflare.inc). Only while tonePending(): no
 * GL pass came between the tone map and the flare. The sprite maps are the texture cache's BC twins (loaded with the twin
 * armed, gltex.cpp), sampled with GL's sampler state; the opaque depth is copied into a shared twin. GL current. false: why;
 * nothing queued (the caller flushes and draws on GL). */
constexpr int kFlareMaxSprites = 256;
struct FlareRun
{
	int n = 0;                       //!< sprites, GL's draw order
	const float * rows = nullptr;    //!< n x 4 corners x (pos, col, uv, lit) x 4 floats: GL's four attributes
	const unsigned * tex = nullptr;  //!< n: each sprite's GL 2D texture (the cache's name)
	float proj[16] = {};             //!< flareProj, GL's column order
	float slack = 0.0f;              //!< flareSlack
	int red = 0;                     //!< flareRed (bit 0: no occlusion)
	unsigned depth = 0;              //!< flareDepth (Scene::fxDepthTexId), vp[2] x vp[3]; 0: none (red bit 0)
	int vp[4] = {};                  //!< GL's viewport: the draws' and flareViewport
	GlProc gp = nullptr;             //!< the maps' sampler state and the depth's format
};
bool toneFlareQueue( const FlareRun & f, std::string & why );
void toneFlareOnGl( const char * why );
FxStat takeToneFlareStat();

/* Lane VKFAR2 P7: cellpost.cpp's depth of field (the game's or the thin lens), motion blur (alone or fused) and object blur
 * recorded in the DEFERRED tone pass before the flare (res/shaders/hlsl/post: dof_*, doflens_*, mblur, taa_mv, objblur_*,
 * post7_*; vkpost7.inc). The view's samples and depth are copied and resolved as GL's blits keep them (post7_resolve.hlsl),
 * the passes run as compute, the picture goes back into every sample (post7_copy_ps.hlsl). The object blur's velocity and
 * mask are copied into shared twins. Only while tonePending() on the tone-mapped view (not linear), D24S8, RG16F vectors.
 * GL current. false: why; nothing queued (the caller flushes and runs GL's passes). */
struct PostRun
{
	int w = 0, h = 0;                 //!< the frame (the view's size)
	int vp[4] = {};                   //!< GL's viewport: the copy back's
	bool dof = false, lens = false, mb = false, ob = false, fuse = false;
	int radius = 3, row = 2;          //!< the game's blur: taps 2 radius + 1, its weight row
	float dc[4][4] = {};              //!< the composite's c0..c3
	int farOnly = 0, dofRed = 0;
	float lz[4] = {}, lensP[4] = {}, lensQ[4] = {};
	float reproj[16] = {};            //!< the vectors' reprojection, GL's column order
	float jitter[2] = {};
	float mc[2][4] = {};              //!< the blur's mc0, mc1
	bool noVelocity = false;          //!< WW_CELL_MBLUR_RED=novelocity: the vectors cleared
	unsigned obVel = 0, obMask = 0;   //!< the inputs' velocity and mask (GL 2D textures, w x h)
	float obReproj[16] = {}, obJitter[2] = {};
	float shutter = 0.0f, maxPx = 0.0f;
	int tile = 16, samples = 15, objRed = 0;
	float zNear = 0.0f, zFar = 0.0f, seed = 0.0f;
	GlProc gp = nullptr;              //!< the twins' formats
};
bool tonePostQueue( const PostRun & p, std::string & why );
void tonePostOnGl( const char * why );
FxStat takeTonePostStat();

/* Lane VKOBJ1 (plan row G4, slice 1): the objects' opaque depth prepass (cellgpucull.cpp, lookdev_csmdepth_q.prog) on the
 * present device into a shared multisample depth target (sharedTarget type 0 with samples, GL_DEPTH_COMPONENT24 / 32F) that
 * GL's Hi-Z then reads. GL clears it; the two spans (back faces culled, then two-sided) run in one crossing (depth-only
 * LEQUAL: order-free), GL's clip z through VK_EXT_depth_clip_control, glPolygonOffset( slope, unit ) as the depth bias.
 * The pool is copied each run as for the cascades. Red WW_VKVIEW_OBJ_RED=1: GL's prepass. */
struct ObjRun
{
	int w = 0, h = 0, samples = 1;  //!< the target (viewport 0, 0, w, h)
	unsigned tex = 0;               //!< the shared target's GL name
	std::uintptr_t key = 0;         //!< its sharedTarget key
	unsigned pos = 0, idx = 0;      //!< GL's pool buffers
	std::size_t posBytes = 0, idxBytes = 0;
	const float * mats = nullptr;   //!< 16 floats per occluder, GL column-major
	std::size_t nMats = 0;
	const std::uint32_t * cmds = nullptr;  //!< 5 words per command (VkDrawIndexedIndirectCommand)
	std::size_t nCmds = 0;
	std::size_t split = 0;          //!< [0, split) back faces culled (GL CCW front), [split, nCmds) two-sided
	float clip[16] = {};            //!< csmClipFromView, GL column-major
	float unit = 2.0f, slope = 1.0f;  //!< glPolygonOffset( slope, unit )
	int red = 0;                    //!< gates only: 1 depth test off, 2 GL's winding taken as Vulkan's
};
//! GL current, the target cleared: the prepass runs on Vulkan. false: why (GL draws it)
bool objRun( const ObjRun & t, std::string & why );
void objOnGl( const char * why );
struct ObjStat
{
	int runs = 0, gl = 0, fails = 0;
	double cpuMs = 0.0, maxMs = 0.0;
	std::string glWhy, failWhy;
	int pipelines = 0, cmds = 0, twoSided = 0;
	double buildMs = 0.0;
};
ObjStat takeObjStat();
//! what objRun cannot take on this device (empty: none)
std::string objRefusal();

/* lane VKAIR1: a pass's b31 bytes from the program GL is about to draw with (glGetUniform*, arrays element by element,
 * a matrix's GL column k at + 16 k as the row_major HLSL reads it). Fn: the caller's GL functions (Qt's 4.x core) */
template <class Fn> void fxPack( Fn * fn, unsigned prog, const FxField * f, int n, std::uint8_t * out )
{
	for ( int i = 0; i < n; i++ ) {
		const FxField & x = f[i];
		const int stride = 16 * ( x.cols > 0 ? x.cols : 1 );
		for ( int e = 0; e < x.count; e++ ) {
			std::string nm( x.name );
			if ( x.count > 1 )
				nm += "[" + std::to_string( e ) + "]";
			const int l = fn->glGetUniformLocation( prog, nm.c_str() );
			if ( l < 0 )
				continue;  // not in the program: GL reads 0 there too
			std::uint8_t * at = out + x.offset + e * stride;
			float v[16] = {};
			if ( x.isInt )
				fn->glGetUniformiv( prog, l, reinterpret_cast<int *>( v ) );
			else
				fn->glGetUniformfv( prog, l, v );
			if ( x.cols > 0 ) {
				for ( int c = 0; c < x.cols; c++ )
					for ( int r = 0; r < x.words; r++ )
						reinterpret_cast<float *>( at + 16 * c )[r] = v[c * x.words + r];
			} else {
				for ( int w = 0; w < x.words; w++ )
					reinterpret_cast<float *>( at )[w] = v[w];
			}
		}
	}
}

/* lane VKAIR1: a texture slot from what the program samples now: the sampler's unit, the name bound there (type 0 2D,
 * 1 3D, 2 2D array), its size, format, levels and sampling. false: why (a state the Vulkan samplers do not hold). The
 * caller sets twin / shared / still. GL enums as numbers: this header carries no GL header */
template <class Fn> bool fxTexOf( Fn * fn, unsigned prog, const char * sampler, int type, FxTex & t, std::string & why )
{
	t.type = type;
	t.tex = 0;
	const int l = fn->glGetUniformLocation( prog, sampler );
	if ( l < 0 )
		return true;  // not in the program: the zero texture
	int unit = 0, active = 0x84C0;
	fn->glGetUniformiv( prog, l, &unit );
	fn->glGetIntegerv( 0x84E0, &active );  // GL_ACTIVE_TEXTURE
	fn->glActiveTexture( unsigned( 0x84C0 + unit ) );
	const unsigned target = type == 1 ? 0x806Fu : type == 2 ? 0x8C1Au : 0x0DE1u;
	int name = 0, sobj = 0;
	fn->glGetIntegerv( type == 1 ? 0x806A : type == 2 ? 0x8C1D : 0x8069, &name );  // GL_TEXTURE_BINDING_3D / 2D_ARRAY / 2D
	fn->glGetIntegerv( 0x8919, &sobj );  // GL_SAMPLER_BINDING
	bool ok = true;
	if ( name ) {
		int w = 0, h = 0, d = 1, fmt = 0, minF = 0, wrap = 0, maxL = 0;
		fn->glGetTexLevelParameteriv( target, 0, 0x1000, &w );  // GL_TEXTURE_WIDTH
		fn->glGetTexLevelParameteriv( target, 0, 0x1001, &h );
		if ( type )
			fn->glGetTexLevelParameteriv( target, 0, 0x8071, &d );  // GL_TEXTURE_DEPTH
		fn->glGetTexLevelParameteriv( target, 0, 0x1003, &fmt );  // GL_TEXTURE_INTERNAL_FORMAT
		fn->glGetTexParameteriv( target, 0x2801, &minF );  // GL_TEXTURE_MIN_FILTER
		fn->glGetTexParameteriv( target, 0x2802, &wrap );  // GL_TEXTURE_WRAP_S
		fn->glGetTexParameteriv( target, 0x813D, &maxL );  // GL_TEXTURE_MAX_LEVEL
		const bool mip = minF >= 0x2700 && minF <= 0x2703, repeat = wrap == 0x2901, nearest = minF == 0x2600;
		int levels = 1;
		while ( mip && levels <= maxL && levels < 16 ) {
			int lw = 0;
			fn->glGetTexLevelParameteriv( target, levels, 0x1000, &lw );
			if ( lw < 1 )
				break;
			levels++;
		}
		t.tex = unsigned( name );
		t.w = w;
		t.h = h;
		t.d = type ? d : 1;
		t.glFmt = unsigned( fmt );
		t.levels = levels;
		t.sampler = mip ? kFxMipRepeat : nearest ? ( repeat ? kFxNearestRepeat : kFxNearestClamp ) : repeat ? kFxLinearRepeat : kFxLinearClamp;
		if ( sobj ) {
			ok = false;
			why = std::string( sampler ) + ": a sampler object on its unit";
		} else if ( mip && ( !repeat || minF != 0x2703 ) ) {
			ok = false;
			why = std::string( sampler ) + ": a mipmapped sampling other than trilinear repeat";
		} else if ( w < 1 || h < 1 ) {
			ok = false;
			why = std::string( sampler ) + ": no level 0";
		}
	}
	fn->glActiveTexture( unsigned( active ) );
	return ok;
}

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
//! lane VKLIGHT1: the pass's route line ("vk ao:", "vk contact:", "vk ssgi:"), one a 5 s window while on Vulkan
void lightSay( int pass, bool wanted, const char * glWhy );
//! lane VKSHADOW1: the cascades' route line ("vk csm:"), one a 5 s window while on Vulkan
void csmSay( bool wanted, const char * glWhy );
//! lane VKOBJ1: the objects' prepass route line ("vk obj:"), one a 5 s window while on Vulkan
void objSay( bool wanted, const char * glWhy );
}
#endif

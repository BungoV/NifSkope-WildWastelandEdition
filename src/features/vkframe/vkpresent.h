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
/* lane VKWATER4: the fence waits' red and refusal, shared with vkframe.cpp. fenceRed: WW_VKFENCE_RED=<site>[,<site>...]
 * (gates only) names this site (a faked VK_TIMEOUT, pending or not). fenceMiss: a wait that returned vkResult (not
 * VK_SUCCESS): the why, also on stderr (`vk fence:`) */
bool fenceRed( const char * site );
std::string fenceMiss( const char * site, int vkResult );

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
//! lane VKFAR2 (row L8): the live shared frame's size (false: none in this context)
bool toneFrameSize( int & w, int & h );
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
 * GL_RGBA16F / RGBA32F / RGBA8 / RG32F / RG16F / R32F / R8 / DEPTH24_STENCIL8 (type 3, lane VKFAR6: a D16 cube array,
 * w = h, d layer-faces, 6 a cube; GL_DEPTH_COMPONENT16 only). GL current. Same key and shape: the same
 * name; another shape: the old one goes first (a full drain). false: why; tex 0 (the owner makes GL's own). The GL
 * texture has NEAREST filters; the owner sets its own. levels (lane VKAIR1): a mip chain (colour formats; the clouds'
 * noise volumes), the owner fills each level. samples (lane VKOBJ1): a multisample 2D (type 0, one level; the objects'
 * depth prepass in GL_DEPTH_COMPONENT24 / 32F), GL_TEXTURE_2D_MULTISAMPLE with fixed sample locations. type 3 (lane
 * VKOBJ4): a cube array of w x w, d layers (6 a cube, as farTwin counts) as GL_TEXTURE_CUBE_MAP_ARRAY (the lamps' D16 shadow
 * cubes, cellShadow). */
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

/* Lane VKOBJ4 item 2: the bloom chain (vkframe.cpp) on the present device. shareDevice (GL current): the present device,
 * made on GL's GPU if it is not yet (the view off too), and its handles for wwVkFrameSetDevice. onWorker: fn( arg ) runs
 * on the present worker (the queue is the worker's alone), the caller waits for it; false: fn's false (why). borrowTarget:
 * another owner's image on the present device joins the shared targets as a borrowed entry (objBlFrameRead finds GL's
 * framebuffer over it), never freed here; an image of another device is refused. GL current. */
struct Device
{
	void * instance = nullptr, * physical = nullptr, * device = nullptr, * queue = nullptr, * getDeviceProcAddr = nullptr;
	unsigned family = 0;
	unsigned char uuid[16] = {};
};
bool shareDevice( GlProc gl, Device & out, std::string & why );
bool onWorker( bool ( *fn )( void * arg, std::string & why ), void * arg, std::string & why );
struct Borrow
{
	void * device = nullptr, * image = nullptr, * view = nullptr;
	void * attView = nullptr;  //!< a depth / stencil format's depth and stencil view (the render pass attachment)
	unsigned tex = 0, glFmt = 0;  //!< the GL name in the current context; GL_RGBA16F / GL_DEPTH24_STENCIL8 / GL_DEPTH32F_STENCIL8
	int w = 0, h = 0, samples = 1;
};
bool borrowTarget( std::uintptr_t key, const Borrow & b, std::string & why );
void borrowDrop( std::uintptr_t key );

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
/* Lane VKWATER4 (row G5): the ultra ocean (cellocean.cpp) is pass LightOcean, kernels ocean_vk.hlsl stages 1-6; 7-10
 * (the *A kernels) write the water's shared arrays in place (LightImage::share), so nothing is copied. Red
 * WW_VKVIEW_OCEAN_RED=1 (or WW_VKVIEW off): GL's draws; =prev: the water reads the arrays the frame before wrote. */
// LightGi (lane VKFAR3 A1 b): the screen GI's own run (after the volume mask, which stays GL); red WW_VKVIEW_GIHALF_RED=1
// LightContactFar (lane VKSHADOW3): shadowset.cpp contactFarPass's three stages, contactfar_vk.hlsl; red WW_VKVIEW_CONTACTFAR_RED=1
enum LightPass { LightAo = 0, LightContact = 1, LightSsgi = 2, LightOcean = 3, LightGi = 4, LightContactFar = 5, LightPasses = 6 };
// LightAoxZ / Xe / R8 (lane VKFAR3): the ported AO methods' depth mips, XeGTAO's term + edges, the r8 stages (aox_vk.hlsl)
enum LightKernel { LightAoGbuf = 0, LightAoMip, LightAoRaw, LightAoBlur, LightAoBlurF, LightContactK, LightSsgiK, LightAoxZ, LightAoxXe,
	LightAoxR8, LightOceanSpecK, LightOceanFftK, LightOceanFinBcK, LightOceanFinDK, LightOceanMipBcK, LightOceanMipDK,
	LightOceanFinBcAK, LightOceanFinDAK, LightOceanMipBcAK, LightOceanMipDAK,
	LightGiHalf, LightCfarMin, LightCfarMarch, LightCfarComp, LightKernels };	// LightGiHalf (VKFAR3 A1 b): the screen GI's DDGI read (gihalf_vk.hlsl)
struct LightImage
{
	unsigned tex = 0;              //!< GL's texture (GL_TEXTURE_2D; 0 for a twin only the dispatches use)
	unsigned glFmt = 0;            //!< its internal format: RGBA32F / RGBA16F / RG32F / R32F / R8 / RG8
	int w = 0, h = 0, levels = 1;  //!< levels > 1: every level is copied in (an input only)
	bool in = false, out = false;  //!< copied into the twin before / back into GL's texture after
	int tiles = 0, outLevel = 0;   //!< tiles > 0 (an output): tex is a GL_TEXTURE_2D_ARRAY; the twin's `tiles` side-by-side
	                               //!< w / tiles wide tiles go back into its layers 0.. at level outLevel
	std::uintptr_t share = 0;      //!< lane VKWATER4: a sharedTarget key (2D or 2D array): the dispatches use that image's
	                               //!< level outLevel itself (every layer; w = its width x layers, h its height), no twin, no copy
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
	// lane VKFAR3 (A1 b), LightGiHalf: binding 31 = GL's uniforms (kT_gihalf), 44 = the DDGI atlas found under ddgiKey
	// (ddgiAtlasView; none: refused), 46 = the first dispatch's t[0] (the g-buffer). ddgiGlBuf (GL's name of that buffer)
	// joins GL's signal / wait lists
	std::vector<std::uint8_t> ubo; //!< empty: 256 zero bytes
	const void * ddgiKey = nullptr;
	unsigned ddgiGlBuf = 0;
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
	int copies = 0, shared = 0;        //!< lane VKWATER4: glCopyImageSubData calls / shared-target entries, since the last take
};
LightStat takeLightStat( int pass );
//! lane VKWATER4: the water draws' ocean inputs (waterOcean*) since the last take: copied into a twin / sampled in place
struct OceanWaterStat
{
	int copies = 0, shared = 0;
};
OceanWaterStat takeOceanWaterStat();

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
	bool mesh = false;              //!< lane VKSHADOW4: first / n index the run's mesh draws (CsmRun::mdraws), not the pool's commands
	/* lane VKSHADOW6: the cache's Scroll remap (sunshadow.cpp remap, lookdev_csmremap): one triangle over (0, 0, vw, vh) writes
	 * depth 1, or clamp(o * zScale + zBias, 0, 1) with o = CsmRun::scratch layer src at (x + srcX, y + srcY) inside
	 * [0, srcW) x [0, srcH); LEQUAL, no cull, no bias, no scissor (sc, clip, unit, slope, first, n unused) */
	bool remap = false;
	int src = 0, srcX = 0, srcY = 0, srcW = 0, srcH = 0;
	float zScale = 1.0f, zBias = 0.0f;
	/* lane VKSHADOW7: atTex != 0 (mesh draws only): alpha-tested (lookdev_csmdepth_at.frag), a fragment is discarded where
	 * texture( atTex, uv * uvScale + uvOffset ).a * atAlpha < atRef. atTex = a texture cache BC twin's GL name (objTexMake),
	 * sampled with the GL texture's own wrap and filters at anisotropy 1 (lane VKSHADOW9, ruling R1: GL's alpha test binds the
	 * same through features/shadows/shadowatsampler.h); every mesh the draws use needs CsmMesh::uv */
	unsigned atTex = 0;
	float uvScale[2] = { 1.0f, 1.0f }, uvOffset[2] = { 0.0f, 0.0f }, atAlpha = 1.0f, atRef = 0.0f;
	/* lane VKSHADOW10: sway (mesh draws only, not alpha-tested): lookdev_csmdepth.vert's far wind with lodTreeAnim 1
	 * (csm_sway_view_vs.hlsl): farSwayAmp, this cascade's farSwayTime, farSwayBlock (view space centre, half size) and
	 * globalUniforms.viewMatrix (GL column-major mat3); every mesh the draw uses needs CsmMesh::wind */
	bool sway = false;
	float swayAmp = 0.0f, swayTime = 0.0f, swayBlock[4] = {}, swayView[9] = {};
	//! lane VKSHADOW11: an alpha-tested draw from the QAlpha queue (sunshadow.cpp atSpan through csmSpanMDraws; CsmStat::qAlphaDraws)
	bool queued = false;
};
/* Lane VKSHADOW4 (item 3 part 1): a per-draw caster (sunshadow.cpp drawShape, the whole shape or its cullIdx ranges) as a
 * mesh csmRun keeps on the device while (key, gen) and its sizes match (vklamp.inc lampPool's law): key = the shape,
 * gen = the hash of the arrays GL's draw takes (CPU-skinned positions included) */
struct CsmMesh
{
	std::uintptr_t key = 0;
	std::uint64_t gen = 0;
	const float * pos = nullptr;          //!< vec3 positions
	std::uint32_t verts = 0;
	const std::uint16_t * idx = nullptr;  //!< triangle list, mesh-local
	std::uint32_t indices = 0;
	const float * uv = nullptr;           //!< lane VKSHADOW7: vec2 per vertex (alpha-tested draws; gen covers it too)
	const float * wind = nullptr;         //!< lane VKSHADOW10: vec4 colours per vertex, .a = the wind weight (sway draws; gen covers it)
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
	const CsmMesh * meshes = nullptr;      //!< lane VKSHADOW4: the per-draw casters' meshes
	std::size_t nMeshes = 0;
	const std::uint32_t * mdraws = nullptr;  //!< 4 words a draw: mesh, matrix (in mats), first index, index count (mesh-local)
	std::size_t nMDraws = 0;
	unsigned scratch = 0;           //!< lane VKSHADOW6: the remaps' source, a shared D16 array of `layers` layers (0: no remap draws)
	std::uintptr_t scratchKey = 0;
};
/* lane VKSHADOW11: a QAlpha queue span (sunshadow.cpp atSpan: n 5-word commands from first, all of one pooled shape whose
 * entry starts at index entryFirst) as mesh draws of that shape's CsmMesh. The pool holds the shape's triangles verbatim
 * (cellqueue.cpp poolEnsure), so the mesh-local first index = the command's firstIndex - entryFirst; empty commands add nothing */
inline void csmSpanMDraws( const std::uint32_t * cmds, std::size_t first, std::size_t n, std::uint32_t entryFirst, std::uint32_t mesh,
	std::uint32_t matrix, std::vector<std::uint32_t> & out )
{
	for ( std::size_t k = first; k < first + n; k++ ) {
		const std::uint32_t * c = cmds + k * 5;
		if ( c[0] && c[2] >= entryFirst )
			out.insert( out.end(), { mesh, matrix, c[2] - entryFirst, c[0] } );
	}
}
//! GL current, its own cascade draws issued: the queued draws run on Vulkan into the shared targets. false: why (GL draws them)
bool csmRun( const CsmRun & t, std::string & why );
//! lane VKSHADOW9: csmRun would take this alpha map (a BC twin, not sRGB with its decode skipped); false: the caster stays on GL
bool csmAtTexOk( unsigned name, std::string & why );
void csmOnGl( const char * why );
struct CsmStat
{
	int runs = 0, gl = 0, fails = 0;
	double cpuMs = 0.0, maxMs = 0.0;
	std::string glWhy, failWhy;
	int pipelines = 0, draws = 0, cmds = 0;
	double buildMs = 0.0;
	//! lane VKSHADOW4, the mesh pool: meshes copied / served kept, copies for a new generation, repacks, gates-only red skips
	//! (WW_VKCSM_POOL_RED: 1 the key alone, 2 grow without copying the kept meshes again)
	int copies = 0, hits = 0, regen = 0, repacks = 0, redSkips = 0, mdraws = 0;
	//! lane VKSHADOW5: queue draws that follow a mesh draw inside one render pass (csmWork rebinds the queue's buffers there)
	int requeues = 0;
	//! lane VKSHADOW6: remap draws run, and of those on the gates-only red pipeline (WW_VKCSM_REMAP_RED=1: GL_ALWAYS)
	int remaps = 0, remapRed = 0;
	//! lane VKSHADOW7: alpha-tested draws run, the distinct textures they bound in the run
	int alphaDraws = 0, alphaTex = 0;
	//! lane VKSHADOW10: sway draws run
	int swayDraws = 0;
	//! lane VKSHADOW11: of the alpha-tested draws, those from the QAlpha queue (CsmDraw::queued)
	int qAlphaDraws = 0;
	double copyKb = 0.0;
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
// kT_gihalf (lane VKFAR3): the 13 fields gihalf_vk.hlsl reads of its 176 (4112 bytes; spirv-cross --reflect of the
// shipped SPIR-V, reflect_table.py, cut to the used ones by spirv-cross --hlsl)
inline constexpr FxField kT_gihalf[] = {
	{ "cellOn", 1248, 1, 1, 1, 0 }, { "cellRow", 1264, 4, 3, 0, 0 }, { "cellGiLoop8", 3604, 1, 1, 1, 0 },
	{ "cellDdgiOn", 3772, 1, 1, 1, 0 }, { "cellDdgiCount", 3776, 1, 1, 1, 0 }, { "cellDdgiCOrigin", 3792, 4, 4, 0, 0 },
	{ "cellDdgiCStep", 3856, 4, 4, 0, 0 }, { "cellDdgiCDims", 3920, 4, 4, 1, 0 }, { "cellDdgiCBase", 3984, 1, 4, 1, 0 },
	{ "cellDdgiRed", 4048, 1, 1, 1, 0 }, { "giFull", 4084, 2, 1, 1, 0 }, { "giProj", 4096, 2, 1, 0, 0 }, { "giStep", 4104, 1, 1, 1, 0 },
};
inline constexpr int kT_gihalfBytes = 4112;
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
// lane VKCLOUD2: the march with the fly-through box (cloud_fly_ps / cloud_fly_a_ps: + cf*, the box skip)
// kT_fly: 64 fields, 1236 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_fly[] = {
	{ "atRay", 0, 3, 1, 0, 0 }, { "atMieS", 12, 1, 1, 0, 0 }, { "atMieE", 16, 1, 1, 0, 0 }, { "atHaze", 20, 3, 1, 0, 0 },
	{ "atOzone", 32, 3, 1, 0, 0 }, { "atG", 44, 1, 1, 0, 0 }, { "atHazeAlbedo", 48, 3, 1, 0, 0 },
	{ "atHazeSpec", 64, 3, 1, 0, 0 }, { "atMsOld", 76, 1, 1, 0, 0 }, { "cvBands.baseKm", 80, 3, 1, 0, 0 },
	{ "cvBands.topKm", 96, 3, 1, 0, 0 }, { "cvBands.density", 112, 3, 1, 0, 0 }, { "cvBands.coverage", 128, 3, 1, 0, 0 },
	{ "cvBands.sheet", 140, 1, 1, 0, 0 }, { "cvBands.mapRect", 144, 4, 1, 0, 0 }, { "cvBands.windKm", 160, 3, 1, 0, 0 },
	{ "cvCal", 176, 1, 34, 0, 0 }, { "cvViewSize", 720, 2, 1, 0, 0 }, { "cvCamR", 728, 1, 1, 0, 0 },
	{ "cvSunW", 736, 3, 1, 0, 0 }, { "cvMoonW", 752, 3, 1, 0, 0 }, { "cvSunK", 764, 1, 1, 0, 0 },
	{ "cvMoonK", 768, 1, 1, 0, 0 }, { "cvKeyW", 772, 3, 1, 0, 0 }, { "cvKeyK", 784, 1, 1, 0, 0 },
	{ "cvCam", 800, 3, 1, 0, 3 }, { "cvProj", 848, 4, 1, 0, 0 }, { "cvCamKm", 864, 3, 1, 0, 0 },
	{ "cvHistSize", 880, 2, 1, 0, 0 }, { "cvStride", 888, 1, 1, 0, 0 }, { "cvPhase", 896, 2, 1, 0, 0 },
	{ "cvSteps", 904, 1, 1, 1, 0 }, { "cvTaps", 908, 1, 1, 1, 0 }, { "cvOctaves", 912, 1, 1, 1, 0 },
	{ "cvLightKm", 916, 1, 1, 0, 0 }, { "cvPhasePrm", 928, 4, 1, 0, 0 }, { "cvAmb", 944, 3, 1, 0, 0 },
	{ "cvAmbOn", 956, 1, 1, 0, 0 }, { "cvApOn", 960, 1, 1, 0, 0 }, { "cvDeckAp", 976, 4, 1, 0, 0 },
	{ "cvFrame", 992, 1, 1, 0, 0 }, { "cvMaxKm", 996, 1, 1, 0, 0 }, { "cvSlab", 1000, 1, 1, 0, 0 },
	{ "cvProbe", 1004, 1, 1, 1, 0 }, { "cvProbeRect", 1008, 4, 1, 0, 0 }, { "cvProbeBand", 1024, 1, 1, 1, 0 },
	{ "cvMsOn", 1028, 1, 1, 0, 0 }, { "cvMsG", 1032, 1, 1, 0, 0 }, { "cvTint", 1040, 3, 3, 0, 0 },
	{ "cvWaterK", 1088, 3, 1, 0, 0 }, { "cvSceneZ", 1104, 4, 1, 0, 0 }, { "cvTopOn", 1120, 1, 1, 0, 0 },
	{ "cvAmbDark", 1124, 1, 1, 0, 0 }, { "cvIce", 1136, 3, 1, 0, 0 }, { "cvShaft", 1152, 4, 1, 0, 0 },
	{ "cvAlbedoD", 1168, 3, 1, 0, 0 }, { "cfOrgKm", 1184, 3, 1, 0, 0 }, { "cfDim", 1200, 3, 1, 0, 0 },
	{ "cfCellKm", 1212, 1, 1, 0, 0 }, { "cfStepKm", 1216, 1, 1, 0, 0 }, { "cfMaxSteps", 1220, 1, 1, 1, 0 },
	{ "cfSafe", 1224, 1, 1, 0, 0 }, { "cfOn", 1228, 1, 1, 0, 0 }, { "cfSkip", 1232, 1, 1, 0, 0 },
};
inline constexpr int kT_flyBytes = 1248;
// kT_fly_a: 65 fields, 1236 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_fly_a[] = {
	{ "atRay", 0, 3, 1, 0, 0 }, { "atMieS", 12, 1, 1, 0, 0 }, { "atMieE", 16, 1, 1, 0, 0 }, { "atHaze", 20, 3, 1, 0, 0 },
	{ "atOzone", 32, 3, 1, 0, 0 }, { "atG", 44, 1, 1, 0, 0 }, { "atHazeAlbedo", 48, 3, 1, 0, 0 },
	{ "atHazeSpec", 64, 3, 1, 0, 0 }, { "atMsOld", 76, 1, 1, 0, 0 }, { "cvBands.baseKm", 80, 3, 1, 0, 0 },
	{ "cvBands.topKm", 96, 3, 1, 0, 0 }, { "cvBands.density", 112, 3, 1, 0, 0 }, { "cvBands.coverage", 128, 3, 1, 0, 0 },
	{ "cvBands.sheet", 140, 1, 1, 0, 0 }, { "cvBands.mapRect", 144, 4, 1, 0, 0 }, { "cvBands.windKm", 160, 3, 1, 0, 0 },
	{ "cvCal", 176, 1, 34, 0, 0 }, { "cvViewSize", 720, 2, 1, 0, 0 }, { "cvCamR", 728, 1, 1, 0, 0 },
	{ "cvSunW", 736, 3, 1, 0, 0 }, { "cvMoonW", 752, 3, 1, 0, 0 }, { "cvSunK", 764, 1, 1, 0, 0 },
	{ "cvMoonK", 768, 1, 1, 0, 0 }, { "cvKeyW", 772, 3, 1, 0, 0 }, { "cvKeyK", 784, 1, 1, 0, 0 },
	{ "cvCam", 800, 3, 1, 0, 3 }, { "cvProj", 848, 4, 1, 0, 0 }, { "cvCamKm", 864, 3, 1, 0, 0 },
	{ "cvHistSize", 880, 2, 1, 0, 0 }, { "cvStride", 888, 1, 1, 0, 0 }, { "cvPhase", 896, 2, 1, 0, 0 },
	{ "cvSteps", 904, 1, 1, 1, 0 }, { "cvTaps", 908, 1, 1, 1, 0 }, { "cvOctaves", 912, 1, 1, 1, 0 },
	{ "cvLightKm", 916, 1, 1, 0, 0 }, { "cvPhasePrm", 928, 4, 1, 0, 0 }, { "cvAmb", 944, 3, 1, 0, 0 },
	{ "cvAmbOn", 956, 1, 1, 0, 0 }, { "cvApOn", 960, 1, 1, 0, 0 }, { "cvDeckAp", 976, 4, 1, 0, 0 },
	{ "cvFrame", 992, 1, 1, 0, 0 }, { "cvMaxKm", 996, 1, 1, 0, 0 }, { "cvSlab", 1000, 1, 1, 0, 0 },
	{ "cvProbe", 1004, 1, 1, 1, 0 }, { "cvProbeRect", 1008, 4, 1, 0, 0 }, { "cvProbeBand", 1024, 1, 1, 1, 0 },
	{ "cvMsOn", 1028, 1, 1, 0, 0 }, { "cvMsG", 1032, 1, 1, 0, 0 }, { "cvTint", 1040, 3, 3, 0, 0 },
	{ "cvWaterK", 1088, 3, 1, 0, 0 }, { "cvSceneZ", 1104, 4, 1, 0, 0 }, { "cvTopOn", 1120, 1, 1, 0, 0 },
	{ "cvAmbDark", 1124, 1, 1, 0, 0 }, { "cvIce", 1136, 3, 1, 0, 0 }, { "cvShaft", 1152, 4, 1, 0, 0 },
	{ "cvBlueSlice", 1168, 1, 1, 1, 0 }, { "cvAlbedoD", 1172, 3, 1, 0, 0 }, { "cfOrgKm", 1184, 3, 1, 0, 0 },
	{ "cfDim", 1200, 3, 1, 0, 0 }, { "cfCellKm", 1212, 1, 1, 0, 0 }, { "cfStepKm", 1216, 1, 1, 0, 0 },
	{ "cfMaxSteps", 1220, 1, 1, 1, 0 }, { "cfSafe", 1224, 1, 1, 0, 0 }, { "cfOn", 1228, 1, 1, 0, 0 },
	{ "cfSkip", 1232, 1, 1, 0, 0 },
};
inline constexpr int kT_fly_aBytes = 1248;
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
// lane VKFAR2 (row L8): kFxMipClamp = trilinear CLAMP_TO_EDGE (the precipitation atlases, cellprecip.cpp loadDds)
enum FxSampler { kFxLinearClamp, kFxNearestClamp, kFxLinearRepeat, kFxMipRepeat, kFxNearestRepeat, kFxMipRepeatAniso, kFxMipClamp, kFxSamplers };
//! one texture slot of a pass (HLSL t<i>): a GL texture copied into its twin, or a shared target read as is
struct FxTex
{
	unsigned tex = 0;               //!< GL's name (0 and no shared: a zero texture, as GL's unbound unit reads)
	int type = 0;                   //!< 0 2D, 1 3D, 2 2D array, 3 cube array (sharedTarget's)
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
	int refusedPacks = 0;           //!< rain / snow: fxPack calls made for runs then refused (asked before the packs: 0)
	int reusedHandles = 0;          //!< lane VKWATER4: water ray runs whose depth twin kept its VkImage handle over a sharedGen() move
	int fenceMisses = 0;            //!< lane VKWATER4: waterRun's wait on the last crossing's fence that did not return VK_SUCCESS
};
FxStat takeVolSkyStat();
//! lane VKCLOUD2: the fly march's t0.. (cloud_fly_ps / cloud_fly_a_ps reflected; t7 cvBlue only in _a, cfSdf Load'd)
inline constexpr int kFlyTex = 9;
struct CloudRun
{
	bool accum = false;             //!< the _a twins (Cloud Accumulation)
	bool fly = false;               //!< lane VKCLOUD2: the march with the fly-through box (tex in kFlyTex order)
	int mw = 0, mh = 0, hw = 0, hh = 0, cur = 0;
	std::uintptr_t marchKey = 0, histKey[2] = {};  //!< shared targets: key = RGBA16F colour, key + 1 = RG32F depth
	FxTex tex[kFlyTex];             //!< the march's t0.. (march: map shape detail sun moon scene trans; _a: + blue at t3;
	                                //!< fly: shape detail map trans sun moon scene blue sdf)
	std::uint8_t march[kT_flyBytes > kT_march_aBytes ? kT_flyBytes : kT_march_aBytes] = {};
	std::uint8_t resolve[kT_resolve_aBytes] = {};
	int red = 0;                    //!< gates only: 1 the resolve's march / history inputs swapped, 2 the march's t1 / t2
	                                //!< swapped, 4 no march (the caller filled the march targets), 8 the fly's cfSdf and
	                                //!< cvShape swapped (lane VKCLOUD2), 32 no shared lookup: every slot copied (VKCLOUD8)
};
//! GL current: the march (into the shared march targets) and the resolve (into hist[cur]) on Vulkan, one crossing
bool cloudRun( const CloudRun & t, std::string & why );
void cloudOnGl( const char * why );
FxStat takeCloudStat();

// lane VKCLOUD4: the fly-through box's bake (cloudfly.cpp runPass): cloud_flybake_ps / cloud_flysdf_ps's b31 (reflected)
// kT_flybake: 14 fields, 672 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_flybake[] = {
	{ "cvBands.baseKm", 0, 3, 1, 0, 0 }, { "cvBands.topKm", 16, 3, 1, 0, 0 }, { "cvBands.density", 32, 3, 1, 0, 0 },
	{ "cvBands.coverage", 48, 3, 1, 0, 0 }, { "cvBands.sheet", 60, 1, 1, 0, 0 }, { "cvBands.mapRect", 64, 4, 1, 0, 0 },
	{ "cvBands.windKm", 80, 3, 1, 0, 0 }, { "cvCal", 96, 1, 34, 0, 0 }, { "cfOrgKm", 640, 3, 1, 0, 0 },
	{ "cfCellKm", 652, 1, 1, 0, 0 }, { "cfLayer", 656, 1, 1, 1, 0 }, { "cfMargin", 660, 1, 1, 0, 0 },
	{ "cfSlackKm", 664, 1, 1, 0, 0 }, { "cfProbe", 668, 1, 1, 1, 0 },
};
inline constexpr int kT_flybakeBytes = 672;
inline constexpr int kT_flybakeLayerAt = 656;
// kT_flysdf: 3 fields, 28 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_flysdf[] = {
	{ "cfAxis", 0, 1, 1, 1, 0 }, { "cfLayer", 4, 1, 1, 1, 0 }, { "cfDim", 16, 3, 1, 0, 0 },
};
inline constexpr int kT_flysdfBytes = 32;
inline constexpr int kT_flysdfLayerAt = 4;
inline constexpr int kFlyBakeTex = 3;  //!< cloud_flybake_ps's t0 cvShape, t1 cvDetail, t2 cvMap; cloud_flysdf_ps's t0 cfSrc
struct FlyBakeRun
{
	int pass = 0;                   //!< 0 the occupancy bake (cloud_flybake), 1..3 the distance field along x, y, z (cloud_flysdf)
	int from = 0, to = 0;           //!< the slices drawn, [from, to)
	std::uintptr_t dst = 0;         //!< the written volume's sharedTarget key (3D R8, made with slices)
	std::uintptr_t src = 0;         //!< passes 1..3: the read volume's sharedTarget key
	FxTex tex[kFlyBakeTex];         //!< pass 0: the density's inputs (fxTexOf over GL's program)
	std::uint8_t loose[kT_flybakeBytes] = {};  //!< b31 packed from GL's program (fxPack, kT_flybake or kT_flysdf); cfLayer is set here
	int red = 0;                    //!< gates only: 1 every slice into slice 0, 2 slice k into k + 1, 4 the other pass's program
};
//! GL current: one pass over slices [from, to) of t.dst on Vulkan (one render pass a slice), one crossing; false: why, GL draws it
bool flyBakeRun( const FlyBakeRun & t, std::string & why );
void flyBakeOnGl( const char * why );
FxStat takeFlyBakeStat();

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

// lane VKFAR2 (row L8 slice 1): rain / snow (cell_precip_gpu): precip_view_cs's b31 (GL's names; pcFirst / pcCount / pcRed
// are not GL's, precipRun writes them) and precip_view_ps's (spirv-cross --reflect of the shipped SPIR-V)
inline constexpr FxField kT_precipcs[] = {
	{ "pcSeedS", 0, 1, 1, 1, 0 }, { "pcL", 4, 1, 1, 0, 0 }, { "pcInner", 8, 1, 1, 0, 0 }, { "pcOffXY", 16, 2, 1, 0, 0 },
	{ "pcOffZ", 32, 1, 16, 0, 0 }, { "pcNCand", 288, 1, 1, 1, 0 }, { "pcBest", 292, 1, 1, 1, 0 },
	{ "pcCand", 304, 1, 16, 1, 0 }, { "pcFallU", 560, 1, 16, 0, 0 }, { "pcHalfU", 816, 1, 16, 0, 0 },
	{ "pcWind", 1072, 2, 1, 0, 0 }, { "pcSway", 1080, 1, 1, 0, 0 }, { "pcSpin", 1084, 1, 1, 0, 0 },
	{ "pcT", 1088, 1, 1, 0, 0 }, { "pcCols", 1092, 1, 1, 1, 0 }, { "pcCells", 1096, 1, 1, 1, 0 },
	{ "pcCellUv", 1104, 2, 1, 0, 0 }, { "pcRoofOn", 1112, 1, 1, 1, 0 }, { "pcRoofCam", 1120, 3, 1, 0, 0 },
	{ "pcRoofInv", 1132, 1, 1, 0, 0 }, { "pcRow", 1136, 4, 3, 0, 0 }, { "pcProj", 1184, 4, 1, 0, 4 },
	{ "pcFocal", 1248, 1, 1, 0, 0 }, { "pcMinPx", 1252, 1, 1, 0, 0 }, { "pcShutter", 1256, 1, 1, 0, 0 },
	{ "pcThin", 1260, 1, 1, 0, 0 }, { "pcAtlas", 1264, 4, 1, 0, 0 }, { "pcInset", 1280, 2, 1, 0, 0 },
	{ "pcFar", 1288, 1, 1, 1, 0 }, { "pcHasTex", 1292, 1, 1, 1, 0 }, { "pcFirst", 1296, 1, 1, 1, 0 },
	{ "pcCount", 1300, 1, 1, 1, 0 }, { "pcRed", 1304, 1, 1, 1, 0 }, { "pcViewport", 1312, 4, 1, 0, 0 },
	{ "fogOn", 1328, 1, 1, 1, 0 }, { "fogK", 1344, 4, 6, 0, 0 }, { "fogView", 1440, 4, 1, 0, 0 },
	{ "fogDistScale", 1456, 1, 1, 0, 0 }, { "fogSun", 1472, 4, 1, 0, 0 }, { "fogSunColour", 1488, 4, 1, 0, 0 },
	{ "fogUnit", 1504, 1, 1, 0, 0 }, { "fogProbe", 1520, 4, 1, 0, 0 }, { "fogRed", 1536, 1, 1, 1, 0 },
	{ "fogNight", 1552, 4, 1, 0, 0 }, { "fogNightHue", 1568, 1, 1, 0, 0 }, { "atAp", 1584, 4, 1, 0, 0 },
	{ "atApProj", 1600, 4, 1, 0, 0 }, { "atApDouble", 1616, 1, 1, 0, 0 }, { "volOn", 1620, 1, 1, 1, 0 },
	{ "volRect", 1632, 4, 1, 0, 0 }, { "volFarClamp", 1648, 1, 1, 1, 0 }, { "volSlices", 1664, 4, 1, 0, 0 },
	{ "volProbe", 1680, 1, 1, 1, 0 }, { "volFogBlend", 1684, 1, 1, 1, 0 }, { "volFar", 1696, 4, 1, 0, 0 },
	{ "volPack", 1712, 4, 1, 0, 0 }, { "volPackDims", 1728, 4, 1, 0, 0 }, { "volAirMed", 1744, 4, 1, 0, 0 },
	{ "volAirLayer", 1760, 4, 1, 0, 0 }, { "volAirGround", 1776, 4, 1, 0, 0 }, { "volAirFlags", 1792, 1, 1, 1, 0 },
	{ "volAirTerrU", 1808, 4, 1, 0, 0 }, { "volAirTerrV", 1824, 4, 1, 0, 0 }, { "volAirTerrFU", 1840, 4, 1, 0, 0 },
	{ "volAirTerrFV", 1856, 4, 1, 0, 0 }, { "volAirTerrRef", 1872, 1, 1, 0, 0 }, { "volAirRain", 1888, 4, 1, 0, 0 },
	{ "volAirRainFar", 1904, 4, 1, 0, 0 }, { "volAirRainXf", 1920, 4, 1, 0, 4 }, { "volAirDrift", 1984, 4, 1, 0, 0 },
	{ "volAirDriftUv", 2000, 4, 6, 0, 0 }, { "volAirDriftRed", 2096, 1, 1, 1, 0 },
};
inline constexpr int kT_precipcsBytes = 2112;
inline constexpr FxField kT_precipps[] = {
	{ "pcHasTex", 0, 1, 1, 1, 0 }, { "pcFar", 4, 1, 1, 1, 0 }, { "pcAtlas", 16, 4, 1, 0, 0 },
	{ "pcNear", 32, 2, 1, 0, 0 },
};
inline constexpr int kT_precippsBytes = 48;
// slice 3: the look's members past those (precip_look_view_cs / _ps, cell_precip_look.prog), packed into the same bytes
inline constexpr FxField kT_preciplookcs[] = {
	{ "pcLook", 2100, 1, 1, 1, 0 }, { "pcMmPerView", 2104, 1, 1, 0, 0 },
};
inline constexpr FxField kT_preciplookps[] = {
	{ "pcLook", 40, 1, 1, 1, 0 }, { "pcHasN", 44, 1, 1, 1, 0 }, { "pcClump", 48, 2, 1, 0, 0 }, { "pcAlbedo", 56, 1, 1, 0, 0 },
	{ "pcTrans", 60, 1, 1, 0, 0 },
};
inline constexpr int kT_preciplookpsBytes = 64;
/* slice 2b: precip_lit_view_cs's b31 = kT_precipcs's 72 fields at the same offsets (checked against the reflect) and these
 * past them: cell_precip_gpu_gs's light chain (cell_lights, cell_cluster, cell_volpart, ww_sunshadow, cell_airmed) and
 * the GS's own lightingInfluence / fxLitMode / pcTrans (spirv-cross --reflect of the shipped SPIR-V; reflect_table.py) */
inline constexpr FxField kT_preciplitcs[] = {
	{ "cellOn", 2100, 1, 1, 1, 0 }, { "cellLightCount", 2104, 1, 1, 1, 0 }, { "cellRow", 2112, 4, 3, 0, 0 },
	{ "cellHasDalc", 2160, 1, 1, 1, 0 }, { "cellGiAmb", 2164, 1, 1, 1, 0 }, { "cellGiFill", 2168, 1, 1, 1, 0 },
	{ "cellDalc", 2176, 4, 3, 0, 0 }, { "cellHasDir", 2224, 1, 1, 1, 0 }, { "cellExtDalc", 2228, 1, 1, 1, 0 },
	{ "cellSunScale", 2232, 1, 1, 0, 0 }, { "cellDirColor", 2240, 3, 1, 0, 0 }, { "cellDirTo", 2256, 3, 1, 0, 0 },
	{ "cellInterior", 2268, 1, 1, 1, 0 }, { "cellCenter", 2272, 3, 1, 0, 0 }, { "cellProbe", 2284, 1, 1, 1, 0 },
	{ "cellProbeGain", 2288, 1, 1, 0, 0 }, { "cellAmboCount", 2292, 1, 1, 1, 0 }, { "cellAmbo", 2304, 4, 16, 0, 0 },
	{ "cellAmboK", 2560, 4, 16, 0, 0 }, { "cellAmboBox", 2816, 4, 48, 0, 0 }, { "cellLightPhys", 3584, 4, 1, 0, 0 },
	{ "cellLightPhysB", 3600, 4, 1, 0, 0 }, { "cellRed", 3616, 1, 1, 1, 0 }, { "cellGiOn", 3620, 1, 1, 1, 0 },
	{ "cellGiOrigin", 3632, 3, 1, 0, 0 }, { "cellGiVoxel", 3644, 1, 1, 0, 0 }, { "cellGiDims", 3648, 3, 1, 0, 0 },
	{ "cellGiPad", 3660, 1, 1, 0, 0 }, { "cellGiRooms", 3664, 1, 1, 1, 0 }, { "cellRoomsOrigin", 3668, 3, 1, 0, 0 },
	{ "cellRoomsCell", 3680, 1, 1, 0, 0 }, { "cellRoomsDims", 3684, 3, 1, 0, 0 }, { "cellGiSky", 3696, 1, 1, 1, 0 },
	{ "cellGiFineOn", 3700, 1, 1, 1, 0 }, { "cellGiFineN", 3704, 1, 1, 1, 0 }, { "cellGiFineRed", 3708, 1, 1, 1, 0 },
	{ "cellGiFineBox", 3712, 4, 8, 0, 0 }, { "cellGiFineDim", 3840, 4, 8, 0, 0 }, { "cellPass", 3968, 1, 1, 1, 0 },
	{ "cellPassRed", 3972, 1, 1, 1, 0 }, { "cellIsOn", 3976, 1, 1, 1, 0 }, { "cellIsLinear", 3980, 1, 1, 1, 0 },
	{ "cellIsExposure", 3984, 1, 1, 0, 0 }, { "cellIsE", 3988, 1, 1, 0, 0 }, { "cellIsAdapted", 3992, 1, 1, 0, 0 },
	{ "cellIsCine", 4000, 3, 1, 0, 0 }, { "cellIsTint", 4016, 4, 1, 0, 0 }, { "cellIsWbD", 4032, 3, 1, 0, 0 },
	{ "cellIsLutOn", 4044, 1, 1, 1, 0 }, { "cellIsRed", 4048, 1, 1, 1, 0 }, { "cellIsVanCurve", 4052, 1, 1, 1, 0 },
	{ "cellIsBloomOn", 4056, 1, 1, 1, 0 }, { "cellIsBloomRect", 4064, 4, 1, 0, 0 }, { "cellShadowOn", 4080, 1, 1, 1, 0 },
	{ "cellShadowTexel", 4084, 1, 1, 0, 0 }, { "cellShadowFadeOn", 4088, 1, 1, 1, 0 },
	{ "cellShadowFade", 4096, 1, 16, 0, 0 }, { "cellFarOn", 4352, 1, 1, 1, 0 }, { "cellFarBits", 4356, 1, 1, 1, 0 },
	{ "cellFarProbe", 4360, 1, 1, 1, 0 }, { "cellFarCell", 4364, 1, 1, 0, 0 }, { "cellFarBand", 4368, 2, 1, 0, 0 },
	{ "cellFarRed", 4376, 1, 1, 1, 0 }, { "cellFarPlacedOn", 4380, 1, 1, 1, 0 },
	{ "cellFarPlacedOrigin", 4384, 3, 1, 0, 0 }, { "cellFarPlacedVoxel", 4396, 1, 1, 0, 0 },
	{ "cellFarPlacedDims", 4400, 3, 1, 0, 0 }, { "cellCtOn", 4412, 1, 1, 1, 0 }, { "cellCtProj", 4416, 4, 1, 0, 0 },
	{ "cellCtRect", 4432, 4, 1, 0, 0 }, { "cellCtRed", 4448, 1, 1, 1, 0 }, { "cellGiLoop8", 4452, 1, 1, 1, 0 },
	{ "cellAoDecalOn", 4456, 1, 1, 1, 0 }, { "cellAoDecalRect", 4464, 4, 1, 0, 0 }, { "cellSkyVis", 4480, 1, 1, 0, 0 },
	{ "cellSpecOcc", 4484, 1, 1, 1, 0 }, { "cellSpecSkyBase", 4488, 1, 1, 0, 0 }, { "cellGoboOn", 4492, 1, 1, 1, 0 },
	{ "cellGoboRot", 4496, 1, 1, 1, 0 }, { "cellGoboFlat", 4500, 1, 1, 1, 0 }, { "cellGoboGam", 4504, 1, 1, 1, 0 },
	{ "cellGoboTbl", 4508, 1, 1, 1, 0 }, { "cellGoboMap", 4512, 1, 1, 1, 0 }, { "cellCubeMat", 4528, 4, 1, 0, 0 },
	{ "cellSsgiOn", 4544, 1, 1, 1, 0 }, { "cellSsgiRect", 4560, 4, 1, 0, 0 }, { "cellSsgiK", 4576, 2, 1, 0, 0 },
	{ "cellSsgiRed", 4584, 1, 1, 1, 0 }, { "cellClusterOn", 4588, 1, 1, 1, 0 }, { "cellClusterDims", 4592, 4, 1, 0, 0 },
	{ "cellClusterOrigin", 4608, 2, 1, 0, 0 }, { "cellClusterDepth", 4624, 4, 1, 0, 0 },
	{ "cellClusterLog", 4640, 2, 1, 0, 0 }, { "cellClusterBase", 4648, 1, 1, 1, 0 }, { "volPart", 4652, 1, 1, 1, 0 },
	{ "volPartRed", 4656, 1, 1, 1, 0 }, { "volPartDump", 4660, 1, 1, 1, 0 }, { "vpTiled", 4664, 1, 1, 1, 0 },
	{ "volCsm", 4668, 1, 1, 1, 0 }, { "volTerms", 4672, 1, 1, 1, 0 }, { "volVeil", 4676, 1, 1, 1, 0 },
	{ "volEnvGrid", 4688, 4, 1, 0, 0 }, { "fxLaw", 4704, 2, 1, 0, 0 }, { "cellFxEmitK", 4712, 1, 1, 0, 0 },
	{ "cellFxUnlitK", 4716, 1, 1, 0, 0 }, { "vpSun", 4720, 4, 1, 0, 0 }, { "csmMat", 4736, 4, 3, 0, 4 },
	{ "csmInvRange", 4928, 3, 1, 0, 0 }, { "csmParams", 4944, 4, 1, 0, 0 }, { "csmSplitOffset", 4960, 4, 1, 0, 0 },
	{ "csmProbe", 4976, 1, 1, 1, 0 }, { "csmRed", 4980, 1, 1, 1, 0 }, { "csmForce", 4984, 1, 1, 0, 0 },
	{ "ussOn", 4988, 1, 1, 1, 0 }, { "ussCsmOn", 4992, 1, 1, 1, 0 }, { "ussCsmFilter", 5008, 4, 1, 0, 0 },
	{ "ussCsmBias", 5024, 3, 1, 0, 0 }, { "ussTerrOn", 5036, 1, 1, 1, 0 }, { "ussWorldFromView", 5040, 4, 1, 0, 4 },
	{ "ussTerrXform", 5104, 4, 1, 0, 0 }, { "ussTerrRange", 5120, 4, 1, 0, 0 }, { "ussContactOn", 5136, 1, 1, 1, 0 },
	{ "ussContactInvSize", 5140, 2, 1, 0, 0 }, { "ussView", 5148, 1, 1, 1, 0 }, { "ussRed", 5152, 1, 1, 1, 0 },
	{ "ussCsmWorld", 5168, 4, 1, 0, 0 }, { "ussCsmTexel", 5184, 3, 1, 0, 0 }, { "ussCsmPhase", 5196, 1, 1, 0, 0 },
	{ "ussCsmStable", 5200, 4, 1, 0, 0 }, { "ussCsmCellX", 5216, 3, 1, 0, 0 }, { "ussCsmCellY", 5232, 3, 1, 0, 0 },
	{ "ussTail", 5248, 2, 1, 0, 0 }, { "ussTerrAtlas", 5264, 4, 1, 0, 0 }, { "ussTerrSize", 5280, 2, 1, 0, 0 },
	{ "ussTerrNearXform", 5296, 4, 1, 0, 0 }, { "ussTerrPen", 5312, 4, 1, 0, 0 }, { "ussTerrBias", 5328, 4, 1, 0, 0 },
	{ "ussTerrExtra", 5344, 4, 1, 0, 0 }, { "ussTerrSelf", 5360, 4, 1, 0, 0 }, { "ussDim", 5376, 2, 1, 0, 0 },
	{ "ussPbrDim", 5384, 1, 1, 1, 0 }, { "ussCloudOn", 5388, 1, 1, 1, 0 }, { "ussCloudFromView", 5392, 4, 1, 0, 4 },
	{ "ussCloudFar", 5456, 4, 1, 0, 0 }, { "airMed", 5472, 4, 1, 0, 0 }, { "airLayer", 5488, 4, 1, 0, 0 },
	{ "airGround", 5504, 4, 1, 0, 0 }, { "airTerrOn", 5520, 1, 1, 1, 0 }, { "airTerrXf", 5536, 4, 1, 0, 0 },
	{ "airTerrFineXf", 5552, 4, 1, 0, 0 }, { "airTerrRef", 5568, 1, 1, 0, 0 }, { "volRed", 5572, 1, 1, 1, 0 },
	{ "volPhase", 5584, 4, 3, 0, 0 }, { "volGi", 5632, 4, 1, 0, 0 }, { "airSunLayer", 5648, 1, 1, 1, 0 },
	{ "volShell", 5652, 1, 1, 0, 0 }, { "cellGiAirOn", 5656, 1, 1, 1, 0 }, { "cellGiAirOrigin", 5664, 3, 1, 0, 0 },
	{ "cellGiAirVoxel", 5676, 1, 1, 0, 0 }, { "cellGiAirDims", 5680, 3, 1, 0, 0 }, { "airEnvFogHue", 5692, 1, 1, 1, 0 },
	{ "lightingInfluence", 5696, 1, 1, 0, 0 }, { "fxLitMode", 5700, 1, 1, 1, 0 }, { "pcTrans", 5704, 1, 1, 0, 0 },
};
inline constexpr int kT_preciplitcsBytes = 5712;
struct PrecipRun
{
	int w = 0, h = 0, vp[4] = {};    //!< the tone frame's size, GL's viewport
	unsigned color = 0;             //!< the tone frame's colour (toneFrameIs)
	int first = 0, n = 0;           //!< the run: glDrawArrays( GL_POINTS, first, n )
	bool lit = false, fogged = false, roofOn = false;  //!< lit: slice 2b (the sun alone; the rest refused); roofOn: pcRoofOn, tex[0] GL's pcRoof (L8 term 7); fogged: slice 2a
	bool litBlock = false;          //!< cs holds kT_preciplitcs (a lit run without it is refused)
	bool look = false;              //!< slice 3: cell_precip_look.prog's run (cs / ps also hold kT_preciplook*)
	FxTex tex[14];                  //!< (13: the lit pass's t6 cellShadow, GL's shared lamp cubes; L8 term 5) (12: the lit pass's t30 volAirNoise, the air's drift; L8 term 4) (10: the lit pass's t31 csmMap, GL's shared cascades; 11: its t34 ussCloudMap; L8 term 3) t0 pcRoof (none yet), t1 pcTex (the atlas; a BC one twinned by farTwin), the air's t25 atApTex, t26 volTex, t27 volTile, t28 volAirTerr, t29 volAirRainMap, the look's pcTexN, the lit pass's t17 airTerr (L8 terrain air), the lit pass's t19 volEnvHist (L8 term 2, the ambient's keep)
	std::uint8_t cs[kT_preciplitcsBytes] = {};  //!< fxPack( cell_precip_gpu, kT_precipcs ) (+ kT_preciplookcs for a look run, kT_preciplitcs for a lit one)
	std::uint8_t ps[kT_preciplookpsBytes] = {};  //!< fxPack( cell_precip_gpu, kT_precipps ) (+ kT_preciplookps for a look run)
	long long red = 0;              //!< gates only (long long: term 7's reds pass bit 30): 1 the cull radius 0, 2 the strip's last two corners swapped, 4 no near fade, 8 the air dropped, 16 the look dropped (the plain pipelines draw a look run), 32 the atlas dropped (t1 / t7 the zero texture), 128 the light dropped (lit = 1), 256 the lamps' lights dropped (the zero texel buffer), 512 b0 dropped (globalUniforms zero), 1024 the cluster's lists dropped (the zero texel buffer), 2048 the local sun law dropped (vpSun.w 0), 4096 the terrain air's grid dropped (t8 the zero texture), 16384 the ambient's history dropped (t9 the zero texture), 32768 the ambient dropped (the packed volTerms' bit 8 cleared), 65536 the sun's shadow map dropped (t31 the zero D16 array, all lit), 131072 the sun's shadow dropped (the packed volCsm cleared), 262144 the clouds' map dropped (t34 the zero texture), 524288 cascades 1 and 2 swapped (the packed csmMat[1] / csmMat[2]), 1048576 the drift's noise dropped (t30 the zero 3D texture), 2097152 the drift dropped (the packed volAirDrift.x 0), 4194304 the drift's octaves swapped (the packed volAirDriftUv rows 0-2 / 3-5), 8388608 the lamps' cubes dropped (t6 the zero D16 cube array), 16777216 the lamps' shadows off (the packed cellShadowOn 0), 33554432 their fade dropped (the packed cellShadowFadeOn 0), 67108864 the gobos off (the packed cellGoboOn 0), 134217728 their table dropped (the packed cellGoboTbl 0), 268435456 their uv flipped (the packed cellGoboRot 3), 536870912 the roof map dropped (pcRoof the zero texture), 1073741824 the roof off (the packed pcRoofOn 0), 2147483648 the map's axes swapped (the packed pcRoofCam x / y), 4294967296 the interior off (the packed cellInterior 0), 8589934592 the interior's ambient off (the packed cellHasDalc 0), 17179869184 the ambient's GI rule (the packed cellGiAmb 2)
	const void * global = nullptr;  //!< GL's globalUniforms (b0, 336 bytes; the cluster reads projectionMatrix); null: zeros (a cluster run refused)
	std::uint8_t * recOut = nullptr;  //!< gates only: the run's records read back (n x 112 bytes, 192 a look run; host-visible records, a fence wait); null in the app
	struct LitIn
	{
		int terms = -1, interior = -1, partRed = -1, csm = 0, airTerr = 0;
		float drift = 0.0f, sunA = 0.0f;
		int shadow = -1, gobo = -1, cluster = -1, cellRed = -1;  //!< the lamps' switches (L8 term 2a): cellShadowOn, cellGoboOn, cellClusterOn, cellRed
		float phys = 0.0f;              //!< cellLightPhys.x (the physical falloff)
		float envKeep = 0.0f;           //!< volEnvGrid.w: the ambient's keep on (L8 term 2; it reads b0's projection)
		int gi = 0;                     //!< GI names active in GL's program (a WW_CELL_PRT / RT GI variant reads a GI volume; L8 term 1)
		int uss = 0, cloud = 0, csmProbe = 0, csmRed = 0;  //!< L8 term 3: ussOn (the shadow set's composition), ussCloudOn, csmProbe, csmRed
		int restir = -1;                //!< L8 term 5: cellRestirOn (-1 absent; present = GL's ReSTIR variant, its cubes not read)
		int goboPin = 0;                //!< L8 term 6: 1 when cellGoboRot / Flat / Gam / Map is non-zero (a pin or red: not judged)
	};
	LitIn litIn;                    //!< a lit run's switches as precipLitRead reads them from GL (before any pack; -1 absent)
	struct TexBuf { unsigned tex = 0, buf = 0, glFmt = 0; long long off = 0, bytes = 0; };
	TexBuf lights, cluster;         //!< the lamps' t0 cellLights / t45 cellCluster as GL samples them (precipTexBufOf; tex 0: the zero buffer)
	int packs = 0;                  //!< fxPack calls made for this run (a refusal adds them to FxStat::refusedPacks)
};
/* GL current, the tone frame bound, cell_precip_gpu's uniforms set: one run of particles on Vulkan, a record pass then
 * instanced quads (depth LEQUAL no write, blend SRC_ALPHA / ONE_MINUS_SRC_ALPHA, stencil ALWAYS ref 1 REPLACE mask 0x03,
 * as GL's). false: why; nothing drawn (GL draws its points). It asks precipAdmits first. */
bool precipRun( const PrecipRun & t, std::string & why );
/* the refusals that need no packed bytes (the device, the target, the run, a lit run's litIn): asked before the packs so
 * a refused run pays none. false: why, counted as a refusal (with t.packs) */
bool precipAdmits( const PrecipRun & t, std::string & why );
//! every refusal a lit run's switches would fire, in precipLitWhy's order (its count; at most max written): the census
int precipLitWhys( const PrecipRun::LitIn & l, const char ** out, int max );
//! a lit run's refusal from its switches (null: the light chain's admitted terms); precipAdmits and precipRun's block check
const char * precipLitWhy( const PrecipRun::LitIn & l );
void precipOnGl( const char * why );
FxStat takePrecipStat();

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
/* the kernels are built when their pass first runs, not all on the first queue. Per part (tonePostPart: the sources, dof,
 * lens, motion blur with its fused kernels, object blur, copy back): pipelines built and their ms since launch; most: the
 * most one queue built, and that queue's build ms */
constexpr int kPostParts = 6;
struct PostBuild
{
	int pipes[kPostParts] = {};
	double ms[kPostParts] = {};
	int most = 0;
	double mostMs = 0.0;
};
PostBuild tonePostBuild();
const char * tonePostPart( int k );

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
 * caller sets twin / shared / still. GL enums as numbers: this header carries no GL header. mipClamp (lane VKFAR2, L8): a
 * trilinear CLAMP_TO_EDGE texture (S and T) is taken as kFxMipClamp; the other callers keep refusing it */
template <class Fn> bool fxTexOf( Fn * fn, unsigned prog, const char * sampler, int type, FxTex & t, std::string & why, bool mipClamp = false )
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
	const unsigned target = type == 1 ? 0x806Fu : type == 2 ? 0x8C1Au : type == 3 ? 0x9009u : 0x0DE1u;
	int name = 0, sobj = 0;
	fn->glGetIntegerv( type == 1 ? 0x806A : type == 2 ? 0x8C1D : type == 3 ? 0x900A : 0x8069, &name );  // GL_TEXTURE_BINDING_3D / 2D_ARRAY / CUBE_MAP_ARRAY / 2D
	fn->glGetIntegerv( 0x8919, &sobj );  // GL_SAMPLER_BINDING
	bool ok = true;
	if ( name ) {
		int w = 0, h = 0, d = 1, fmt = 0, minF = 0, wrap = 0, wrapT = 0, maxL = 0;
		fn->glGetTexLevelParameteriv( target, 0, 0x1000, &w );  // GL_TEXTURE_WIDTH
		fn->glGetTexLevelParameteriv( target, 0, 0x1001, &h );
		if ( type )
			fn->glGetTexLevelParameteriv( target, 0, 0x8071, &d );  // GL_TEXTURE_DEPTH
		fn->glGetTexLevelParameteriv( target, 0, 0x1003, &fmt );  // GL_TEXTURE_INTERNAL_FORMAT
		fn->glGetTexParameteriv( target, 0x2801, &minF );  // GL_TEXTURE_MIN_FILTER
		fn->glGetTexParameteriv( target, 0x2802, &wrap );  // GL_TEXTURE_WRAP_S
		fn->glGetTexParameteriv( target, 0x2803, &wrapT );  // GL_TEXTURE_WRAP_T
		fn->glGetTexParameteriv( target, 0x813D, &maxL );  // GL_TEXTURE_MAX_LEVEL
		const bool mip = minF >= 0x2700 && minF <= 0x2703, repeat = wrap == 0x2901, nearest = minF == 0x2600;
		const bool mipClampOk = mipClamp && mip && minF == 0x2703 && type == 0 && wrap == 0x812F && wrapT == 0x812F;  // CLAMP_TO_EDGE
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
		t.sampler = mipClampOk ? kFxMipClamp : mip ? kFxMipRepeat : nearest ? ( repeat ? kFxNearestRepeat : kFxNearestClamp ) : repeat ? kFxLinearRepeat : kFxLinearClamp;
		if ( sobj ) {
			ok = false;
			why = std::string( sampler ) + ": a sampler object on its unit";
		} else if ( mip && !mipClampOk && ( !repeat || minF != 0x2703 ) ) {
			ok = false;
			why = std::string( sampler ) + ( mipClamp ? ": a mipmapped sampling other than trilinear repeat or clamp-to-edge"
				: ": a mipmapped sampling other than trilinear repeat" );
		} else if ( w < 1 || h < 1 ) {
			ok = false;
			why = std::string( sampler ) + ": no level 0";
		}
	}
	fn->glActiveTexture( unsigned( active ) );
	return ok;
}

/* Lane VKFAR3 (row A1): the DDGI probe atlas (bakert's device, rtddgi_vk.inc) imported onto the present device as a
 * VkBuffer + an RGBA32F texel view, so a pass on the view reads it in place (vkddgi.inc). The fields are what
 * bakert::ddgiGpuShare hands out (rtddgi_gpu.h DdgiGpuShare); the import must repeat the exporter's buffer (size, usage)
 * and allocation (size, memory type, dedicated), so the exporter says them rather than the importer guessing.
 * Order: no semaphore of its own. bakert's submit signals vkDone once and GL waits it once (rtddgicell.cpp), so a reader
 * on the present device is ordered through GL: its crossing's glDone comes after GL's vkDone wait in GL's stream, and
 * GL's wait on the crossing's vkDone comes before GL's next glDone to bakert (which holds bakert's next write). A reader
 * that runs outside such a crossing (deferred, no GL signal) is not ordered against bakert's writes. */
struct DdgiAtlasShare
{
	void * memory = nullptr;          //!< an NT handle of the atlas's memory (OPAQUE_WIN32); the import closes it, always
	std::uint64_t memorySize = 0;     //!< the allocation's size
	std::uint64_t bufferSize = 0;     //!< the exporter's VkBuffer size
	std::uint64_t atlasBytes = 0;     //!< the atlas itself, at offset 0 (probes x 193 texels x 16 bytes)
	std::uint32_t usage = 0;          //!< the exporter's VkBufferUsageFlags (UNIFORM_TEXEL_BUFFER among them)
	std::uint32_t memoryType = 0;     //!< the exporter's memory type index (the same GPU: the same index)
	bool dedicated = true;
	std::uint8_t deviceUuid[16] = {}, driverUuid[16] = {};
};
//! the import under key (one a volume; an older one under key is dropped first). false: why; nothing kept
bool ddgiAtlasImport( const void * key, const DdgiAtlasShare & in, std::string & why );
/* the texel view (VkBufferView as a handle) and the atlas's texels under key; 0 when none. With cascades (the preset
 * default) GL reads ONE buffer it copies every cascade's atlas into (rtddgicell.cpp combinedUpdate); with the view on
 * that buffer is a farTexelBuffer (GL writes it, the present device holds the view): its key (an address) is looked up
 * here too, so a consumer asks one function. GL's copies are GL work, so a reader behind GL's crossing sees them. */
std::uint64_t ddgiAtlasView( const void * key, std::uint64_t * texels = nullptr );
/* moves whenever any view above may have changed (an import, a drop, a texel buffer made or dropped): a consumer whose
 * descriptor set holds a view rewrites it when this moved (never by comparing handles: the driver reuses them) */
std::uint64_t ddgiAtlasGen();
/* the import under key goes (drains the worker and waits the device idle first): the ms it took, -1 when nothing was
 * under key (returns at once). An import is made at a volume's hand-over only (rtddgicell.cpp vkDdgiImport): the view
 * turned on live gets one at the next hand-over; with cascades the cascades' buffer follows the switch every frame */
double ddgiAtlasDrop( const void * key );
/* gates only: texels [first, first + count) fetched through the view on the worker (ddgi_atlas_fetch_cs), 4 floats each.
 * red 1: a view one texel late (offset minTexelBufferOffsetAlignment rounded to a texel), red 2: a view over the first
 * half of the atlas (fetches past it read 0). false: why */
bool ddgiAtlasRead( const void * key, std::uint32_t first, std::uint32_t count, std::vector<float> & out, int red, std::string & why );
/* lane VKFAR2 (row L8): a lit run's switches straight from the program, no pack (precipDraw and the gate, then
 * precipAdmits); an int absent from the program reads -1 (the refusals then refuse) */
template <class Fn> void precipLitRead( Fn * fn, unsigned prog, PrecipRun & t )
{
	auto geti = [&]( const char * nm ) {
		const int l = fn->glGetUniformLocation( prog, nm );
		int v[4] = { -1, 0, 0, 0 };
		if ( l >= 0 )
			fn->glGetUniformiv( prog, l, v );
		return v[0];
	};
	auto getf = [&]( const char * nm, int k ) {  // a vec4 writes four floats
		const int l = fn->glGetUniformLocation( prog, nm );
		float v[4] = {};
		if ( l >= 0 )
			fn->glGetUniformfv( prog, l, v );
		return v[k];
	};
	t.litIn.terms = geti( "volTerms" );
	t.litIn.interior = geti( "cellInterior" );
	t.litIn.partRed = geti( "volPartRed" );
	t.litIn.csm = geti( "volCsm" );
	t.litIn.airTerr = geti( "airTerrOn" );
	t.litIn.drift = getf( "volAirDrift", 0 );
	t.litIn.sunA = getf( "vpSun", 3 );
	t.litIn.shadow = geti( "cellShadowOn" );
	t.litIn.gobo = geti( "cellGoboOn" );
	for ( const char * nm : { "cellGoboRot", "cellGoboFlat", "cellGoboGam", "cellGoboMap" } )
		t.litIn.goboPin |= geti( nm ) > 0 ? 1 : 0;
	t.litIn.cluster = geti( "cellClusterOn" );
	t.litIn.cellRed = geti( "cellRed" );
	t.litIn.phys = getf( "cellLightPhys", 0 );
	t.litIn.envKeep = getf( "volEnvGrid", 3 );
	t.litIn.uss = geti( "ussOn" );  // L8 term 3 (VKFAR6): absent names read -1 (refused: a shadowed run needs them read)
	t.litIn.cloud = geti( "ussCloudOn" );
	t.litIn.csmProbe = geti( "csmProbe" );
	t.litIn.csmRed = geti( "csmRed" );
	t.litIn.restir = geti( "cellRestirOn" );
	// L8 term 1 (VKFAR5): the default program compiles the GI out (cellGiOn the constant false without WW_CELL_PRT)
	t.litIn.gi = 0;
	for ( const char * nm : { "cellGiOn", "cellGi", "cellGiSlots", "cellRooms", "cellGiAir", "cellGiAirOn", "cellDdgiOn", "cellDdgiAtlas" } )
		t.litIn.gi += fn->glGetUniformLocation( prog, nm ) >= 0;
}

/* lane VKFAR6 (row L8 term 3): GL's csmMap as fxTexOf reads it, plus the state the lit set's compare sampler copies
 * (sunshadow.cpp csmTexParams, the live map): a D16 array, linear, clamp to border 1, compare LEQUAL. false: why */
template <class Fn> bool precipCsmOf( Fn * fn, unsigned prog, FxTex & t, std::string & why )
{
	if ( !fxTexOf( fn, prog, "csmMap", 2, t, why ) )
		return false;
	const int l = fn->glGetUniformLocation( prog, "csmMap" );
	if ( !t.tex ) {
		if ( l < 0 )
			return true;  // not in the program: nothing reads it
		why = "csmMap: no texture on its unit";
		return false;
	}
	int unit = 0, active = 0x84C0;
	fn->glGetUniformiv( prog, l, &unit );
	fn->glGetIntegerv( 0x84E0, &active );  // GL_ACTIVE_TEXTURE
	fn->glActiveTexture( unsigned( 0x84C0 + unit ) );
	int mode = 0, func = 0, minF = 0, magF = 0, ws = 0, wt = 0;
	float border[4] = {};
	fn->glGetTexParameteriv( 0x8C1A, 0x884C, &mode );  // GL_TEXTURE_COMPARE_MODE
	fn->glGetTexParameteriv( 0x8C1A, 0x884D, &func );  // GL_TEXTURE_COMPARE_FUNC
	fn->glGetTexParameteriv( 0x8C1A, 0x2801, &minF );
	fn->glGetTexParameteriv( 0x8C1A, 0x2800, &magF );
	fn->glGetTexParameteriv( 0x8C1A, 0x2802, &ws );
	fn->glGetTexParameteriv( 0x8C1A, 0x2803, &wt );
	fn->glGetTexParameterfv( 0x8C1A, 0x1004, border );  // GL_TEXTURE_BORDER_COLOR (as floats: NVIDIA's int read of 1.0 is not 0x7FFFFFFF)
	fn->glActiveTexture( unsigned( active ) );
	if ( t.glFmt != 0x81A5u ) {  // GL_DEPTH_COMPONENT16
		why = "csmMap: not a D16 array";
		return false;
	}
	if ( mode != 0x884E || func != 0x0203 ) {  // COMPARE_REF_TO_TEXTURE, LEQUAL
		why = "csmMap: a compare other than LEQUAL on the texture";
		return false;
	}
	if ( minF != 0x2601 || magF != 0x2601 || ws != 0x812D || wt != 0x812D ) {  // LINEAR, CLAMP_TO_BORDER
		why = "csmMap: a filter or wrap other than linear clamp-to-border";
		return false;
	}
	for ( int k = 0; k < 4; k++ )
		if ( border[k] != 1.0f ) {
			why = "csmMap: a border other than 1";
			return false;
		}
	return true;
}

/* lane VKFAR6 (row L8 term 5): GL's cellShadow (the lamps' cubes, celllamps.cpp shadowAlloc) as fxTexOf reads it, plus
 * the state the lit set's compare sampler copies: a D16 cube array, linear, clamp to edge, compare LEQUAL. false: why */
template <class Fn> bool precipCubeOf( Fn * fn, unsigned prog, FxTex & t, std::string & why )
{
	if ( !fxTexOf( fn, prog, "cellShadow", 3, t, why ) )
		return false;
	const int l = fn->glGetUniformLocation( prog, "cellShadow" );
	if ( !t.tex ) {
		if ( l < 0 )
			return true;  // not in the program: nothing reads it
		why = "cellShadow: no texture on its unit";
		return false;
	}
	int unit = 0, active = 0x84C0;
	fn->glGetUniformiv( prog, l, &unit );
	fn->glGetIntegerv( 0x84E0, &active );  // GL_ACTIVE_TEXTURE
	fn->glActiveTexture( unsigned( 0x84C0 + unit ) );
	int mode = 0, func = 0, minF = 0, magF = 0, ws = 0, wt = 0, wr = 0;
	fn->glGetTexParameteriv( 0x9009, 0x884C, &mode );  // GL_TEXTURE_COMPARE_MODE
	fn->glGetTexParameteriv( 0x9009, 0x884D, &func );  // GL_TEXTURE_COMPARE_FUNC
	fn->glGetTexParameteriv( 0x9009, 0x2801, &minF );
	fn->glGetTexParameteriv( 0x9009, 0x2800, &magF );
	fn->glGetTexParameteriv( 0x9009, 0x2802, &ws );
	fn->glGetTexParameteriv( 0x9009, 0x2803, &wt );
	fn->glGetTexParameteriv( 0x9009, 0x8072, &wr );  // GL_TEXTURE_WRAP_R
	fn->glActiveTexture( unsigned( active ) );
	if ( t.glFmt != 0x81A5u ) {  // GL_DEPTH_COMPONENT16
		why = "cellShadow: not a D16 cube array";
		return false;
	}
	if ( mode != 0x884E || func != 0x0203 ) {  // COMPARE_REF_TO_TEXTURE, LEQUAL
		why = "cellShadow: a compare other than LEQUAL on the texture";
		return false;
	}
	if ( minF != 0x2601 || magF != 0x2601 || ws != 0x812F || wt != 0x812F || wr != 0x812F ) {  // LINEAR, CLAMP_TO_EDGE
		why = "cellShadow: a filter or wrap other than linear clamp-to-edge";
		return false;
	}
	return true;
}

/* lane VKFAR2 (row L8 term 2a): a texture buffer the program samples (the lamps' cellLights / cellCluster): the name on
 * the sampler's unit, its data store, range and format. false: why (bound with no data store) */
template <class Fn> bool precipTexBufOf( Fn * fn, unsigned prog, const char * sampler, PrecipRun::TexBuf & b, std::string & why )
{
	b = PrecipRun::TexBuf();
	const int l = fn->glGetUniformLocation( prog, sampler );
	if ( l < 0 )
		return true;  // not in the program: the zero buffer
	int unit = 0, active = 0x84C0, name = 0;
	fn->glGetUniformiv( prog, l, &unit );
	fn->glGetIntegerv( 0x84E0, &active );  // GL_ACTIVE_TEXTURE
	fn->glActiveTexture( unsigned( 0x84C0 + unit ) );
	fn->glGetIntegerv( 0x8C2C, &name );  // GL_TEXTURE_BINDING_BUFFER
	if ( name ) {
		int buf = 0, off = 0, size = 0, fmt = 0;
		fn->glGetTexLevelParameteriv( 0x8C2Au, 0, 0x8C2D, &buf );   // GL_TEXTURE_BUFFER_DATA_STORE_BINDING
		fn->glGetTexLevelParameteriv( 0x8C2Au, 0, 0x919D, &off );   // GL_TEXTURE_BUFFER_OFFSET
		fn->glGetTexLevelParameteriv( 0x8C2Au, 0, 0x919E, &size );  // GL_TEXTURE_BUFFER_SIZE
		fn->glGetTexLevelParameteriv( 0x8C2Au, 0, 0x1003, &fmt );   // GL_TEXTURE_INTERNAL_FORMAT
		b.tex = unsigned( name );
		b.buf = unsigned( buf );
		b.off = off;
		b.bytes = size;
		b.glFmt = unsigned( fmt );
	}
	fn->glActiveTexture( unsigned( active ) );
	if ( b.tex && ( !b.buf || b.bytes <= 0 ) ) {
		why = std::string( sampler ) + ": a texture buffer with no data store";
		return false;
	}
	return true;
}

}

#ifndef WW_VKPRESENT_NO_QT
class QOpenGLWindow;
class QPaintDevice;
#include <functional>
#include <iterator>
#include <map>
bool wwFogLeakShaderDefine();  // src/features/air/cellvolair.cpp: WW_CELL_FOGLEAK in the cell programs (precipDraw)
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
//! lane VKFAR3: the view is on Vulkan by its switch and redEnv unset, with or without a view this frame (a resize, a grab)
bool shareKept( const char * redEnv );
//! lane VKLIGHT1: the pass's route line ("vk ao:", "vk contact:", "vk ssgi:"), one a 5 s window while on Vulkan
void lightSay( int pass, bool wanted, const char * glWhy );
//! lane VKSHADOW1: the cascades' route line ("vk csm:"), one a 5 s window while on Vulkan
void csmSay( bool wanted, const char * glWhy );
//! lane VKOBJ1: the objects' prepass route line ("vk obj:"), one a 5 s window while on Vulkan
void objSay( bool wanted, const char * glWhy );
//! lane VKFAR2 (row L8): rain / snow's route line ("vk precip:"), one a 5 s window while on Vulkan
void precipSay( bool wanted, const char * glWhy );

/* lane VKFAR2 (row L8, the app hook): cellprecip.cpp's run, GL current, prog (cell_precip_gpu / cell_precip_look) in use
 * with the run's uniforms set, the atlas on pcTex's unit (pcTexN's for a look run), the draw framebuffer bound. On the
 * present device's tone frame with the view on Vulkan: the run through precipRun. false: GL draws its points (the reason
 * in the "vk precip:" line); a target other than the tone frame returns false with no line. Red WW_VKVIEW_PRECIP_RED=1:
 * GL's. GL enums as numbers (this header carries no GL header). */
template <class Fn> bool precipDraw( Fn * fn, unsigned prog, int first, int count, const void * global = nullptr )
{
	int fbo = 0, color = 0;
	fn->glGetIntegerv( 0x8CA6, &fbo );  // GL_DRAW_FRAMEBUFFER_BINDING
	if ( fbo )
		fn->glGetFramebufferAttachmentParameteriv( 0x8CA9, 0x8CE0, 0x8CD1, &color );  // DRAW_FRAMEBUFFER, COLOR_ATTACHMENT0, OBJECT_NAME
	if ( !fbo || !toneFrameIs( unsigned( color ) ) )
		return false;  // not the present device's tone frame: GL's, as before
	if ( !shareWanted( "WW_VKVIEW_PRECIP_RED" ) ) {
		precipSay( false, "red WW_VKVIEW_PRECIP_RED=1 or the view not on Vulkan" );
		return false;
	}
	auto geti = [&]( const char * nm ) {
		int v = 0;
		const int l = fn->glGetUniformLocation( prog, nm );
		if ( l >= 0 )
			fn->glGetUniformiv( prog, l, &v );
		return v;
	};
	static PrecipRun t;
	t = PrecipRun();
	std::string why;
	bool drew = false;
	const bool vol = geti( "volOn" ) != 0;
	if ( fn->glGetUniformLocation( prog, "pcSeedS" ) < 0 ) {
		precipOnGl( "the CPU field (cell_precip.prog): no Vulkan twin" );
	} else if ( vol && !wwFogLeakShaderDefine() ) {
		// the loader injects WW_CELL_FOGLEAK into GL's program by default (glcontext.cpp); the record pass is compiled with
		// it (precip_view_cs.hlsl), so WW_CELL_FOGLEAK=0 is the shape it does not match
		precipOnGl( "WW_CELL_FOGLEAK=0: the record pass is the FOGLEAK twin" );
	} else {
		toneFrameSize( t.w, t.h );
		fn->glGetIntegerv( 0x0BA2, t.vp );  // GL_VIEWPORT
		t.color = unsigned( color );
		t.first = first;
		t.n = count;
		t.global = global;
		t.lit = geti( "cellInterior" ) != 0 || geti( "cellHasDir" ) != 0;  // the geometry stage's vpLight condition
		t.roofOn = geti( "pcRoofOn" ) != 0;
		t.look = fn->glGetUniformLocation( prog, "pcLook" ) >= 0;
		t.fogged = vol || geti( "fogOn" ) != 0;
		if ( t.lit )
			precipLitRead( fn, prog, t );
		if ( !precipAdmits( t, why ) ) {  // a refused run reads no textures and packs nothing (counted there)
			precipSay( true, nullptr );
			return false;
		}
		// twins: one a GL atlas (sizes differ by layer; the DDS never changes once loaded), one a slot for the air
		static std::map<unsigned, char> atlasKeys;
		static char airKeys[5];
		static const char * const kAir[5] = { "atApTex", "volTex", "volTile", "volAirTerr", "volAirRainMap" };
		static const int kAirType[5] = { 1, 1, 0, 2, 0 };
		bool ok = fxTexOf( fn, prog, "pcTex", 0, t.tex[1], why, true );  // the DDS atlas: BC, trilinear clamp (precipRun twins it)
		if ( ok && t.tex[1].tex ) {
			t.tex[1].twin = reinterpret_cast<std::uintptr_t>( &atlasKeys[t.tex[1].tex] );
			t.tex[1].still = true;
		}
		if ( ok && t.roofOn ) {  // L8 term 7: GL's pcRoof (cellprecip.cpp's R32F map) into t0 / t47; copied every run (not still: the map follows the cell)
			static char roofKey;
			ok = fxTexOf( fn, prog, "pcRoof", 0, t.tex[0], why );
			t.tex[0].twin = reinterpret_cast<std::uintptr_t>( &roofKey );
		}
		for ( int k = 0; k < 5 && ok; k++ ) {
			ok = fxTexOf( fn, prog, kAir[k], kAirType[k], t.tex[2 + k], why );
			t.tex[2 + k].twin = reinterpret_cast<std::uintptr_t>( &airKeys[k] );
		}
		if ( ok && t.lit && t.litIn.airTerr > 0 ) {  // L8 terrain air: GL's airTerr (cellvolair.cpp's pass grid) into the lit set's t17
			static char terrKey;
			ok = fxTexOf( fn, prog, "airTerr", 2, t.tex[8], why );
			t.tex[8].twin = reinterpret_cast<std::uintptr_t>( &terrKey );
		}
		if ( ok && t.lit && ( t.litIn.terms & 8 ) ) {  // L8 term 2: GL's volEnvHist (the air's env history) into the lit set's t19
			static char envKey;
			ok = fxTexOf( fn, prog, "volEnvHist", 1, t.tex[9], why );
			t.tex[9].twin = reinterpret_cast<std::uintptr_t>( &envKey );
		}
		if ( ok && t.lit && t.litIn.csm > 0 && t.litIn.interior == 0 ) {  // L8 term 3: GL's csmMap (shared, read in place) into t31
			ok = precipCsmOf( fn, prog, t.tex[10], why );
			if ( ok && t.litIn.cloud > 0 ) {  // the clouds' ussCloudMap into t34
				static char cloudKey;
				ok = fxTexOf( fn, prog, "ussCloudMap", 0, t.tex[11], why );
				t.tex[11].twin = reinterpret_cast<std::uintptr_t>( &cloudKey );
			}
		}
		if ( ok && t.lit && ( t.litIn.terms & 2 ) && t.litIn.shadow > 0 )  // L8 term 5: GL's cellShadow (shared, read in place) into t6
			ok = precipCubeOf( fn, prog, t.tex[13], why );
		if ( ok && t.lit && t.litIn.drift > 0.0f ) {  // L8 term 4: GL's volAirNoise (the air's drift) into the lit set's t30
			static char noiseKey;
			ok = fxTexOf( fn, prog, "volAirNoise", 1, t.tex[12], why );
			t.tex[12].twin = reinterpret_cast<std::uintptr_t>( &noiseKey );
		}
		if ( ok && t.look ) {
			ok = fxTexOf( fn, prog, "pcTexN", 0, t.tex[7], why, true );
			if ( ok && t.tex[7].tex ) {
				t.tex[7].twin = reinterpret_cast<std::uintptr_t>( &atlasKeys[t.tex[7].tex] );
				t.tex[7].still = true;
			}
		}
		if ( ok && t.lit && ( t.litIn.terms & 2 ) )  // L8 term 2a: the lamps' lights (and the cluster's lists)
			ok = precipTexBufOf( fn, prog, "cellLights", t.lights, why ) && precipTexBufOf( fn, prog, "cellCluster", t.cluster, why );
		if ( !ok ) {
			precipOnGl( why.c_str() );
		} else {
			fxPack( fn, prog, kT_precipcs, int( std::size( kT_precipcs ) ), t.cs );
			fxPack( fn, prog, kT_precipps, int( std::size( kT_precipps ) ), t.ps );
			t.packs = 2;
			if ( t.look ) {
				fxPack( fn, prog, kT_preciplookcs, int( std::size( kT_preciplookcs ) ), t.cs );
				fxPack( fn, prog, kT_preciplookps, int( std::size( kT_preciplookps ) ), t.ps );
				t.packs += 2;
			}
			if ( t.lit ) {  // slice 2b: the light chain's block (its head is kT_precipcs's, packed again the same)
				fxPack( fn, prog, kT_preciplitcs, int( std::size( kT_preciplitcs ) ), t.cs );
				t.litBlock = true;
				t.packs++;
			}
			drew = precipRun( t, why );  // a refusal is counted there (with t.packs)
		}
	}
	precipSay( true, nullptr );
	return drew;
}
}
#endif

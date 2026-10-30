#ifndef RTDDGI_H
#define RTDDGI_H

/*! RAY-TRACED DDGI, MILESTONE 1: THE VOLUME OFFLINE (lane RTXGI1, 2026-10-29; notes/rtxgi1/GUIDE.md, PLAN.md).
 *
 *  bungo 10-29: "We use what Arc Raiders uses, build RTXGI into nifskope". A dynamic diffuse GI volume [J19, J21]
 *  written from the papers' math (no NVIDIA SDK code): a regular probe grid over the cell, each probe traces N rays
 *  (spherical Fibonacci, a random rotation each update, the first 32 fixed), shades their hits (sun behind a shadow
 *  ray, the weather's sky on a miss, the cell's placed lights behind shadow rays, albedo x the volume's own
 *  irradiance at the hit = the infinite bounce), and blends them into an 8x8 octahedral irradiance tile and a 16x16
 *  distance tile (mean, mean^2) per probe with hysteresis. Probes are first moved out of walls (5 relocation
 *  iterations) and classified Off / Asleep / Vigilant from their 32 fixed rays.
 *
 *  Two runs of the same steps on the same packed inputs: the GPU (bakert's Vulkan device, ray query, the kernels in
 *  src/rtddgi/) and the CPU twin here (probebvh, src/rtddgimath.h). The GPU is used when spec.gpu = 1 (the CLI's
 *  --gpu 1) and the device has ray query; anything else runs the CPU twin and says why. No env key is read.
 *
 *  M1 draws nothing: `nifskope -no-gui rtddgi ...` builds a volume from a soup and dumps it (rtddgiDump), so its
 *  per-probe irradiance can be compared with tests/bounce_reference at the same points. Nothing outside that CLI
 *  calls this code: the viewer is byte for byte as before.
 *
 *  M2 (2026-10-29): RtDdgiLive keeps a volume on the GPU and updates it a few times a frame; the cell view
 *  (src/rtddgicell.h, switch WW_CELL_RTDDGI / the "Ray-traced GI" row, default off) builds one for the open cell and its
 *  surfaces read it (res/shaders/cell_ddgi.glsl). The CLI and its dumps are unchanged. */

#include <QString>
#include <QStringList>

#include <cstdint>
#include <functional>
#include <vector>

struct ProbeSoup;
struct ProbeSkyLight;
struct WwCellLighting;
namespace bakert { struct DdgiGpuSession; }

struct RtDdgiSpec
{
	float spacing = 140.0f;     //!< GUIDE 4: a probe every 2 m (140 units) to start
	float zScale = 1.0f;        //!< vertical spacing = spacing x zScale (OPTIMIZE_RESEARCH lever 8: 0.5-0.75)
	int rays = 128;             //!< per probe, the 32 fixed included (33..256)
	int frames = 48;            //!< updates
	float hysteresis = 0.97f;   //!< GUIDE 2.3 (ours, inside [J19]'s 0.85-0.98)
	float sharpness = 50.0f;    //!< the distance blend's cosine power
	float bias = 0.3f;          //!< [J21 eq 2] self-shadow bias B
	float distClamp = 0.0f;     //!< the distance blend clamps here; 0 = 2 x |spacing vector| (ours)
	float traceMax = 400000.0f; //!< every ray's reach (probegi's sun reach)
	float fixtureClear = 24.0f; //!< probegi's: the last units before a lamp a shadow ray does not test
	quint32 seed = 1;           //!< the updates' rotations
	int maxProbes = 65536;      //!< refuse a bigger grid (the rays buffer is probes x rays x 16 bytes)
	bool place = true;          //!< the grid's box from probePlace's probes (else the soup's box inside the rect)
	bool relocate = true, classify = true;
	bool skipAsleep = true;     //!< GUIDE 2.6: Off and Asleep probes trace nothing
	bool oneBounce = false;     //!< the hits take no irradiance from the volume (bounce_reference's one bounce)
	bool noSky = false;         //!< misses are black (bounce_reference adds no sky)
	int gpu = 0;                //!< 0 the CPU twin, 1 the GPU when it can (else the twin, and the path says why)
	/*! refuters for the gates (comma list): "noback" backface hits shaded as front faces, "nocheb" the bounce sampler
	 *  without Chebyshev, "nobias" without the self-shadow bias, "norot" no per-update rotation; M1FIX: "hwhit" (GPU
	 *  only) M1's trace that trusts the hardware's hit on bakert's grown BLAS, "notie" no tie pass (both runs) */
	QString red;
	bool dumpFixed = false;     //!< keep each probe's 32 fixed-ray distances for rtddgi_fixed.tsv (the CLI's --dump-fixed)
	float rect[4] = { 0, 0, 0, 0 };   //!< minX minY maxX maxY (world), the placement's and the grid's xy box
	/*! M2: the grid's box given (world lo xyz, hi xyz; the cell view's: the probe GI's grid round the camera), no
	 *  placement; unused while hi <= lo on any axis */
	float box[6] = { 0, 0, 0, 0, 0, 0 };
	bool fitProbes = false;     //!< M2: over maxProbes the spacing grows until the grid fits (else the run refuses)
};

struct RtDdgiVolume
{
	bool ok = false;
	QString error;
	float origin[3] = { 0, 0, 0 };  //!< the first probe (world)
	float step[3] = { 0, 0, 0 };
	int dims[3] = { 0, 0, 0 };
	float distClamp = 0.0f;
	std::vector<float> offset;      //!< 3 a probe (relocation)
	std::vector<int> state;         //!< rtddgi::State a probe
	std::vector<quint32> age;       //!< updates blended a probe
	std::vector<float> irr;         //!< 8 x 8 x 4 a probe: encoded rgb (gamma 5), sky visibility (linear, M1FIX)
	std::vector<float> dist;        //!< 16 x 16 x 2 a probe: mean, mean^2
	std::vector<float> fixedDist;   //!< 32 a probe: its fixed rays' stored distances (spec.dumpFixed only)
	double blasGrow = 0.0;          //!< GPU: how far bakert grew the BLAS's edges (the trace re-tests the true triangles)
	// census
	QString path;                   //!< "gpu (<device>)" or "cpu" + why
	int probes = 0, off = 0, asleep = 0, vigilant = 0, moved = 0;
	int fresh = 0;                  //!< M3: probes a scroll reset whose classification has not come back yet
	qint64 rays = 0;
	double msBuild = 0, msReloc = 0, msFrames = 0;
	int skyClamped = 0;             //!< sky cube parts clamped to 0 (rtddgimath.h skyRadianceCube)
	int probeCount() const { return dims[0] * dims[1] * dims[2]; }
	//! probe p's position (world): grid + offset
	void position( int p, float out[3] ) const;
};

//! Build and converge a volume over `soup` lit by `lighting` (lights, interior directional) and `sky` (sun, weather).
bool rtddgiRun( const ProbeSoup & soup, const WwCellLighting & lighting, const ProbeSkyLight & sky, const RtDdgiSpec & spec,
	RtDdgiVolume * out );

/*! M2 (2026-10-29): a volume kept on the GPU and updated a few times a frame (the cell view's, src/rtddgicell.h).
 *  open() packs the inputs as rtddgiRun does (spec.frames unused), builds the device's scene and buffers, relocates
 *  and classifies; volume() then holds the grid, the states, offsets and the census (no tiles). step(k) runs k updates
 *  (the rotations updateRotation(spec.seed, n) for the volume's update numbers n, as the CLI's) and copies the tiles to
 *  the host: irr(), dist(), age() in the GPU's layout (rtddgi_gpu.h), valid until the next step or close. There is no
 *  CPU twin here: open() fails with why() when the GPU path cannot run (a frame's worth of rays is a GPU's work).
 *
 *  M3 (2026-10-29): open( ..., share ) exports the device's atlas to GL (rtddgi_gpu.h ddgiGpuShare: the cell view
 *  reads it in place, no copy back); a step may scroll the volume first (whole cells: the kept probes keep their data,
 *  the new planes reset and are relocated and classified alone), skip the host copies, and return before the device
 *  is done (a later step, or finish(), collects it). */
struct RtDdgiStep
{
	int k = 0;                            //!< updates (0..16)
	int scroll[3] = { 0, 0, 0 };          //!< whole cells the volume's first probe moves first (0 0 0: none)
	bool host = true;                     //!< the tiles back to the host (irr(), dist(), age() once the step is done)
	bool wait = true;                     //!< false: submit only (lastMs() then tells an earlier, finished step)
	std::function<bool()> beforeSubmit;   //!< interop: rtddgi_gpu.h DdgiGpuStepIn::beforeSubmit
};

class RtDdgiLive
{
public:
	RtDdgiLive();
	~RtDdgiLive();
	RtDdgiLive( const RtDdgiLive & ) = delete;
	RtDdgiLive & operator=( const RtDdgiLive & ) = delete;

	bool open( const ProbeSoup & soup, const WwCellLighting & lighting, const ProbeSkyLight & sky, const RtDdgiSpec & spec,
		bool share = false );
	bool isOpen() const;
	//! k updates (0..16), host copies, waits; false: the device was busy (busy() true: nothing ran, try next frame) or failed (why())
	bool step( int k );
	//! M3: the whole order (see RtDdgiStep)
	bool step( const RtDdgiStep & order );
	//! M3: waits for a step still running on the device and takes its results (a dump's tiles)
	bool finish();
	void close();

	const RtDdgiVolume & volume() const;
	const RtDdgiSpec & spec() const;
	QString why() const;
	bool busy() const;
	int updates() const;              //!< updates run since open (the rotations' numbers)
	int updatesSinceScroll() const;   //!< M3: updates since the last scroll (or the open)
	bool submitted() const;           //!< M3: the last step put a submit in (the GL side then waits on vkDone once)
	bakert::DdgiGpuSession * session() const;   //!< M3: for ddgiGpuShare
	//! M3: scrolls since open, cells moved by the last one (|x| + |y| + |z|), probes it reset, probes reset in all
	int scrolls() const;
	int lastScrollCells() const;
	int lastScrollFresh() const;
	qint64 freshTotal() const;
	const float * irr() const;        //!< 64 vec4 a probe (encoded rgb, sky visibility), nullptr before the first host step
	const float * dist() const;       //!< 256 vec2 a probe
	const quint32 * age() const;
	//! the last FINISHED step: GPU ms of trace, blend, border summed over its updates, [3] the copies to the host, [4] its
	//! scroll (-1: no timer, or none); lastWallMs: the last step on the CPU's clock
	const double * lastMs() const;
	double lastWallMs() const;
	qint64 lastRays() const;
	//! the volume with this step's tiles and ages copied in (rtddgiDump's input)
	void snapshot( RtDdgiVolume * out ) const;
	//! lane RESTIRDI1: the packed lights the session holds (32 floats a light, the soup's local frame: [29] the shadow
	//! sphere's radius, [30] 1 for a shadow-casting light) and that frame's offset (local = world - (ox, oy, 0))
	const std::vector<float> & packedLights() const;
	double localX() const;
	double localY() const;

private:
	struct Impl;
	Impl * d;
};

//! M2: a key of everything in `lighting` the volume's shading reads (the cell view rebuilds when it moves)
quint64 rtddgiLightsKey( const WwCellLighting & lighting );

/*! The gate's dump in folder `dir`:
 *    rtddgi_probes.tsv  per probe: index, ix iy iz, x y z (world, offset in), state, age, then E (decoded x 2 pi) on
 *                       +X -X +Y -Y +Z -Z, rgb each (18 columns), then the sky visibility on the same six axes
 *                       (6 columns, M1FIX); `#` lines: the spec and the census
 *    rtddgi_fixed.tsv   (spec.dumpFixed) per probe: index, state, its 32 fixed rays' stored distances (1e27 a miss,
 *                       negative a backface), the last trace of them (classification's when nothing moved after it)
 *    rtddgi_points.txt  per non-Off probe six lines "x y z nx ny nz" (the axes): tests/bounce_reference's points
 *    rtddgi_irr.bin / rtddgi_dist.bin  the tiles as stored (int32 probes, int32 S, then floats) */
bool rtddgiDump( const RtDdgiVolume & vol, const RtDdgiSpec & spec, const QString & dir, QString * err );

//! The irradiance (decoded x 2 pi) of probe p for unit direction (x, y, z), the bilinear tile read
void rtddgiProbeIrradiance( const RtDdgiVolume & vol, int p, const float dir[3], float out[3] );

//! The sky visibility (0..1, the cosine-weighted miss share, M1FIX) of probe p for unit direction (x, y, z), bilinear
float rtddgiProbeSkyVis( const RtDdgiVolume & vol, int p, const float dir[3] );

/*! `nifskope -no-gui rtddgi --soup <f> --rect minX,minY,maxX,maxY --out <dir> [--light x,y,z,radius,r,g,b ...]
 *  [--sun tx,ty,tz,r,g,b] [--sky r,g,b | 18 values] [--spacing s] [--zscale k] [--rays n] [--frames n] [--hyst h]
 *  [--seed n] [--one-bounce] [--no-sky] [--gpu 0|1] [--no-place] [--no-reloc] [--no-classify] [--trace-asleep]
 *  [--dump-fixed] [--red noback,nocheb,nobias,norot,hwhit,notie]` (probegi's light/sun/sky arguments) */
int rtddgiCli( const QStringList & args );

/*! lane RESTIRDI1: `nifskope -no-gui restir --soup <f> --rect minX,minY,maxX,maxY --out <dir> --cam ex,ey,ez,tx,ty,tz
 *  [--size W,H] [--fov deg] [--frames n] [--light x,y,z,radius,r,g,b ...] [--lights <file>] [--only i] [--candidates C]
 *  [--spatial K] [--gpu 0|1] [--red novis,nopdf,notemporal,noclamp,nohole]`: ReSTIR DI over the soup's lights (every
 *  CLI light casts shadows) on a g-buffer the CPU traces from the camera; the CPU twin (src/restirmath.h) and, with
 *  --gpu 1, the GPU (src/rtddgi/restir.comp) run the same frames; prints their differences, writes restir_frame.tsv */
int restirCli( const QStringList & args );

#endif // RTDDGI_H

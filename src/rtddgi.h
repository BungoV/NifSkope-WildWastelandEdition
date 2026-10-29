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
 *  calls this code: the viewer is byte for byte as before. */

#include <QString>
#include <QStringList>

#include <cstdint>
#include <vector>

struct ProbeSoup;
struct ProbeSkyLight;
struct WwCellLighting;

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
	 *  without Chebyshev, "nobias" without the self-shadow bias, "norot" no per-update rotation */
	QString red;
	float rect[4] = { 0, 0, 0, 0 };   //!< minX minY maxX maxY (world), the placement's and the grid's xy box
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
	std::vector<float> irr;         //!< 8 x 8 x 4 a probe: encoded rgb (gamma 5), 1
	std::vector<float> dist;        //!< 16 x 16 x 2 a probe: mean, mean^2
	// census
	QString path;                   //!< "gpu (<device>)" or "cpu" + why
	int probes = 0, off = 0, asleep = 0, vigilant = 0, moved = 0;
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

/*! The gate's dump in folder `dir`:
 *    rtddgi_probes.tsv  per probe: index, ix iy iz, x y z (world, offset in), state, age, then E (decoded x 2 pi) on
 *                       +X -X +Y -Y +Z -Z, rgb each (18 columns); `#` lines: the spec and the census
 *    rtddgi_points.txt  per non-Off probe six lines "x y z nx ny nz" (the axes): tests/bounce_reference's points
 *    rtddgi_irr.bin / rtddgi_dist.bin  the tiles as stored (int32 probes, int32 S, then floats) */
bool rtddgiDump( const RtDdgiVolume & vol, const RtDdgiSpec & spec, const QString & dir, QString * err );

//! The irradiance (decoded x 2 pi) of probe p for unit direction (x, y, z), the bilinear tile read
void rtddgiProbeIrradiance( const RtDdgiVolume & vol, int p, const float dir[3], float out[3] );

/*! `nifskope -no-gui rtddgi --soup <f> --rect minX,minY,maxX,maxY --out <dir> [--light x,y,z,radius,r,g,b ...]
 *  [--sun tx,ty,tz,r,g,b] [--sky r,g,b | 18 values] [--spacing s] [--zscale k] [--rays n] [--frames n] [--hyst h]
 *  [--seed n] [--one-bounce] [--no-sky] [--gpu 0|1] [--no-place] [--no-reloc] [--no-classify] [--trace-asleep]
 *  [--red noback,nocheb,nobias,norot]` (probegi's light/sun/sky arguments) */
int rtddgiCli( const QStringList & args );

#endif // RTDDGI_H

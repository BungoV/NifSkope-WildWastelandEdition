#ifndef PROBERELIGHT_H
#define PROBERELIGHT_H

/*! RELIGHT THE BAKE WITHOUT REBAKING, ON THE GPU (lane GPURELIGHT1, 2026-10-04; docs/cloud/GPURELIGHT1_DESIGN.md,
 *  docs/PRTP_PLAN.md 2aq).
 *
 *  probeGiRelight (src/probegi.h) lights the bake's surfels, gathers the probes, runs the bounce passes and blends
 *  the grid, tracing every shadow and visibility ray each time. After the rays, every step is LINEAR in the lights
 *  and every operator depends on geometry only. So the relight records its operators once (ProbeRelightOps, filled
 *  by probeGiRelight when ProbeGiSpec::record is set) and a light change re-runs only the arithmetic:
 *    1. direct: each surfel sums its PAIRS (the lights its shadow rays reached: distance, N.L, the spot's cosine),
 *       with the light's live color, radius (at most the baked one) and curve; plus the sun and the interior
 *       directional; B1 = albedo x E
 *    2. gather: each probe sums its LINKS (omega x glass tint x the six axis cosines) x B, x the unlinked
 *       renormalization, + its sky part
 *    3. feed: each surfel reads its probes (BOUNCE2's lists): B = B1 + albedo x E_probes / pi; 2-3 repeat
 *    4. grid: each voxel slot blends its probes (ROOMCLAMP1's lists); grown slots take their neighbours' mean
 *  probeRelightCpu runs it in double (the gate's reference: the same sums in the same order as probeGiRelight);
 *  ProbeRelightGpu runs it in GL 4.3 compute (float32, a thread per row; the only atomic is the settle test's max).
 *
 *  DOORS (bungo 2026-10-03: "a closed door blocks light fully, unless the door itself has a hole or glass pane"). The
 *  bake is made with the doors open (they only tag the openings). Every recorded ray that crosses a door's box is
 *  re-traced against THAT door's real triangles (ProbeSoup::DoorGeom: the placed door's mesh as it stands, closed,
 *  with its ALPHATEST1 alpha-test mask and its blended glass): a solid face stops it (0), a hole lets it pass (1), a
 *  pane tints it (the bake's glass rule, 1 - a (1 - c)). Links are aggregates over a surfel's cell, so a link's
 *  transmittance is the mean over 16 points spread over the surfel's cell. A closed door then multiplies each entry
 *  that crosses it by that transmittance (visibility entries of the feed and the grid: 0 or 1, renormalized).
 *  The light a closed door's own face would bounce back is not added (it has no surfels): the closed share is dark.
 *
 *  THE LIGHT RECORD shared with FARVIEW1 (docs/cloud/FARVIEW1b_DESIGN.md 3.3): lights_X_Y.wlt (the light table,
 *  FARVIEW1b's byte layout) and relight_X_Y.wlp (this lane: per light, its pairs keyed by surfel id = the index in
 *  that sector's .tbk surfel order). Lights that a switch (enable parent) or a script turns on and off are their
 *  own entries with their own group key; the live state picks them. Layouts: probeRelightWriteRecords. */

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <vector>

#include "probebvh.h"
#include "probeplace.h"

struct WwCellLight;

/*! The doors' real geometry as a tracer (ProbeSoup::DoorGeom), local to the relight's tracer origin O (x, y), the
 *  bake tracer's float conversion, so a ray meets a door's triangle here exactly where the soup's own BVH would.
 *  crossed(): the door boxes a segment crosses (at most two kept, nearest first, `over` counts the rest) and each
 *  one's transmittance when closed: rgb (0 at a solid face, the alpha-test holes pass, the panes tint 1 - a (1 - c))
 *  and the same with the panes opaque (the "glassopaque" red). clearEnd: the last units before q are not tested. */
class ProbeDoorTracer
{
public:
	void build( const ProbeSoup & soup, const double O[2] );
	bool any() const { return !boxes.empty(); }
	int crossed( const double p[3], const double q[3], double clearEnd, int slot[2], float T[8], int * over ) const;
	//! a link: the mean over 16 points spread over the surfel's cell (normal n, cell size cs) seen from the probe
	void linkMean( const double probe[3], const double p[3], const double n[3], double cs, int slot[2], float T[8] ) const;
	int doors() const { return int( boxes.size() ); }
	bool geometry() const { return hasGeom; }
private:
	struct D
	{
		double lo[3], hi[3];
		probebvh::Bvh solid;
		probebvh::AlphaMask mask;
		std::vector<float> glass;      //!< local
		std::vector<float> glassT;     //!< 3 a triangle, 0..1
	};
	std::vector<D> boxes;
	double O[2] = { 0, 0 };
	bool hasGeom = false;
	void trace( const D & d, const double p[3], const double q[3], double clearEnd, float T[4] ) const;
	bool boxHit( const D & d, const double p[3], const double q[3], double clearEnd, double * tEnter ) const;
};

//! The recorded operators of one relight (probegi.cpp fills them; layouts above)
struct ProbeRelightOps
{
	bool built = false;
	QString error;
	int surfels = 0, probes = 0;
	std::vector<double> nrm, alb;           //!< 3 a surfel
	std::vector<double> pos;                //!< 3 a surfel (census and records only)
	/*! the surfel's id in the bake's files: (file, index in that file's order: its surfels, then its tail's sides)
	 *  -- the first file that holds it; SIDES6 .tbk v5 and v4 both give this order */
	std::vector<int> sid;                   //!< 2 a surfel
	QStringList files;                      //!< the .tbk names, sid's file index
	// ---- the lights as baked (the pairs' reach) and their identity
	struct Light
	{
		float pos[3] = { 0, 0, 0 };
		float radius = 0.0f;                //!< as baked: the live radius may shrink, never grow
		float color[3] = { 0, 0, 0 };       //!< linear, as baked (on or off at the start)
		float bias = 0.0f, scale = 1.0f, exponent = 2.0f, cone = 1.0f, cosOuter = 0.0f;
		float dir[3] = { 1, 0, 0 };
		bool spot = false;
		bool onAtStart = true;              //!< false: switched off at the start (its pairs are still recorded)
		quint32 ref = 0;                    //!< the placed reference (0: a synthetic light)
		quint64 groupKey = 0;               //!< FARVIEW1b 3.2: kind (62-63) | parity (40) | plugin (24-39) | local form (0-23)
		quint16 flags = 0;                  //!< FARVIEW1b light flags: 1 spot, 4 negative, 8 flicker, 16 ambient only
	};
	std::vector<Light> lights;
	// ---- 1. direct: the pairs, by surfel
	std::vector<int> pairStart;             //!< surfel i: pairs [start[i], start[i + 1])
	std::vector<int> pairLight;
	std::vector<double> pairD, pairNL, pairDL;   //!< distance, N.L, -(L . spot dir) (spots)
	std::vector<int> pairDoor;              //!< 2 a pair: door slots crossed (-1 none)
	std::vector<float> pairT;               //!< 8 a pair: per door slot rgb transmittance when closed + the same with panes opaque (red)
	bool hasDir = false;
	double dirTo[3] = { 0, 0, 1 }, dirColor[3] = { 0, 0, 0 };
	std::vector<double> dirK;               //!< per surfel: N . dirTo / |dirTo| when > 0, else 0 (interior directional)
	std::vector<double> sunK;               //!< per surfel: N . sunTo where the sun ray passed, else 0
	bool sunOn = false;
	std::vector<double> le;                 //!< 3 a surfel: its own glow (EMISSIVEGI1), added once to B1
	double sun[3] = { 0, 0, 0 };
	// ---- 2. gather: the links, by probe
	std::vector<int> linkStart;
	std::vector<int> linkSurf;
	std::vector<double> linkOmega;          //!< omega
	std::vector<double> linkTint;           //!< 3 a link: the glass on the way
	std::vector<double> linkCos;            //!< 6 a link: max(axis . dir, 0)
	std::vector<int> linkDoor;              //!< 2 a link
	std::vector<float> linkT;               //!< 8 a link (pairT's layout; the mean over 16 points of the surfel's cell)
	std::vector<double> kUnl;               //!< per probe
	std::vector<double> skyE;               //!< 18 a probe: the sky's part (fixed with the weather)
	bool skyOn = false;
	// ---- 3. feed: the probes each surfel reads
	std::vector<int> feedStart, feedProbe;
	std::vector<double> feedW;
	std::vector<int> feedDoor;              //!< 2 an entry
	std::vector<float> feedT;               //!< 8 an entry (pairT's layout; a visibility entry reads 0 where r, g, b are all 0, else 1)
	int passes = 1;                         //!< the passes the recording relight ran
	int maxPasses = 64;
	bool fixedPasses = false;
	double settle = 1e-3;
	bool redGrow = false;
	// ---- 4. grid: the slots (a voxel's slot 0 or 1), each blending probes or growing from its neighbours
	int dims[3] = { 0, 0, 0 };
	size_t nVox = 0;
	std::vector<int> slotVox;               //!< voxel index
	std::vector<quint8> slotWhich;          //!< 0 = grid, 1 = grid2
	std::vector<int> blendStart, blendProbe;   //!< per slot (empty range: grown or bare)
	std::vector<double> blendW;
	std::vector<int> blendDoor;             //!< 2 an entry
	std::vector<float> blendT;              //!< 8 an entry (feedT's rule)
	std::vector<int> growSlot;              //!< grown slots in their order (ring 1, then ring 2)
	std::vector<int> growStart, growSrc;    //!< per grown slot: the neighbour slots averaged
	int growRing0 = 0;                      //!< growSlot[0 .. growRing0) = ring 1, the rest ring 2
	// ---- doors
	std::vector<quint32> doorRefs;          //!< door slot -> the door's ref
	qint64 doorRays = 0;                    //!< census: entries re-traced against a door's real geometry
	qint64 doorEntries[4] = { 0, 0, 0, 0 }; //!< pairs, links, feed, blend entries crossing a door box
	qint64 doorStopped[4] = { 0, 0, 0, 0 }; //!< of them fully stopped (T = 0) when the door is closed
	qint64 doorTinted[4] = { 0, 0, 0, 0 };  //!< of them partly through (a hole, a pane: 0 < T < 1)
	qint64 doorOver2 = 0;                   //!< entries crossing more than two door boxes (the third on is ignored)
	bool doorGeometry = false;              //!< the soup carried the doors' real geometry (else a closed door = a blanket cut)
	double msRecord = 0;
};

//! The live state of one relight
struct ProbeRelightState
{
	std::vector<float> color;               //!< 3 a light: the live color x dimmer (0: off); empty = as baked
	std::vector<float> radius;              //!< per light; empty = as baked; > baked is clamped (census)
	std::vector<quint8> doorClosed;         //!< per door slot; empty = all open (the bake)
	float sun[3] = { -1, -1, -1 };          //!< < 0 = as baked
	int passes = 0;                         //!< 0 = until settled (the ops' rule), n = exactly n
	/*! deliberate defects (the gates' reds): "nobounce" pass 1 only, "doorblanket" a closed door stops every entry
	 *  that crosses its box (its holes and panes ignored), "glassopaque" a pane stops light like wood, "dooropen" a
	 *  closed door is ignored */
	QString red;
};

struct ProbeRelightOut
{
	std::vector<float> B1, B;               //!< 3 a surfel: direct, after the last pass
	std::vector<float> E;                   //!< 18 a probe (after the last gather)
	std::vector<float> grid, grid2;         //!< probegi.h's layout (6 slabs x nVox x rgba); sky untouched
	int passes = 0;
	bool settled = false;
	std::vector<double> passLog;            //!< per pass: change, sum B, max B
	int slotsEmptied = 0;                   //!< slots a closed door left with no visible probe (valid 0)
	int radiusClamped = 0;
	double ms = 0;                          //!< wall time of the run
	double msKernel[5] = { 0, 0, 0, 0, 0 }; //!< GPU: direct, gather (all), feed (all), grid, readback
};

//! The relight from the operators, in double (the CPU reference; the same sums as probeGiRelight)
bool probeRelightCpu( const ProbeRelightOps & ops, const ProbeRelightState & st, ProbeRelightOut * out, QString * err );

/*! GL 4.3 compute. Own offscreen context, made current on the calling thread for each call (the caller's own context,
 *  if any, is restored). Needs a QGuiApplication. */
class ProbeRelightGpu
{
public:
	ProbeRelightGpu();
	~ProbeRelightGpu();
	bool init( QString * why );                       //!< context + programs; false: no GL 4.3 (why says)
	bool upload( const ProbeRelightOps & ops, QString * why );
	bool run( const ProbeRelightState & st, ProbeRelightOut * out, QString * why, bool readGrid = true );
	QString renderer() const;
	double msUpload() const;
private:
	struct Impl;
	Impl * d;
};

//! One census line
QString probeRelightCensusText( const ProbeRelightOps & ops );

/*! The shared light record (FARVIEW1 reuses it): per sector of the bake (the .tbk names), lights_X_Y.wlt (FARVIEW1b
 *  3.6 byte for byte) and relight_X_Y.wlp:
 *    header 64 B: 'WLP1', u32 version 1, i32 sector X, i32 sector Y, u32 light count (= the .wlt's), u32 pair count,
 *                 u64 light-set hash (= the .wlt's), u32 surfel count of the sector's .tbk (front + tail sides),
 *                 u32 flags (bit 0: door transmittances present), u32 door count, u8[20] reserved
 *    u32 pairStart[light count + 1]   (pairs sorted by light index, then surfel id)
 *    pair, 24 B: u32 surfel id (the index in the .tbk's surfel order) | f32 distance | f16 N.L | f16 spot cosine |
 *                u16 door slot[2] (0xFFFF none) | u8 rgb transmittance through slot 0's door when closed (255 = clear) |
 *                u8 rgb through slot 1's | u8 pad[2]
 *    u32 door ref[door count]
 *  A sector's .wlt holds the lights that reach its surfels plus the lights standing in it (4096-unit cells). The
 *  light-set hash = FNV-1a 64 over the plugin table, then the groups, then the lights (their bytes as written).
 *  A surfel shared by two sectors' files is written under the first file only (probegi's dedupe order). */
bool probeRelightWriteRecords( const ProbeRelightOps & ops, const QString & dir, const QStringList & plugins,
	QString * err, QString * census );

/*! `gpurelight --soup <f> --bake <dir> --out <dir> --light x,y,z,radius,r,g,b[,group] ... [--state <file>]
 *  [--passes n] [--red r] [--gpu-red r]`: relight the given bake (an interior), record the operators, then for each
 *  state line run the reference (probeGiRelight with the state's lights, and the closed doors' real triangles merged
 *  into the soup), the CPU relight and the GPU relight, and dump all three for tests/spells/gpurelight1_cells.py. */
int probeRelightCli( const QStringList & args );

//! main(): `-no-gui gpurelight` wants a QGuiApplication for the GPU (only when the platform plugin is beside the exe)
bool probeRelightGpuWantedForArgs( int argc, char ** argv );

#endif // PROBERELIGHT_H

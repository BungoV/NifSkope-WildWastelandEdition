// lane CELLSTREAM1: the exterior cell view's block of full cells, streamed round the camera (cellstream.h)
#include "cellstream.h"
#include "cellstreamtex.h"	// lane STREAMTEX1: the stream's file textures, counted by cell
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "features/far-lod/cellfarhide.h"	// lane FARHOLE2: the far field leaves room for every loaded cell
#include "features/far-lod/cellfarlod.h"	// lane LARGEREF4: the far build's map for the large refs
#include "cellmesh.h"
#include "cellstreamrule.h"	// lane AIRPERF1 r4: when a view streams
#include "features/gi/probeplace.h"	// lane AIRPERF1 r4: the far terrain's soup
#include "features/gi/rttraceset.h"	// lane AIRPERF1 r4: the ray-traced GI's traced set follows the loaded cells
#include "features/gi/rttracesetrule.h"
#include "features/gi/probealbedo.h"	// lane AIRPERF1 r5: the far field's colours
#include "features/gi/rtfartrace.h"	// lane AIRPERF1 r5: the far field as the trace takes it
#include <array>
#include <memory>
#include "cellsplat.h"
#include "esmdata.h"		// lane WEATHERBLEND6: the camera cell's regions
#include "features/far-ground/landlodspec.h"	// lane LANDSPEC6: streamCentre
#include "features/far-lod/largeref.h"		// lane LARGEREF2: who draws each large ref
#include "cellmodsource.h"	// lane LANDSPEC8: uGridsToLoad
#include "features/far-lod/impostorchunk.h"
#include "features/far-lod/lodpreview.h"	// lane PERFLOAD1 P8: the near grid follow / freeze / off
#include "gl/bsshape.h"
#include "features/near/cellcull.h"
#include "gl/cellgrass.h"
#include "features/water/cellwater.h"
#include "gl/glnode.h"
#include "gl/glproperty.h"
#include "gl/glcontroller.h"	// lane STREAMGN1: the controllers synced at a cell's hookup
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/texpool.h"	// lane PERFFIX4: the streaming texture pool's budgets
#include "gl/streamtexpool.h"	// lane STREAMTEX1
#include <QSettings>
#include "data/nifitemcache.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <thread>
#include <typeinfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#ifdef __MINGW32__
#include <pthread.h>
#endif
#endif

extern std::vector<NifItem *> * g_nifResetGrave;   // lane CELLSPIKE3: nifmodel.cpp resetNiBlock
// lane STREAMTEX4 (gl/cellbindless.cpp): declared outside the anonymous namespace below, or the link misses them
namespace WwBl { extern long long freedLiveAll, staleAll; }

namespace {

constexpr float kCell = 4096.0f;

/* lane CELLSPIKE3: the old path of each fix, live: WW_CELL_SPIKE3_RED holds any of drain (freed-block rows freed in
 * the frame, no item estimate), write (a slice starts whenever the budget has room), lock (builder and reaper take the item pool's lock per item) */
bool spikeRed( const char * part )
{
	WW_SWITCH_STATIC( QString, r, wwEnvStr( "WW_CELL_SPIKE3_RED" ) );
	return r.contains( QLatin1String( part ) );
}

// lane CELLSPIKE3: this thread's processor time, ms (cycle counter scaled by the TSC rate seen since the first call)
double threadCpuMs()
{
#ifdef Q_OS_WIN
	static const quint64 tsc0 = __rdtsc();
	static QElapsedTimer wall;
	if ( !wall.isValid() )
		wall.start();
	ULONG64 cyc = 0;
	QueryThreadCycleTime( GetCurrentThread(), &cyc );
	const double ms = double( wall.nsecsElapsed() ) * 1.0e-6;
	static double perMs = 0.0;
	if ( ms > 200.0 && perMs == 0.0 )
		perMs = double( __rdtsc() - tsc0 ) / ms;
	return perMs > 0.0 ? double( cyc ) / perMs : 0.0;
#else
	return 0.0;
#endif
}

// lane FRAMESPEED1: the process's page faults so far (a frame's off-processor ms with many faults = paging, few = pre-emption)
qint64 processFaults()
{
#ifdef Q_OS_WIN
	PROCESS_MEMORY_COUNTERS pmc = {};
	pmc.cb = sizeof pmc;
	return GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof pmc ) ? qint64( pmc.PageFaultCount ) : 0;
#else
	return 0;
#endif
}
using Key = std::pair<int, int>;

enum class CellState { Queued, Building, Ready, Writing, Attached };

struct StreamCell
{
	int x = 0, y = 0;
	CellState st = CellState::Queued;
	std::shared_ptr<CellChunk> chunk;
	int cellBlock = -1;
	QPointer<Node> cellNode;
	std::vector<std::pair<int, QString>> takes;     //!< every block the cell was written into
	size_t takesSeen = 0;
	QHash<int, QPointer<Node>> nodes;               //!< the scene objects made for its node and shape blocks
	std::vector<int> containers;                    //!< blocks whose children are wired: cell, ground, billboards
	QSet<int> grounds;
	QHash<int, int> childrenSeen;                   //!< per container: the Children rows already wired
	bool emitted = false;                           //!< every block written; links and hookup still to do
	size_t linked = 0;                              //!< containers whose links are updated
	double writeMs = 0.0;
	int pfx = 0;                                    //!< lane LIVESET3: particle copies it holds (the stream's cap)
	quint32 lref = 0;                               //!< lane LARGEREF2: a large-ref unit (x, y: the cell it stands in)
	bool soupAsked = false;                         //!< lane AIRPERF1 r5: a soup-only gather was started for it
	bool nearOff = false;                           //!< lane PERFLOAD1 P8: attached, but hidden by the near grid's OFF
};

/* lane LARGEREF2 (LARGEREF-STREAM): a large ref a loaded cell lists but whose own cell is not loaded is drawn by a unit
 * of its own, keyed (kLrX, ref) beside the cells; LargeRef::Owners decides who draws it, so it is drawn once. */
constexpr int kLrX = INT_MIN + 1;
Key lrKey( quint32 ref ) { return Key( kLrX, int( ref ) ); }
Key keyOf( const StreamCell & c ) { return c.lref ? lrKey( c.lref ) : Key( c.x, c.y ); }

bool isNodeType( const QString & t )
{
	return t == QLatin1String( "BSTriShape" ) || t == QLatin1String( "NiNode" ) || t == QLatin1String( "BSOrderedNode" )
		|| t == QLatin1String( "NiBillboardNode" ) || t == QLatin1String( "NiParticleSystem" )
		|| t == QLatin1String( "BSStripParticleSystem" ) || t == QLatin1String( "NiParticles" )
		|| t == QLatin1String( "BSMasterParticleSystem" );
}

const char * g_phase = "idle";   // the tick step a traced line came from
// lane CELLSTREAM1: WW_CELL_STREAM_TRACE=1 prints each load, write, attach, unload and scene remake to stderr
void trace( const QString & s )
{
	WW_SWITCH_STATIC( bool, on, wwEnvInt( "WW_CELL_STREAM_TRACE" ) > 0 );
	if ( !on )
		return;
	fprintf( stderr, "stream: %s [%s]\n", qPrintable( s ), g_phase );
	fflush( stderr );
}

double msOf( const QElapsedTimer & t ) { return double( t.nsecsElapsed() ) * 1.0e-6; }

int cellOf( float v ) { return int( std::floor( v / kCell ) ); }

struct FarTile
{
	int block = -1, x = 0, y = 0;
	bool land = false;
	QPointer<Node> node, parent;
};

static bool threadGone( const std::thread & th );

struct QStringStdHash	// lane STREAMTEX1: the pool's key hash
{
	size_t operator()( const QString & s ) const { return size_t( qHash( s ) ); }
};

class Stream final : public CellBlockSource
{
public:
	NifModel * nif = nullptr;
	std::shared_ptr<CellStreamSeed> seed;
	QMetaObject::Connection gone;
	int ccx = 0, ccy = 0;
	std::map<Key, StreamCell> cells;
	std::set<Key> wanted, prefetch;
	std::map<QString, std::set<int>> pool;          //!< reset blocks by type, lowest number first
	std::vector<std::pair<int, QString>> toReset;   //!< freed blocks still holding a cell's fields
	std::vector<Node *> toClear, toFree;            //!< a left cell's scene nodes, out of the scene, freed a few a frame
	//! lane CELLSTREAM1: a freed shape's vertex arrays, handed to a thread of their own to give back (pure memory)
	struct DeadArrays
	{
		QVector<Vector3> verts, norms, tangents, bitangents;
		QVector<Color4> colors;
		QVector<TexCoords> coords;
		QVector<Triangle> triangles;
	};
	std::vector<DeadArrays> dead;
	std::vector<NifItem *> deadItems;   //!< lane CELLSPIKE3: reset blocks' old rows, freed by the reaper too
	std::thread reaper;
	std::atomic<bool> reaperDone { true };
	double clearEstMs = 0.0, freeEstMs = 0.0;   //!< the dearest recent item of each, decaying
	double resetEstMs = 0.0;                    //!< lane CELLSPIKE3: the same for a freed block's reset
	double sliceEstMs = 0.0;                    //!< lane CELLSPIKE3: ... and for a write slice
	StreamCell * writing = nullptr;
	std::thread worker;
	std::shared_ptr<CellChunk> job;
	std::atomic<bool> jobDone { false };
	Scene * scene = nullptr;
	QPointer<Node> root;
	std::vector<FarTile> farTiles;
	CellRefTable baseRefs;
	CellPickTable basePicks;
	bool dirty = false;
	QElapsedTimer sinceRebuild;   //!< lane STREAMGN1: a filling block is lit at most this often (kLitEarlyMs)
	int litEarly = 0;
	CellStreamStats st;
	QString red;
	double budgetMs = 4.0;
	double wirePerNodeMs = 0.4;   //!< running average: one scene node made from a written block
	int sliceNodes = 0;           //!< node blocks taken in this slice, still to be wired
	float shift = 2048.0f;
	bool fixedCentre = false;     //!< lane LANDSPEC6: a fixed world's block never shifts (red WW_LANDSPEC_RED=fixedfollow)
	//! lane AIRPERF1 r4: the Cell Streaming row (or WW_CELL_STREAM=0 set live) holds the block at the eye it had:
	//! the cells it was loading finish (pausedFill), then nothing loads or unloads
	bool paused = false, pausedFill = false, saidPaused = false;
	Vector3 pausedEye;
	int saidCx = INT_MIN, saidCy = INT_MIN;
	//! lane AIRPERF1 r4: the ray-traced GI's traced set (features/gi/rttraceset.h): handed over after each rebuild()
	bool traceDirty = false, traceOn = false;
	int traceRing = -1;
	void traceSync( int ring, int ex, int ey );
	//! lane AIRPERF1 r5: a far shape as the trace takes it (rtfartrace.h), read once and kept while it reaches the ring
	struct TraceFar
	{
		quint64 sig = 0;                              //!< rtFarShapeSig when it was looked at
		std::array<int, 4> span { { 0, 0, -1, -1 } };   //!< its cells (x0, y0, x1, y1)
		int src = 0;                                  //!< 0 the far terrain, 1 far objects
		bool flat = false;                            //!< read under the fardirt red
		std::shared_ptr<const RtFarShape> shape;
	};
	QHash<int, TraceFar> traceFar;                //!< by block
	std::unique_ptr<ProbeAlbedo> traceAlb;        //!< the maps' reader, held only while shapes are still to read
	bool traceMore = false;                       //!< far shapes of the ring are not read yet (a few ms a tick)
	int traceReds = -1;                           //!< the far reds at the last hand-over (a changed word hands over again)
	int traceReads = 0;
	double traceReadMs = 0.0, traceReadMaxMs = 0.0;
	int traceFarRead( int ring, double budgetMs );
	//! lane AIRPERF1 r5: the worker's job gathers a soup for a cell that is drawn already (pumpWorker)
	bool soupJob = false;
	//! lane LANDSPEC7: a fixed world builds no cell round the eye past WCTR +- 2: the block never moves there, and the
	//! game never loads those cells (red WW_LANDSPEC_RED=fixedopen: the prefetch row is built)
	bool noPrefetch = false;
	int gridHalf = LandLodSpec::kFixedGridHalf;	//!< lane LANDSPEC8: uGridsToLoad / 2 from the game's inis (was 2)
	//! lane PERFLOAD1 P8: the near grid mode last applied, its line's gen, the once-a-second distance line
	int gridMode = LodPreview::Follow;
	quint64 gridSaid = 0;
	QElapsedTimer gridSayT;
	void nearOffCell( StreamCell & c, bool off );
	void nearGrid( const Vector3 & eye );
	CellStreamCamera cam;         //!< lane WEATHERBLEND6: the eye's cell and its regions, for the region walk
	//! lane STREAMTEX1: the file textures each written cell's properties name (cellstreamtex.h); lane STREAMTEX3: the
	//! counts give the PERFFIX4 pool its tiers, the budget shown in the census is the pool's (read at the open)
	StreamTexPool<QString, QStringStdHash> texPool { std::uint64_t( cellStreamTexPoolBytes() ), 2, cellStreamTexRed() };
	QHash<int, Key> blockCell;	//!< every block a cell was written into -> that cell (no entry: the open's own)
	Key texCentre { INT_MIN, INT_MIN };	//!< the centre the last census was taken at
	// lane LARGEREF2: the large refs (CellFarHide::largeRefsWanted), counted over the attached cells
	bool lrOn = false;
	LargeRef::Owners lr;
	std::set<Key> lrWanted;	//!< the units the layer draws now
	// lane LARGEREF6 (LARGEREF-FLATMASK-FIX): the flat mask's answer, the keep boxes published last, the line's last counts
	LargeRef::FlatMask lrMask = LargeRef::FlatMask::Accept;
	std::vector<LargeRef::Box> lrBoxes;
	int lrCutLast = -1, lrPromLast = -1;
	bool lrFlatMask();
	bool want( const Key & k ) const { return wanted.count( k ) > 0 || lrWanted.count( k ) > 0; }

	void texCensus();

	~Stream() override
	{
		farHideForget();
		if ( traceOn )
			rtTraceSetForget( nif );   // lane AIRPERF1 r4
		if ( worker.joinable() )
			worker.join();
		if ( reaper.joinable() )
			reaper.join();
		for ( NifItem * i : deadItems )
			delete i;
		for ( Node * n : toClear )
			n->clear();
		for ( Node * n : toClear )
			NodeList::release( n );
		for ( Node * n : toFree )
			NodeList::release( n );
		QObject::disconnect( gone );
	}

	QModelIndex take( NifModel * m, const QString & type ) override
	{
		QModelIndex idx;
		auto p = pool.find( type );
		if ( p != pool.end() && !p->second.empty() ) {
			const int b = *p->second.begin();
			p->second.erase( p->second.begin() );
			idx = m->getBlockIndex( b );
			st.recycled++;
		}
		if ( !idx.isValid() ) {
			idx = m->insertNiBlock( type );
			st.inserted++;
		}
		if ( writing && idx.isValid() ) {
			writing->takes.emplace_back( m->getBlockNumber( idx ), type );
			blockCell.insert( writing->takes.back().first, keyOf( *writing ) );	// lane STREAMTEX1
		}
		if ( idx.isValid() && isNodeType( type ) )
			sliceNodes++;
		return idx;
	}

	double wireOwedMs() const override { return sliceNodes * wirePerNodeMs; }

	QHash<QString, std::shared_ptr<NifModel>> pfxSrc;   //!< lane LIVESET3: particle models, one parse a stream
	int pfxLive = 0;                                    //!< particle copies in the attached cells

	void adopt( NifModel * m, int block ) override
	{
		if ( writing ) {
			writing->takes.emplace_back( block, m->itemName( m->getBlockIndex( block ) ) );
			blockCell.insert( block, keyOf( *writing ) );	// lane STREAMTEX1
		}
	}

	QHash<QString, std::shared_ptr<NifModel>> * pfxModels() override { return &pfxSrc; }

	int pfxRoom() const override
	{
		WW_SWITCH_STATIC( int, cap, wwEnvIsSet( "WW_CELL_PARTICLES_MAX" ) ? wwEnvInt( "WW_CELL_PARTICLES_MAX" ) : 512 );
		return cap - pfxLive;
	}

	bool tick( Scene * sc, const Vector3 & eye );
	bool busy() const;
	//! lane CELLSTREAM1: the builder thread is mid-job (its heap traffic stalls the main thread's frees)
	bool building() const { return worker.joinable() && !( jobDone.load() && threadGone( worker ) ); }
	bool unloadWaiting = false;   //!< a cell left the block and waits for an idle builder to go

private:
	bool resolve( Scene * sc );
	void shiftCentre( const Vector3 & eye );
	void pumpWorker( const Vector3 & eye );
	void unload( StreamCell & c );
	void drain( const QElapsedTimer & t );
	void bury( const QElapsedTimer & t );
	void reap();
	bool write( StreamCell & c, double budget );
	void wire( StreamCell & c );
	bool attach( StreamCell & c, const QElapsedTimer & t, double budget );
	void showFar( int x, int y, bool show );
public:
	// lane FARHOLE2 (cellfarhide.h): the far shapes past the hand tiles that a loaded cell masks
	QHash<int, QPointer<Node>> maskShapes;
	const void * hideDoc = nullptr;
	int hideMode = -1;
	void farHide( StreamCell & c, bool hide );
	void farHideRefresh( int x, int y );
	void farHideForget();
	void registerMasks( Scene * sc );
	void lrCell( const StreamCell & c, bool on );
	void lrPublish();
private:
	void rebuild();
	Node * makeNode( const QModelIndex & idx, const QString & type );
	void setRootSlot( int from, int to );
};

/* lane CELLSTREAM1: the builder has fully exited, its thread-local caches freed. Its job flag is set before that,
 * and those frees stall the main thread's heap work (a join would wait them out on the main thread). */
static bool threadGone( const std::thread & th )
{
#if defined( Q_OS_WIN ) && defined( __MINGW32__ )
	HANDLE h = static_cast<HANDLE>( pthread_gethandle( const_cast<std::thread &>( th ).native_handle() ) );
	return !h || WaitForSingleObject( h, 0 ) == WAIT_OBJECT_0;
#else
	( void ) th;
	return true;
#endif
}

std::map<const NifModel *, std::unique_ptr<Stream>> & streams()
{
	static std::map<const NifModel *, std::unique_ptr<Stream>> m;
	return m;
}

Stream * streamFor( const NifModel * nif )
{
	auto & m = streams();
	if ( !nif )
		return m.empty() ? nullptr : m.begin()->second.get();
	auto it = m.find( nif );
	return it == m.end() ? nullptr : it->second.get();
}

bool Stream::resolve( Scene * sc )
{
	trace( QStringLiteral( "resolve (scene %1, root %2)" ).arg( sc == scene ? "same" : "new" ).arg( root ? "kept" : "lost" ) );
	scene = sc;
	root = sc->findNode( nif, nif->getBlockIndex( seed->rootBlock ) );
	if ( !root )
		return false;
	// a scene made again from the document: every node object is new, the attached cells came with it
	farTiles.clear();
	for ( size_t i = 0; i < seed->farHand.size(); i++ ) {
		FarTile f;
		f.block = seed->farHand[i][0];
		f.x = seed->farHand[i][1];
		f.y = seed->farHand[i][2];
		f.land = int( i ) < seed->farLand;
		f.node = sc->findNode( nif, nif->getBlockIndex( f.block ) );
		f.parent = f.node ? f.node->parentNode() : nullptr;
		farTiles.push_back( f );
	}
	registerMasks( sc );
	// lane CELLSPIKE2: the attached cells' nodes are found again here, in ONE pass over the new scene; before, each
	// later unload scanned every scene node for its own (81 ms a cell on the first crossing). Red WW_CELL_UNLOAD_RED=1
	WW_SWITCH_STATIC( bool, rescanRed, wwEnvStr( "WW_CELL_UNLOAD_RED" ) == QLatin1String( "1" ) );
	QHash<int, StreamCell *> owner;
	for ( auto & kv : cells ) {
		StreamCell & c = kv.second;
		if ( c.st == CellState::Writing ) {   // its half-written node was never in the document's tree
			trace( QStringLiteral( "requeue %1,%2 (written when the scene was made again)" ).arg( c.x ).arg( c.y ) );
			g_phase = "unload";
			unload( c );
			c.st = CellState::Queued;
		} else if ( c.st == CellState::Attached && c.cellBlock >= 0 ) {
			c.cellNode = sc->findNode( nif, nif->getBlockIndex( c.cellBlock ) );
			c.nodes.clear();
			if ( !c.lref )	// lane LARGEREF2: a unit holds no far tile
				showFar( c.x, c.y, false );	// lane FRAMESPEED2: the nohandover red is retired (SHOTPASS1 confirmed it live)
			if ( !rescanRed )
				for ( const auto & tk : c.takes )
					if ( isNodeType( tk.second ) )
						owner.insert( tk.first, &c );
		}
	}
	if ( !owner.isEmpty() ) {
		QElapsedTimer f;
		f.start();
		for ( Node * n : sc->nodes.list() ) {
			const int b = nif->getBlockNumber( n->index() );
			if ( StreamCell * c = owner.value( b ) )
				c->nodes.insert( b, n );
		}
		trace( QStringLiteral( "resolve: %1 cell node blocks found again in %2 ms" ).arg( owner.size() ).arg( msOf( f ), 0, 'f', 1 ) );
	}
	return true;
}

void Stream::shiftCentre( const Vector3 & eye )
{
	// the block moves when the eye is `shift` units into the neighbour cell (half a cell by default)
	const float lim = 0.5f * kCell + shift;
	for ( int guard = 0; guard < 256; guard++ ) {
		const float dx = eye[0] - ( float( ccx ) + 0.5f ) * kCell;
		const float dy = eye[1] - ( float( ccy ) + 0.5f ) * kCell;
		bool moved = false;
		if ( dx >= lim ) { ccx++; moved = true; } else if ( dx <= -lim ) { ccx--; moved = true; }
		if ( dy >= lim ) { ccy++; moved = true; } else if ( dy <= -lim ) { ccy--; moved = true; }
		if ( !moved )
			break;
	}
}

void Stream::pumpWorker( const Vector3 & eye )
{
	if ( worker.joinable() && jobDone.load() && threadGone( worker ) ) {
		worker.join();
		std::shared_ptr<CellChunk> done = std::move( job );
		job.reset();
		st.buildMaxMs = std::max( st.buildMaxMs, double( done->buildMs ) );
		auto it = cells.find( done->largeRef ? lrKey( done->largeRef ) : Key( done->cx, done->cy ) );
		if ( soupJob ) {
			/* lane AIRPERF1 r5: a soup-only job. The cell is drawn by the chunk it has; the second build is dropped as
			 * the build of a cell that left the block is (its buckets go with `done`), only its soup is kept */
			soupJob = false;
			const bool live = it != cells.end() && it->second.st == CellState::Attached && it->second.chunk;
			const bool got = live && !it->second.chunk->soup && done->ok && done->soup;
			if ( got ) {
				it->second.chunk->soup = done->soup;
				traceDirty = true;
			}
			int owed = 0;
			for ( const auto & kv : cells )
				if ( cellstreamrule::soupOwed( true, kv.second.st == CellState::Attached && kv.second.chunk && kv.second.chunk->ok,
						kv.second.chunk && kv.second.chunk->soup, kv.second.soupAsked ) )
					owed++;
			std::fprintf( stderr, "cell stream: soup %s for %d,%d%s (loaded before the Ray-traced GI row went on) in %lld ms, "
				"%lld triangles, %d cells still owe theirs\n", got ? "gathered" : ( live ? "not gathered (the build gave none)"
				: "dropped (the cell left meanwhile)" ), done->cx, done->cy, done->largeRef ? " (large ref)" : "",
				static_cast<long long>( done->buildMs ), static_cast<long long>( got ? rtTraceUnitTris( *done->soup ) : 0 ), owed );
			std::fflush( stderr );
		} else if ( it != cells.end() && it->second.st == CellState::Building ) {
			trace( QStringLiteral( "built %1,%2 in %3 ms" ).arg( done->cx ).arg( done->cy ).arg( done->buildMs ) );
			it->second.chunk = std::move( done );
			it->second.st = CellState::Ready;
			st.built++;
		}
		// else: the cell left the block while it was built; its buckets go with `done`
	}
	if ( worker.joinable() )
		return;
	const bool held = paused && !pausedFill;   // lane AIRPERF1 r4: Cell Streaming off: no cell build starts
	// lane CELLSTREAM1: leaving cells go, their blocks reset and built cells are written before the next build
	// starts: the builder's heap traffic stalls the main thread's model edits by up to tens of ms
	if ( unloadWaiting || !toReset.empty() || !toClear.empty() || !toFree.empty() )
		return;
	for ( const auto & kv : cells )
		if ( want( kv.first ) && ( kv.second.st == CellState::Ready || kv.second.st == CellState::Writing ) )
			return;
	const int ex = cellOf( eye[0] ), ey = cellOf( eye[1] );
	StreamCell * best = nullptr;
	double bestScore = 1.0e30;
	int queued = 0;
	for ( auto & kv : cells ) {
		StreamCell & c = kv.second;
		if ( c.st != CellState::Queued )
			continue;
		queued++;
		const double d = std::hypot( double( c.x - ex ), double( c.y - ey ) );
		const double score = ( want( kv.first ) ? 0.0 : 1000.0 ) + d;
		if ( score < bestScore ) {
			bestScore = score;
			best = &c;
		}
	}
	if ( held ) {
		best = nullptr;
		bestScore = 1.0e30;
	}
	/* lane AIRPERF1 r5: no cell waits to be built: a drawn cell that was built before the Ray-traced GI row went on
	 * has its soup gathered now, nearest the eye first, one job at a time (red WW_CELL_RTSET_RED=nolatesoup: such a
	 * cell is traced only after it was streamed again) */
	bool soupOnly = false;
	if ( !best && rtTraceSetFollows() && !rtTraceSetRed( "nolatesoup" ) ) {
		for ( auto & kv : cells ) {
			StreamCell & c = kv.second;
			if ( !cellstreamrule::soupOwed( true, c.st == CellState::Attached && c.chunk && c.chunk->ok, c.chunk && c.chunk->soup,
					c.soupAsked ) )
				continue;
			const double d = std::hypot( double( c.x - ex ), double( c.y - ey ) );
			if ( d < bestScore ) {
				bestScore = d;
				best = &c;
			}
		}
		soupOnly = best != nullptr;
	}
	if ( !best )
		return;
	if ( soupOnly ) {
		best->soupAsked = true;
		soupJob = true;
		trace( QStringLiteral( "soup %1,%2 (drawn already) eye %3,%4 d %5" ).arg( best->x ).arg( best->y ).arg( ex ).arg( ey )
			.arg( bestScore, 0, 'f', 2 ) );
	} else {
		best->st = CellState::Building;
		// lane SHORE1: the eye cell, the pick's distance and the queue it beat, so a log proves nearest-first
		trace( QStringLiteral( "build %1,%2 (%3) eye %4,%5 d %6 of %7 queued" ).arg( best->x ).arg( best->y )
			.arg( want( keyOf( *best ) ) ? ( best->lref ? "large ref" : "wanted" ) : "prefetch" ).arg( ex ).arg( ey )
			.arg( std::fmod( bestScore, 1000.0 ), 0, 'f', 2 ).arg( queued ) );
	}
	job = std::make_shared<CellChunk>();
	job->seed = seed;
	job->cx = best->x;
	job->cy = best->y;
	job->largeRef = best->lref;	// lane LARGEREF2
	job->wantSoup = soupOnly || rtTraceSetFollows();	// lane AIRPERF1 r4: the cell's soup for the ray-traced GI
	// the enable facts of the 5x5 the cell is wanted in (the fixed block's rule)
	const bool now = want( keyOf( *best ) );
	const int fx = now ? ccx : ex, fy = now ? ccy : ey;
	job->facts[0] = fx - 2;
	job->facts[1] = fy - 2;
	job->facts[2] = fx + 2;
	job->facts[3] = fy + 2;
	if ( soupOnly )   // the facts the drawn chunk was built with: the soup is of what is drawn
		for ( int k = 0; k < 4; k++ )
			job->facts[k] = best->chunk->facts[k];
	jobDone = false;
	std::shared_ptr<CellChunk> j = job;
	std::atomic<bool> * flag = &jobDone;
	// a thread a job: the loaders' per-thread caches go with it
	// lane CELLSPIKE3: the builder takes item slots a run per lock; one lock per item, held by a low-priority thread
	// the machine had pre-empted, stalled the main thread 10-35 ms off the processor (red lock)
	const bool ownSlots = !spikeRed( "lock" );
	worker = std::thread( [j, flag, ownSlots]() {
#ifdef Q_OS_WIN
		// lane CELLSTREAM1: the builder yields to the main thread (its frame slices are what the gate times)
		SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
#endif
		std::optional<NifItemThreadCache> itemRun;
		if ( ownSlots )
			itemRun.emplace();
		cellChunkBuild( *j );
		flag->store( true );
	} );
}

Node * Stream::makeNode( const QModelIndex & idx, const QString & type )
{
	Node * n = nullptr;
	if ( type == QLatin1String( "BSTriShape" ) ) {
		BSShape * s = new BSShape( scene, idx );
		scene->shapes += static_cast<Shape *>( s );
		n = s;
	} else if ( type == QLatin1String( "NiBillboardNode" ) ) {
		n = new BillboardNode( scene, idx );
	} else if ( type == QLatin1String( "NiNode" ) || type == QLatin1String( "BSOrderedNode" ) ) {
		n = new Node( scene, idx );
	} else {
		return scene->getNode( nif, idx );
	}
	scene->nodes.add( n );
	n->update( nif, idx );
	return n;
}

void Stream::setRootSlot( int from, int to )
{
	const QModelIndex iRoot = nif->getBlockIndex( seed->rootBlock );
	const QModelIndex iCh = nif->getIndex( iRoot, "Children" );
	const int n = iCh.isValid() ? nif->rowCount( iCh ) : 0;
	for ( int r = 0; r < n; r++ ) {
		const QModelIndex iL = nif->getIndex( iCh, r );
		if ( nif->getLink( iL ) == from ) {
			nif->setLink( iL, to );
			nif->updateBlockLinks( seed->rootBlock );
			return;
		}
	}
	if ( from == -1 && to >= 0 )
		addLink( nif, iRoot, QStringLiteral( "Children" ), to );
	nif->updateBlockLinks( seed->rootBlock );
}

/* lane FARHOLE2: every far shape that is not a ring-0 hand tile (showFar hands those over per cell) nor an
 * instanced object ring (its rows are filtered at draw) masks the triangles centred in a loaded cell: ring 1+
 * terrain and water, the game's .btr/.bto chunks, flattened object rings. Registered with the cells its vertices span,
 * so an attach re-takes only the shapes that reach that cell. */
void Stream::registerMasks( Scene * sc )
{
	hideDoc = nif;
	maskShapes.clear();
	QSet<int> hand;
	for ( const auto & h : seed->farHand )
		hand.insert( h[0] );
	for ( Shape * sh : sc->shapes ) {
		if ( !sh || !sh->getName().startsWith( QLatin1String( "FarLOD" ) ) )
			continue;
		const int b = nif->getBlockNumber( sh->index() );
		if ( b < 0 || hand.contains( b ) )
			continue;
		const QSharedPointer<CellMesh> cm = cellMeshFor( nif, sh->index() );
		if ( cm && cm->inst )
			continue;
		const QVector<Vector3> & V = cm ? cm->verts : sh->verts;
		int x0 = -1000000, y0 = -1000000, x1 = 1000000, y1 = 1000000;   // unknown span: every cell
		if ( !V.isEmpty() ) {
			const Transform w = sh->restWorldTrans();
			float lo[2] = { 3.4e38f, 3.4e38f }, hi[2] = { -3.4e38f, -3.4e38f };
			for ( const Vector3 & v : V ) {
				const Vector3 p = w * v;
				lo[0] = std::min( lo[0], p[0] );
				lo[1] = std::min( lo[1], p[1] );
				hi[0] = std::max( hi[0], p[0] );
				hi[1] = std::max( hi[1], p[1] );
			}
			x0 = cellOf( lo[0] );
			y0 = cellOf( lo[1] );
			x1 = cellOf( hi[0] );
			y1 = cellOf( hi[1] );
		}
		// lane LARGEREF6: the keep boxes apply to far objects only (terrain and water under a box must still give way)
		const QString bn = sh->getName();
		CellFarHide::registerMask( nif, b, x0, y0, x1, y1,
			!bn.startsWith( QLatin1String( "FarLOD terrain" ) ) && !bn.startsWith( QLatin1String( "FarLOD water" ) ) );
		maskShapes.insert( b, sh );
		sh->wwRefreshData();   // a scene made again: its arrays are whole until the mask applies
	}
	hideMode = CellFarHide::mode();
	sc->transformDirty = true;
	trace( QStringLiteral( "far mask: %1 shapes registered" ).arg( maskShapes.size() ) );
}

void Stream::farHide( StreamCell & c, bool hide )
{
	if ( !nif )
		return;
	QVector<quint32> refs;
	if ( c.chunk )
		for ( int i = 0; i < c.chunk->refs.size(); i++ )
			refs << c.chunk->refs.at( i ).refForm;
	CellFarHide::setCell( nif, c.x, c.y, hide, refs );
	farHideRefresh( c.x, c.y );
}

//! the masked shapes reaching (x, y) (every one for INT_MIN) take their arrays again; the cards follow the cells
void Stream::farHideRefresh( int x, int y )
{
	for ( const int b : CellFarHide::maskBlocks( nif, x, y ) ) {
		Node * n = maskShapes.value( b ).data();
		if ( Shape * sh = static_cast<Shape *>( n ) )
			sh->wwRefreshData();
	}
	const CellFarHide::Hidden * h = CellFarHide::hiddenFor( nif );
	ImpostorChunk::cellFarHideSet( ( h && CellFarHide::mode() != CellFarHide::Off ) ? h->cells : QSet<qint64>() );
	if ( scene && root )
		scene->transformDirty = true;
}

//! the stream ends: every masked shape takes its whole arrays again, every card draws
void Stream::farHideForget()
{
	for ( auto it = maskShapes.cbegin(); it != maskShapes.cend(); ++it )
		if ( Shape * sh = static_cast<Shape *>( it.value().data() ) )
			sh->wwRefreshData();
	maskShapes.clear();
	if ( hideDoc ) {
		CellFarHide::clear( hideDoc );
		ImpostorChunk::cellFarHideSet( QSet<qint64>() );
	}
	hideDoc = nullptr;
	if ( scene && root && nif )   // root dies with the scene
		scene->transformDirty = true;
}

/* lane LARGEREF2: a cell joined (on) or left the attached set. A unit the layer no longer wants goes at once, in the
 * same step as the cell that now draws its ref (no frame with two copies); the far rows follow the attached units. */
void Stream::lrCell( const StreamCell & c, bool on )
{
	if ( !lrOn || c.lref )
		return;
	bool changed = lr.cell( c.x, c.y, on );
	changed = lrFlatMask() || changed;	// lane LARGEREF6: the flat mask's answer moves the layer (promote) or the boxes
	if ( !changed )
		return;
	bool gone = false;
	for ( auto it = cells.begin(); it != cells.end(); ) {
		StreamCell & u = it->second;
		if ( !u.lref || lr.layer().count( u.lref ) || &u == writing ) {
			++it;
			continue;
		}
		trace( QStringLiteral( "large ref %1 let go (cell %2,%3 %4)" ).arg( u.lref, 8, 16, QLatin1Char( '0' ) ).arg( c.x )
			.arg( c.y ).arg( on ? "attached" : "left" ) );
		if ( u.st == CellState::Attached || u.st == CellState::Writing )
			unload( u );
		it = cells.erase( it );
		gone = true;
	}
	if ( gone )
		lrPublish();
}

/* lane LARGEREF6 (LARGEREF-FLATMASK-FIX): after the owners moved. A candidate (a listed ref seen by its flat far copy
 * alone) gets its footprint once: the REFR's position, rotation and scale with the base's OBND. promote settles the
 * layer by the cut share; keepbox hands the mask the footprints over the loaded cells and refreshes the blocks there.
 * The line prints when the counts move. */
bool Stream::lrFlatMask()
{
	if ( lrMask == LargeRef::FlatMask::Accept || !seed || !seed->world )
		return false;
	const std::set<uint32_t> need = lr.pending();
	for ( uint32_t r : need ) {
		LargeRef::Box b;
		int x = 0, y = 0;
		EsmRefr e;
		float lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
		if ( lr.ownCell( r, x, y ) && seed->world->refrByForm( r, x, y, e ) && seed->world->objectBounds( e.base, lo, hi ) )
			b = LargeRef::footprint( e.pos, e.rot[2], e.scale, lo, hi );
		lr.setFootprint( r, b );
	}
	const bool changed = lr.reeval();
	if ( lrMask == LargeRef::FlatMask::KeepBox && nif ) {
		std::vector<LargeRef::Box> boxes = lr.keepBoxes();
		if ( boxes != lrBoxes ) {
			const QSet<qint64> cells = CellFarHide::setKeepBoxes( nif, boxes );
			lrBoxes = std::move( boxes );
			QSet<int> blocks;
			for ( qint64 k : cells )
				for ( int b : CellFarHide::maskBlocks( nif, int( k >> 32 ), int( qint32( quint32( k & 0xFFFFFFFFu ) ) ) ) )
					blocks.insert( b );
			for ( int b : blocks )
				if ( Shape * sh = static_cast<Shape *>( maskShapes.value( b ).data() ) )
					sh->wwRefreshData();
			if ( scene && root )
				scene->transformDirty = true;
		}
	}
	int cut = 0, prom = 0;
	lr.maskCounts( cut, prom );
	if ( cut != lrCutLast || prom != lrPromLast ) {
		lrCutLast = cut;
		lrPromLast = prom;
		if ( lrMask == LargeRef::FlatMask::KeepBox )
			std::fprintf( stderr, "cell stream: large refs flat mask keepbox: %d far copies reach the loaded cells, %d boxes kept\n",
				cut, int( lrBoxes.size() ) );
		else
			std::fprintf( stderr, "cell stream: large refs flat mask promote: %d far copies reach the loaded cells, %d promoted "
				"(cut share >= %.2f)\n", cut, prom, double( lr.promoteShare() ) );
	}
	return changed;
}

//! lane LARGEREF2: the attached units' refs: their far rows hide (CellFarHide::setExtraRefs, red nohide)
void Stream::lrPublish()
{
	if ( !lrOn || !nif )
		return;
	QVector<quint32> refs;
	for ( const auto & kv : cells )
		if ( kv.second.lref && kv.second.st == CellState::Attached && kv.second.chunk && !kv.second.nearOff )
			for ( int i = 0; i < kv.second.chunk->refs.size(); i++ )
				refs << kv.second.chunk->refs.at( i ).refForm;
	CellFarHide::setExtraRefs( nif, refs );
	if ( scene && root )
		scene->transformDirty = true;
}

//! lane PERFLOAD1 P8: one attached cell (or large-ref unit) out of the draw for the near grid's OFF, or back in
void Stream::nearOffCell( StreamCell & c, bool off )
{
	if ( c.st != CellState::Attached || c.nearOff == off )
		return;
	c.nearOff = off;
	if ( c.cellNode )
		c.cellNode->makeParent( off ? nullptr : root.data() );
	if ( !c.lref ) {
		showFar( c.x, c.y, off );   // the far tile and its rows fill the hole
		farHide( c, !off );
	}
	dirty = true;
}

/*! lane PERFLOAD1 P8: the LOD preview's near grid (lodpreview.h). FREEZE keeps the block where it was (tick's frozen
 *  eye), OFF also takes every attached cell out of the draw; FOLLOW puts them back, no reopen. Telemetry: a line on each
 *  change, then once a second while not following: the frozen cells and the eye's distance to the block's edge
 *  (negative = outside it). */
void Stream::nearGrid( const Vector3 & eye )
{
	const int mode = LodPreview::grid();
	if ( mode != gridMode ) {
		const bool off = mode == LodPreview::Off;
		bool anyLr = false;
		for ( auto & kv : cells ) {
			nearOffCell( kv.second, off );
			anyLr = anyLr || kv.second.lref;
		}
		if ( anyLr )
			lrPublish();
		gridMode = mode;
	}
	const bool say = LodPreview::gen() != gridSaid || ( mode != LodPreview::Follow
		&& ( !gridSayT.isValid() || gridSayT.elapsed() >= 1000 ) );
	if ( !say )
		return;
	gridSaid = LodPreview::gen();
	gridSayT.start();
	const float x0 = float( ccx - gridHalf ) * kCell, x1 = float( ccx + gridHalf + 1 ) * kCell;
	const float y0 = float( ccy - gridHalf ) * kCell, y1 = float( ccy + gridHalf + 1 ) * kCell;
	const float edge = std::min( std::min( eye[0] - x0, x1 - eye[0] ), std::min( eye[1] - y0, y1 - eye[1] ) );
	int attached = 0, hidden = 0;
	for ( const auto & kv : cells )
		if ( kv.second.st == CellState::Attached ) {
			attached++;
			hidden += kv.second.nearOff ? 1 : 0;
		}
	qInfo().noquote() << QStringLiteral( "lod preview: near grid %1: cells %2..%3, %4..%5 %6; %7 attached, %8 hidden; "
		"eye %9 units from the grid edge%10" ).arg( LodPreview::gridName( mode ) ).arg( ccx - gridHalf )
		.arg( ccx + gridHalf ).arg( ccy - gridHalf ).arg( ccy + gridHalf )
		.arg( mode == LodPreview::Follow ? QStringLiteral( "(following)" ) : QStringLiteral( "(frozen)" ) )
		.arg( attached ).arg( hidden ).arg( double( edge ), 0, 'f', 0 )
		.arg( edge < 0.0f ? QStringLiteral( " (outside it)" ) : QString() );
}

void Stream::showFar( int x, int y, bool show )
{
	for ( FarTile & f : farTiles ) {
		if ( f.x != x || f.y != y || !f.node )
			continue;
		f.node->makeParent( show ? f.parent.data() : nullptr );
	}
}

void Stream::wire( StreamCell & c )
{
	for ( size_t i = c.takesSeen; i < c.takes.size(); i++ ) {
		const QString & t = c.takes[i].second;
		if ( t == QLatin1String( "BSOrderedNode" ) ) {
			c.containers.push_back( c.takes[i].first );
			c.grounds.insert( c.takes[i].first );
		} else if ( t == QLatin1String( "NiBillboardNode" ) ) {
			c.containers.push_back( c.takes[i].first );
		}
	}
	c.takesSeen = c.takes.size();
	// parents first: the cell's own children (ground, billboards) are made before their children are read
	// lane CELLSTREAM1: only the rows added since the last slice (reading every child each slice grew with the cell)
	for ( size_t ci = 0; ci < c.containers.size(); ci++ ) {
		Node * parent = c.nodes.value( c.containers[ci] ).data();
		if ( !parent )
			continue;
		const QModelIndex iP = nif->getBlockIndex( c.containers[ci] );
		const QModelIndex iCh = nif->getIndex( iP, "Children" );
		const int rows = iCh.isValid() ? nif->rowCount( iCh ) : 0;
		int & seen = c.childrenSeen[c.containers[ci]];
		for ( int r = std::min( seen, rows ); r < rows; r++ ) {
			const qint32 l = nif->getLink( nif->getIndex( iCh, r ) );
			if ( l < 0 || c.nodes.contains( l ) )
				continue;
			const QModelIndex iC = nif->getBlockIndex( l );
			Node * n = makeNode( iC, nif->itemName( iC ) );
			c.nodes.insert( l, n );
			if ( n )
				n->makeParent( parent );
		}
		seen = rows;
	}
}

bool Stream::write( StreamCell & c, double budget )
{
	QElapsedTimer t;
	t.start();
	if ( !c.chunk || !c.chunk->ok ) {
		if ( c.chunk && !c.chunk->error.isEmpty() )
			st.lastError = QStringLiteral( "%1,%2: %3" ).arg( c.x ).arg( c.y ).arg( c.chunk->error );
		st.failed++;
		c.st = CellState::Attached;   // nothing to show: its far tile stays
		return false;
	}
	if ( c.emitted ) {   // lane CELLSTREAM1: the hookup runs in its own slices
		g_phase = "attach";
		const bool hooked = attach( c, t, budget );
		const double ms = msOf( t );
		st.sliceMaxMs = std::max( st.sliceMaxMs, ms );
		if ( ms > budget + 2.0 )
			trace( QStringLiteral( "slow attach slice %1,%2: %3 ms (%4 of %5 containers linked)" ).arg( c.x ).arg( c.y )
				.arg( ms, 0, 'f', 1 ).arg( c.linked ).arg( c.containers.size() ) );
		c.writeMs += ms;
		sliceEstMs = std::max( 0.9 * sliceEstMs, ms );
		return hooked;
	}
	writing = &c;
	if ( c.st == CellState::Ready ) {
		const QModelIndex iCell = take( nif, QStringLiteral( "NiNode" ) );
		nif->set<QString>( iCell, "Name", c.lref ? QStringLiteral( "large ref %1" ).arg( c.lref, 8, 16, QLatin1Char( '0' ) )
			: QStringLiteral( "cell %1,%2" ).arg( c.x ).arg( c.y ) );
		nif->set<quint32>( iCell, "Flags", 14 );
		nif->set<float>( iCell, "Scale", 1.0f );
		c.cellBlock = nif->getBlockNumber( iCell );
		c.containers = { c.cellBlock };
		Node * n = makeNode( iCell, QStringLiteral( "NiNode" ) );
		c.cellNode = n;
		c.nodes.insert( c.cellBlock, n );
		c.st = CellState::Writing;
	}
	QString err;
	// the slice stops when its writing plus the wiring it owes (wireOwedMs) would pass the budget
	g_phase = "emit";
	sliceNodes = 0;
	const bool done = cellChunkEmit( nif, *c.chunk, *this, nif->getBlockIndex( c.cellBlock ),
		std::max( 0.25, budget - msOf( t ) ), &err );
	const double emitMs = msOf( t );
	g_phase = "wire";
	const size_t before = c.nodes.size();
	wire( c );
	if ( done && c.chunk && c.chunk->pfxCopies > c.pfx ) {
		// lane LIVESET3 (STREAMPSYS1): the copied branches' nodes were made with their parent; the cell keeps them
		pfxLive += c.chunk->pfxCopies - c.pfx;
		c.pfx = c.chunk->pfxCopies;
		for ( const auto & tk : c.takes )
			if ( isNodeType( tk.second ) && !c.nodes.contains( tk.first ) )
				if ( Node * pn = scene->nodes.get( nif->getBlockIndex( tk.first ) ) )
					c.nodes.insert( tk.first, pn );
	}
	const int made = int( c.nodes.size() - before );
	if ( made > 0 )
		wirePerNodeMs = 0.7 * wirePerNodeMs + 0.3 * ( ( msOf( t ) - emitMs ) / made );
	sliceNodes = 0;
	writing = nullptr;
	if ( !err.isEmpty() )
		st.lastError = QStringLiteral( "%1,%2: %3" ).arg( c.x ).arg( c.y ).arg( err );
	const double wireMs = msOf( t ) - emitMs;
	g_phase = "attach";
	bool hooked = false;
	if ( done ) {
		c.emitted = true;
		hooked = attach( c, t, budget );
	}
	const double ms = msOf( t );
	st.sliceMaxMs = std::max( st.sliceMaxMs, ms );
	if ( ms > budget + 2.0 )
		trace( QStringLiteral( "slow slice %1,%2: %3 ms (emit %4, wire %5 nodes %6, attach %7)" ).arg( c.x ).arg( c.y )
			.arg( ms, 0, 'f', 1 ).arg( emitMs, 0, 'f', 1 ).arg( wireMs, 0, 'f', 1 ).arg( int( c.nodes.size() - before ) )
			.arg( ms - emitMs - wireMs, 0, 'f', 1 )
			+ ( building() ? QStringLiteral( " worker busy" ) : QStringLiteral( " worker idle" ) ) );
	c.writeMs += ms;
	sliceEstMs = std::max( 0.9 * sliceEstMs, ms );
	return hooked;
}

namespace
{
struct CardSync
{
	int owners = 0, ctrls = 0, missing = 0, active = 0;
};

/*! lane STREAMGN1: a cell's controllers read again at its hookup, so a streamed card animates as the open's did. A
 *  copied branch's scene objects are made with its parent, before every controller, interpolator and data block of the
 *  copy is written; in recycled, quietly reset blocks they kept what they read then. Each node and property with a
 *  Controller chain syncs its list, every controller re-reads flags, interpolator and data, then takes the sequence.
 *  Red WW_CELL_STREAM_RED=nocardsync leaves it out. */
void syncControllers( const NifModel * nif, Scene * scene, IControllable * o, CardSync & s )
{
	const QModelIndex iFirst = nif->getBlockIndex( nif->getLink( o->index(), "Controller" ) );
	if ( !iFirst.isValid() )
		return;
	s.owners++;
	auto chain = [&]( auto && each ) {
		int guard = 0;
		for ( QModelIndex i = iFirst; i.isValid() && nif->blockInherits( i, "NiTimeController" ) && guard++ < 64;
			i = nif->getBlockIndex( nif->getLink( i, "Next Controller" ) ) )
			each( i );
	};
	bool missing = false;
	chain( [&]( const QModelIndex & i ) { missing |= !o->findController( i ); } );
	if ( missing ) {
		s.missing++;
		o->update( nif, o->index() );
	}
	chain( [&]( const QModelIndex & i ) {
		s.ctrls++;
		o->update( nif, i );
		if ( Controller * ct = o->findController( i ) )
			s.active += ct->active ? 1 : 0;
	} );
	o->setSequence( scene->animGroup );
}
}

bool Stream::attach( StreamCell & c, const QElapsedTimer & t, double budget )
{
	// lane CELLSTREAM1: the containers' links a few a slice, then the hookup itself (one step: no half-shown cell)
	while ( c.linked < c.containers.size() ) {
		if ( msOf( t ) >= budget )
			return false;
		nif->updateBlockLinks( c.containers[c.linked++] );
	}
	if ( msOf( t ) >= budget * 0.75 )
		return false;
	setRootSlot( -1, c.cellBlock );
	if ( c.cellNode && root )
		c.cellNode->makeParent( root.data() );
	if ( !c.lref ) {	// lane LARGEREF2: a unit's far rows go by lrPublish
		showFar( c.x, c.y, false );   // the near ground replaces the far tile in the same step: no overlap
		farHide( c, true );   // lane FARHOLE2: and the far objects, cards and rings past 0 it holds
	}
	if ( red != QLatin1String( "nocardsync" ) && scene ) {	// lane STREAMGN1: syncControllers
		CardSync s;
		QSet<IControllable *> seen;
		for ( auto it = c.nodes.cbegin(); it != c.nodes.cend(); ++it ) {
			Node * n = it.value().data();
			if ( !n )
				continue;
			if ( !seen.contains( n ) ) {
				seen.insert( n );
				syncControllers( nif, scene, n, s );
			}
			for ( Property * pr : n->propertyList() )
				if ( pr && !seen.contains( pr ) ) {
					seen.insert( pr );
					syncControllers( nif, scene, pr, s );
				}
		}
		if ( s.ctrls > 0 ) {
			std::fprintf( stderr, "cell stream: controllers synced at the hookup of %d,%d: %d owners, %d controllers (%d active), %d lists rebuilt\n",
				c.x, c.y, s.owners, s.ctrls, s.active, s.missing );
			std::fflush( stderr );
		}
	}
	c.st = CellState::Attached;
	if ( gridMode == LodPreview::Off ) {	// lane PERFLOAD1 P8: a cell attached under the near grid's OFF stays hidden
		c.nearOff = false;
		nearOffCell( c, true );
	}
	if ( c.lref )
		lrPublish();
	else
		lrCell( c, true );
	st.attaches++;
	trace( QStringLiteral( "attach %1,%2 block %3 after %4 ms of writing" ).arg( c.x ).arg( c.y ).arg( c.cellBlock ).arg( c.writeMs, 0, 'f', 1 ) );
	dirty = true;
	return true;
}

void Stream::unload( StreamCell & c )
{
	QElapsedTimer t;
	t.start();
	trace( QStringLiteral( "unload %1,%2 state %3" ).arg( c.x ).arg( c.y ).arg( int( c.st ) ) );
	if ( c.cellNode )
		c.cellNode->makeParent( nullptr );
	if ( c.cellBlock >= 0 && c.st == CellState::Attached )
		setRootSlot( c.cellBlock, -1 );
	const double msRoot = msOf( t );
	const bool wasAttached = c.st == CellState::Attached;	// lane LARGEREF2
	if ( !c.lref && !c.nearOff ) {	// lane PERFLOAD1 P8: a cell the near grid's OFF hid has its far tile out already
		showFar( c.x, c.y, true );
		farHide( c, false );   // lane FARHOLE2
	}
	c.nearOff = false;
	const double msFar = msOf( t );
	QSet<int> nodeBlocks, propBlocks;
	for ( const auto & tk : c.takes ) {
		const int b = tk.first;
		cellMeshDrop( nif, b );
		wwCellCullForget( nif, b );
		wwCellWaterForget( nif, b );
		wwCellLandForget( nif, b );
		wwCellGrassForget( nif, b );
		wwCellFxLitForget( nif, b );
		wwCellXemiForget( nif, b );   // lane LIGHTS2 (XEMI1)
		blockCell.remove( b );   // lane STREAMTEX1
		( isNodeType( tk.second ) ? nodeBlocks : propBlocks ).insert( b );
	}
	texPool.dropCell( texPool.cellId( keyOf( c ).first, keyOf( c ).second ) );   // lane STREAMTEX1: its textures lose this cell's count
	const double msTakes = msOf( t );
	std::vector<Node *> ns;
	for ( auto it = c.nodes.cbegin(); it != c.nodes.cend(); ++it )
		if ( it.value() )
			ns.push_back( it.value().data() );
	int scanned = 0;
	if ( int( ns.size() ) < nodeBlocks.size() ) {   // the scene was made again: find them by block
		ns.clear();
		scanned = scene->nodes.list().size();
		st.scannedNodes += scanned;
		for ( Node * n : scene->nodes.list() )
			if ( nodeBlocks.contains( nif->getBlockNumber( n->index() ) ) )
				ns.push_back( n );
	}
	const double msForget = msOf( t );
	QSet<Shape *> gone;
	for ( Node * n : ns )
		if ( Shape * s = dynamic_cast<Shape *>( n ) )
			gone.insert( s );
	if ( !gone.isEmpty() )
		scene->shapes.erase( std::remove_if( scene->shapes.begin(), scene->shapes.end(),
			[&gone]( Shape * s ) { return gone.contains( s ); } ), scene->shapes.end() );
	// lane CELLSTREAM1: the cell's properties are the ones its nodes hold; one pass each over the scene lists
	QSet<Property *> props;
	for ( Node * n : ns )
		for ( Property * pr : n->propertyList() )
			if ( propBlocks.contains( nif->getBlockNumber( pr->index() ) ) )
				props.insert( pr );
	const double msShapes = msOf( t );
	// lane CELLSTREAM1: out of the scene now, cleared and freed a few a frame (bury); 213 at once took 7.6 ms
	scene->nodes.takeMany( QSet<Node *>( ns.begin(), ns.end() ) );
	toClear.insert( toClear.end(), ns.begin(), ns.end() );
	const double msNodes = msOf( t );
	scene->properties.delMany( props );
	const double msProps = msOf( t );
	toReset.insert( toReset.end(), c.takes.begin(), c.takes.end() );
	pfxLive -= c.pfx;
	c.pfx = 0;
	c.takes.clear();
	c.takesSeen = 0;
	c.nodes.clear();
	c.containers.clear();
	c.grounds.clear();
	c.cellNode = nullptr;
	c.cellBlock = -1;
	c.chunk.reset();
	st.unloads++;
	if ( c.lref )	// lane LARGEREF2: its far rows draw again; a cell's leaving may hand its refs to the layer
		lrPublish();
	else if ( wasAttached )
		lrCell( c, false );
	// lane CELLSPIKE2: each part of the dearest unload (the dump's worstUnload field, the trace past 3 ms)
	const double ms = msOf( t );
	const QString parts = QStringLiteral( "wuRoot %1 wuFar %2 wuForget %3 wuFind %4 wuShapes %5 wuNodesOut %6 wuProps %7 "
		"wuTail %8 wuNodes %9 wuTakes %10 wuScanned %11" ).arg( msRoot, 0, 'f', 2 ).arg( msFar - msRoot, 0, 'f', 2 )
		.arg( msTakes - msFar, 0, 'f', 2 ).arg( msForget - msTakes, 0, 'f', 2 ).arg( msShapes - msForget, 0, 'f', 2 )
		.arg( msNodes - msShapes, 0, 'f', 2 ).arg( msProps - msNodes, 0, 'f', 2 ).arg( ms - msProps, 0, 'f', 2 )
		.arg( ns.size() ).arg( nodeBlocks.size() + propBlocks.size() ).arg( scanned );
	if ( ms > 3.0 )
		trace( QStringLiteral( "slow unload %1,%2: %3 ms (%4)" ).arg( c.x ).arg( c.y ).arg( ms, 0, 'f', 1 ).arg( parts ) );
	if ( ms > st.unloadMaxMs )
		st.worstUnload = QStringLiteral( "wuMs %1 " ).arg( ms, 0, 'f', 2 ) + parts;
	st.unloadMaxMs = std::max( st.unloadMaxMs, ms );
	dirty = true;
}

void Stream::bury( const QElapsedTimer & t )
{
	// lane CELLSTREAM1: every node of the left cells drops its links first (they count each other), then each goes
	QElapsedTimer d;
	d.start();
	// lane CELLSTREAM1: an item starts only if the dearest recent one would still fit (the budget check alone
	// let one 10 ms free run past it); the first of a frame may take the half budget
	int items = 0;
	auto fits = [&]( double est ) { return msOf( t ) + est < budgetMs || ( items == 0 && msOf( t ) < 0.5 * budgetMs ); };
	while ( !toClear.empty() && fits( clearEstMs ) ) {
		Node * n = toClear.back();
		toClear.pop_back();
		const double n0 = msOf( d );
		if ( Shape * sh = dynamic_cast<Shape *>( n ) ) {   // its arrays go to the reaper, not the heap here
			dead.emplace_back();
			DeadArrays & a = dead.back();
			a.verts.swap( sh->verts );
			a.norms.swap( sh->norms );
			a.tangents.swap( sh->tangents );
			a.bitangents.swap( sh->bitangents );
			a.colors.swap( sh->colors );
			a.coords.swap( sh->coords );
			a.triangles.swap( sh->triangles );
		}
		n->clear();
		items++;
		clearEstMs = std::max( 0.9 * clearEstMs, msOf( d ) - n0 );
		if ( msOf( d ) - n0 > 2.0 )
			trace( QStringLiteral( "slow node clear: %1 ms (%2)" ).arg( msOf( d ) - n0, 0, 'f', 1 ).arg( QString::fromLatin1( typeid( *n ).name() ) ) );
		toFree.push_back( n );
	}
	while ( toClear.empty() && !toFree.empty() && fits( freeEstMs ) ) {
		Node * n = toFree.back();
		toFree.pop_back();
		const double n0 = msOf( d );
		QString kind = QString::fromLatin1( typeid( *n ).name() );
		if ( const Shape * sh = dynamic_cast<const Shape *>( n ) )
			kind += QStringLiteral( " verts cap %1 tris cap %2" ).arg( sh->verts.capacity() ).arg( sh->triangles.capacity() );
		NodeList::release( n );
		items++;
		freeEstMs = std::max( 0.9 * freeEstMs, msOf( d ) - n0 );
		if ( msOf( d ) - n0 > 2.0 )
			trace( QStringLiteral( "slow node free: %1 ms (%2)" ).arg( msOf( d ) - n0, 0, 'f', 1 ).arg( kind ) );
	}
	st.buryMaxMs = std::max( st.buryMaxMs, msOf( d ) );
}

void Stream::reap()
{
	if ( ( dead.empty() && deadItems.empty() ) || !reaperDone.load() )
		return;
	if ( reaper.joinable() )
		reaper.join();   // done already: a quick join
	reaperDone = false;
	const bool ownSlots = !spikeRed( "lock" );
	reaper = std::thread( [batch = std::move( dead ), items = std::move( deadItems ), done = &reaperDone, ownSlots]() mutable {
		batch.clear();
		std::optional<NifItemThreadCache> itemRun;   // lane CELLSPIKE3: the frees go back in one splice (red lock: per item)
		if ( ownSlots )
			itemRun.emplace();
		for ( NifItem * i : items )   // lane CELLSPIKE3: plain frees (pool slots, strings); no model or parent is touched
			delete i;
		itemRun.reset();
		done->store( true );
	} );
	dead.clear();
	deadItems.clear();
}

void Stream::drain( const QElapsedTimer & t )
{
	// freed blocks go back to their type's defaults before reuse, a few a frame (their scene nodes gone first:
	// a node's persistent index would make the reset loud)
	QElapsedTimer d;
	d.start();
	if ( toReset.empty() || building() || !toClear.empty() || !toFree.empty() )
		return;
	// lane CELLSTREAM1: a freed block nothing points into resets without row signals (red WW_CELL_STREAM_RED=loudreset)
	const QSet<int> pinned = red == QLatin1String( "loudreset" ) ? QSet<int>() : nif->blocksWithPersistent();
	const bool loud = red == QLatin1String( "loudreset" );
	const double scanMs = msOf( d );
	// lane CELLSPIKE3: the old rows go to the reaper (red: freed here), and a reset starts only if the dearest recent
	// one still fits the budget (the first of a frame may take the half budget, as bury)
	const bool redDrain = spikeRed( "drain" );
	g_nifResetGrave = redDrain ? nullptr : &deadItems;
	int items = 0;
	auto fits = [&]() { return redDrain ? msOf( t ) < budgetMs
		: ( msOf( t ) + resetEstMs < budgetMs || ( items == 0 && msOf( t ) < 0.5 * budgetMs ) ); };
	while ( !toReset.empty() && fits() ) {   // the writes wait for an empty pool queue anyway
		const auto tk = toReset.back();
		toReset.pop_back();
		const double r0 = msOf( d );
		const bool quiet = !loud && !pinned.contains( tk.first );
		st.quietResets += quiet ? 1 : 0;
		st.loudResets += quiet ? 0 : 1;
		const bool reset = nif->resetNiBlock( tk.first, quiet );
		items++;
		resetEstMs = std::max( 0.9 * resetEstMs, msOf( d ) - r0 );
		if ( reset )
			pool[tk.second].insert( tk.first );
		if ( msOf( d ) - r0 > 3.0 )
			trace( QStringLiteral( "slow reset %1 %2%8: %3 ms (remove %4 insert %5 links %6, %7 persistent, scan %9 ms)" )
				.arg( tk.second ).arg( tk.first ).arg( msOf( d ) - r0, 0, 'f', 1 ).arg( nif->resetMs[0], 0, 'f', 1 )
				.arg( nif->resetMs[1], 0, 'f', 1 ).arg( nif->resetMs[2], 0, 'f', 1 ).arg( nif->persistentNow )
				.arg( quiet ? " quiet" : "" ).arg( scanMs, 0, 'f', 2 )
				+ ( worker.joinable() && !jobDone.load() ? QStringLiteral( " worker busy" ) : QStringLiteral( " worker idle" ) ) );
	}
	g_nifResetGrave = nullptr;
	st.drainMaxMs = std::max( st.drainMaxMs, msOf( d ) );
	if ( msOf( d ) > budgetMs )
		trace( QStringLiteral( "slow drain: %1 ms (scan %2 ms, tick at %3 ms on entry)" ).arg( msOf( d ), 0, 'f', 1 )
			.arg( scanMs, 0, 'f', 2 ).arg( msOf( t ) - msOf( d ), 0, 'f', 1 ) );
}

void Stream::rebuild()
{
	QElapsedTimer t;
	t.start();
	WwCellLighting L = seed->base;
	CellRefTable & refs = cellRefTableMutable();
	refs = baseRefs;
	CellPickTable & picks = cellPickTableMutable();
	picks = basePicks;
	wwCellFxLitBegin( nif );
	for ( auto & kv : cells ) {
		const StreamCell & c = kv.second;
		if ( c.st != CellState::Attached || !c.chunk || !c.chunk->ok )
			continue;
		const CellChunk & k = *c.chunk;
		L.lights += k.lights.lights;
		L.ambientLights += k.lights.ambientLights;
		L.offLights += k.lights.offLights;
		L.nifxFixtures += k.lights.nifxFixtures;	// lane NIFXLIGHT1
		L.nifxOffPlaced += k.lights.nifxOffPlaced;
		for ( int i = 0; i < k.refs.size(); i++ )
			refs.append( k.refs.at( i ) );
		for ( int i = 0; i < k.refs.cellCount(); i++ )
			refs.addCell( k.refs.cellAt( i ) );
		for ( int i = 0; i < k.picks.size(); i++ )
			picks.append( k.picks.at( i ) );
		QVector<int> serial;
		for ( const WwFxLitModel & m : k.fxModels )
			serial.append( wwCellFxLitModel( nif, m ) );
		for ( const auto & fs : k.fxShapes )
			if ( fs.second >= 0 && fs.second < serial.size() )
				wwCellFxLitShape( nif, fs.first, serial[fs.second] );
		for ( const auto & xs : k.xemiShapes )
			wwCellXemiShape( nif, xs.first, xs.second );   // lane LIGHTS2 (XEMI1)
	}
	refs.tallyCells();
	wwCellLightsPublish( nif, L );
	wwCellFxLitPick( nif );
	dirty = false;
	traceDirty = true;   // lane AIRPERF1 r4: the traced set follows (tick)
	st.rebuildMaxMs = std::max( st.rebuildMaxMs, msOf( t ) );
}

namespace
{
constexpr double kFarReadMs = 4.0;   //!< lane AIRPERF1 r5: the main thread's time a tick for reading far shapes (one at least)

bool spanInRing( const std::array<int, 4> & s, int cx, int cy, int ring )
{
	return !( s[2] < cx - ring || s[0] > cx + ring || s[3] < cy - ring || s[1] > cy + ring );
}
}

/*! lane AIRPERF1 r5: the far shapes that reach the ring are read into what the trace takes (rtfartrace.h), each once,
 *  within `budgetMs` a call (one shape at least). Returns how many of the ring's shapes are still to read. A shape
 *  that left the ring by more than two cells is freed. */
int Stream::traceFarRead( int ring, double budgetMs )
{
	if ( !scene || rtTraceSetRed( "nofar" ) ) {
		traceFar.clear();
		traceAlb.reset();
		return 0;
	}
	QElapsedTimer t;
	t.start();
	const bool flatRed = rtTraceSetRed( "fardirt" ), noObj = rtTraceSetRed( "nofarobj" ), noWater = rtTraceSetRed( "nofarwater" );
	QSet<int> seen;
	int pending = 0;
	bool built = false;
	for ( Shape * sh : scene->shapes ) {
		if ( !sh )
			continue;
		const QString bn = sh->getName();
		// round 6: far water is a source of its own (2); the endless sea's one plane stays out (rttracesetrule.h farSrcOf)
		const int src = rttracesetrule::farSrcOf( bn.toLatin1().constData() );
		if ( src < 0 )
			continue;
		const int b = nif->getBlockNumber( sh->index() );
		const quint64 sig = b < 0 ? 0 : rtFarShapeSig( nif, sh );
		if ( !sig )
			continue;
		seen.insert( b );
		TraceFar & tf = traceFar[b];
		if ( tf.sig != sig ) {   // new, or the far field was rebuilt: the cells it reaches
			tf = TraceFar();
			tf.sig = sig;
			tf.src = src;
			const QSharedPointer<CellMesh> cm = cellMeshFor( nif, sh->index() );
			if ( cm && cm->inst ) {
				bool first = true;
				for ( const qint64 k : cm->inst->rowCell ) {
					const int x = int( k >> 32 ), y = int( qint32( quint32( k & 0xFFFFFFFFll ) ) );
					if ( first )
						tf.span = { { x, y, x, y } };
					else
						tf.span = { { std::min( tf.span[0], x ), std::min( tf.span[1], y ), std::max( tf.span[2], x ), std::max( tf.span[3], y ) } };
					first = false;
				}
				if ( first )   // rows without their cells: looked at wherever the ring is
					tf.span = { { INT_MIN / 2, INT_MIN / 2, INT_MAX / 2, INT_MAX / 2 } };
			} else {
				const QVector<Vector3> & V = cm ? cm->verts : sh->verts;
				const Transform w = sh->restWorldTrans();
				float lo[2] = { 3.4e38f, 3.4e38f }, hi[2] = { -3.4e38f, -3.4e38f };
				for ( const Vector3 & v : V ) {
					const Vector3 p = w * v;
					lo[0] = std::min( lo[0], p[0] );
					lo[1] = std::min( lo[1], p[1] );
					hi[0] = std::max( hi[0], p[0] );
					hi[1] = std::max( hi[1], p[1] );
				}
				tf.span = { { cellOf( lo[0] ), cellOf( lo[1] ), cellOf( hi[0] ), cellOf( hi[1] ) } };
			}
		}
		if ( !spanInRing( tf.span, ccx, ccy, ring ) ) {
			if ( !spanInRing( tf.span, ccx, ccy, ring + 2 ) )
				tf.shape.reset();
			continue;
		}
		if ( ( tf.src == 1 && noObj ) || ( tf.src == 2 && noWater ) || ( tf.shape && tf.flat == flatRed ) )
			continue;
		if ( built && msOf( t ) >= budgetMs ) {
			pending++;
			continue;
		}
		if ( !traceAlb )
			traceAlb = std::make_unique<ProbeAlbedo>( seed ? seed->dataRoot : QString() );
		tf.shape = rtFarShapeBuild( nif, sh, tf.src, *traceAlb, flatRed );
		tf.flat = flatRed;
		built = true;
		traceReads++;
		traceReadMs += tf.shape->ms;
		traceReadMaxMs = std::max( traceReadMaxMs, tf.shape->ms );
	}
	for ( auto it = traceFar.begin(); it != traceFar.end(); )
		it = seen.contains( it.key() ) ? std::next( it ) : traceFar.erase( it );
	if ( !pending )
		traceAlb.reset();   // the maps go; the shapes keep their colours
	return pending;
}

/*! lane AIRPERF1 r4: the loaded cells' soups and the far field round them go to the ray-traced GI's traced set
 *  (features/gi/rttraceset.h), which merges and publishes them off the main thread.
 *  Round 5: the far field is what is drawn out there, terrain AND objects (the far objects' arrays, the instanced far
 *  rows, the game's own far chunks), each triangle with its diffuse map's colour, cut to the cells within `ring` of
 *  the block's centre and to what no loaded cell traces already (rttracesetrule.h farSkip), the objects within a
 *  triangle budget, nearest cells first (farCap). Reds WW_CELL_RTSET_RED = nofar (no far field), nofarobj (round 4:
 *  the terrain alone), fardirt (round 4: one stated tone), nofarcap (no budget). */
void Stream::traceSync( int ring, int ex, int ey )
{
	QElapsedTimer t;
	t.start();
	std::vector<RtTraceUnit> units;
	std::set<Key> loaded;
	int noSoup = 0;
	for ( const auto & kv : cells ) {
		const StreamCell & c = kv.second;
		if ( c.st != CellState::Attached || !c.chunk || !c.chunk->ok )
			continue;
		if ( !c.lref )
			loaded.insert( Key( c.x, c.y ) );
		if ( !c.chunk->soup ) {
			noSoup++;
			continue;
		}
		RtTraceUnit u;
		u.x = c.x;
		u.y = c.y;
		u.lref = c.lref;
		u.soup = c.chunk->soup;
		units.push_back( std::move( u ) );
	}
	std::vector<RtFarPick> picks;
	RtFarInfo info;
	std::set<Key> farCells;
	info.pending = traceFarRead( ring, kFarReadMs );
	traceMore = info.pending > 0;
	int terrainShapes = 0, objShapes = 0, waterShapes = 0;
	std::set<quint32> cardRefs;   // round 6: the carded trees traced as their authored far mesh
	if ( !traceFar.isEmpty() ) {
		const CellFarHide::Hidden * hidden = CellFarHide::hiddenFor( nif );
		const bool noObj = rtTraceSetRed( "nofarobj" ), noWater = rtTraceSetRed( "nofarwater" ), noCards = rtTraceSetRed( "nofarcards" );
		const auto cellXY = []( qint64 k, int & x, int & y ) {
			x = int( k >> 32 );
			y = int( qint32( quint32( k & 0xFFFFFFFFll ) ) );
		};
		QList<int> blocks = traceFar.keys();
		std::sort( blocks.begin(), blocks.end() );   // one order, so the same picks give the same key
		std::vector<int> pickBlock;
		std::map<qint64, std::uint64_t> objCell;   // the far objects' triangles a cell
		for ( const int b : blocks ) {
			const TraceFar & tf = *traceFar.constFind( b );
			if ( !tf.shape )
				continue;
			info.cacheShapes++;
			info.cacheMb += tf.shape->bytes() / 1048576.0;
			if ( ( tf.src == 1 && noObj ) || ( tf.src == 2 && noWater ) || !spanInRing( tf.span, ccx, ccy, ring ) )
				continue;
			const RtFarShape & f = *tf.shape;
			RtFarPick p;
			for ( int i = 0; i < int( f.runs.size() ); i++ ) {
				const RtFarShape::Run & r = f.runs[size_t( i )];
				int x = 0, y = 0;
				cellXY( r.cell, x, y );
				if ( r.count <= 0 || !rttracesetrule::inRing( x, y, ccx, ccy, ring ) || ( r.kind == 3 && noCards ) )
					continue;
				if ( rttracesetrule::farSkip( r.ref, r.ref && hidden && hidden->refs.contains( r.ref ), loaded.count( Key( x, y ) ) > 0 ) )
					continue;
				p.runs.push_back( i );
				if ( f.src == 1 )
					objCell[r.cell] += std::uint64_t( r.count );
			}
			if ( p.runs.empty() )
				continue;
			p.shape = tf.shape;
			picks.push_back( std::move( p ) );
			pickBlock.push_back( b );
		}
		// the far objects' budget: whole cells, nearest the centre first
		std::vector<rttracesetrule::FarCell> oc;
		oc.reserve( objCell.size() );
		for ( const auto & kv : objCell ) {
			rttracesetrule::FarCell c;
			cellXY( kv.first, c.x, c.y );
			c.tris = kv.second;
			oc.push_back( c );
		}
		std::uint64_t dropped = 0;
		const std::uint64_t cap = rtTraceSetRed( "nofarcap" ) ? ~std::uint64_t( 0 ) : rttracesetrule::kFarObjTriCap;
		const size_t keep = rttracesetrule::farCap( oc, ccx, ccy, cap, &dropped );
		std::set<qint64> kept;
		for ( size_t i = 0; i < keep; i++ )
			kept.insert( CellFarHide::cellKey( oc[i].x, oc[i].y ) );
		info.objCells = int( keep );
		info.objCellsDropped = int( oc.size() - keep );
		info.objDropped = qint64( dropped );
		std::vector<RtFarPick> out;
		quint64 key = 1469598103934665603ull;
		const auto mix = [&key]( quint64 v ) { key = ( key ^ v ) * 1099511628211ull; };
		for ( size_t n = 0; n < picks.size(); n++ ) {
			RtFarPick & p = picks[n];
			const RtFarShape & f = *p.shape;
			if ( f.src == 1 && keep < oc.size() ) {
				std::vector<int> left;
				for ( const int i : p.runs )
					if ( kept.count( f.runs[size_t( i )].cell ) )
						left.push_back( i );
				p.runs.swap( left );
			}
			if ( p.runs.empty() )
				continue;
			mix( quint64( pickBlock[n] ) );
			mix( f.sig );
			mix( f.flat > 0 ? 2 : 1 );
			qint64 tris = 0, byKind[4] = { 0, 0, 0, 0 };
			for ( const int i : p.runs ) {
				const RtFarShape::Run & r = f.runs[size_t( i )];
				int x = 0, y = 0;
				cellXY( r.cell, x, y );
				farCells.insert( Key( x, y ) );
				tris += r.count;
				byKind[r.kind & 3] += r.count;
				if ( r.kind == 3 && r.ref )
					cardRefs.insert( r.ref );
				mix( quint64( i ) );
			}
			// round 6: a triangle counts under what it is (water and a carded tree's authored mesh have their own counts)
			info.terrainTris += byKind[0];
			info.objTris += byKind[1];
			info.waterTris += byKind[2];
			info.cardTris += byKind[3];
			( f.src == 2 ? waterShapes : ( f.src == 1 ? objShapes : terrainShapes ) )++;
			if ( f.flat > 0 )
				info.flat += tris;
			out.push_back( std::move( p ) );
		}
		picks.swap( out );
		info.key = key;
		info.cardTrees = int( cardRefs.size() );
		// the carded trees of the ring's far cells, and those with no authored mesh row (never traced: no mesh is made up)
		QSet<qint64> ringCells;
		for ( int y = ccy - ring; y <= ccy + ring; y++ )
			for ( int x = ccx - ring; x <= ccx + ring; x++ )
				if ( !loaded.count( Key( x, y ) ) )
					ringCells.insert( ImpostorChunk::cellFarCellKey( x, y ) );
		ImpostorChunk::cellFarCardsMeshIn( ringCells, &info.cardsInRing, &info.cardsNoMesh );
	}
	info.cells = int( farCells.size() );
	info.shapes = int( picks.size() );
	const int nUnits = int( units.size() );
	const RtFarInfo said = info;
	rtTraceSetSync( nif, std::move( units ), ccx, ccy, ex, ey, ring, std::move( picks ), info, noSoup );
	traceDirty = false;
	traceRing = ring;
	traceOn = true;
	std::fprintf( stderr, "cell stream: traced set handed over: %d units of %d loaded cells (%d without a soup), far terrain %lld "
		"triangles from %d shapes, far objects %lld triangles from %d shapes in %d cells (budget %lld: %lld triangles of %d cells "
		"left out), far cells %d, %lld far triangles on a stated tone, %d far shapes still to read (ring %d round centre %d,%d), "
		"camera cell %d,%d, gather %.2f ms | far shapes read so far %d in %.1f ms (max %.1f ms a shape), kept %d shapes %.1f MB\n",
		nUnits, int( loaded.size() ), noSoup, static_cast<long long>( said.terrainTris ), terrainShapes,
		static_cast<long long>( said.objTris ), objShapes, said.objCells, static_cast<long long>( rttracesetrule::kFarObjTriCap ),
		static_cast<long long>( said.objDropped ), said.objCellsDropped, said.cells, static_cast<long long>( said.flat ), said.pending,
		ring, ccx, ccy, ex, ey, msOf( t ), traceReads, traceReadMs, traceReadMaxMs, said.cacheShapes, said.cacheMb );
	// round 6: far water and far tree cards
	std::fprintf( stderr, "cell stream: traced set handed over: far water %lld triangles from %d shapes%s | far tree cards %lld "
		"triangles = the authored far mesh of %d carded trees%s; the ring's far cells draw %d cards, %d of them have no authored "
		"mesh row and are not traced\n",
		static_cast<long long>( said.waterTris ), waterShapes, rtTraceSetRed( "nofarwater" ) ? " (RED nofarwater)" : "",
		static_cast<long long>( said.cardTris ), said.cardTrees, rtTraceSetRed( "nofarcards" ) ? " (RED nofarcards)" : "",
		said.cardsInRing, said.cardsNoMesh );
	std::fflush( stderr );
}

bool Stream::busy() const
{
	if ( worker.joinable() || dirty || unloadWaiting || !toReset.empty() || !toClear.empty() || !toFree.empty() )
		return true;
	for ( const auto & kv : cells ) {
		const CellState s = kv.second.st;
		if ( s == CellState::Attached || ( s == CellState::Ready && !want( kv.first ) ) )
			continue;
		return true;
	}
	return false;
}

bool Stream::tick( Scene * sc, const Vector3 & eye )
{
	if ( !nif || !sc )
		return false;
	QElapsedTimer t;
	t.start();
	const double cpu0 = threadCpuMs();
	const qint64 faults0 = processFaults();
	if ( sc != scene || !root ) {
		if ( !resolve( sc ) )
			return false;
	}
	nif->streamQuiet = true;
	nif->setState( BaseModel::Processing );
	bool changed = false;
	if ( CellFarHide::mode() != hideMode ) {   // lane FARHOLE2: WW_CELL_FARHIDE_RED set live
		hideMode = CellFarHide::mode();
		farHideRefresh( INT_MIN, INT_MIN );
		changed = true;
	}

	// lane PERFLOAD1 P8: the near grid's FREEZE / OFF hold the block at the eye they began at (the camera flies free)
	Vector3 gridEye = eye;
	if ( LodPreview::grid() != LodPreview::Follow ) {
		float fe[3] = { eye[0], eye[1], eye[2] };
		if ( !LodPreview::frozenEye( fe ) )
			LodPreview::setFrozenEye( fe );
		gridEye = Vector3( fe[0], fe[1], fe[2] );
	}
	// lane AIRPERF1 r4: Cell Streaming off (the row, or WW_CELL_STREAM=0 set live) holds the block where it is
	const bool liveOn = cellStreamLiveOn();
	if ( !liveOn && !paused ) {
		paused = pausedFill = true;
		pausedEye = gridEye;
		std::fprintf( stderr, "cell stream: off (%s): the block stays at centre %d,%d; the cells it was loading finish, then "
			"nothing loads or unloads\n", cellStreamPinned() ? "WW_CELL_STREAM=0" : "the Cell Streaming row", ccx, ccy );
		std::fflush( stderr );
	} else if ( liveOn && paused ) {
		paused = pausedFill = false;
		std::fprintf( stderr, "cell stream: on (%s): follows the camera again from centre %d,%d, camera cell %d,%d\n",
			cellStreamPinned() ? "WW_CELL_STREAM=1" : "the Cell Streaming row", ccx, ccy, cellOf( eye[0] ), cellOf( eye[1] ) );
		std::fflush( stderr );
	}
	if ( paused )
		gridEye = pausedEye;
	nearGrid( eye );
	if ( !fixedCentre )	// lane LANDSPEC6: the game skips every grid shift in a fixed world
		shiftCentre( gridEye );
	if ( ccx != saidCx || ccy != saidCy || paused != saidPaused ) {   // lane AIRPERF1 r4: the block's centre, on every move
		saidCx = ccx;
		saidCy = ccy;
		saidPaused = paused;
		std::fprintf( stderr, "cell stream: centre %d,%d (camera cell %d,%d), %d x %d cells%s\n", ccx, ccy, cellOf( eye[0] ),
			cellOf( eye[1] ), 2 * gridHalf + 1, 2 * gridHalf + 1, paused ? ", held (streaming off)"
				: fixedCentre ? ", a fixed world's block" : "" );
		std::fflush( stderr );
	}
	if ( texCentre != Key( ccx, ccy ) )	// lane STREAMTEX1: a crossing (the first tick: the open's census)
		texCensus();
	wanted.clear();
	for ( int dy = -gridHalf; dy <= gridHalf; dy++ )
		for ( int dx = -gridHalf; dx <= gridHalf; dx++ )
			wanted.insert( Key( ccx + dx, ccy + dy ) );
	// the row the next move needs, as soon as the eye is in the neighbour cell
	prefetch.clear();
	const int ex = cellOf( eye[0] ), ey = cellOf( eye[1] );
	const int gx = cellOf( gridEye[0] ), gy = cellOf( gridEye[1] );	// lane PERFLOAD1 P8: the prefetch row of the held block
	if ( !cam.valid || ex != cam.cx || ey != cam.cy ) {	// lane WEATHERBLEND6: the regions read on entering a cell only
		cam.valid = true;
		cam.cx = ex;
		cam.cy = ey;
		cam.cellForm = 0;
		cam.regions = seed && seed->world ? seed->world->cellRegions( ex, ey, &cam.cellForm ) : QVector<quint32>();
		cam.world = seed && seed->world ? seed->world->worldspace() : 0;
		cam.gen++;
	}
	cam.eye[0] = eye[0];
	cam.eye[1] = eye[1];
	for ( int dy = -gridHalf; dy <= gridHalf; dy++ )
		for ( int dx = -gridHalf; dx <= gridHalf; dx++ )
			if ( !noPrefetch && !paused && !wanted.count( Key( gx + dx, gy + dy ) ) )	// AIRPERF1 r4: no prefetch while held
				prefetch.insert( Key( gx + dx, gy + dy ) );

	// lane LARGEREF2: the units the layer draws (a ref listed by an attached cell whose own cell is not attached)
	lrWanted.clear();
	if ( lrOn )
		for ( const quint32 r : lr.layer() ) {
			const Key k = lrKey( r );
			lrWanted.insert( k );
			if ( !cells.count( k ) ) {
				StreamCell c;
				c.lref = r;
				lr.ownCell( r, c.x, c.y );
				cells.emplace( k, std::move( c ) );
			}
		}

	unloadWaiting = false;
	const double msUnload0 = msOf( t );
	for ( auto it = cells.begin(); it != cells.end(); ) {
		StreamCell & c = it->second;
		const bool w = want( it->first ), p = prefetch.count( it->first ) > 0;
		if ( w ) {
			++it;
			continue;
		}
		if ( c.st == CellState::Attached || c.st == CellState::Writing ) {
			if ( red == QLatin1String( "nounload" ) ) {
				++it;
				continue;
			}
			// lane CELLSTREAM1: with the builder idle, and one cell a frame once half the budget is spent
			if ( building() || ( changed && msOf( t ) > budgetMs * 0.5 ) ) {
				unloadWaiting = true;
				++it;
				continue;
			}
			g_phase = "unload";
			unload( c );
			changed = true;
			it = cells.erase( it );
		} else if ( p && c.st != CellState::Queued ) {
			++it;
		} else if ( !p ) {
			it = cells.erase( it );   // a build still running is dropped when it arrives
		} else {
			++it;
		}
	}
	for ( const Key & k : wanted )
		if ( !cells.count( k ) ) {
			StreamCell c;
			c.x = k.first;
			c.y = k.second;
			cells.emplace( k, std::move( c ) );
		}
	for ( const Key & k : prefetch )
		if ( !cells.count( k ) ) {
			StreamCell c;
			c.x = k.first;
			c.y = k.second;
			cells.emplace( k, std::move( c ) );
		}
	const double msUnload = msOf( t ) - msUnload0;
	g_phase = "pump";
	const double msPump0 = msOf( t );
	pumpWorker( eye );
	const double msPump = msOf( t ) - msPump0;

	// a cell's blocks ascend while it is written (the ground's ordered node draws by block number): the pool only
	// fills between cells
	bool midCell = false;
	for ( const auto & kv : cells )
		midCell |= kv.second.st == CellState::Writing;
	g_phase = "bury";
	const double msBury0 = msOf( t );
	if ( !building() )
		bury( t );
	g_phase = "drain";
	const double msDrain0 = msOf( t );
	if ( !midCell )
		drain( t );
	reap();
	const double msWrite0 = msOf( t );
	// lane CELLSPIKE3: a slice always writes one bucket, so it starts only if the dearest recent slice still ends
	// under the frame cap (2 budgets less 1 ms), or at the very start of a frame (red: whenever the budget has room)
	const bool redWrite = spikeRed( "write" );
	while ( !building() && msOf( t ) < budgetMs
		&& ( redWrite || msOf( t ) < 0.5 || msOf( t ) + sliceEstMs < 2.0 * budgetMs - 1.0 ) ) {
		// the cell being written, else the nearest built one the block wants
		StreamCell * next = nullptr;
		double bestD = 1.0e30;
		for ( auto & kv : cells ) {
			StreamCell & c = kv.second;
			if ( !want( kv.first ) )
				continue;
			if ( c.st == CellState::Writing ) {
				next = &c;
				break;
			}
			// a new cell waits for the freed blocks: it takes them back instead of growing the file
			if ( c.st == CellState::Ready && toReset.empty() ) {
				const double d = std::hypot( double( c.x - ex ), double( c.y - ey ) );
				if ( d < bestD ) {
					bestD = d;
					next = &c;
				}
			}
		}
		if ( !next )
			break;
		g_phase = "write";
		write( *next, budgetMs - msOf( t ) );
		changed = true;
		if ( next->emitted && next->st != CellState::Attached )
			break;   // lane CELLSTREAM1: its hookup waits for the next frame's budget
	}
	const double msWrite = msOf( t ) - msWrite0;
	double msRebuild0 = 0.0, msRebuild = 0.0;
	if ( dirty && toReset.empty() ) {
		/* lane STREAMGN1: a filling block is lit as it fills, not only when whole (the first open waited 90 s for its
		 * 36th cell; its cards took no lamp light till then). The chunk worker writes only its own chunk, so this
		 * rebuild may run beside it; never while a cell is half written. Red WW_CELL_STREAM_RED=nolitearly. */
		constexpr qint64 kLitEarlyMs = 750;
		int wanted = 0, attached = 0;
		bool midWrite = false;
		for ( const auto & kv : cells ) {
			if ( !want( kv.first ) )
				continue;
			wanted++;
			attached += kv.second.st == CellState::Attached ? 1 : 0;
			midWrite |= kv.second.st == CellState::Writing;
		}
		const bool whole = attached == wanted && !worker.joinable();
		const bool early = !whole && attached < wanted && attached > 0 && !midWrite && red != QLatin1String( "nolitearly" )
			&& ( !sinceRebuild.isValid() || sinceRebuild.elapsed() >= kLitEarlyMs );
		if ( whole || early ) {
			g_phase = "rebuild";
			msRebuild0 = msOf( t );
			rebuild();
			msRebuild = msOf( t ) - msRebuild0;
			changed = true;
			sinceRebuild.start();
			if ( early ) {
				litEarly++;
				std::fprintf( stderr, "cell stream: lit early %d: rebuild with %d of %d wanted cells attached (centre %d,%d), %.1f ms\n",
					litEarly, attached, wanted, ccx, ccy, msRebuild );
				std::fflush( stderr );
			}
		}
	}
	if ( paused && pausedFill && !dirty ) {   // lane AIRPERF1 r4: the held block is whole: from here nothing is built
		bool whole = true;
		for ( const auto & kv : cells )
			whole = whole && ( !want( kv.first ) || kv.second.st == CellState::Attached );
		if ( whole ) {
			pausedFill = false;
			std::fprintf( stderr, "cell stream: off: the held block is whole (centre %d,%d), no build starts until it is on\n", ccx, ccy );
			std::fflush( stderr );
		}
	}
	// lane AIRPERF1 r4: the ray-traced GI's traced set is the loaded cells and the far terrain round them
	if ( rtTraceSetFollows() ) {
		const int ring = rtTraceSetRing( nif, gridHalf );
		// lane AIRPERF1 r5: a changed far red hands over again; far shapes still to read are read a few ms a tick, then
		// one more hand-over brings them
		const int reds = ( rtTraceSetRed( "nofar" ) ? 1 : 0 ) | ( rtTraceSetRed( "nofarobj" ) ? 2 : 0 )
			| ( rtTraceSetRed( "fardirt" ) ? 4 : 0 ) | ( rtTraceSetRed( "nofarcap" ) ? 8 : 0 )
			| ( rtTraceSetRed( "nofarwater" ) ? 16 : 0 ) | ( rtTraceSetRed( "nofarcards" ) ? 32 : 0 )
			| ( rtTraceSetRed( "oneblas" ) ? 64 : 0 );   // round 6
		if ( reds != traceReds ) {
			traceReds = reds;
			traceDirty = true;
		}
		if ( traceMore && !dirty && !traceDirty && traceOn && ring == traceRing ) {
			g_phase = "traceset";
			if ( traceFarRead( ring, kFarReadMs ) == 0 ) {
				traceMore = false;
				traceDirty = true;
			}
		}
		if ( !dirty && ( traceDirty || ring != traceRing || !traceOn ) ) {
			g_phase = "traceset";
			traceSync( ring, ex, ey );
		}
		rtTraceSetTick( nif, ex, ey );
	} else if ( traceOn ) {
		rtTraceSetForget( nif );   // the row went off: the kept soup stays as it is, nothing follows
		traceOn = false;
	}

	g_phase = "idle";
	const double msIdle0 = msOf( t );
	nif->restoreState();
	nif->getProcessingResult();
	nif->streamQuiet = false;
	if ( changed ) {
		sc->transformDirty = true;
		sc->invalidateBounds();
	}
	if ( msOf( t ) > 2.0 * budgetMs )
		trace( QStringLiteral( "slow tick: %1 ms (pump %2, bury %5, drain %6, write %7, rebuild %8, model state %3)%4" ).arg( msOf( t ), 0, 'f', 1 ).arg( msPump, 0, 'f', 1 )
			.arg( msOf( t ) - msIdle0, 0, 'f', 1 ).arg( building() ? QStringLiteral( " worker busy" ) : QStringLiteral( " worker idle" ) )
			.arg( msDrain0 - msBury0, 0, 'f', 1 ).arg( msWrite0 - msDrain0, 0, 'f', 1 ).arg( msWrite, 0, 'f', 1 ).arg( msRebuild, 0, 'f', 1 ) );
	const double cpuMs = threadCpuMs() - cpu0;	// lane CELLSPIKE3: on-processor ms
	auto stages = [&]() {
		return QStringLiteral( "wtMs %1 wtPre %2 wtUnload %3 wtPump %4 wtBury %5 wtDrain %6 wtWrite %7 wtRebuild %8 wtIdle %9" )
			.arg( msOf( t ), 0, 'f', 2 ).arg( msUnload0, 0, 'f', 2 ).arg( msUnload, 0, 'f', 2 ).arg( msPump, 0, 'f', 2 )
			.arg( msDrain0 - msBury0, 0, 'f', 2 ).arg( msWrite0 - msDrain0, 0, 'f', 2 ).arg( msWrite, 0, 'f', 2 )
			.arg( msRebuild, 0, 'f', 2 ).arg( msOf( t ) - msIdle0, 0, 'f', 2 )
			+ QStringLiteral( " wtCpu %1 wtFaults %2" ).arg( cpuMs, 0, 'f', 2 ).arg( processFaults() - faults0 );
	};
	if ( msOf( t ) > st.tickMaxMs )   // lane CELLSPIKE2: the stages of the step's dearest frame (dump field worstTick)
		st.worstTick = stages();
	if ( cpuMs > st.cpuMaxMs ) {   // lane FRAMESPEED1: the step's dearest frame ON the processor (the CPU-only bar)
		st.cpuMaxMs = cpuMs;
		st.worstCpuTick = stages();
	}
	st.tickMaxMs = std::max( st.tickMaxMs, msOf( t ) );
	return changed;
}

void Stream::texCensus()
{
	const auto c = texPool.cross();
	const auto mb = []( std::uint64_t b ) { return QString::number( double( b ) / 1048576.0, 'f', 1 ); };
	st.texLine = QStringLiteral( "crossing %1 centre %2,%3 kept %4 (%5 MB) refd %6 (%7 MB) evicted %8 (%9 MB)" )
		.arg( c.crossing ).arg( ccx ).arg( ccy ).arg( c.kept ).arg( mb( c.keptBytes ) ).arg( c.refd ).arg( mb( c.refdBytes ) )
		.arg( c.evicted ).arg( mb( c.evictedBytes ) )
		+ QStringLiteral( " reloaded %1 (%2 MB) released %3 (%4 MB) loads %5 (%6 MB) keepEvicted %7 budget %8 MB%9" )
		.arg( c.reloaded ).arg( mb( c.reloadedBytes ) ).arg( c.released ).arg( mb( c.releasedBytes ) ).arg( c.loads )
		.arg( mb( c.loadBytes ) ).arg( c.keepEvicted ).arg( mb( texPool.budgetBytes() ) )
		.arg( texPool.red() ? QStringLiteral( " RED off" ) : QString() );
	// the walk dump's form: key value pairs only (cell_stream_check.py row (e))
	st.texPairs = QStringLiteral( "texCrossing %1 texKept %2 texKeptMb %3 texRefd %4 texRefdMb %5 texFreed %6 texFreedMb %7 " )
		.arg( c.crossing ).arg( c.kept ).arg( mb( c.keptBytes ) ).arg( c.refd ).arg( mb( c.refdBytes ) ).arg( c.evicted )
		.arg( mb( c.evictedBytes ) )
		+ QStringLiteral( "texReloadedN %1 texReloadedMb %2 texReleasedMb %3 texLoadsMb %4 texKeepFreed %5 texBudgetMb %6 texRed %7" )
		.arg( c.reloaded ).arg( mb( c.reloadedBytes ) ).arg( mb( c.releasedBytes ) ).arg( mb( c.loadBytes ) ).arg( c.keepEvicted )
		.arg( mb( texPool.budgetBytes() ) ).arg( texPool.red() ? 1 : 0 )
		// lane STREAMTEX4 (STREAMTEX2's census, gl/cellbindless.cpp; row (f)): since launch, bindless handles dropped
		// while a frame naming them may still be queued, and handles found cached under a new texture's name
		+ QStringLiteral( " texFreedLive %1 texStaleNames %2 texUnfenced %3" )
		.arg( WwBl::freedLiveAll ).arg( WwBl::staleAll ).arg( cellStreamTexUnfenced() ? 1 : 0 )
		// lane STREAMTEX4 (OPENCELLS): what tier 1 holds resident, by why (streamtexpool.h Census)
		+ QStringLiteral( " texBaseOnly %1 texBaseOnlyMb %2 texUnnoted %3 texUnnotedMb %4" )
		.arg( c.baseOnly ).arg( mb( c.baseOnlyBytes ) ).arg( c.unnoted ).arg( mb( c.unnotedBytes ) );
	st.texReloaded += c.reloaded;
	st.texKeepEvicted += c.keepEvicted;
	texCentre = Key( ccx, ccy );
	std::fprintf( stderr, "stream tex: %s\n", qPrintable( st.texLine ) );
}

QString keyList( const std::vector<Key> & keys )
{
	QStringList l;
	for ( const Key & k : keys )
		l << QStringLiteral( "%1,%2" ).arg( k.first ).arg( k.second );
	return l.join( QLatin1Char( ' ' ) );
}

} // namespace

/* lane AIRPERF1 r4: every exterior view streams, opened by a spec (WW_CELL_OPEN) or not; a harness that needs a
 * fixed block says WW_CELL_STREAM=0 (cellstreamrule.h; red WW_CELL_STREAM_RED=specfixed = the rule before). The census
 * walk opens exterior blocks itself and must weld them. */
bool cellStreamWanted()
{
	const int rule = cellstreamrule::openRule( wwEnvIsSet( "WW_CELL_STREAM" ), wwEnvInt( "WW_CELL_STREAM" ),
		!wwEnvIsEmpty( "WW_CELL_OPEN" ), !wwEnvIsEmpty( "WW_CELL_CENSUS_TEST" ),
		wwEnvStr( "WW_CELL_STREAM_RED" ) == QLatin1String( "specfixed" ) );
	static int said = -1;
	if ( rule != said ) {
		said = rule;
		std::fprintf( stderr, "cell stream: %s\n", cellstreamrule::openWords( rule ) );
		std::fflush( stderr );
	}
	return cellstreamrule::openOn( rule );
}

static bool g_cellStreamRow = true;   // the row; lane AIRPERF1 r5: saved (CellView/CellStream), ships on
static bool g_cellStreamRowRead = false;

static void cellStreamRowRead()
{
	if ( g_cellStreamRowRead )
		return;
	g_cellStreamRowRead = true;
	const QSettings s;
	const bool saved = s.contains( QStringLiteral( "CellView/CellStream" ) );
	const bool savedOn = saved && s.value( QStringLiteral( "CellView/CellStream" ) ).toBool();
	g_cellStreamRow = cellstreamrule::rowStart( saved, savedOn );
	std::fprintf( stderr, "cell stream: the Cell Streaming row starts %s (%s)%s\n", g_cellStreamRow ? "on" : "off",
		saved ? "the saved setting CellView/CellStream" : "nothing saved: it ships on",
		wwEnvIsSet( "WW_CELL_STREAM" ) ? ", WW_CELL_STREAM is set: the pin decides" : "" );
	std::fflush( stderr );
}

bool cellStreamRowOn()
{
	cellStreamRowRead();
	return g_cellStreamRow;
}

void cellStreamSetRow( bool on )
{
	cellStreamRowRead();
	if ( on == g_cellStreamRow )
		return;
	g_cellStreamRow = on;
	QSettings().setValue( QStringLiteral( "CellView/CellStream" ), on );
	std::fprintf( stderr, "cell stream: the Cell Streaming row %s%s\n", on ? "on" : "off",
		cellStreamPinned() ? " (WW_CELL_STREAM is set: the pin decides)" : "" );
	std::fflush( stderr );
}

bool cellStreamPinned()
{
	return wwEnvIsSet( "WW_CELL_STREAM" );
}

bool cellStreamLiveOn()
{
	cellStreamRowRead();
	return cellstreamrule::liveOn( wwEnvIsSet( "WW_CELL_STREAM" ), wwEnvInt( "WW_CELL_STREAM" ), g_cellStreamRow,
		wwEnvInt( "WW_CELL_STREAM_ROW_RED" ) > 0 );
}

void cellStreamStart( NifModel * nif, std::shared_ptr<CellStreamSeed> seed )
{
	if ( !nif || !seed )
		return;
	cellStreamStop( nif );
	auto s = std::make_unique<Stream>();
	s->nif = nif;
	s->seed = seed;
	s->fixedCentre = seed->fixedWorld && wwEnvStr( "WW_LANDSPEC_RED" ) != QLatin1String( "fixedfollow" );
	LandLodSpec::streamCentre( s->fixedCentre, seed->fixedX, seed->fixedY, seed->cx, seed->cy, s->ccx, s->ccy );	// LANDSPEC6
	s->noPrefetch = s->fixedCentre && wwEnvStr( "WW_LANDSPEC_RED" ) != QLatin1String( "fixedopen" );	// LANDSPEC7
	QString gridNote;
	s->gridHalf = cellGameGridHalf( &gridNote );	// LANDSPEC8
	std::fprintf( stderr, "cell stream: uGridsToLoad %s: %d x %d cells\n", qPrintable( gridNote ), 2 * s->gridHalf + 1,
		2 * s->gridHalf + 1 );
	if ( seed->fixedWorld )
		std::fprintf( stderr, "cell stream: fixed world, block centre %d,%d (%s)%s\n", s->ccx, s->ccy,
			s->fixedCentre ? "WCTR, no shifts" : "RED fixedfollow: the open's, shifting",
			s->noPrefetch ? ", no prefetch past it" : s->fixedCentre ? ", RED fixedopen: prefetch round the eye" : "" );
	if ( wwEnvIsSet( "WW_CELL_STREAM_BUDGET" ) )
		s->budgetMs = std::clamp( wwEnvStr( "WW_CELL_STREAM_BUDGET" ).toDouble(), 0.5, 100.0 );
	if ( wwEnvIsSet( "WW_CELL_STREAM_SHIFT" ) )
		s->shift = std::clamp( float( wwEnvInt( "WW_CELL_STREAM_SHIFT" ) ), 0.0f, 4096.0f );
	s->red = wwEnvStr( "WW_CELL_STREAM_RED" );
	/* lane LARGEREF2 (LARGEREF-STREAM): large refs in the streamed view, counted per document (largeref.h Owners).
	 * Red WW_CELL_LARGEREFS_RED=streamdouble: a ref stays in the layer while its own cell draws it (two copies). */
	if ( CellFarHide::largeRefsWanted() && seed->world ) {
		s->lrOn = true;
		s->lr = LargeRef::Owners( wwEnvStr( "WW_CELL_LARGEREFS_RED" ) == QLatin1String( "streamdouble" ) );
		/* lane LARGEREF3 (LARGEREF-FLAT, ruling a): over a flat far field the layer draws no unit (the far copy alone until
		 * the ref's own cell attaches); red WW_CELL_LARGEREFS_RED=flatdouble keeps the layer there too */
		const bool flatDouble = wwEnvStr( "WW_CELL_LARGEREFS_RED" ) == QLatin1String( "flatdouble" );
		s->lr.setFullOutside( LargeRef::fullOutside( LargeRef::Far( seed->largeRefFar ), flatDouble ) );
		/* lane LARGEREF4 (LARGEREF-FLAT-MIXED, LARGEREF-BUDGET): the far build's own map decides per ref, at its own cell
		 * (the FO4CS type's .bto fill, a refused object ring); red WW_CELL_LARGEREFS_RED=permode: one kind per mode */
		const bool perMode = wwEnvStr( "WW_CELL_LARGEREFS_RED" ) == QLatin1String( "permode" );
		/* lane LARGEREF4 (LARGEREF-MAP-LIVE): a live far rebuild starts the stream again (cellview.cpp); the map is read
		 * now, not carried; red WW_CELL_LARGEREFS_RED=stalemap: the map this stream read before wins */
		const bool stale = wwEnvStr( "WW_CELL_LARGEREFS_RED" ) == QLatin1String( "stalemap" );
		LargeRef::FarMap mapNow;
		const bool haveNow = cellFarLodLargeRefMap( nif, mapNow );
		const LargeRef::FarMap * use = LargeRef::streamMap( haveNow ? &mapNow : nullptr, seed->largeRefMap.get(), stale );
		const bool heldUsed = use && use != &mapNow;
		const LargeRef::FarMap farMap = use ? *use : LargeRef::FarMap();
		const bool haveMap = use != nullptr;
		seed->largeRefMap = haveMap ? std::make_shared<const LargeRef::FarMap>( farMap ) : nullptr;
		if ( haveMap && !perMode )
			s->lr.setFarMap( farMap, flatDouble );
		s->lr.setRows( seed->world->largeRefRows() );
		/* lane LARGEREF6 (LARGEREF-FLATMASK-FIX): WW_CELL_LARGEREFS_FLATMASK=accept|keepbox|promote[:share] (launch key);
		 * accept (today's cut) until the hold's pictures pick one */
		{
			float share = 0.25f;
			s->lrMask = LargeRef::flatMaskOf( wwEnvStr( "WW_CELL_LARGEREFS_FLATMASK" ).toLower().toStdString(), share );
			s->lr.setFlatMask( s->lrMask, share );
			if ( s->lrMask != LargeRef::FlatMask::Accept )
				std::fprintf( stderr, "cell stream: large refs flat mask: %s (promote share %.2f)\n",
					s->lrMask == LargeRef::FlatMask::KeepBox ? "keepbox" : "promote", double( share ) );
		}
		if ( haveMap ) {
			int byRef = 0, flat = 0, none = 0;
			for ( const LargeRef::Row & r : seed->world->largeRefRows() ) {
				const LargeRef::Far k = farMap.at( r.rx, r.ry );
				( k == LargeRef::Far::ByRef ? byRef : k == LargeRef::Far::Flat ? flat : none )++;
			}
			std::fprintf( stderr, "cell stream: large refs far map %d rectangles, RNAM rows by own cell: %d ours by ref, %d flat, "
				"%d none%s%s\n", int( farMap.size() ), byRef, flat, none,
				perMode ? " (RED permode: one kind per mode)" : "",
				heldUsed ? " (RED stalemap: the map read before the live rebuild)" : "" );
		}
		std::fprintf( stderr, "cell stream: large refs on, %d refs in %d RNAM rows%s%s\n", int( s->lr.refs() ),
			int( seed->world->largeRefRows().size() ),
			wwEnvStr( "WW_CELL_LARGEREFS_RED" ) == QLatin1String( "streamdouble" ) ? " (RED streamdouble)" : "",
			s->lr.fullOutside() ? ( seed->largeRefFar == int( LargeRef::Far::Flat ) ? " (RED flatdouble)" : "" )
				: "; far objects flat: no layer, the far copy alone" );
	}
	s->baseRefs = cellRefTable();
	s->basePicks = cellPickTableMutable();
	s->st.on = true;
	const NifModel * key = nif;
	s->gone = QObject::connect( nif, &QObject::destroyed, [key]() {
		auto & m = streams();
		auto it = m.find( key );
		if ( it != m.end() ) {
			it->second->nif = nullptr;
			m.erase( it );
		}
	} );
	streams()[nif] = std::move( s );
}

void cellStreamStop( NifModel * nif )
{
	auto & m = streams();
	auto it = m.find( nif );
	if ( it != m.end() )
		m.erase( it );   // joins its worker
}

std::shared_ptr<CellStreamSeed> cellStreamSuspend( NifModel * nif )
{
	auto & m = streams();
	auto it = m.find( nif );
	if ( it == m.end() )
		return nullptr;
	Stream & s = *it->second;
	std::shared_ptr<CellStreamSeed> seed = s.seed;
	// the tables as the open left them: the streamed cells' refs, picks, lights and fx shapes go with their blocks
	cellRefTableMutable() = s.baseRefs;
	cellPickTableMutable() = s.basePicks;
	wwCellFxLitBegin( nif );
	wwCellLightsPublish( nif, seed->base );
	wwCellFxLitPick( nif );
	m.erase( it );   // joins its worker
	return seed;
}

bool cellStreamTick( NifModel * nif, Scene * scene, const Vector3 & eye )
{
	Stream * s = streamFor( nif );
	return ( s && nif ) ? s->tick( scene, eye ) : false;
}

const CellStreamCamera * cellStreamCamera( const NifModel * nif )
{
	const Stream * s = nif ? streamFor( nif ) : nullptr;
	return ( s && s->cam.valid ) ? &s->cam : nullptr;
}

void cellStreamSnapCentre( const Vector3 & eye )
{
	// lane SHORE2: shiftCentre's hysteresis keeps the block of eyes seen before the path camera (the shot refit's)
	for ( auto & kv : streams() ) {
		if ( kv.second->fixedCentre )
			continue;	// lane LANDSPEC6: a fixed world's block stays on WCTR
		kv.second->ccx = cellOf( eye[0] );
		kv.second->ccy = cellOf( eye[1] );
	}
}

int cellStreamTexKeepSec()
{
	WW_SWITCH_STATIC( int, keep, wwEnvIsSet( "WW_CELL_STREAM_TEXKEEP" ) ? wwEnvInt( "WW_CELL_STREAM_TEXKEEP" ) : 5 );
	return std::max( keep, 0 );
}

namespace
{
struct TexPoolCfg
{
	int preset = 3, mb = 0;
	bool pinned = false;
};
TexPoolCfg texPoolCfg()
{
	TexPoolCfg c;
	const QByteArray v = wwEnv( "WW_CELL_STREAM_TEXPOOL" ).trimmed().toLower();
	static const char * names[4] = { "low", "medium", "high", "ultra" };
	if ( !v.isEmpty() ) {
		c.pinned = true;
		for ( int k = 0; k < 4; k++ )
			if ( v == names[k] )
				c.preset = k;
		bool ok = false;
		const int mb = v.toInt( &ok );
		if ( ok && mb > 3 ) {
			c.preset = -1;
			c.mb = mb;
		}
		return c;
	}
	c.preset = std::clamp( QSettings().value( QStringLiteral( "WW/CellTexPool" ), 3 ).toInt(), 0, 3 );
	return c;
}
} // namespace

qint64 cellStreamTexPoolBytes()
{
	const TexPoolCfg c = texPoolCfg();	// read each call (every 2 s): a live set or the menu takes at once
	return WwTexPool::budgetBytes( c.preset >= 0 ? c.preset : c.mb );
}

int cellStreamTexPoolPreset()
{
	return texPoolCfg().preset;
}

void cellStreamTexPoolSetPreset( int preset )
{
	if ( texPoolCfg().pinned )
		return;
	QSettings().setValue( QStringLiteral( "WW/CellTexPool" ), std::clamp( preset, 0, 3 ) );
}

int cellStreamGeoKeepSec()
{
	WW_SWITCH_STATIC( int, keep, wwEnvIsSet( "WW_CELL_STREAM_GEOKEEP" ) ? wwEnvInt( "WW_CELL_STREAM_GEOKEEP" ) : cellStreamTexKeepSec() );
	return std::clamp( keep, 0, 60 );
}

void cellStreamCountGeoEvicted( const NifModel * nif, int n )
{
	if ( Stream * s = streamFor( nif ) )
		s->st.geoEvicted += n;
}

bool cellStreamOn( const NifModel * nif )
{
	return nif && streamFor( nif ) != nullptr;
}

void cellStreamCountEvicted( const NifModel * nif, int n )
{
	if ( Stream * s = streamFor( nif ) )
		s->st.texEvicted += n;
}

bool cellStreamTexUnfenced()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_CELL_STREAMTEX_RED" ) == QLatin1String( "unfenced" )
		|| wwEnvStr( "WW_CELL_STREAMTEX_RED" ) == QLatin1String( "nofence" ) );
	return red;
}

bool cellStreamTexRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_CELL_STREAMTEX_RED" ) == QLatin1String( "off" ) );
	return red;
}

bool cellStreamTexCounting( const NifModel * nif )
{
	return nif && streamFor( nif ) != nullptr;
}

void cellStreamTexNote( const NifModel * nif, int block, const QStringList & names )
{
	Stream * s = streamFor( nif );
	if ( !s || QThread::currentThread() != nif->thread() )
		return;
	auto it = s->blockCell.constFind( block );
	const auto cell = it == s->blockCell.cend() ? s->texPool.kBase : s->texPool.cellId( it->first, it->second );
	for ( const QString & n : names )
		if ( !n.isEmpty() )
			s->texPool.note( cell, n );
}

// lane STREAMTEX5 (STREAMTEX4-WWCOVKEY): TREELEAF1's "<file>|wwcov<ref>" copy is pooled as <file>, the name note() got;
// red WW_CELL_STREAMTEX_RED=rawkey: the cache's own name (the copy was unnoted and could go while its cell was loaded)
static bool texRawKeyRed()
{
	WW_SWITCH_STATIC( bool, red, wwEnvStr( "WW_CELL_STREAMTEX_RED" ) == QLatin1String( "rawkey" ) );
	return red;
}

static QString texPoolKey( QStringView name )
{
	const std::ptrdiff_t at = texRawKeyRed() ? -1 : streamTexCovAt( name.utf16(), std::ptrdiff_t( name.size() ) );
	return ( at < 0 ? name : name.left( at ) ).toString();
}

void cellStreamTexLoaded( const NifModel * nif, const QString & name, qint64 bytes )
{
	if ( Stream * s = streamFor( nif ) )
		s->texPool.loaded( texPoolKey( name ), std::uint64_t( std::max<qint64>( bytes, 0 ) ) );
}

void cellStreamTexFreed( const NifModel * nif, const QString & name, bool counted )
{
	if ( Stream * s = streamFor( nif ) ) {
		if ( counted )
			s->texPool.freed( texPoolKey( name ) );
		else
			s->texPool.gone( texPoolKey( name ) );
	}
}

std::function<int( QStringView )> cellStreamTexTiers( const NifModel * nif )
{
	const Stream * s = streamFor( nif );
	if ( !s || s->texPool.red() )
		return nullptr;
	using Pool = StreamTexPool<QString, QStringStdHash>;
	return [s]( QStringView name ) {
		switch ( s->texPool.rule( texPoolKey( name ) ) ) {
		case Pool::Rule::Pool:
			return 0;	// its cells left
		case Pool::Rule::Keep:
			return WwTexPool::kTierKeep;	// a loaded cell names it
		default:
			return 1;	// no streamed cell named it
		}
	};
}

bool cellStreamBusy( const NifModel * nif )
{
	if ( nif ) {
		const Stream * s = streamFor( nif );
		return s && s->busy();
	}
	for ( const auto & kv : streams() )
		if ( kv.second->busy() )
			return true;
	return false;
}

CellStreamStats cellStreamStats( const NifModel * nif, bool resetMaxima )
{
	Stream * s = streamFor( nif );
	if ( !s )
		return CellStreamStats();
	CellStreamStats r = s->st;
	r.cx = s->ccx;
	r.cy = s->ccy;
	std::vector<Key> exp( s->wanted.begin(), s->wanted.end() ), got;
	r.attached = r.ready = r.pending = 0;
	for ( const auto & kv : s->cells ) {
		if ( kv.second.lref )	// lane LARGEREF2: the units are not cells (the coverage line counts them)
			continue;
		switch ( kv.second.st ) {
		case CellState::Attached:
			if ( kv.second.cellBlock >= 0 )
				got.push_back( kv.first );
			r.attached++;
			break;
		case CellState::Ready:
			r.ready++;
			break;
		default:
			r.pending++;
		}
	}
	r.expected = keyList( exp );
	r.loaded = keyList( got );
	r.match = exp == got;
	r.blocks = s->nif ? s->nif->getBlockCount() : 0;
	if ( s->nif ) {   // lane CELLSTREAM1: who holds the model's persistent indexes, by block type
		const QModelIndexList pl = s->nif->persistentIndexes();
		r.persistent = pl.size();
		QHash<QString, int> byType;
		for ( const QModelIndex & i : pl ) {
			const int b = s->nif->getBlockNumber( i );
			byType[b >= 0 ? s->nif->itemName( s->nif->getBlockIndex( b ) ) : QStringLiteral( "-" )]++;
		}
		std::vector<std::pair<int, QString>> top;
		for ( auto it = byType.cbegin(); it != byType.cend(); ++it )
			top.emplace_back( it.value(), it.key() );
		std::sort( top.rbegin(), top.rend() );
		for ( size_t k = 0; k < top.size() && k < 6; k++ )
			r.persistentTop += QStringLiteral( "%1%2=%3" ).arg( k ? " " : "" ).arg( top[k].second ).arg( top[k].first );
	}
	if ( s->scene ) {
		r.sceneNodes = int( s->scene->nodes.list().size() );
		r.sceneProps = int( std::distance( s->scene->properties.begin(), s->scene->properties.end() ) );
		r.sceneShapes = int( s->scene->shapes.size() );
	}
	r.pooled = int( s->toReset.size() );
	for ( const auto & p : s->pool )
		r.pooled += int( p.second.size() );
	if ( resetMaxima ) {
		s->st.tickMaxMs = s->st.sliceMaxMs = s->st.unloadMaxMs = s->st.drainMaxMs = s->st.rebuildMaxMs = s->st.buryMaxMs = 0;
		s->st.buildMaxMs = s->st.cpuMaxMs = 0;
		s->st.worstTick.clear();
		s->st.worstCpuTick.clear();
		s->st.worstUnload.clear();
		s->st.scannedNodes = 0;
	}
	return r;
}

QString cellStreamCoverage( const NifModel * nif, int * holes, int * doubles, int * samples )
{
	Stream * s = streamFor( nif );
	if ( holes )
		*holes = 0;
	if ( doubles )
		*doubles = 0;
	if ( samples )
		*samples = 0;
	if ( !s || !s->nif || !s->scene )
		return QStringLiteral( "coverage: no stream" );
	struct Tri { float x[3], y[3]; };
	std::vector<Tri> nearT, farT;
	auto addShape = [s]( Shape * sh, std::vector<Tri> & out ) {
		if ( !sh )
			return;
		QVector<Vector3> V = sh->verts;
		QVector<Triangle> T = sh->triangles;
		if ( V.isEmpty() || T.isEmpty() ) {
			if ( const QSharedPointer<CellMesh> cm = cellMeshFor( s->nif, sh->index() ) ) {
				V = cm->verts;
				T = cm->triangles;
			}
		}
		const Transform w = sh->worldTrans();
		for ( const Triangle & tr : T ) {
			if ( tr[0] >= V.size() || tr[1] >= V.size() || tr[2] >= V.size() )
				continue;
			Tri o;
			for ( int k = 0; k < 3; k++ ) {
				const Vector3 p = w * V[tr[k]];
				o.x[k] = p[0];
				o.y[k] = p[1];
			}
			const float area = ( o.x[1] - o.x[0] ) * ( o.y[2] - o.y[0] ) - ( o.x[2] - o.x[0] ) * ( o.y[1] - o.y[0] );
			if ( std::fabs( area ) > 1.0e-3f )
				out.push_back( o );
		}
	};
	std::set<Key> attached;
	for ( const auto & kv : s->cells ) {
		const StreamCell & c = kv.second;
		if ( c.st != CellState::Attached || c.cellBlock < 0 || c.lref )	// lane LARGEREF2: a unit has no ground
			continue;
		attached.insert( kv.first );
		for ( auto it = c.nodes.cbegin(); it != c.nodes.cend(); ++it ) {
			Node * n = it.value().data();
			Node * p = n ? n->parentNode() : nullptr;
			if ( p && c.grounds.contains( p->id() ) )
				addShape( dynamic_cast<Shape *>( n ), nearT );
		}
	}
	std::set<Key> farCells;
	for ( const FarTile & f : s->farTiles ) {
		if ( !f.land )
			continue;
		farCells.insert( Key( f.x, f.y ) );
		if ( f.node && f.node->parentNode() )
			addShape( dynamic_cast<Shape *>( f.node.data() ), farT );
	}
	// a hash grid of 128-unit squares
	constexpr float G = 128.0f;
	auto gridOf = []( const std::vector<Tri> & tris ) {
		QHash<qint64, std::vector<int>> g;
		for ( int i = 0; i < int( tris.size() ); i++ ) {
			const Tri & t = tris[i];
			const int x0 = int( std::floor( std::min( { t.x[0], t.x[1], t.x[2] } ) / G ) );
			const int x1 = int( std::floor( std::max( { t.x[0], t.x[1], t.x[2] } ) / G ) );
			const int y0 = int( std::floor( std::min( { t.y[0], t.y[1], t.y[2] } ) / G ) );
			const int y1 = int( std::floor( std::max( { t.y[0], t.y[1], t.y[2] } ) / G ) );
			for ( int y = y0; y <= y1; y++ )
				for ( int x = x0; x <= x1; x++ )
					g[( qint64( x ) << 32 ) ^ quint32( y )].push_back( i );
		}
		return g;
	};
	const auto nearG = gridOf( nearT ), farG = gridOf( farT );
	auto covered = []( const std::vector<Tri> & tris, const QHash<qint64, std::vector<int>> & g, float px, float py ) {
		const qint64 k = ( qint64( int( std::floor( px / G ) ) ) << 32 ) ^ quint32( int( std::floor( py / G ) ) );
		const auto it = g.constFind( k );
		if ( it == g.cend() )
			return false;
		for ( const int i : it.value() ) {
			const Tri & t = tris[i];
			const float d0 = ( t.x[1] - t.x[0] ) * ( py - t.y[0] ) - ( t.y[1] - t.y[0] ) * ( px - t.x[0] );
			const float d1 = ( t.x[2] - t.x[1] ) * ( py - t.y[1] ) - ( t.y[2] - t.y[1] ) * ( px - t.x[1] );
			const float d2 = ( t.x[0] - t.x[2] ) * ( py - t.y[2] ) - ( t.y[0] - t.y[2] ) * ( px - t.x[2] );
			if ( ( d0 >= 0 && d1 >= 0 && d2 >= 0 ) || ( d0 <= 0 && d1 <= 0 && d2 <= 0 ) )
				return true;
		}
		return false;
	};
	int nH = 0, nD = 0, nS = 0, edges = 0;
	const int dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	const float offs[4] = { -128.0f, -16.0f, 16.0f, 128.0f };
	for ( const Key & a : attached ) {
		for ( const auto & d : dirs ) {
			const Key n( a.first + d[0], a.second + d[1] );
			if ( attached.count( n ) || !farCells.count( n ) )
				continue;
			edges++;
			// the border line and the outward normal
			const float bx = d[0] > 0 ? float( a.first + 1 ) * kCell : d[0] < 0 ? float( a.first ) * kCell : 0.0f;
			const float by = d[1] > 0 ? float( a.second + 1 ) * kCell : d[1] < 0 ? float( a.second ) * kCell : 0.0f;
			for ( float u = 4.0f; u < kCell; u += 8.0f ) {
				for ( const float o : offs ) {
					float px, py;
					if ( d[0] != 0 ) {
						px = bx + float( d[0] ) * o;
						py = float( a.second ) * kCell + u;
					} else {
						px = float( a.first ) * kCell + u;
						py = by + float( d[1] ) * o;
					}
					const bool hn = covered( nearT, nearG, px, py ), hf = covered( farT, farG, px, py );
					nS++;
					nH += ( !hn && !hf ) ? 1 : 0;
					nD += ( hn && hf ) ? 1 : 0;
				}
			}
		}
	}
	if ( holes )
		*holes = nH;
	if ( doubles )
		*doubles = nD;
	if ( samples )
		*samples = nS;
	// lane FARHOLE2: what the far field still draws inside the loaded cells (single-token keys, word pairs)
	//   farinstDouble  drawn instanced rows whose REFR a loaded cell lists (the near cell draws it too): must be 0
	//   farinstNoref   drawn rows with no REFR placed in a loaded cell: must be 0
	//   farinstReach   drawn rows placed in a loaded cell whose owner is not loaded (right: nothing near draws them)
	//   farmaskCentred drawn triangles of the masked far shapes centred in a loaded cell: must be 0
	//   farcardsPlaced far cards placed in a loaded cell still drawn: must be 0
	// lane LARGEREF2: a unit's ref counts as listed (its far rows must hide) but its cell is not loaded
	//   largeRefUnits  attached large-ref units; largeRefDouble  of them, a ref an attached cell also draws: must be 0
	//   largeRefEmpty  attached units that placed nothing (the ref not where RNAM says): must be 0
	QSet<qint64> loadedCells;
	QSet<quint32> listed, cellListed;
	int lrUnits = 0, lrDouble = 0, lrEmpty = 0;
	for ( const auto & kv : s->cells ) {
		const StreamCell & c = kv.second;
		if ( c.st != CellState::Attached || c.lref || !c.chunk )
			continue;
		for ( int i = 0; i < c.chunk->refs.size(); i++ )
			cellListed.insert( c.chunk->refs.at( i ).refForm );
	}
	for ( const auto & kv : s->cells ) {
		const StreamCell & c = kv.second;
		if ( c.st != CellState::Attached )
			continue;
		if ( c.lref ) {
			lrUnits++;
			const int n = c.chunk ? c.chunk->refs.size() : 0;
			lrEmpty += n ? 0 : 1;
			for ( int i = 0; i < n; i++ ) {
				lrDouble += cellListed.contains( c.chunk->refs.at( i ).refForm ) ? 1 : 0;
				listed.insert( c.chunk->refs.at( i ).refForm );
			}
			continue;
		}
		loadedCells.insert( CellFarHide::cellKey( c.x, c.y ) );
		if ( c.chunk )
			for ( int i = 0; i < c.chunk->refs.size(); i++ )
				if ( c.chunk->refs.at( i ).refForm )
					listed.insert( c.chunk->refs.at( i ).refForm );
	}
	qint64 iRows = 0, iHidden = 0, iDouble = 0, iNoref = 0, iReach = 0, mCentred = 0, mShapes = 0;
	for ( Shape * sh : s->scene->shapes ) {
		if ( !sh )
			continue;
		if ( const CellMeshInst * d = sh->wwFarInstData() ) {
			const CellFarHide::View vw = CellFarHide::view( *d );
			const qint64 n = d->rowCell.size();
			std::vector<char> vis( size_t( n ), 0 );
			for ( const CellMeshInst::Cmd & m : *vw.cmds )
				for ( quint32 i = m.baseInstance; i < m.baseInstance + m.instanceCount && qint64( i ) < n; i++ )
					vis[i] = 1;
			for ( qint64 i = 0; i < n; i++ ) {
				iRows++;
				if ( !vis[size_t( i )] ) {
					iHidden++;
					continue;
				}
				const bool in = loadedCells.contains( d->rowCell[i] );
				const quint32 ref = i < d->rowRef.size() ? d->rowRef[i] : 0u;
				if ( ref && listed.contains( ref ) )
					iDouble++;
				else if ( !ref && in )
					iNoref++;
				else if ( in )
					iReach++;
			}
			continue;
		}
		if ( !s->maskShapes.contains( s->nif->getBlockNumber( sh->index() ) ) || !sh->parentNode() )
			continue;
		mShapes++;
		const Transform w = sh->worldTrans();
		const QVector<Vector3> & V = sh->verts;
		for ( const Triangle & t : sh->triangles ) {
			if ( t[0] >= V.size() || t[1] >= V.size() || t[2] >= V.size() || ( t[0] == t[1] && t[1] == t[2] ) )
				continue;
			const Vector3 p = w * ( ( V[t[0]] + V[t[1]] + V[t[2]] ) / 3.0f );
			mCentred += loadedCells.contains( CellFarHide::cellKey( cellOf( p[0] ), cellOf( p[1] ) ) ) ? 1 : 0;
		}
	}
	return QStringLiteral( "coverage edges %1 samples %2 holes %3 doubles %4 nearTris %5 farTris %6" )
		.arg( edges ).arg( nS ).arg( nH ).arg( nD ).arg( nearT.size() ).arg( farT.size() )
		+ QStringLiteral( " farhideMode %1 farinstRows %2 farinstHidden %3 farinstDouble %4 farinstNoref %5 farinstReach %6"
			" farmaskShapes %7 farmaskTris %8 farmaskCentred %9" )
		.arg( CellFarHide::mode() ).arg( iRows ).arg( iHidden ).arg( iDouble ).arg( iNoref ).arg( iReach ).arg( mShapes )
		.arg( CellFarHide::maskedTris( s->nif ) ).arg( mCentred )
		+ QStringLiteral( " farcardsPlaced %1" ).arg( ImpostorChunk::cellFarCardsShownIn( loadedCells ) )
		+ ( s->lrOn ? QStringLiteral( " largeRefUnits %1 largeRefDouble %2 largeRefEmpty %3" ).arg( lrUnits ).arg( lrDouble )
			.arg( lrEmpty ) : QString() );
}

qint64 cellStreamPrivateBytes()
{
#ifdef Q_OS_WIN
	PROCESS_MEMORY_COUNTERS_EX pmc;
	if ( GetProcessMemoryInfo( GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>( &pmc ), sizeof( pmc ) ) )
		return qint64( pmc.PrivateUsage );
#endif
	return 0;
}

void cellStreamHeapBytes( qint64 * busy, qint64 * held )
{
	qint64 b = 0, h = 0;
#ifdef Q_OS_WIN
	HANDLE heaps[256];
	const DWORD n = GetProcessHeaps( 256, heaps );
	for ( DWORD i = 0; i < n && i < 256; i++ ) {
		if ( !HeapLock( heaps[i] ) )
			continue;
		PROCESS_HEAP_ENTRY e;
		e.lpData = nullptr;
		while ( HeapWalk( heaps[i], &e ) ) {
			if ( e.wFlags & PROCESS_HEAP_ENTRY_BUSY )
				b += qint64( e.cbData );
			else if ( !( e.wFlags & ( PROCESS_HEAP_REGION | PROCESS_HEAP_UNCOMMITTED_RANGE ) ) )
				h += qint64( e.cbData );
		}
		HeapUnlock( heaps[i] );
	}
#endif
	if ( busy )
		*busy = b;
	if ( held )
		*held = h;
}

void cellStreamRegionBytes( qint64 out[3] )
{
	out[0] = out[1] = out[2] = 0;
#ifdef Q_OS_WIN
	MEMORY_BASIC_INFORMATION m;
	const char * p = nullptr;
	while ( VirtualQuery( p, &m, sizeof( m ) ) == sizeof( m ) ) {
		if ( m.State == MEM_COMMIT && m.Type == MEM_PRIVATE ) {
			const qint64 n = qint64( m.RegionSize );
			out[n < ( 1 << 20 ) ? 0 : n < ( qint64( 64 ) << 20 ) ? 1 : 2] += n;
		}
		const char * next = static_cast<const char *>( m.BaseAddress ) + m.RegionSize;
		if ( next <= p )
			break;
		p = next;
	}
#endif
}

void cellStreamTrace( const QString & line )
{
	trace( line );
}

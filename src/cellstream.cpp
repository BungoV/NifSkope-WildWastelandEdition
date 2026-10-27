// lane CELLSTREAM1: the exterior cell view's block of full cells, streamed round the camera (cellstream.h)
#include "cellstream.h"
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

#include "cellfarhide.h"	// lane FARHOLE2: the far field leaves room for every loaded cell
#include "cellmesh.h"
#include "cellsplat.h"
#include "impostorchunk.h"
#include "gl/bsshape.h"
#include "gl/cellcull.h"
#include "gl/cellgrass.h"
#include "gl/cellwater.h"
#include "gl/glnode.h"
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "data/nifitemcache.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QSet>

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
};

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
	CellStreamStats st;
	QString red;
	double budgetMs = 4.0;
	double wirePerNodeMs = 0.4;   //!< running average: one scene node made from a written block
	int sliceNodes = 0;           //!< node blocks taken in this slice, still to be wired
	float shift = 2048.0f;

	~Stream() override
	{
		farHideForget();
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
		if ( writing && idx.isValid() )
			writing->takes.emplace_back( m->getBlockNumber( idx ), type );
		if ( idx.isValid() && isNodeType( type ) )
			sliceNodes++;
		return idx;
	}

	double wireOwedMs() const override { return sliceNodes * wirePerNodeMs; }

	QHash<QString, std::shared_ptr<NifModel>> pfxSrc;   //!< lane LIVESET3: particle models, one parse a stream
	int pfxLive = 0;                                    //!< particle copies in the attached cells

	void adopt( NifModel * m, int block ) override
	{
		if ( writing )
			writing->takes.emplace_back( block, m->itemName( m->getBlockIndex( block ) ) );
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
			showFar( c.x, c.y, red == QLatin1String( "nohandover" ) );
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
		auto it = cells.find( Key( done->cx, done->cy ) );
		if ( it != cells.end() && it->second.st == CellState::Building ) {
			trace( QStringLiteral( "built %1,%2 in %3 ms" ).arg( done->cx ).arg( done->cy ).arg( done->buildMs ) );
			it->second.chunk = std::move( done );
			it->second.st = CellState::Ready;
			st.built++;
		}
		// else: the cell left the block while it was built; its buckets go with `done`
	}
	if ( worker.joinable() )
		return;
	// lane CELLSTREAM1: leaving cells go, their blocks reset and built cells are written before the next build
	// starts: the builder's heap traffic stalls the main thread's model edits by up to tens of ms
	if ( unloadWaiting || !toReset.empty() || !toClear.empty() || !toFree.empty() )
		return;
	for ( const auto & kv : cells )
		if ( wanted.count( kv.first ) && ( kv.second.st == CellState::Ready || kv.second.st == CellState::Writing ) )
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
		const double score = ( wanted.count( kv.first ) ? 0.0 : 1000.0 ) + d;
		if ( score < bestScore ) {
			bestScore = score;
			best = &c;
		}
	}
	if ( !best )
		return;
	best->st = CellState::Building;
	// lane SHORE1: the eye cell, the pick's distance and the queue it beat, so a log proves nearest-first
	trace( QStringLiteral( "build %1,%2 (%3) eye %4,%5 d %6 of %7 queued" ).arg( best->x ).arg( best->y )
		.arg( wanted.count( Key( best->x, best->y ) ) ? "wanted" : "prefetch" ).arg( ex ).arg( ey )
		.arg( std::fmod( bestScore, 1000.0 ), 0, 'f', 2 ).arg( queued ) );
	job = std::make_shared<CellChunk>();
	job->seed = seed;
	job->cx = best->x;
	job->cy = best->y;
	// the enable facts of the 5x5 the cell is wanted in (the fixed block's rule)
	const bool now = wanted.count( Key( best->x, best->y ) ) > 0;
	const int fx = now ? ccx : ex, fy = now ? ccy : ey;
	job->facts[0] = fx - 2;
	job->facts[1] = fy - 2;
	job->facts[2] = fx + 2;
	job->facts[3] = fy + 2;
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
		CellFarHide::registerMask( nif, b, x0, y0, x1, y1 );
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
		nif->set<QString>( iCell, "Name", QStringLiteral( "cell %1,%2" ).arg( c.x ).arg( c.y ) );
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
	if ( red != QLatin1String( "nohandover" ) )
		showFar( c.x, c.y, false );   // the near ground replaces the far tile in the same step: no overlap
	farHide( c, true );   // lane FARHOLE2: and the far objects, cards and rings past 0 it holds
	c.st = CellState::Attached;
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
	showFar( c.x, c.y, true );
	farHide( c, false );   // lane FARHOLE2
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
		( isNodeType( tk.second ) ? nodeBlocks : propBlocks ).insert( b );
	}
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
	st.rebuildMaxMs = std::max( st.rebuildMaxMs, msOf( t ) );
}

bool Stream::busy() const
{
	if ( worker.joinable() || dirty || unloadWaiting || !toReset.empty() || !toClear.empty() || !toFree.empty() )
		return true;
	for ( const auto & kv : cells ) {
		const CellState s = kv.second.st;
		if ( s == CellState::Attached || ( s == CellState::Ready && !wanted.count( kv.first ) ) )
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

	shiftCentre( eye );
	wanted.clear();
	for ( int dy = -2; dy <= 2; dy++ )
		for ( int dx = -2; dx <= 2; dx++ )
			wanted.insert( Key( ccx + dx, ccy + dy ) );
	// the row the next move needs, as soon as the eye is in the neighbour cell
	prefetch.clear();
	const int ex = cellOf( eye[0] ), ey = cellOf( eye[1] );
	for ( int dy = -2; dy <= 2; dy++ )
		for ( int dx = -2; dx <= 2; dx++ )
			if ( !wanted.count( Key( ex + dx, ey + dy ) ) )
				prefetch.insert( Key( ex + dx, ey + dy ) );

	unloadWaiting = false;
	const double msUnload0 = msOf( t );
	for ( auto it = cells.begin(); it != cells.end(); ) {
		StreamCell & c = it->second;
		const bool w = wanted.count( it->first ) > 0, p = prefetch.count( it->first ) > 0;
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
			if ( !wanted.count( kv.first ) )
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
	if ( dirty && !worker.joinable() && toReset.empty() ) {
		bool writingAny = false;
		for ( const auto & kv : cells )
			writingAny |= wanted.count( kv.first ) && kv.second.st != CellState::Attached;
		if ( !writingAny ) {
			g_phase = "rebuild";
			msRebuild0 = msOf( t );
			rebuild();
			msRebuild = msOf( t ) - msRebuild0;
			changed = true;
		}
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
	if ( msOf( t ) > st.tickMaxMs )   // lane CELLSPIKE2: the stages of the step's dearest frame (dump field worstTick)
		st.worstTick = QStringLiteral( "wtMs %1 wtPre %2 wtUnload %3 wtPump %4 wtBury %5 wtDrain %6 wtWrite %7 wtRebuild %8 wtIdle %9" )
			.arg( msOf( t ), 0, 'f', 2 ).arg( msUnload0, 0, 'f', 2 ).arg( msUnload, 0, 'f', 2 ).arg( msPump, 0, 'f', 2 )
			.arg( msDrain0 - msBury0, 0, 'f', 2 ).arg( msWrite0 - msDrain0, 0, 'f', 2 ).arg( msWrite, 0, 'f', 2 )
			.arg( msRebuild, 0, 'f', 2 ).arg( msOf( t ) - msIdle0, 0, 'f', 2 )
			+ QStringLiteral( " wtCpu %1" ).arg( threadCpuMs() - cpu0, 0, 'f', 2 );   // lane CELLSPIKE3: on-processor ms
	st.tickMaxMs = std::max( st.tickMaxMs, msOf( t ) );
	return changed;
}

QString keyList( const std::vector<Key> & keys )
{
	QStringList l;
	for ( const Key & k : keys )
		l << QStringLiteral( "%1,%2" ).arg( k.first ).arg( k.second );
	return l.join( QLatin1Char( ' ' ) );
}

} // namespace

bool cellStreamWanted()
{
	if ( wwEnvIsSet( "WW_CELL_STREAM" ) )
		return wwEnvInt( "WW_CELL_STREAM" ) != 0;
	// a harness's fixed block stays fixed; the census walk opens exterior blocks itself and must weld them
	return wwEnvIsEmpty( "WW_CELL_OPEN" ) && wwEnvIsEmpty( "WW_CELL_CENSUS_TEST" );
}

void cellStreamStart( NifModel * nif, std::shared_ptr<CellStreamSeed> seed )
{
	if ( !nif || !seed )
		return;
	cellStreamStop( nif );
	auto s = std::make_unique<Stream>();
	s->nif = nif;
	s->seed = seed;
	s->ccx = seed->cx;
	s->ccy = seed->cy;
	if ( wwEnvIsSet( "WW_CELL_STREAM_BUDGET" ) )
		s->budgetMs = std::clamp( wwEnvStr( "WW_CELL_STREAM_BUDGET" ).toDouble(), 0.5, 100.0 );
	if ( wwEnvIsSet( "WW_CELL_STREAM_SHIFT" ) )
		s->shift = std::clamp( float( wwEnvInt( "WW_CELL_STREAM_SHIFT" ) ), 0.0f, 4096.0f );
	s->red = wwEnvStr( "WW_CELL_STREAM_RED" );
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

int cellStreamTexKeepSec()
{
	WW_SWITCH_STATIC( int, keep, wwEnvIsSet( "WW_CELL_STREAM_TEXKEEP" ) ? wwEnvInt( "WW_CELL_STREAM_TEXKEEP" ) : 5 );
	return std::max( keep, 0 );
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
		s->st.buildMaxMs = 0;
		s->st.worstTick.clear();
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
		if ( c.st != CellState::Attached || c.cellBlock < 0 )
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
	QSet<qint64> loadedCells;
	QSet<quint32> listed;
	for ( const auto & kv : s->cells ) {
		const StreamCell & c = kv.second;
		if ( c.st != CellState::Attached )
			continue;
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
		+ QStringLiteral( " farcardsPlaced %1" ).arg( ImpostorChunk::cellFarCardsShownIn( loadedCells ) );
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

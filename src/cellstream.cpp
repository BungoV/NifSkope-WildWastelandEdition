// lane CELLSTREAM1: the exterior cell view's block of full cells, streamed round the camera (cellstream.h)
#include "cellstream.h"

#include "cellmesh.h"
#include "cellsplat.h"
#include "gl/bsshape.h"
#include "gl/cellcull.h"
#include "gl/cellgrass.h"
#include "gl/cellwater.h"
#include "gl/glnode.h"
#include "gl/glproperty.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QSet>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <set>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#ifdef __MINGW32__
#include <pthread.h>
#endif
#endif

namespace {

constexpr float kCell = 4096.0f;
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
	double writeMs = 0.0;
};

bool isNodeType( const QString & t )
{
	return t == QLatin1String( "BSTriShape" ) || t == QLatin1String( "NiNode" ) || t == QLatin1String( "BSOrderedNode" )
		|| t == QLatin1String( "NiBillboardNode" );
}

const char * g_phase = "idle";   // the tick step a traced line came from
// lane CELLSTREAM1: WW_CELL_STREAM_TRACE=1 prints each load, write, attach, unload and scene remake to stderr
void trace( const QString & s )
{
	static const bool on = qEnvironmentVariableIntValue( "WW_CELL_STREAM_TRACE" ) > 0;
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
		if ( worker.joinable() )
			worker.join();
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
	bool write( StreamCell & c, double budget );
	void wire( StreamCell & c );
	void attach( StreamCell & c );
	void showFar( int x, int y, bool show );
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
		}
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
	for ( auto & kv : cells ) {
		StreamCell & c = kv.second;
		if ( c.st != CellState::Queued )
			continue;
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
	trace( QStringLiteral( "build %1,%2 (%3)" ).arg( best->x ).arg( best->y ).arg( wanted.count( Key( best->x, best->y ) ) ? "wanted" : "prefetch" ) );
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
	worker = std::thread( [j, flag]() {
#ifdef Q_OS_WIN
		// lane CELLSTREAM1: the builder yields to the main thread (its frame slices are what the gate times)
		SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
#endif
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
	for ( size_t ci = 0; ci < c.containers.size(); ci++ ) {
		Node * parent = c.nodes.value( c.containers[ci] ).data();
		if ( !parent )
			continue;
		const QModelIndex iP = nif->getBlockIndex( c.containers[ci] );
		for ( const qint32 l : nif->getLinkArray( iP, "Children" ) ) {
			if ( l < 0 || c.nodes.contains( l ) )
				continue;
			const QModelIndex iC = nif->getBlockIndex( l );
			Node * n = makeNode( iC, nif->itemName( iC ) );
			c.nodes.insert( l, n );
			if ( n )
				n->makeParent( parent );
		}
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
	const int made = int( c.nodes.size() - before );
	if ( made > 0 )
		wirePerNodeMs = 0.7 * wirePerNodeMs + 0.3 * ( ( msOf( t ) - emitMs ) / made );
	sliceNodes = 0;
	writing = nullptr;
	if ( !err.isEmpty() )
		st.lastError = QStringLiteral( "%1,%2: %3" ).arg( c.x ).arg( c.y ).arg( err );
	g_phase = "attach";
	if ( done )
		attach( c );
	const double ms = msOf( t );
	st.sliceMaxMs = std::max( st.sliceMaxMs, ms );
	if ( ms > budget + 2.0 )
		trace( QStringLiteral( "slow slice %1,%2: %3 ms (emit %4, wire %5 nodes %6)" ).arg( c.x ).arg( c.y )
			.arg( ms, 0, 'f', 1 ).arg( emitMs, 0, 'f', 1 ).arg( ms - emitMs, 0, 'f', 1 ).arg( int( c.nodes.size() - before ) )
			+ ( building() ? QStringLiteral( " worker busy" ) : QStringLiteral( " worker idle" ) ) );
	c.writeMs += ms;
	return done;
}

void Stream::attach( StreamCell & c )
{
	for ( const int b : c.containers )
		nif->updateBlockLinks( b );
	setRootSlot( -1, c.cellBlock );
	if ( c.cellNode && root )
		c.cellNode->makeParent( root.data() );
	if ( red != QLatin1String( "nohandover" ) )
		showFar( c.x, c.y, false );   // the near ground replaces the far tile in the same step: no overlap
	c.st = CellState::Attached;
	st.attaches++;
	trace( QStringLiteral( "attach %1,%2 block %3 after %4 ms of writing" ).arg( c.x ).arg( c.y ).arg( c.cellBlock ).arg( c.writeMs, 0, 'f', 1 ) );
	dirty = true;
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
	showFar( c.x, c.y, true );
	QSet<int> nodeBlocks, propBlocks;
	for ( const auto & tk : c.takes ) {
		const int b = tk.first;
		cellMeshDrop( nif, b );
		wwCellCullForget( nif, b );
		wwCellWaterForget( nif, b );
		wwCellLandForget( nif, b );
		wwCellGrassForget( nif, b );
		wwCellFxLitForget( nif, b );
		( isNodeType( tk.second ) ? nodeBlocks : propBlocks ).insert( b );
	}
	std::vector<Node *> ns;
	for ( auto it = c.nodes.cbegin(); it != c.nodes.cend(); ++it )
		if ( it.value() )
			ns.push_back( it.value().data() );
	if ( int( ns.size() ) < nodeBlocks.size() ) {   // the scene was made again: find them by block
		ns.clear();
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
	if ( msOf( t ) > 3.0 )
		trace( QStringLiteral( "slow unload %1,%2: %3 ms (forget %4, shapes+props %5, nodes out %6, props %7; %8 nodes %9 props)" )
			.arg( c.x ).arg( c.y ).arg( msOf( t ), 0, 'f', 1 ).arg( msForget, 0, 'f', 1 ).arg( msShapes - msForget, 0, 'f', 1 )
			.arg( msNodes - msShapes, 0, 'f', 1 ).arg( msOf( t ) - msNodes, 0, 'f', 1 ).arg( ns.size() ).arg( props.size() ) );
	toReset.insert( toReset.end(), c.takes.begin(), c.takes.end() );
	c.takes.clear();
	c.takesSeen = 0;
	c.nodes.clear();
	c.containers.clear();
	c.grounds.clear();
	c.cellNode = nullptr;
	c.cellBlock = -1;
	c.chunk.reset();
	st.unloads++;
	st.unloadMaxMs = std::max( st.unloadMaxMs, msOf( t ) );
	dirty = true;
}

void Stream::bury( const QElapsedTimer & t )
{
	// lane CELLSTREAM1: every node of the left cells drops its links first (they count each other), then each goes
	QElapsedTimer d;
	d.start();
	while ( !toClear.empty() && msOf( t ) < budgetMs ) {
		Node * n = toClear.back();
		toClear.pop_back();
		n->clear();
		toFree.push_back( n );
	}
	while ( toClear.empty() && !toFree.empty() && msOf( t ) < budgetMs ) {
		NodeList::release( toFree.back() );
		toFree.pop_back();
	}
	st.buryMaxMs = std::max( st.buryMaxMs, msOf( d ) );
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
	while ( !toReset.empty() && msOf( t ) < budgetMs ) {   // the writes wait for an empty pool queue anyway
		const auto tk = toReset.back();
		toReset.pop_back();
		const double r0 = msOf( d );
		const bool quiet = !loud && !pinned.contains( tk.first );
		st.quietResets += quiet ? 1 : 0;
		st.loudResets += quiet ? 0 : 1;
		if ( nif->resetNiBlock( tk.first, quiet ) )
			pool[tk.second].insert( tk.first );
		if ( msOf( d ) - r0 > 3.0 )
			trace( QStringLiteral( "slow reset %1 %2%8: %3 ms (remove %4 insert %5 links %6, %7 persistent, scan %9 ms)" )
				.arg( tk.second ).arg( tk.first ).arg( msOf( d ) - r0, 0, 'f', 1 ).arg( nif->resetMs[0], 0, 'f', 1 )
				.arg( nif->resetMs[1], 0, 'f', 1 ).arg( nif->resetMs[2], 0, 'f', 1 ).arg( nif->persistentNow )
				.arg( quiet ? " quiet" : "" ).arg( scanMs, 0, 'f', 2 )
				+ ( worker.joinable() && !jobDone.load() ? QStringLiteral( " worker busy" ) : QStringLiteral( " worker idle" ) ) );
	}
	st.drainMaxMs = std::max( st.drainMaxMs, msOf( d ) );
	if ( msOf( d ) > budgetMs )
		trace( QStringLiteral( "slow drain: %1 ms" ).arg( msOf( d ), 0, 'f', 1 ) );
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
	if ( sc != scene || !root ) {
		if ( !resolve( sc ) )
			return false;
	}
	nif->streamQuiet = true;
	nif->setState( BaseModel::Processing );
	bool changed = false;

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
	if ( !building() )
		bury( t );
	g_phase = "drain";
	if ( !midCell )
		drain( t );
	while ( !building() && msOf( t ) < budgetMs ) {
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
	}
	if ( dirty && !worker.joinable() && toReset.empty() ) {
		bool writingAny = false;
		for ( const auto & kv : cells )
			writingAny |= wanted.count( kv.first ) && kv.second.st != CellState::Attached;
		if ( !writingAny ) {
			g_phase = "rebuild";
			rebuild();
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
		trace( QStringLiteral( "slow tick: %1 ms (pump %2, model state %3)%4" ).arg( msOf( t ), 0, 'f', 1 ).arg( msPump, 0, 'f', 1 )
			.arg( msOf( t ) - msIdle0, 0, 'f', 1 ).arg( building() ? QStringLiteral( " worker busy" ) : QStringLiteral( " worker idle" ) ) );
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
	if ( qEnvironmentVariableIsSet( "WW_CELL_STREAM" ) )
		return qEnvironmentVariableIntValue( "WW_CELL_STREAM" ) != 0;
	return qEnvironmentVariableIsEmpty( "WW_CELL_OPEN" );   // a harness's fixed block stays fixed
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
	if ( qEnvironmentVariableIsSet( "WW_CELL_STREAM_BUDGET" ) )
		s->budgetMs = std::clamp( qEnvironmentVariable( "WW_CELL_STREAM_BUDGET" ).toDouble(), 0.5, 100.0 );
	if ( qEnvironmentVariableIsSet( "WW_CELL_STREAM_SHIFT" ) )
		s->shift = std::clamp( float( qEnvironmentVariableIntValue( "WW_CELL_STREAM_SHIFT" ) ), 0.0f, 4096.0f );
	s->red = qEnvironmentVariable( "WW_CELL_STREAM_RED" );
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

bool cellStreamTick( NifModel * nif, Scene * scene, const Vector3 & eye )
{
	Stream * s = streamFor( nif );
	return ( s && nif ) ? s->tick( scene, eye ) : false;
}

int cellStreamTexKeepSec()
{
	static const int keep = qEnvironmentVariableIsSet( "WW_CELL_STREAM_TEXKEEP" ) ? qEnvironmentVariableIntValue( "WW_CELL_STREAM_TEXKEEP" ) : 5;
	return std::max( keep, 0 );
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
	return QStringLiteral( "coverage edges %1 samples %2 holes %3 doubles %4 nearTris %5 farTris %6" )
		.arg( edges ).arg( nS ).arg( nH ).arg( nD ).arg( nearT.size() ).arg( farT.size() );
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

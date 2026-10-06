#include "cellmesh.h"

#include "cellspeed.h"
#include "celltexahead.h"
#include "model/nifmodel.h"

#include <QCoreApplication>
#include <QHash>
#include <QObject>
#include <QThread>

#include <algorithm>
#include <cstdio>

// lane SPEED1 (2026-10-02): see cellmesh.h. Main thread only: the document and the scene live there.

namespace
{

struct DocMeshes
{
	QHash<const NifItem *, QSharedPointer<CellMesh>> byBlock;
	bool hooked = false;
};

QHash<const BaseModel *, DocMeshes> & table()
{
	static QHash<const BaseModel *, DocMeshes> t;
	return t;
}

thread_local int quietDepth = 0;

// after every change of the table: what the by-name lookup's one test reads
void recount()
{
	int n = 0;
	for ( const DocMeshes & d : table() )
		n += d.byBlock.size();
	cellMeshWaiting.store( n, std::memory_order_relaxed );
}

/* The table belongs to the main thread. A generator worker saving a document of its own (NifModel::save asks
 * here) is never one of ours, and must not read a table the main thread may be writing. */
bool offMainThread()
{
	return !qApp || QThread::currentThread() != qApp->thread();
}

/* The rows, exactly as the cell view wrote them before this lane (cellview.cpp emitBucket, the slow path):
 * the same fields in the same order, so a saved file is the same file. */
void writeRows( NifModel * nif, const QModelIndex & iShape, const CellMesh & m )
{
	CellSpeed::Acc speedAcc( "welded rows written on demand" );
	// the gate's red control for the saved file: the rows come back without their normals
	static const bool redRows = qgetenv( "WW_CELL_SPEED_RED" ) == "rows";
	nif->setState( BaseModel::Processing );
	QModelIndex iVertexData = nif->getIndex( iShape, "Vertex Data" );
	nif->updateArraySize( iVertexData );
	const int nv = m.verts.size();
	for ( int v = 0; v < nv; v++ ) {
		QModelIndex row = nif->index( v, 0, iVertexData );
		nif->set<Vector3>( row, "Vertex", m.verts[v] );
		nif->set<HalfVector2>( row, "UV", HalfVector2( m.coords[v] ) );
		if ( !redRows )
			nif->set<ByteVector3>( row, "Normal", ByteVector3( m.norms[v] ) );
		nif->set<ByteVector3>( row, "Tangent", ByteVector3( m.tangents[v] ) );
		nif->set<float>( row, "Bitangent X", m.bitangents[v][0] );
		nif->set<float>( row, "Bitangent Y", m.bitangents[v][1] );
		nif->set<float>( row, "Bitangent Z", m.bitangents[v][2] );
		if ( v < m.uv2y.size() ) {   // lane FARLOD2: the far objects' array layer
			const QModelIndex iUv2 = nif->getIndex( row, "UV 2" );
			if ( iUv2.isValid() )
				nif->set<HalfVector2>( row, "UV 2", HalfVector2( Vector2( 0.0f, m.uv2y[v] ) ) );
		}
		if ( m.withColour ) {
			const Color4 & c = m.colors[v];
			nif->set<ByteColor4>( row, "Vertex Colors", ByteColor4( FloatVector4( c[0], c[1], c[2], c[3] ) ) );
		}
	}
	QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
	nif->updateArraySize( iTriangles );
	nif->setArray<Triangle>( iTriangles, m.triangles );
	nif->restoreState();
}

} // namespace

std::atomic<int> cellMeshWaiting{ 0 };

CellMeshQuiet::CellMeshQuiet() { quietDepth++; }
CellMeshQuiet::~CellMeshQuiet() { quietDepth--; }

void cellMeshRowsAsked( const BaseModel * model, const NifItem * block, const NifItem * array )
{
	// the gate's red control for this net: the reader gets the empty array, as it would without it
	static const bool redNoNet = qgetenv( "WW_CELL_SPEED_RED" ) == "nonet";
	if ( quietDepth > 0 || redNoNet || offMainThread() || table().isEmpty() )
		return;
	auto it = table().constFind( model );
	if ( it == table().constEnd() || !it->byBlock.contains( block ) )
		return;
	if ( !array->hasName( "Vertex Data" ) && !array->hasName( "Triangles" ) )
		return;
	// the key was put by cellMeshPut from a NifModel, so the model is one; the lookup is const, the rows are not
	NifModel * nif = const_cast<NifModel *>( static_cast<const NifModel *>( model ) );
	CellSpeed::count( "welded shapes written because a reader asked for their rows by name", 1 );
	cellMeshMaterialize( nif, nif->getBlockIndex( nif->getBlockNumber( block ) ) );
}

bool cellMeshOn()
{
	static const bool on = qgetenv( "WW_CELL_SPEED_RED" ) != "slow";
	return on;
}

void cellMeshClear( const NifModel * nif )
{
	CellTexAhead::stop( nif );   // a cell is about to be built in this document again
	auto it = table().find( nif );
	if ( it != table().end() )
		it->byBlock.clear();
	recount();
}

void cellMeshPut( NifModel * nif, const QModelIndex & iShape, const QSharedPointer<CellMesh> & mesh )
{
	const NifItem * block = nif ? nif->getBlockItem( iShape ) : nullptr;
	if ( !block || !mesh )
		return;
	DocMeshes & d = table()[nif];
	if ( !d.hooked ) {
		/* A block's address is this table's key, so the table must not outlive the blocks: the document
		 * being emptied (another file opened in the window) or destroyed drops everything it held. A reset
		 * that keeps the blocks (loadIndex, a paste) is not that, and happens in the middle of a cell build. */
		d.hooked = true;
		QObject::connect( nif, &QAbstractItemModel::modelReset, nif, [nif]() {
			if ( nif->getBlockCount() == 0 )
				cellMeshClear( nif );
		} );
		const BaseModel * key = nif;
		QObject::connect( nif, &QObject::destroyed, [key]() { table().remove( key ); recount(); } );
	}
	CellTexAhead::arm( nif );   // a cell document: its texture files are read ahead (celltexahead.h)
	d.byBlock.insert( block, mesh );
	recount();
}

QSharedPointer<CellMesh> cellMeshFor( const NifModel * nif, const QModelIndex & iShape )
{
	if ( offMainThread() || table().isEmpty() || !nif )
		return {};
	auto it = table().constFind( nif );
	if ( it == table().constEnd() || it->byBlock.isEmpty() )
		return {};
	const NifItem * block = nif->getBlockItem( iShape );
	auto mit = it->byBlock.constFind( block );
	if ( mit == it->byBlock.constEnd() )
		return {};
	// the document must state this mesh's counts and hold none of its rows (a block address met again by
	// chance is not ours, and a shape that has rows is the document's)
	if ( nif->get<quint32>( block, "Num Vertices" ) != quint32( mit.value()->verts.size() )
		|| nif->get<quint32>( block, "Num Triangles" ) != quint32( mit.value()->triangles.size() ) )
		return {};
	// (looked at child by child: asking for the array by name is what writes the rows, see cellMeshRowsAsked)
	for ( const NifItem * child : block->children() ) {
		if ( child->hasName( "Vertex Data" ) && child->childCount() != 0 )
			return {};
	}
	return mit.value();
}

bool cellMeshMaterialize( NifModel * nif, const QModelIndex & index )
{
	if ( offMainThread() || table().isEmpty() || !nif || !index.isValid() )
		return false;
	auto it = table().find( nif );
	if ( it == table().end() || it->byBlock.isEmpty() )
		return false;
	const NifItem * block = nif->getBlockItem( index );
	auto mit = it->byBlock.find( block );
	if ( mit == it->byBlock.end() )
		return false;
	const QSharedPointer<CellMesh> mesh = mit.value();
	it->byBlock.erase( mit );   // first: from here on the document is the shape's one source
	recount();
	const QModelIndex iShape = nif->getBlockIndex( nif->getBlockNumber( block ) );
	if ( !iShape.isValid() )
		return false;
	writeRows( nif, iShape, *mesh );
	return true;
}

int cellMeshMaterializeAll( NifModel * nif )
{
	if ( offMainThread() || table().isEmpty() || !nif )
		return 0;
	auto it = table().find( nif );
	if ( it == table().end() || it->byBlock.isEmpty() )
		return 0;
	// in block order, so the document is filled the same way every time
	QVector<int> blocks;
	for ( auto mit = it->byBlock.constBegin(); mit != it->byBlock.constEnd(); ++mit )
		blocks.append( nif->getBlockNumber( mit.key() ) );
	std::sort( blocks.begin(), blocks.end() );
	int n = 0;
	for ( int b : blocks )
		n += ( b >= 0 && cellMeshMaterialize( nif, nif->getBlockIndex( b ) ) ) ? 1 : 0;
	return n;
}

int cellMeshCount( const NifModel * nif )
{
	auto it = table().constFind( nif );
	return it == table().constEnd() ? 0 : it->byBlock.size();
}

int cellMeshWaitingBytes( const NifModel * nif, const NifItem * block )
{
	if ( offMainThread() || table().isEmpty() )
		return -1;
	auto it = table().constFind( nif );
	if ( it == table().constEnd() || !it->byBlock.contains( block ) )
		return -1;
	return int( nif->get<quint32>( block, "Data Size" ) );
}

void cellMeshSaveIfAsked( NifModel * nif )
{
	const QByteArray path = qgetenv( "WW_CELL_SPEED_SAVE" );
	if ( path.isEmpty() || !nif )
		return;
	const int waiting = cellMeshCount( nif );
	// what the header says of the blocks while the rows still wait: the old path's sizes, row bytes included
	qint64 stated = 0;
	const QModelIndex iSizes = nif->getIndex( nif->getHeaderIndex(), "Block Size" );
	for ( int r = 0; r < nif->rowCount( iSizes ); r++ )
		stated += nif->get<int>( nif->index( r, 0, iSizes ) );
	// the net under by-name readers: every other waiting shape is asked for its rows the way a mesh tool asks
	// (the vertex rows and the triangles by turns); the rest are left to the save
	QVector<int> blocks;
	auto it = table().constFind( nif );
	if ( it != table().constEnd() ) {
		for ( auto mit = it->byBlock.constBegin(); mit != it->byBlock.constEnd(); ++mit )
			blocks.append( nif->getBlockNumber( mit.key() ) );
	}
	std::sort( blocks.begin(), blocks.end() );
	int asked = 0, had = 0;
	for ( int i = 0; i < blocks.size(); i += 2, asked++ ) {
		const QModelIndex iShape = nif->getBlockIndex( blocks[i] );
		const bool tris = ( i & 2 ) != 0;
		const int rows = nif->rowCount( nif->getIndex( iShape, tris ? "Triangles" : "Vertex Data" ) );
		had += ( rows > 0 && rows == nif->get<int>( iShape, tris ? "Num Triangles" : "Num Vertices" ) ) ? 1 : 0;
	}
	const bool ok = nif->saveToFile( QString::fromLocal8Bit( path ) );   // the save itself asks for the rows
	fprintf( stderr, "cell speed save: %d shapes waited beside the document, %d after the save, file %s\n",
		waiting, cellMeshCount( nif ), ok ? "written" : "NOT written" );
	fprintf( stderr, "cell speed net: %d shapes asked for their rows by name, %d had them\n", asked, had );
	fprintf( stderr, "cell speed header: %lld bytes of blocks stated before the save\n", (long long) stated );
}

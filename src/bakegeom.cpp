/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "bakegeom.h"

#include "gl/gltools.h"
#include "model/nifmodel.h"
#include "nifsnapshot.h"

#include <QBuffer>
#include <QSet>

#include <algorithm>
#include <functional>

namespace BakeGeom
{

namespace
{

//! Duplicate one block in place, returning the copy.
/*! Lifted from spDuplicateBlock: save the branch to a buffer and load it into a
 *  fresh block. Appending at getBlockCount() keeps every existing block number
 *  valid, which matters because callers hold numbers across this. */
QModelIndex duplicateBlock( NifModel * nif, const QModelIndex & src )
{
	if ( !src.isValid() )
		return QModelIndex();

	QByteArray data;
	QBuffer buffer( &data );
	// Opening ReadWrite does not work — same note as spDuplicateBlock.
	if ( !( buffer.open( QIODevice::WriteOnly ) && nif->saveIndex( buffer, src ) ) )
		return QModelIndex();
	if ( !buffer.open( QIODevice::ReadOnly ) )
		return QModelIndex();

	QModelIndex dst = nif->insertNiBlock( nif->itemName( src ), nif->getBlockCount() );
	if ( !dst.isValid() )
		return QModelIndex();
	if ( !nif->loadIndex( buffer, dst ) )
		return QModelIndex();
	return dst;
}

//! Write a vertex position whatever precision the record actually uses.
/*! The landmine this exists for: nif.xml names BOTH the Vector3 and the
 *  HalfVector3 variant "Vertex", so `set<Vector3>` on a half record does nothing
 *  and returns false. A converter that ignored the return once wrote a whole
 *  file with no vertices in it and reported success. */
bool setVertexPos( NifModel * nif, const QModelIndex & iVertex, const Vector3 & p )
{
	NifItem * item = nif->getItem( iVertex, "Vertex" );
	if ( !item )
		return false;
	if ( item->hasValueType( NifValue::tHalfVector3 ) )
		return nif->set<HalfVector3>( item, HalfVector3( p ) );
	return nif->set<Vector3>( item, p );
}

bool isParticleBlock( const NifModel * nif, const QModelIndex & idx )
{
	const QString t = nif->itemName( idx );
	return nif->blockInherits( idx, { "NiParticleSystem", "NiPSysData", "NiPSysModifier",
	                                  "NiPSysModifierCtlr", "BSPositionData" } )
	       || t.startsWith( QLatin1String( "NiPSys" ) )
	       || t.startsWith( QLatin1String( "BSPSys" ) );
}

bool isLightningBlock( const NifModel * nif, const QModelIndex & idx )
{
	return nif->itemName( idx ) == QLatin1String( "BSProceduralLightningController" );
}

//! Drop the -1 holes removeNiBlock leaves behind in link arrays.
/*! removeNiBlock nulls the links that pointed at the block but leaves the array
 *  ENTRY, so a node ends up claiming children it does not have. */
void compactLinkArrays( NifModel * nif )
{
	static const char * const arrays[][2] = {
		{ "Children", "Num Children" },
		{ "Extra Data List", "Num Extra Data List" },
		{ "Effects", "Num Effects" },
	};

	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex idx = nif->getBlockIndex( b );
		for ( const auto & a : arrays ) {
			QModelIndex iArr = nif->getIndex( idx, a[0] );
			if ( !iArr.isValid() )
				continue;
			QVector<qint32> kept;
			const int n = nif->rowCount( iArr );
			for ( int i = 0; i < n; i++ ) {
				const qint32 l = nif->getLink( nif->getIndex( iArr, i ) );
				if ( l >= 0 )
					kept.append( l );
			}
			if ( kept.size() == n )
				continue;
			nif->set<uint>( idx, a[1], quint32( kept.size() ) );
			nif->updateArraySize( iArr );
			for ( int i = 0; i < kept.size(); i++ )
				nif->setLink( nif->getIndex( iArr, i ), kept.at( i ) );
		}
	}
}

} // namespace

QModelIndex writeShape( NifModel * nif, const Capture & cap, QString * error )
{
	auto fail = [&]( const QString & why ) {
		if ( error )
			*error = why;
		return QModelIndex();
	};

	if ( !nif )
		return fail( QStringLiteral( "no model" ) );
	if ( cap.tris.isEmpty() || cap.tris.size() % 3 != 0 )
		return fail( QStringLiteral( "'%1' has %2 vertices, which is not whole triangles" )
			.arg( cap.name ).arg( cap.tris.size() ) );
	if ( cap.uvs.size() != cap.tris.size() )
		return fail( QStringLiteral( "'%1' has %2 UVs for %3 vertices" )
			.arg( cap.name ).arg( cap.uvs.size() ).arg( cap.tris.size() ) );
	if ( nif->getBSVersion() < 100 )
		return fail( QStringLiteral( "baking needs a BSTriShape file (BS version %1 has none)" )
			.arg( nif->getBSVersion() ) );

	const QList<int> roots = nif->getRootLinks();
	if ( roots.isEmpty() )
		return fail( QStringLiteral( "this file has no root block" ) );
	const QModelIndex iRoot = nif->getBlockIndex( roots.first() );

	// Vertex indices are 16-bit. 65535 corners is 10922 sprites, well past what
	// any of these emitters run, but a silent wrap would be a nightmare to find.
	if ( cap.tris.size() > 65535 )
		return fail( QStringLiteral( "'%1' captured %2 vertices; a BSTriShape holds 65535" )
			.arg( cap.name ).arg( cap.tris.size() ) );

	/* World space in, shape space out.
	 *
	 * The capture is in world coordinates and the new shape hangs under the root,
	 * so the root's own transform has to come off first — it is identity in most
	 * FO4 files but not in all of them, and getting this wrong displaces the whole
	 * bake by the root offset.
	 *
	 * Then re-centre on the centroid, as the loading-screen converter does for
	 * every other shape: translation carries the position, vertices stay small.
	 */
	Transform rootInv;
	if ( Transform::canConstruct( nif, iRoot ) )
		rootInv = Transform( nif, iRoot ).inverted();

	QVector<Vector3> local;
	local.reserve( cap.tris.size() );
	Vector3 centroid;
	for ( const Vector3 & w : cap.tris ) {
		const Vector3 p = rootInv * w;
		local.append( p );
		centroid += p;
	}
	centroid /= float( local.size() );
	for ( Vector3 & p : local )
		p -= centroid;

	const bool haveColors = ( cap.cols.size() == cap.tris.size() );
	const qsizetype nv = local.size();
	const qsizetype nt = nv / 3;

	QModelIndex iShape = nif->insertNiBlock( QStringLiteral( "BSTriShape" ), nif->getBlockCount() );
	if ( !iShape.isValid() )
		return fail( QStringLiteral( "could not create a BSTriShape" ) );

	nif->set<QString>( iShape, "Name", cap.name.isEmpty()
		? QStringLiteral( "BakedEffect" ) : cap.name );
	nif->set<Vector3>( iShape, "Translation", centroid );
	nif->set<float>( iShape, "Scale", 1.0f );

	/* The layout the OBJ importer has been shipping: 28 bytes without colours, 32
	 * with. Full precision (the 0x0040... bit) is not optional here — half floats
	 * step ~0.0078 units at Z ≈ 111 and these are thin ribbons, so quantising them
	 * is visible as the strip breaking up.
	 */
	std::uint64_t vertexDesc = 0x0001B00000650407ULL;
	if ( nif->getBSVersion() >= 130 )
		vertexDesc = vertexDesc | 0x0040000000000000ULL;
	if ( haveColors )
		vertexDesc = vertexDesc + 0x0002000007000001ULL;
	const qsizetype stride = haveColors ? 32 : 28;

	nif->set<BSVertexDesc>( iShape, "Vertex Desc", vertexDesc );
	nif->set<quint32>( iShape, "Num Triangles", quint32( nt ) );
	nif->set<quint32>( iShape, "Num Vertices", quint32( nv ) );
	// Desc and Data Size move together or the file will not re-read.
	nif->set<quint32>( iShape, "Data Size", quint32( nv * stride + nt * 6 ) );

	// A billboard has one facing by construction; give every vertex that normal
	// rather than leaving the field at zero.
	Vector3 nrm = cap.facing;
	if ( nrm.length() < 1.0e-4f )
		nrm = Vector3( 0.0f, -1.0f, 0.0f );
	nrm.normalize();
	nrm = -nrm;
	Vector3 upRef = ( std::fabs( nrm[2] ) < 0.9f ) ? Vector3( 0.0f, 0.0f, 1.0f )
	                                               : Vector3( 1.0f, 0.0f, 0.0f );
	Vector3 tan = Vector3::crossproduct( upRef, nrm );
	tan.normalize();

	int posWritten = 0, uvWritten = 0;

	nif->setState( BaseModel::Processing );

	QModelIndex iVerts = nif->getIndex( iShape, "Vertex Data" );
	if ( iVerts.isValid() ) {
		nif->updateArraySize( iVerts );
		for ( qsizetype i = 0; i < nv; i++ ) {
			QModelIndex iVertex = nif->getIndex( iVerts, int( i ) );
			if ( !iVertex.isValid() )
				continue;
			if ( setVertexPos( nif, iVertex, local.at( i ) ) )
				posWritten++;
			if ( nif->set<HalfVector2>( iVertex, "UV", cap.uvs.at( i ) ) )
				uvWritten++;
			nif->set<ByteVector3>( iVertex, "Normal", ByteVector3( nrm ) );
			nif->set<ByteVector3>( iVertex, "Tangent", ByteVector3( tan ) );
			if ( haveColors ) {
				if ( QModelIndex j = nif->getIndex( iVertex, "Vertex Colors" ); j.isValid() )
					nif->set<ByteColor4>( j, ByteColor4( FloatVector4( cap.cols.at( i ) ) ) );
			}
		}
	}

	QVector<Triangle> tri;
	tri.reserve( nt );
	for ( qsizetype t = 0; t < nt; t++ )
		tri.append( Triangle( quint16( t * 3 ), quint16( t * 3 + 1 ), quint16( t * 3 + 2 ) ) );
	QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
	if ( iTriangles.isValid() ) {
		nif->updateArraySize( iTriangles );
		nif->setArray<Triangle>( iTriangles, tri );
	}

	nif->restoreState();

	// Refuse to claim a shape that has no geometry in it. This is the exact
	// failure the vertex-precision bug produced, and it reported success.
	if ( posWritten != nv || uvWritten != nv ) {
		nif->removeNiBlock( nif->getBlockNumber( iShape ) );
		return fail( QStringLiteral( "'%1': wrote %2 of %3 positions and %4 of %3 UVs — "
			"the vertex record did not take the values" )
			.arg( cap.name ).arg( posWritten ).arg( nv ).arg( uvWritten ) );
	}

	BoundSphere bounds( local, true );
	bounds.update( nif, iShape );

	// The properties come along as copies, not shares: the originals hang off the
	// emitter, and the emitter is about to be removed.
	if ( QModelIndex p = duplicateBlock( nif, cap.shaderProperty ); p.isValid() )
		nif->setLink( iShape, "Shader Property", nif->getBlockNumber( p ) );
	if ( QModelIndex p = duplicateBlock( nif, cap.alphaProperty ); p.isValid() )
		nif->setLink( iShape, "Alpha Property", nif->getBlockNumber( p ) );

	// Under the root, alongside the armour. Filling the empty Bolt_01 shape the
	// controller already targets would keep its place in the hierarchy, but that
	// shape only exists for lightning — particles have none — and the converter
	// flattens the hierarchy anyway.
	if ( QModelIndex iChildren = nif->getIndex( iRoot, "Children" ); iChildren.isValid() ) {
		const int n = nif->rowCount( iChildren );
		nif->set<uint>( iRoot, "Num Children", quint32( n + 1 ) );
		nif->updateArraySize( iChildren );
		nif->setLink( nif->getIndex( iChildren, n ), nif->getBlockNumber( iShape ) );
	}

	return iShape;
}

Result write( NifModel * nif, const QVector<Capture> & caps, bool removeEmitters, QString * error )
{
	Result res;
	if ( !nif ) {
		if ( error )
			*error = QStringLiteral( "no model" );
		return res;
	}
	if ( caps.isEmpty() ) {
		if ( error )
			*error = QStringLiteral( "nothing was captured" );
		return res;
	}

	nifSnapshotOp( nif, QStringLiteral( "Bake effects to static geometry" ), [&]() {
		bool anyParticles = false, anyLightning = false;

		for ( const Capture & cap : caps ) {
			QString err;
			const QModelIndex iShape = writeShape( nif, cap, &err );
			if ( !iShape.isValid() ) {
				res.notes << err;
				continue;
			}
			res.shapes++;
			res.vertices += int( cap.tris.size() );
			res.triangles += int( cap.tris.size() / 3 );
			if ( cap.fromParticles )
				anyParticles = true;
			else
				anyLightning = true;
			if ( !cap.shaderProperty.isValid() )
				res.notes << QStringLiteral( "'%1' had no shader property to copy — the baked "
					"shape will draw untextured" ).arg( cap.name );
		}

		if ( removeEmitters && res.shapes > 0 )
			res.emittersRemoved = dropEffects( nif, anyParticles, anyLightning );
	} );

	res.ok = ( res.shapes > 0 );
	if ( !res.ok && error )
		*error = res.notes.isEmpty() ? QStringLiteral( "nothing could be written" )
		                             : res.notes.join( QStringLiteral( "; " ) );
	return res;
}

int dropEffects( NifModel * nif, bool particles, bool lightning )
{
	if ( !nif || !( particles || lightning ) )
		return 0;

	QSet<int> doomed;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex idx = nif->getBlockIndex( b );
		if ( ( particles && isParticleBlock( nif, idx ) )
			|| ( lightning && isLightningBlock( nif, idx ) ) ) {
			doomed << b;
		}
	}
	if ( doomed.isEmpty() )
		return 0;

	// Descending, so renumbering cannot bite.
	QList<int> order = doomed.values();
	std::sort( order.begin(), order.end(), std::greater<int>() );
	for ( const int b : std::as_const( order ) )
		nif->removeNiBlock( b );

	compactLinkArrays( nif );
	return order.size();
}

bool hasEffects( const NifModel * nif, bool * particles, bool * lightning )
{
	bool p = false, l = false;
	if ( nif ) {
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			const QModelIndex idx = nif->getBlockIndex( b );
			if ( nif->itemName( idx ) == QLatin1String( "NiParticleSystem" ) )
				p = true;
			else if ( isLightningBlock( nif, idx ) )
				l = true;
		}
	}
	if ( particles )
		*particles = p;
	if ( lightning )
		*lightning = l;
	return p || l;
}

} // namespace BakeGeom

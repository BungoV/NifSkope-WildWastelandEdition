#include "nifskope.h"
#include "glview.h"
#include "spellbook.h"
#include "ui/widgets/wwnumberfield.h"
#include "gl/hknpdecode.h"
#include "skeletontools.h"
#include "gl/hknpencode.h"
#include "model/nifmodel.h"
#include "data/nifvalue.h"
#include "nifsnapshot.h"
#include "wwcollisionlibrary.h"
#include "wwskin.h"
#include "lib/qhull.h"
#include "lib/nvtristripwrapper.h"
#include "spells/blocks.h"
#include "ui/widgets/physicspanel.h"

#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QColor>
#include <QDockWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEnterEvent>
#include <QFormLayout>
#include <QFrame>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStatusBar>
#include <QSettings>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QBuffer>
#include <QDataStream>

#include <algorithm>
#include <cmath>

bool tlBuildCollisionPreview( NifModel * nif, const QModelIndex & index, int kind,
	float ratio, bool decomposition, float precision, float threshold, int maxHulls,
	QVector<Vector3> & triangleSoup, QString & statistics );
QModelIndex tlCommitCollisionPreview( NifModel * nif, const QModelIndex & index, int kind,
	float ratio, bool decomposition, float precision, float threshold, int maxHulls );
//! Defined in havok.cpp: the body values a creation preset stands for, keyed by
//! the CollisionManager/Create/ setting each is stored under. One definition, so
//! the panel that shows them and the code that writes them cannot drift apart.
QVariantMap tlCollisionPresetDefaults( int preset );
//! Also havok.cpp: make the collision object and rigid body on a node, no shape.
QModelIndex tlCreateCollisionBody( NifModel * nif, const QModelIndex & targetNode );
//! ...and the node such a body belongs on, wrapping the block when it needs one.
QModelIndex tlCollisionAttachNode( NifModel * nif, const QModelIndex & index, bool ownNode );

/*! Block lookup and shape-chain descent, at file scope.
 *
 *  These were members of CollisionManagerPanel below, which is the only reason
 *  the collision lint could not be reached from anywhere else — they are `const`
 *  and touch nothing but `nif`. Lifted here so the whole-file collision check can
 *  walk the same shapes the panel does; the panel's own methods forward to them,
 *  so there is one implementation and not a call site changed.
 */
static QModelIndex tlCollBlockIndex( const NifModel * nif, int block )
{
	return nif && nif->isValidBlockNumber( block ) ? nif->getBlockIndex( block ) : QModelIndex();
}

//! The first shape that is not a wrapper: descends Mopp/Transform/List chains.
static int tlCollFirstLeafShape( const NifModel * nif, int shape, int depth = 0 )
{
	if ( !nif || !nif->isValidBlockNumber( shape ) || depth > 12 )
		return -1;
	QModelIndex i = tlCollBlockIndex( nif, shape );
	QString t = nif->itemName( i );
	if ( t == QLatin1String( "bhkMoppBvTreeShape" )
		 || t == QLatin1String( "bhkTransformShape" )
		 || t == QLatin1String( "bhkConvexTransformShape" ) )
		return tlCollFirstLeafShape( nif, nif->getLink( i, "Shape" ), depth + 1 );
	if ( t == QLatin1String( "bhkListShape" ) || t == QLatin1String( "bhkConvexListShape" ) ) {
		QVector<qint32> links = nif->getLinkArray( i, "Sub Shapes" );
		return links.isEmpty() ? shape : tlCollFirstLeafShape( nif, links.first(), depth + 1 );
	}
	return shape;
}

/*! Which rigid body holds this shape, and whether through a list.
 *
 *  A shape hangs off a body either as its `Shape` outright or as one entry of a
 *  `bhkListShape`/`bhkConvexListShape` that the body's Shape points at. Both are
 *  ordinary arrangements, and taking a shape out has to know which it is.
 */
qint32 tlBodyHoldingShape( const NifModel * nif, qint32 shape, qint32 * throughList )
{
	if ( throughList )
		*throughList = -1;
	if ( !nif || !nif->isValidBlockNumber( shape ) )
		return -1;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex body = nif->getBlockIndex( b );
		if ( !nif->blockInherits( body, "bhkRigidBody" ) )
			continue;
		const qint32 held = nif->getLink( body, "Shape" );
		if ( held == shape )
			return b;
		const QModelIndex list = tlCollBlockIndex( nif, held );
		if ( list.isValid() && nif->blockInherits( list, { "bhkListShape", "bhkConvexListShape" } )
			&& nif->getLinkArray( list, "Sub Shapes" ).contains( shape ) )
		{
			if ( throughList )
				*throughList = held;
			return b;
		}
	}
	return -1;
}

//! Why this shape cannot go into that body, or empty if it can.
QString tlMoveCollisionShapeRefusal( const NifModel * nif, qint32 shape, qint32 toBody )
{
	if ( !nif )
		return QObject::tr( "No file is open." );
	const QModelIndex iShape = tlCollBlockIndex( nif, shape );
	const QModelIndex iBody = tlCollBlockIndex( nif, toBody );
	if ( !iShape.isValid() || !nif->blockInherits( iShape, "bhkShape" ) )
		return QObject::tr( "That row is not a collision shape." );
	if ( !iBody.isValid() || !nif->blockInherits( iBody, "bhkRigidBody" ) )
		return QObject::tr( "%1 cannot hold a shape — only a rigid body can." )
			.arg( nif->itemName( iBody.isValid() ? iBody : iShape ) );
	if ( tlBodyHoldingShape( nif, shape, nullptr ) == toBody )
		return QObject::tr( "Already in that body." );
	/* A SHAPE CANNOT GO INSIDE ITSELF. The list a body holds is a shape too, so
	 * dragging a bhkListShape onto the body it already serves would ask it to
	 * become one of its own sub-shapes — and the same for anything under it.
	 */
	QVector<qint32> stack{ shape };
	QSet<qint32> seen;
	while ( !stack.isEmpty() ) {
		const qint32 s = stack.takeLast();
		if ( s < 0 || seen.contains( s ) )
			continue;
		seen.insert( s );
		const QModelIndex i = tlCollBlockIndex( nif, s );
		for ( const qint32 sub : nif->getLinkArray( i, "Sub Shapes" ) )
			stack.append( sub );
		stack.append( nif->getLink( i, "Shape" ) );
	}
	if ( seen.contains( nif->getLink( iBody, "Shape" ) ) )
		return QObject::tr( "That would put the shape inside itself." );
	return QString();
}

/*! Move a collision shape out of whatever body holds it and into another.
 *
 *  The Collision Manager lists bodies with their shapes under them, and until
 *  now the only way to get a shape from one body to another was to delete it and
 *  build another somewhere else. Dragging the row is the obvious gesture, and
 *  this is the operation under it — shared, so the drag and whatever else wants
 *  it cannot grow two ideas of what moving a shape means.
 *
 *  Both ends handle a list. Taken out of a `bhkListShape` the entry is removed
 *  and a list left holding one shape is left as a list: that is legal, and
 *  unwrapping it would be a second edit nobody asked for. Put into a body that
 *  already has a shape, a list is made if there is not one, which is exactly
 *  what Create does with Replace off.
 *
 *  \return the empty string on success, or why not.
 */
QString tlMoveCollisionShape( NifModel * nif, qint32 shape, qint32 toBody )
{
	const QString refusal = tlMoveCollisionShapeRefusal( nif, shape, toBody );
	if ( !refusal.isEmpty() )
		return refusal;

	qint32 throughList = -1;
	const qint32 fromBody = tlBodyHoldingShape( nif, shape, &throughList );

	// OUT of the old owner first: a shape briefly in both is a shape the undo
	// step would have to know about twice
	if ( fromBody >= 0 ) {
		if ( throughList >= 0 ) {
			const QModelIndex list = nif->getBlockIndex( throughList );
			QVector<qint32> kept;
			for ( const qint32 sub : nif->getLinkArray( list, "Sub Shapes" ) )
				if ( sub != shape )
					kept.append( sub );
			nif->set<uint>( list, "Num Sub Shapes", uint( kept.size() ) );
			nif->updateArraySize( list, "Sub Shapes" );
			nif->setLinkArray( list, "Sub Shapes", kept );
			if ( nif->getIndex( list, "Filters" ).isValid() ) {
				nif->set<uint>( list, "Num Filters", uint( kept.size() ) );
				nif->updateArraySize( list, "Filters" );
			}
		} else {
			nif->setLink( nif->getBlockIndex( fromBody ), "Shape", -1 );
		}
	}

	// and INTO the new one, beside whatever is already there
	const QModelIndex body = nif->getBlockIndex( toBody );
	const qint32 held = nif->getLink( body, "Shape" );
	const QModelIndex heldIdx = tlCollBlockIndex( nif, held );
	if ( !heldIdx.isValid() ) {
		nif->setLink( body, "Shape", shape );
		return QString();
	}
	QModelIndex list = heldIdx;
	QVector<qint32> shapes;
	if ( nif->blockInherits( heldIdx, { "bhkListShape", "bhkConvexListShape" } ) ) {
		shapes = nif->getLinkArray( heldIdx, "Sub Shapes" );
	} else {
		shapes.append( held );
		list = nif->insertNiBlock( QStringLiteral( "bhkListShape" ) );
		nif->setLink( body, "Shape", nif->getBlockNumber( list ) );
	}
	shapes.append( shape );
	nif->set<uint>( list, "Num Sub Shapes", uint( shapes.size() ) );
	nif->updateArraySize( list, "Sub Shapes" );
	nif->setLinkArray( list, "Sub Shapes", shapes );
	if ( nif->getIndex( list, "Filters" ).isValid() ) {
		nif->set<uint>( list, "Num Filters", uint( shapes.size() ) );
		nif->updateArraySize( list, "Filters" );
	}
	return QString();
}

/* Hoisted out of CollisionManagerPanel so Compile Collision can be a spell.
 *
 * Same reason tlCollBlockIndex above was hoisted: being private members of a
 * dock was the only thing keeping this code unreachable from anywhere else,
 * and unreachable meant untestable. compileSelectedCollision wrote layer
 * Static/0/0 into every packfile it produced for as long as it did because
 * nothing outside a running dock could execute it.
 *
 * Every one of these was already const and read only `nif`, so the move is
 * mechanical; the panel keeps one-line forwarders exactly as it already does
 * for blockIndex and firstLeafShape.
 */

QString tlCollBoneRole( const NifModel * nif, const QHash<int, QString> & boneRoles, int node, int systemBlock )
{
	const QString fromSkin = boneRoles.value( node );
	if ( !fromSkin.isEmpty() )
		return fromSkin;
	if ( nif->isNiBlock( tlCollBlockIndex( nif, systemBlock ), "bhkRagdollSystem" ) )
		return QObject::tr( "ragdoll bone" );
	return QString();
}

/*! node block -> bone role, for every node some skin actually names.
 *
 * Built once per rebuild() and passed down: calling skeletonAnalyse() per row
 * would re-walk every vertex weight in the file once for each collision body.
 */
QHash<int, QString> tlCollBoneRoles( const NifModel * nif )
{
	QHash<int, QString> roles;
	const SkeletonReport report = skeletonAnalyse( nif );
	for ( const SkeletonBoneInfo & b : report.bones ) {
		if ( b.isNotABone() )
			continue;
		roles.insert( b.block, b.verts > 0
			? QObject::tr( "deforming (%1 v)" ).arg( b.verts )
			: QObject::tr( "unused bone" ) );
	}
	return roles;
}

QString tlCollNodeName( const NifModel * nif, int block )
{
	QModelIndex i = tlCollBlockIndex( nif, block );
	if ( !i.isValid() )
		return QObject::tr( "<orphan>" );
	QString n = nif->get<QString>( i, "Name" );
	return n.isEmpty() ? QObject::tr( "Block %1" ).arg( block ) : n;
}

struct CollisionMesh
{
	QVector<Vector3> verts;
	QVector<Triangle> tris;
	/*! Havok material CRC per triangle, parallel to `tris`.
	 *
	 * A body assembled from several shapes carries several materials, and
	 * Compile took the first leaf's for the whole packfile until 2026-08-20.
	 * The compiled format keeps a material per primitive, so the gatherer has
	 * to record which leaf each triangle came from.
	 */
	QVector<quint32> triMaterial;
};

void appendMesh( CollisionMesh & out, const QVector<Vector3> & verts,
	const QVector<Triangle> & tris, const Matrix4 & transform, float scale,
	quint32 material = 0 )
{
	if ( verts.isEmpty() || tris.isEmpty() || out.verts.size() + verts.size() > 65535 ) return;
	quint16 base = quint16( out.verts.size() );
	for ( const Vector3 & v : verts ) out.verts.append( transform * v * scale );
	for ( Triangle t : tris ) {
		if ( t[0] >= verts.size() || t[1] >= verts.size() || t[2] >= verts.size() ) continue;
		t[0] += base; t[1] += base; t[2] += base; out.tris.append( t );
		out.triMaterial.append( material );
	}
}

QVector<Triangle> boxTriangles()
{
	return {
		{ 0, 2, 1 }, { 1, 2, 3 }, { 4, 5, 6 }, { 5, 7, 6 },
		{ 0, 1, 4 }, { 1, 5, 4 }, { 2, 6, 3 }, { 3, 6, 7 },
		{ 0, 4, 2 }, { 2, 4, 6 }, { 1, 3, 5 }, { 3, 7, 5 }
	};
}

void appendSphereMesh( CollisionMesh & out, const Vector3 & center, float radius,
	const Matrix4 & transform, float scale, quint32 material = 0 )
{
	constexpr int slices = 16, stacks = 8;
	constexpr float pi = 3.14159265358979323846f;
	QVector<Vector3> verts;
	QVector<Triangle> tris;
	for ( int y = 0; y <= stacks; y++ ) {
		float phi = -0.5f * pi + pi * float( y ) / float( stacks );
		float cp = std::cos( phi ), sp = std::sin( phi );
		for ( int x = 0; x < slices; x++ ) {
			float a = 2.0f * pi * float( x ) / float( slices );
			verts.append( center + Vector3( std::cos( a ) * cp, std::sin( a ) * cp, sp ) * radius );
		}
	}
	for ( int y = 0; y < stacks; y++ ) for ( int x = 0; x < slices; x++ ) {
		quint16 a = quint16( y * slices + x ), b = quint16( y * slices + ( x + 1 ) % slices );
		quint16 c = quint16( ( y + 1 ) * slices + x ), d = quint16( ( y + 1 ) * slices + ( x + 1 ) % slices );
		tris.append( Triangle( a, b, c ) ); tris.append( Triangle( b, d, c ) );
	}
	appendMesh( out, verts, tris, transform, scale, material );
}

void appendCapsuleMesh( CollisionMesh & out, Vector3 a, Vector3 b, float radius,
	const Matrix4 & transform, float scale, quint32 material = 0 )
{
	constexpr int slices = 16, hemi = 4;
	constexpr float pi = 3.14159265358979323846f;
	Vector3 axis = b - a;
	if ( axis.length() < 1.0e-6f ) { appendSphereMesh( out, a, radius, transform, scale, material ); return; }
	axis.normalize();
	Vector3 u = Vector3::crossproduct( axis, std::fabs( axis[2] ) < 0.9f ? Vector3( 0, 0, 1 ) : Vector3( 0, 1, 0 ) );
	u.normalize(); Vector3 v = Vector3::crossproduct( axis, u ); v.normalize();
	QVector<QPair<Vector3, float>> rings;
	for ( int i = 0; i <= hemi; i++ ) {
		float t = -0.5f * pi + 0.5f * pi * float( i ) / float( hemi );
		rings.append( { a + axis * ( std::sin( t ) * radius ), std::cos( t ) * radius } );
	}
	rings.append( { b, radius } );
	for ( int i = 1; i <= hemi; i++ ) {
		float t = 0.5f * pi * float( i ) / float( hemi );
		rings.append( { b + axis * ( std::sin( t ) * radius ), std::cos( t ) * radius } );
	}
	QVector<Vector3> verts; QVector<Triangle> tris;
	for ( const auto & ring : rings ) for ( int s = 0; s < slices; s++ ) {
		float ang = 2.0f * pi * float( s ) / float( slices );
		verts.append( ring.first + ( u * std::cos( ang ) + v * std::sin( ang ) ) * ring.second );
	}
	for ( int r = 0; r + 1 < rings.size(); r++ ) for ( int s = 0; s < slices; s++ ) {
		quint16 p0 = quint16( r * slices + s ), p1 = quint16( r * slices + ( s + 1 ) % slices );
		quint16 p2 = quint16( ( r + 1 ) * slices + s ), p3 = quint16( ( r + 1 ) * slices + ( s + 1 ) % slices );
		tris.append( Triangle( p0, p1, p2 ) ); tris.append( Triangle( p1, p3, p2 ) );
	}
	appendMesh( out, verts, tris, transform, scale, material );
}

/*! Triangulate a convex shape from its own stored face planes.
 *
 *  A decompiled bhkConvexVerticesShape carries the polytope's planes verbatim
 *  in "Normals" (n.xyz, w = d; n·v + d = 0 on the face), so the faces need no
 *  hull search: each plane's incident vertices, ordered around the face, fanned.
 *  Deterministic, and immune to the two ways qhull failed here -- refusing
 *  near-degenerate prop-scale hulls outright ("wide merge for pinched facet",
 *  8%% of a 100-file A/B), and once looping without returning
 *  (50sTargetPractice03).
 *
 *  Returns empty when the planes do not describe a closed surface over the
 *  vertices (no normals, or fewer than four usable faces) -- the caller falls
 *  back to qhull.
 */
QVector<Triangle> tlCollTriangulateConvexPlanes( const QVector<Vector3> & verts,
	const QVector<Vector4> & planes )
{
	QVector<Triangle> tris;
	if ( verts.size() < 4 || planes.size() < 4 )
		return tris;
	int faces = 0;
	for ( const Vector4 & p : planes ) {
		const Vector3 n( p[0], p[1], p[2] );
		if ( n.length() < 0.5f )
			continue;   // Elric's zero-normal padding planes
		QVector<int> on;
		for ( int i = 0; i < verts.size(); i++ ) {
			if ( std::fabs( Vector3::dotproduct( n, verts.at( i ) ) + p[3] ) < 5.0e-4f )
				on.append( i );
		}
		/* A plane with under three incident vertices has no face area to
		 * contribute -- vanilla hulls carry sliver planes (50sTargetPractice03's
		 * wedge stores 7 planes over 6 distinct vertices), and bailing out here
		 * used to hand exactly those shapes to the qhull fallback, which spins
		 * forever on their duplicate points. Skip the plane, keep the shape.
		 */
		if ( on.size() < 3 )
			continue;
		faces++;
		// order the face's vertices by angle about its own centroid
		Vector3 centre;
		for ( int i : on ) centre += verts.at( i );
		centre /= float( on.size() );
		Vector3 u = verts.at( on.first() ) - centre;
		u = ( u - n * Vector3::dotproduct( n, u ) );
		if ( u.length() < 1.0e-9f )
			return QVector<Triangle>();
		u.normalize();
		const Vector3 w = Vector3::crossproduct( n, u );
		std::sort( on.begin(), on.end(), [&]( int a, int b ) {
			const Vector3 da = verts.at( a ) - centre, db = verts.at( b ) - centre;
			return std::atan2( Vector3::dotproduct( w, da ), Vector3::dotproduct( u, da ) )
				 < std::atan2( Vector3::dotproduct( w, db ), Vector3::dotproduct( u, db ) );
		} );
		for ( int k = 1; k + 1 < on.size(); k++ ) {
			// wind the fan so the triangle faces the plane normal
			const Vector3 e1 = verts.at( on.at( k ) ) - verts.at( on.first() );
			const Vector3 e2 = verts.at( on.at( k + 1 ) ) - verts.at( on.first() );
			if ( Vector3::dotproduct( Vector3::crossproduct( e1, e2 ), n ) >= 0.0f )
				tris.append( Triangle( on.first(), on.at( k ), on.at( k + 1 ) ) );
			else
				tris.append( Triangle( on.first(), on.at( k + 1 ), on.at( k ) ) );
		}
	}
	if ( faces < 4 )
		return QVector<Triangle>();
	return tris;
}

void tlCollAppendEditableMesh( const NifModel * nif, int shapeBlock, CollisionMesh & out,
	const Matrix4 & transform = Matrix4(), int depth = 0 )
{
	if ( !nif->isValidBlockNumber( shapeBlock ) || depth > 20 ) return;
	QModelIndex shape = tlCollBlockIndex( nif, shapeBlock );
	QString type = nif->itemName( shape );
	if ( type.endsWith( QLatin1String( "ListShape" ) ) ) {
		for ( qint32 child : nif->getLinkArray( shape, "Sub Shapes" ) ) tlCollAppendEditableMesh( nif, child, out, transform, depth + 1 );
		return;
	}
	if ( type == QLatin1String( "bhkMoppBvTreeShape" ) ) {
		tlCollAppendEditableMesh( nif, nif->getLink( shape, "Shape" ), out, transform, depth + 1 ); return;
	}
	if ( type == QLatin1String( "bhkTransformShape" ) || type == QLatin1String( "bhkConvexTransformShape" ) ) {
		Matrix4 next( transform ); next.multiply4x3( nif->get<Matrix4>( shape, "Transform" ) );
		tlCollAppendEditableMesh( nif, nif->getLink( shape, "Shape" ), out, next, depth + 1 ); return;
	}
	/* Each LEAF's own Material, carried per triangle. The wrappers above (list,
	 * MOPP, transform) hold a Material field too, but it is the leaves that the
	 * compiled run table is built from -- and taking the first leaf's for all of
	 * them is exactly the flattening this replaces.
	 */
	const quint32 material = nif->get<quint32>( shape, "Material" );
	if ( type == QLatin1String( "bhkBoxShape" ) ) {
		Vector3 d = nif->get<Vector3>( shape, "Dimensions" ); QVector<Vector3> v;
		for ( int z = -1; z <= 1; z += 2 ) for ( int y = -1; y <= 1; y += 2 ) for ( int x = -1; x <= 1; x += 2 )
			v.append( Vector3( d[0] * x, d[1] * y, d[2] * z ) );
		appendMesh( out, v, boxTriangles(), transform, 69.99125f, material ); return;
	}
	if ( type == QLatin1String( "bhkSphereShape" ) ) {
		appendSphereMesh( out, Vector3(), nif->get<float>( shape, "Radius" ), transform, 69.99125f, material ); return;
	}
	if ( type == QLatin1String( "bhkCapsuleShape" ) ) {
		appendCapsuleMesh( out, nif->get<Vector3>( shape, "First Point" ), nif->get<Vector3>( shape, "Second Point" ),
			nif->get<float>( shape, "Radius" ), transform, 69.99125f, material ); return;
	}
	if ( type == QLatin1String( "bhkConvexVerticesShape" ) ) {
		/* Duplicate positions first. Vanilla hulls carry them (the target
		 * practice wedge stores one corner three times), and they poison
		 * everything downstream: the plane fans emit zero-area slivers over
		 * them, and qhull's facet merge SPINS FOREVER on coincident points --
		 * that was a compile that burned 15 CPU-minutes without returning.
		 */
		QVector<Vector4> vv = nif->getArray<Vector4>( shape, "Vertices" ); QVector<Vector3> v;
		for ( const Vector4 & p : vv ) {
			const Vector3 q( p[0], p[1], p[2] );
			bool dup = false;
			for ( const Vector3 & have : std::as_const( v ) )
				dup = dup || ( have - q ).length() < 1.0e-6f;
			if ( !dup )
				v.append( q );
		}
		QVector<Triangle> tris = tlCollTriangulateConvexPlanes( v, nif->getArray<Vector4>( shape, "Normals" ) );
		if ( tris.isEmpty() ) {
			/* No usable planes: hull the cloud. compute_convex_hull's triangles
			 * index the ORIGINAL point array (qh_pointid) -- its hullVerts output
			 * is a per-facet duplicated list in facet order. This used to pass
			 * hullVerts as the vertex array, so every triangle referenced the
			 * wrong vertices: a 100-file A/B measured 4 degenerate triangles and
			 * a garbage hull on EVERY box-shaped polytope that reached qhull.
			 */
			QVector<Vector4> hullVerts, hullNorms;
			tris = compute_convex_hull( v, hullVerts, hullNorms );
		}
		appendMesh( out, v, tris, transform, 69.99125f, material ); return;
	}
	if ( type == QLatin1String( "bhkNiTriStripsShape" ) ) {
		for ( qint32 dataBlock : nif->getLinkArray( shape, "Strips Data" ) ) {
			QModelIndex data = tlCollBlockIndex( nif, dataBlock ); QVector<Vector3> v = nif->getArray<Vector3>( data, "Vertices" );
			QVector<QVector<quint16>> strips; QModelIndex points = nif->getIndex( data, "Points" );
			for ( int r = 0; r < nif->rowCount( points ); r++ ) strips.append( nif->getArray<quint16>( nif->getIndex( points, r ) ) );
			appendMesh( out, v, triangulate( strips ), transform, 1.0f, material );
		}
	}
}

/*! Why the shapes in \a body cannot be merged into one, or the empty string.
 *
 *  Separate from the doing, so the menu can grey the entry and say why in a
 *  tooltip rather than offering something that fails when it is clicked.
 */
QString tlMergeBodyShapesRefusal( const NifModel * nif, qint32 body )
{
	if ( !nif )
		return QObject::tr( "No file is open." );
	const QModelIndex iBody = tlCollBlockIndex( nif, body );
	if ( !iBody.isValid() || !nif->blockInherits( iBody, "bhkRigidBody" ) )
		return QObject::tr( "Only a rigid body holds shapes to merge." );
	const QModelIndex held = tlCollBlockIndex( nif, nif->getLink( iBody, "Shape" ) );
	if ( !held.isValid() || !nif->blockInherits( held, { "bhkListShape", "bhkConvexListShape" } )
		|| nif->getLinkArray( held, "Sub Shapes" ).size() < 2 )
		return QObject::tr( "That body holds one shape; there is nothing to merge it with." );
	return QString();
}

/*! Is every leaf under \a shapeBlock a convex primitive?
 *
 *  Elric picks the compiled shape class off the SOURCE, not off the motion type.
 *  Measured over 1,500 SetDressing files: 228 static systems are polytope-only
 *  and 733 are compressed-mesh-only, so a convex source stays convex whether or
 *  not the body simulates. Wrappers — list, MOPP, transform — are transparent,
 *  and one triangle source anywhere makes the whole body a mesh.
 */
bool tlCollConvexOnly( const NifModel * nif, int shapeBlock, int depth = 0 )
{
	if ( !nif || !nif->isValidBlockNumber( shapeBlock ) || depth > 20 )
		return false;
	const QModelIndex shape = tlCollBlockIndex( nif, shapeBlock );
	const QString type = nif->itemName( shape );
	if ( type.endsWith( QLatin1String( "ListShape" ) ) ) {
		const QVector<qint32> subs = nif->getLinkArray( shape, "Sub Shapes" );
		if ( subs.isEmpty() )
			return false;
		for ( qint32 child : subs ) {
			if ( !tlCollConvexOnly( nif, child, depth + 1 ) )
				return false;
		}
		return true;
	}
	if ( type == QLatin1String( "bhkMoppBvTreeShape" ) || type == QLatin1String( "bhkTransformShape" )
		 || type == QLatin1String( "bhkConvexTransformShape" ) )
		return tlCollConvexOnly( nif, nif->getLink( shape, "Shape" ), depth + 1 );
	return type == QLatin1String( "bhkBoxShape" ) || type == QLatin1String( "bhkSphereShape" )
		|| type == QLatin1String( "bhkCapsuleShape" ) || type == QLatin1String( "bhkConvexVerticesShape" );
}

/*! Face LOOPS of a convex hull, from the shape's own stored planes.
 *
 *  The same walk as tlCollTriangulateConvexPlanes — each plane's incident
 *  vertices ordered around the face — but kept as loops, which is the form an
 *  hknpConvexPolytopeShape stores. \a keptPlanes comes back parallel to the
 *  loops, with Elric's zero-normal padding planes and the sliver planes (under
 *  three incident vertices) dropped, since a polytope face needs three.
 *
 *  False when the planes do not describe a closed surface, which is the caller's
 *  cue to fall back to the mesh path rather than write a broken hull.
 */
bool tlCollHullLoops( const QVector<Vector3> & verts, const QVector<Vector4> & planes,
	QVector<QVector<int>> & loops, QVector<Vector4> & keptPlanes )
{
	loops.clear(); keptPlanes.clear();
	if ( verts.size() < 4 || planes.size() < 4 )
		return false;
	for ( const Vector4 & p : planes ) {
		const Vector3 n( p[0], p[1], p[2] );
		if ( n.length() < 0.5f )
			continue;
		QVector<int> on;
		for ( int i = 0; i < verts.size(); i++ ) {
			if ( std::fabs( Vector3::dotproduct( n, verts.at( i ) ) + p[3] ) < 5.0e-4f )
				on.append( i );
		}
		if ( on.size() < 3 )
			continue;
		Vector3 centre;
		for ( int i : on ) centre += verts.at( i );
		centre /= float( on.size() );
		Vector3 u = verts.at( on.first() ) - centre;
		u = u - n * Vector3::dotproduct( n, u );
		if ( u.length() < 1.0e-9f )
			return false;
		u.normalize();
		const Vector3 w = Vector3::crossproduct( n, u );
		std::sort( on.begin(), on.end(), [&]( int a, int b ) {
			const Vector3 da = verts.at( a ) - centre, db = verts.at( b ) - centre;
			return std::atan2( Vector3::dotproduct( w, da ), Vector3::dotproduct( u, da ) )
				 < std::atan2( Vector3::dotproduct( w, db ), Vector3::dotproduct( u, db ) );
		} );
		// wind the loop so it turns the way the plane's own normal points
		const Vector3 e1 = verts.at( on.at( 1 ) ) - verts.at( on.first() );
		const Vector3 e2 = verts.at( on.at( 2 ) ) - verts.at( on.first() );
		if ( Vector3::dotproduct( Vector3::crossproduct( e1, e2 ), n ) < 0.0f )
			std::reverse( on.begin() + 1, on.end() );
		loops.append( on ); keptPlanes.append( p );
	}
	return loops.size() >= 4;
}

/*! The mass properties an hknpConvexPolytopeShape carries, measured off vanilla.
 *
 *  The solid Havok describes is the hull GROWN by its convex radius, not the
 *  hull, and all three numbers follow from that:
 *
 *  - **Volume** is the Minkowski sum with a ball of radius r:
 *    `V + A·r + ½·Σ(edge length × exterior dihedral)·r² + 4/3·π·r³`. Within 2%
 *    of the stored figure on 271 of 299 vanilla polytopes; for a box it reduces
 *    to the expanded box, where 209 of 216 agree. The bare hull is nowhere near
 *    — PlankHinge02 stores 0.295 against a hull of 0.021.
 *  - **Mass** equals that volume. Every vanilla shape is density 1.
 *  - **Inertia** is that solid's, approximated by scaling the hull about its
 *    centre until its bounding box grows by r on every side. Exact for a box,
 *    and within 15% of vanilla on 255 of 268 polytopes where the plain hull
 *    manages 170. Stored at 1.5× the physical value (see HknpShape).
 */
void tlCollHullMassProperties( const QVector<Vector3> & verts, const QVector<QVector<int>> & loops,
	float radius, float * volume, Vector3 * com, Vector3 * inertiaRaw )
{
	*volume = 0.0f; *com = Vector3(); *inertiaRaw = Vector3();
	if ( loops.isEmpty() )
		return;

	QVector<Vector3> normals( loops.size() );
	double area = 0.0, edgeTerm = 0.0, hullVolume = 0.0;
	Vector3 inside;
	int count = 0;
	for ( const QVector<int> & loop : loops )
		for ( int i : loop ) { inside += verts.at( i ); count++; }
	if ( count )
		inside /= float( count );

	for ( qsizetype f = 0; f < loops.size(); f++ ) {
		const QVector<int> & loop = loops.at( f );
		Vector3 sum;
		for ( int k = 1; k + 1 < loop.size(); k++ )
			sum += Vector3::crossproduct( verts.at( loop.at( k ) ) - verts.at( loop.first() ),
										  verts.at( loop.at( k + 1 ) ) - verts.at( loop.first() ) );
		const float twice = sum.length();
		if ( twice < 1.0e-12f ) { normals[f] = Vector3(); continue; }
		normals[f] = sum / twice;
		area += double( twice ) / 2.0;
		hullVolume += double( twice ) / 2.0
			* std::fabs( Vector3::dotproduct( normals[f], verts.at( loop.first() ) - inside ) ) / 3.0;
	}
	// the r² term wants each edge once, with the angle between the two faces on it
	QHash<quint64, QPair<int, int>> edgeFaces;
	for ( qsizetype f = 0; f < loops.size(); f++ ) {
		const QVector<int> & loop = loops.at( f );
		for ( int k = 0; k < loop.size(); k++ ) {
			const int a = std::min( loop.at( k ), loop.at( ( k + 1 ) % loop.size() ) );
			const int b = std::max( loop.at( k ), loop.at( ( k + 1 ) % loop.size() ) );
			const quint64 key = ( quint64( quint32( a ) ) << 32 ) | quint32( b );
			auto it = edgeFaces.find( key );
			if ( it == edgeFaces.end() )
				edgeFaces.insert( key, { int( f ), -1 } );
			else if ( it->second < 0 )
				it->second = int( f );
		}
	}
	for ( auto it = edgeFaces.constBegin(); it != edgeFaces.constEnd(); ++it ) {
		if ( it->second < 0 )
			continue;
		const int a = int( it.key() >> 32 ), b = int( it.key() & 0xffffffffu );
		const float len = ( verts.at( a ) - verts.at( b ) ).length();
		const float dot = std::clamp( Vector3::dotproduct( normals.at( it->first ), normals.at( it->second ) ), -1.0f, 1.0f );
		edgeTerm += double( len ) * std::acos( dot );
	}
	const double r = radius;
	*volume = float( hullVolume + area * r + 0.5 * edgeTerm * r * r + 4.0 / 3.0 * M_PI * r * r * r );

	// the grown hull: scale about the box centre so every side gains r
	QVector<int> used;
	for ( const QVector<int> & loop : loops )
		for ( int i : loop ) used.append( i );
	Vector3 lo = verts.at( used.first() ), hi = lo;
	for ( int i : used )
		for ( int k = 0; k < 3; k++ ) { lo[k] = std::min( lo[k], verts.at( i )[k] ); hi[k] = std::max( hi[k], verts.at( i )[k] ); }
	Vector3 centre = ( lo + hi ) / 2.0f, scale;
	for ( int k = 0; k < 3; k++ ) {
		const float half = ( hi[k] - lo[k] ) / 2.0f;
		scale[k] = half > 1.0e-9f ? ( half + radius ) / half : 1.0f;
	}
	QVector<Vector3> grown( verts.size() );
	for ( qsizetype i = 0; i < verts.size(); i++ )
		for ( int k = 0; k < 3; k++ )
			grown[i][k] = centre[k] + ( verts.at( i )[k] - centre[k] ) * scale[k];

	/* Tetrahedra from an interior point, summed: the standard polyhedron inertia.
	 * Density comes from the Minkowski volume above rather than the grown hull's
	 * own, so the mass the tensor is built at is the mass actually stored.
	 */
	double tetVolume = 0.0;
	Vector3 centroid;
	struct Tet { double vol; Vector3 a, b, c; };
	QVector<Tet> tets;
	for ( const QVector<int> & loop : loops ) {
		for ( int k = 1; k + 1 < loop.size(); k++ ) {
			const Vector3 a = grown.at( loop.first() ) - inside;
			const Vector3 b = grown.at( loop.at( k ) ) - inside;
			const Vector3 c = grown.at( loop.at( k + 1 ) ) - inside;
			const double d = std::fabs( Vector3::dotproduct( a, Vector3::crossproduct( b, c ) ) ) / 6.0;
			tets.append( { d, a, b, c } );
			tetVolume += d;
			centroid += ( a + b + c ) * float( d / 4.0 );
		}
	}
	if ( tetVolume < 1.0e-12 )
		return;
	centroid /= float( tetVolume );
	const double density = double( *volume ) / tetVolume;
	Vector3 diag;
	for ( const Tet & t : std::as_const( tets ) ) {
		const double m = t.vol * density;
		for ( int axis = 0; axis < 3; axis++ ) {
			const int u = ( axis + 1 ) % 3, v = ( axis + 2 ) % 3;
			double s = 0.0;
			const Vector3 q[3] = { t.a, t.b, t.c };
			for ( int i = 0; i < 3; i++ )
				s += double( q[i][u] ) * q[i][u] + double( q[i][v] ) * q[i][v];
			for ( int i = 0; i < 3; i++ ) {
				const Vector3 & q1 = q[i], & q2 = q[( i + 1 ) % 3];
				s += double( q1[u] ) * q2[u] + double( q1[v] ) * q2[v];
			}
			diag[axis] += float( m * s / 10.0 );
		}
	}
	const float mass = *volume;
	diag[0] -= mass * ( centroid[1] * centroid[1] + centroid[2] * centroid[2] );
	diag[1] -= mass * ( centroid[0] * centroid[0] + centroid[2] * centroid[2] );
	diag[2] -= mass * ( centroid[0] * centroid[0] + centroid[1] * centroid[1] );
	*com = inside + centroid;
	*inertiaRaw = diag * 1.5f;   // Havok stores 1.5x the physical tensor
}

/*! The major-axis frame a synthesized shape carries.
 *
 *  Its packing is not decoded (HknpShape::massMajorAxis), so it cannot be
 *  derived — but it does not have to be invented either: 1,002 of 1,304 vanilla
 *  mass-property objects hold exactly this value (76.8%, and the next most
 *  common appears seven times), so a shape we write takes the one vanilla writes
 *  three times out of four.
 */
constexpr quint64 tlCollMajorAxisDefault = 0xf530800080008000ull;

/*! Turn one editable convex leaf into the hknp shape Elric would have written.
 *
 *  \a transform is baked into the geometry rather than carried, because a
 *  standalone hknp shape has nowhere to put one — vanilla's transforms live in a
 *  compound's instances, and this is the single-shape path.
 *
 *  False if the block is not a convex leaf, or if its hull does not close.
 */
bool tlCollConvexLeafShape( const NifModel * nif, int shapeBlock, const Matrix4 & transform,
	HknpShape & out )
{
	const QModelIndex shape = tlCollBlockIndex( nif, shapeBlock );
	if ( !shape.isValid() )
		return false;
	const QString type = nif->itemName( shape );
	const quint32 material = nif->get<quint32>( shape, "Material" );
	out = HknpShape();
	out.materialCRC = out.shapeMaterialCRC = material;

	if ( type == QLatin1String( "bhkSphereShape" ) ) {
		out.className = QStringLiteral( "hknpSphereShape" );
		out.primType = 1;
		out.primRadius = nif->get<float>( shape, "Radius" );
		out.capA = out.capB = transform * Vector3();
		out.convexRadius = out.primRadius;
		out.isConvex = true;
		return out.primRadius > 0.0f;
	}
	if ( type == QLatin1String( "bhkCapsuleShape" ) ) {
		out.className = QStringLiteral( "hknpCapsuleShape" );
		out.primType = 2;
		out.primRadius = nif->get<float>( shape, "Radius" );
		out.capA = transform * nif->get<Vector3>( shape, "First Point" );
		out.capB = transform * nif->get<Vector3>( shape, "Second Point" );
		/* Elric's conversion, measured on the oracle: the stored core radius is
		 * the NIF's radius shrunk by 1% twice, and the core box is then padded by
		 * core/99, which puts the solid's face at exactly 0.99 * R. Writing R
		 * straight into +0x14 -- as this did -- inflates every capsule by 1.43%
		 * per decompile/recompile cycle. See hknpdecode.cpp, which inverts it.
		 * A SPHERE takes no such treatment: Elric stores its NIF radius verbatim,
		 * checked on smallsandbagpile01 and unchanged above.
		 */
		out.convexRadius = out.primRadius * 0.9801f;
		out.isConvex = true;
		return out.primRadius > 0.0f;
	}

	QVector<Vector3> verts;
	QVector<Vector4> planes;
	if ( type == QLatin1String( "bhkBoxShape" ) ) {
		const Vector3 d = nif->get<Vector3>( shape, "Dimensions" );
		if ( d[0] <= 0.0f || d[1] <= 0.0f || d[2] <= 0.0f )
			return false;
		for ( int z = -1; z <= 1; z += 2 ) for ( int y = -1; y <= 1; y += 2 ) for ( int x = -1; x <= 1; x += 2 )
			verts.append( Vector3( d[0] * x, d[1] * y, d[2] * z ) );
		for ( int axis = 0; axis < 3; axis++ ) for ( int sign = 1; sign >= -1; sign -= 2 ) {
			Vector3 n; n[axis] = float( sign );
			planes.append( Vector4( n[0], n[1], n[2], -d[axis] ) );
		}
		out.convexRadius = nif->get<float>( shape, "Radius" );
	} else if ( type == QLatin1String( "bhkConvexVerticesShape" ) ) {
		for ( const Vector4 & v : nif->getArray<Vector4>( shape, "Vertices" ) )
			verts.append( Vector3( v[0], v[1], v[2] ) );
		planes = nif->getArray<Vector4>( shape, "Normals" );
		out.convexRadius = nif->get<float>( shape, "Radius" );
	} else {
		return false;
	}

	/* A face index is ONE BYTE in the polytope's index array, so a hull past 255
	 * vertices cannot be written -- it would wrap silently into a hull made of
	 * the wrong corners. Refuse and let the mesh path have it; nothing in the
	 * vanilla corpus comes close, the largest polytope there holding a few dozen.
	 */
	if ( verts.size() > 255 )
		return false;
	QVector<QVector<int>> loops;
	QVector<Vector4> kept;
	if ( !tlCollHullLoops( verts, planes, loops, kept ) )
		return false;

	/* The planes and the loops are in the hull's own frame; transforming the
	 * vertices means transforming the planes with them. Rotation only —
	 * tlCollAppendConvexShapes refuses a scaled wrapper, because scaling a hull
	 * moves its planes by a factor the normals cannot carry.
	 */
	Matrix4 rotation( transform );
	Vector3 offset = transform * Vector3();
	for ( Vector3 & v : verts )
		v = transform * v;
	for ( qsizetype i = 0; i < kept.size(); i++ ) {
		const Vector3 n( kept.at( i )[0], kept.at( i )[1], kept.at( i )[2] );
		Vector3 rotated = rotation * n - offset;
		rotated.normalize();
		// a point on the plane, moved with the geometry, fixes the new distance
		const Vector3 on = verts.at( loops.at( i ).first() );
		kept[i] = Vector4( rotated[0], rotated[1], rotated[2], -Vector3::dotproduct( rotated, on ) );
	}

	out.className = QStringLiteral( "hknpConvexPolytopeShape" );
	out.isConvex = true;
	out.verts = verts;
	out.planes = kept;
	out.faces = loops;
	/* Havok's per-face minHalfAngle. Every vanilla box carries 127 or 128, which
	 * is half of a 90° edge over the byte's 0..180° range, so the derivation is
	 * `angle/2 scaled by 255/180` on the face's sharpest edge. The header warns it
	 * only reproduces vanilla about two thirds of the time; it is advisory data
	 * for the solver, not geometry.
	 */
	out.faceAngles.fill( 127, loops.size() );
	for ( qsizetype f = 0; f < loops.size(); f++ ) {
		float sharpest = float( M_PI );
		const Vector3 nf( kept.at( f )[0], kept.at( f )[1], kept.at( f )[2] );
		for ( qsizetype g = 0; g < loops.size(); g++ ) {
			if ( g == f )
				continue;
			// faces that share an edge with this one
			int shared = 0;
			for ( int a : loops.at( f ) ) for ( int b : loops.at( g ) ) if ( a == b ) shared++;
			if ( shared < 2 )
				continue;
			const Vector3 ng( kept.at( g )[0], kept.at( g )[1], kept.at( g )[2] );
			sharpest = std::min( sharpest, std::acos( std::clamp( Vector3::dotproduct( nf, ng ), -1.0f, 1.0f ) ) );
		}
		const float halfDegrees = float( sharpest * 180.0 / M_PI ) / 2.0f;
		out.faceAngles[f] = quint8( std::clamp( int( std::lround( halfDegrees * 255.0f / 90.0f ) ), 0, 255 ) );
	}
	out.shapeFlags = 0x01000143u;   // 74 of 76 vanilla polytopes
	out.hasMassProps = true;
	tlCollHullMassProperties( verts, loops, out.convexRadius, &out.massVolume, &out.massCom, &out.massInertiaRaw );
	out.massMass = out.massVolume;
	out.massMajorAxis = tlCollMajorAxisDefault;
	return true;
}

/*! Every convex leaf under \a shapeBlock, as hknp shapes, transforms baked in.
 *
 *  False as soon as anything in the tree is not a convex leaf this can write, or
 *  carries a scale a hull's planes cannot follow — the caller then compiles a
 *  compressed mesh, which is what happened to every convex source before
 *  2026-08-20.
 */
bool tlCollAppendConvexShapes( const NifModel * nif, int shapeBlock, const Matrix4 & transform,
	QVector<HknpShape> & out, int depth = 0 )
{
	if ( !nif->isValidBlockNumber( shapeBlock ) || depth > 20 )
		return false;
	const QModelIndex shape = tlCollBlockIndex( nif, shapeBlock );
	const QString type = nif->itemName( shape );
	if ( type.endsWith( QLatin1String( "ListShape" ) ) ) {
		const QVector<qint32> subs = nif->getLinkArray( shape, "Sub Shapes" );
		if ( subs.isEmpty() )
			return false;
		for ( qint32 child : subs ) {
			if ( !tlCollAppendConvexShapes( nif, child, transform, out, depth + 1 ) )
				return false;
		}
		return true;
	}
	if ( type == QLatin1String( "bhkMoppBvTreeShape" ) )
		return tlCollAppendConvexShapes( nif, nif->getLink( shape, "Shape" ), transform, out, depth + 1 );
	if ( type == QLatin1String( "bhkTransformShape" ) || type == QLatin1String( "bhkConvexTransformShape" ) ) {
		const Matrix4 own = nif->get<Matrix4>( shape, "Transform" );
		Vector3 t, s; Matrix r;
		own.decompose( t, r, s );
		if ( std::fabs( s[0] - 1.0f ) > 1.0e-4f || std::fabs( s[1] - 1.0f ) > 1.0e-4f
			 || std::fabs( s[2] - 1.0f ) > 1.0e-4f )
			return false;   // a scaled hull needs its planes rescaled; mesh path instead
		Matrix4 next( transform );
		next.multiply4x3( own );
		return tlCollAppendConvexShapes( nif, nif->getLink( shape, "Shape" ), next, out, depth + 1 );
	}
	HknpShape leaf;
	if ( !tlCollConvexLeafShape( nif, shapeBlock, transform, leaf ) )
		return false;
	out.append( leaf );
	return true;
}

/*! Compile a body whose shapes are all convex, the way Elric does.
 *
 *  Returns empty — not an error — when the source is not something this path can
 *  write, so the caller falls through to the compressed mesh that every convex
 *  source used to get.
 *
 *  The physics comes off \a in, which the caller has already read out of the
 *  rigid body, so the two paths cannot disagree about layer, friction or mass.
 *  Geometry does NOT: the leaf shapes are already in Havok units, where the mesh
 *  path converts to game units and back.
 */
QByteArray tlCollCompileConvex( const NifModel * nif, int rootShape, const HknpEncodeInput & in,
	QString * error )
{
	QVector<HknpShape> shapes;
	if ( !tlCollConvexOnly( nif, rootShape ) )
		return QByteArray();
	if ( !tlCollAppendConvexShapes( nif, rootShape, Matrix4(), shapes ) || shapes.isEmpty() )
		return QByteArray();
	HknpSystem sys;
	for ( HknpShape & shape : shapes ) {
		shape.bodyId = 0;
		sys.shapes.append( shape );
	}

	/* Several convex shapes in one body are a COMPOUND, which is how vanilla
	 * carries them and what Elric writes. The instances are all identity here:
	 * tlCollAppendConvexShapes bakes each wrapper's transform into its hull, so
	 * the instance has nothing left to say.
	 */
	if ( shapes.size() > 1 ) {
		HknpCompound compound;
		/* ALWAYS the dynamic class, whatever the body does. Counted over the
		 * corpus: 71 of 71 compounds are hknpDynamicCompoundShape, and 45 of them
		 * sit in bodies that do not simulate — so hknpStaticCompoundShape is a
		 * class Elric knows and never writes, which is also why its type hash was
		 * never sampled into our table.
		 */
		compound.dynamic = true;
		compound.materialCRC = in.materialCRC;
		compound.bodyId = 0;
		/* THE WORD AT +0x10 IS A STRUCT, AND BIT 0 MEANS "I AM CONVEX".
		 *
		 * Fallout 4's own symbols say so in four instructions:
		 *
		 *   hknpShape::asConvexShape        test byte ptr [rcx+0x10], 1
		 *   hknpShape::getFlags             movzx eax, word ptr [rcx+0x10]
		 *   hknpShape::getNumShapeKeyBits   movzx eax, byte ptr [rcx+0x12]
		 *
		 * so +0x10 is u16 m_flags, +0x12 is u8 m_numShapeKeyBits, +0x13 is the
		 * dispatch type. This was the literal 0x01000001 -- flags 1, bit 0 SET --
		 * which told the engine a COMPOUND was a convex shape. It called
		 * asConvexShape, got a non-null answer, wrapped it in an
		 * hknpScaledConvexShape because the reference was scaled, and
		 * hknpScaledConvexShapeBase::calcAabb read a vertex array that was never
		 * there: access violation at 0xFFFFFFFFFFFFFFFF on TrashcanMetalOffice01,
		 * the second crash this mod found.
		 *
		 * Vanilla, measured over 155 compounds in 1,500 SetDressing files: flags
		 * 0x0004 and dispatch 2 on every single one, and m_numShapeKeyBits is
		 * exactly the BIT LENGTH OF THE CHILD COUNT -- n=2 and 3 give 2, n=4..7
		 * give 3, n=8..15 give 4, n=17..20 give 5, no exceptions. (Not
		 * ceil(log2 n): the key space holds n itself, not just 0..n-1.)
		 */
		quint32 keyBits = 0;
		for ( qsizetype v = shapes.size(); v; v >>= 1 )
			keyBits++;
		compound.shapeFlags = 0x02000004u | ( keyBits << 16 );
		for ( qsizetype k = 0; k < shapes.size(); k++ ) {
			compound.children.append( int( k ) );
			compound.instances.append( HknpCompound::Instance() );
		}
		/* The AABB is NOT computed here. It has to equal the root of the BVH, and
		 * only the encoder knows the boxes the tree was built from, so it fills
		 * both from the same numbers.
		 *
		 * Computed here it was computed from the VERTEX LISTS, and a sphere or a
		 * capsule has none -- its geometry is end points and a radius. So every
		 * such child fell out of the bound: a parking meter shipped with an AABB
		 * that stopped below its own head. Worse, a body of nothing BUT capsules
		 * bounded nothing at all, was refused here, and fell through to the mesh
		 * path -- which is why standpipe03 arrived with 14 capsules triangulated.
		 */
		sys.compounds.append( compound );
	}

	HknpBodyPhys phys;
	phys.layer = in.layer;
	phys.packedFilter = ( in.layer & 0xffu ) | ( quint32( in.filterFlags ) << 8 )
		| ( quint32( in.filterGroup ) << 16 );
	phys.hasStoredFilter = true;
	phys.filterFlags = in.filterFlags;
	phys.filterGroup = in.filterGroup;
	phys.materialCRC = in.materialCRC;
	phys.friction = in.friction;
	phys.restitution = in.restitution;
	phys.position = in.center;
	phys.orientation = in.orientation;
	// a KEYFRAMED body names a motion index like a dynamic one; what it lacks is
	// the dyn_motion record behind it. See HknpEncodeInput::keyframed.
	/* dyn_inertia +0x30 is the body position PLUS the body's own centre of mass,
	 * and a KEYFRAMED body carries one as surely as a dynamic one -- 16 of the 28
	 * vanilla keyframed records hold a real value. Computed here, outside the
	 * dynamic-only block, because leaving it inside put every door's centre of
	 * mass at its body origin, 84 to 191 game units out.
	 *
	 * Measured: vault_chairfoldingclosed02 sits at 0.040,-0.203,36.438 game units
	 * and stores 0.080,-0.406,72.875 -- the difference is the shape's own centre
	 * of mass, 0.041,-0.203,36.437, to three decimals. Across 500 SetDressing
	 * files, 99 bodies sit at the origin and every one still carries a NONZERO
	 * value here, which is what says this is a centre of mass and not the
	 * position again. Writing the position alone put the mass on the floor, so a
	 * folding chair rotated to stand itself up and railings drove through it.
	 */
	{
		Vector3 com;
		float comVolume = 0.0f;
		for ( const HknpShape & s : std::as_const( shapes ) ) {
			comVolume += s.massVolume;
			com += s.massCom * s.massVolume;
		}
		if ( comVolume > 1.0e-9f )
			com /= comVolume;
		// DYNAMIC bodies only. A keyframed body is not simulated and stores its
		// POSITION here with no centre-of-mass term -- bldwoodpdoor01 sits at
		// 4.00,-48.00,0.00 and stores exactly that, and so does the cabinet.
		// Adding the term to those put every door 84 units out.
		phys.motionCom = in.dynamic ? ( in.center + com ) : in.center;
	}
	phys.hasMotion = in.dynamic || in.keyframed;
	phys.motionIndex = ( in.dynamic || in.keyframed ) ? 0 : -1;
	/* cinfo +0x18. Compile wrote 0 here on every body it built, which cost every
	 * dynamic body its RAISE_CONTACT_IMPULSE_EVENTS bit -- and without that the
	 * engine never raises the contact event, so it never asks what the body is
	 * made of and it makes no impact sound at all. See HknpBodyFlag, which
	 * carries the disassembly and the corpus counts. `in.dynamic` is exactly the
	 * measured predicate: it is what decides whether a motionProperties record
	 * gets written (sys.motionCount below), and vanilla sets the bit on every
	 * body that has one and no body that does not.
	 */
	phys.cinfoFlags = ( in.dynamic ? quint32( HKNP_RAISE_CONTACT_IMPULSE_EVENTS ) : 0u )
		| ( in.addKeyframed ? quint32( HKNP_ADD_KEYFRAMED ) : 0u )
		| ( in.raiseTriggerEvents ? quint32( HKNP_RAISE_TRIGGER_EVENTS ) : 0u );
	if ( in.keyframed && !in.dynamic ) {
		/* The inertia record's own index must say NO MOTION RECORD, or the engine
		 * indexes a dyn_motion array that is not there -- see dynamicInertia,
		 * which carries the disassembly. 28 of 28 vanilla keyframed records:
		 * 0x0001ffff, inverse mass 0, density 1.0, scale word 0.
		 */
		phys.inertiaTag = 0x0001ffffu;
		phys.inertiaScale = 0u;
		phys.density = 1.0f;
		phys.invMassStored = 0.0f;
	}
	if ( in.dynamic ) {
		phys.mass = std::max( in.mass, 0.001f );
		// density is the body's, so it is the mass over EVERY shape's volume
		float volume = 0.0f;
		for ( const HknpShape & s : std::as_const( shapes ) )
			volume += s.massVolume;
		phys.density = volume > 1.0e-9f ? phys.mass / volume : phys.mass;
		auto inv = []( float v ) { return ( v > 1.0e-12f ) ? 1.0f / v : 0.0f; };
		for ( int a = 0; a < 3; a++ )
			phys.invInertia[a] = inv( in.inertia[a] );
		/* dyn_inertia +0x30 is the body position PLUS the body's own centre of
		 * mass, not the position alone.
		 *
		 * Measured: vault_chairfoldingclosed02 sits at 0.040,-0.203,36.438 game
		 * units and stores 0.080,-0.406,72.875, and the difference -- to three
		 * decimals -- is the shape's own centre of mass, 0.041,-0.203,36.437.
		 * Across 500 SetDressing files, 99 bodies sit at the origin and every one
		 * of them still carries a NONZERO value here, which is what says this is a
		 * centre of mass and not a copy of the position.
		 *
		 * Writing the position alone put the centre of mass on the floor instead
		 * of half way up the object, so a folding chair rotated to stand itself
		 * up, and railings drove themselves through the floor.
		 */
		phys.gravityFactor = in.gravityFactor;
		phys.maxLinVelocity = in.maxLinVelocity;
		phys.maxAngVelocity = in.maxAngVelocity;
		phys.linDamping = in.linDamping;
		phys.angDamping = in.angDamping;
	}
	sys.bodyPhys.append( phys );
	/* The two arrays have their own counts and they do NOT have to agree. A
	 * KEYFRAMED body -- a door, a gate -- carries an inertia record and a motion
	 * INDEX with no dyn_motion behind it, which the assembler already knows how
	 * to write; it just derives both counts from the motion index when the decode
	 * did not supply them, and that wrote a motion array vanilla does not have.
	 */
	sys.motionCount = in.dynamic ? 1 : 0;
	sys.inertiaCount = ( in.dynamic || in.keyframed ) ? 1 : 0;
	sys.dynamic = in.dynamic;
	sys.shapeListOrder = { 0 };
	sys.rootClassName = QStringLiteral( "hknpPhysicsSystemData" );
	return hknpEncodeSystem( sys, error );
}

/*! Merge every shape in \a body into a single mesh collision shape.
 *
 *  A box, a sphere and a mesh in one body are three shapes the engine tests
 *  separately; merged, they are one. tlCollAppendEditableMesh already walks a
 *  shape — through lists, through transform wrappers, and turning a box or a
 *  capsule into triangles on the way — so merging is that walk run over each of
 *  them into ONE mesh, which is why this needs no round trip through geometry
 *  and no join. Geometric shapes and mesh shapes merge by exactly the same
 *  route, because by the time they are appended there is no difference left.
 *
 *  The result is a bhkNiTriStripsShape, built the way Create Accurate Mesh
 *  Collision builds one. No MOPP is generated: that is its own spell, and
 *  guessing at it here would be a second opinion about something that already
 *  has an owner.
 *
 *  \return the empty string on success, or why not.
 */
QString tlMergeBodyShapes( NifModel * nif, qint32 body, int * verticesOut, int * shapesOut )
{
	const QString refusal = tlMergeBodyShapesRefusal( nif, body );
	if ( !refusal.isEmpty() )
		return refusal;

	const qint32 heldBlock = nif->getLink( nif->getBlockIndex( body ), "Shape" );
	const QVector<qint32> subs = nif->getLinkArray( tlCollBlockIndex( nif, heldBlock ), "Sub Shapes" );

	CollisionMesh merged;
	for ( const qint32 sub : subs )
		tlCollAppendEditableMesh( nif, sub, merged, Matrix4(), 0 );
	if ( merged.verts.size() < 3 || merged.tris.isEmpty() )
		return QObject::tr( "Those shapes have no usable geometry between them." );
	// the mesh collision budget, and it is the vertex INDEX that is 16-bit
	if ( merged.verts.size() > 65535 )
		return QObject::tr( "%1 vertices between them: a mesh collision shape holds 65,535." )
			.arg( merged.verts.size() );

	if ( shapesOut )
		*shapesOut = subs.size();
	if ( verticesOut )
		*verticesOut = merged.verts.size();

	nifSnapshotOp( nif, QObject::tr( "Merge collision shapes" ), [&]() {
		/* THE BODY AND THE OLD LIST, HELD BEFORE ANYTHING IS INSERTED.
		 *
		 * Inserting a block renumbers, and both blocks this has to reach at the end
		 * were found before the inserting started. As plain numbers they named
		 * something else by then, and what that did was measured: it pointed the
		 * wrong body's Shape at a box and left the list it meant to remove exactly
		 * where it was. The same trap as the body-targeted drop earlier today,
		 * three functions from here — twice in one file, so it is the file's habit
		 * and not an accident.
		 */
		const QPersistentModelIndex pBody = nif->getBlockIndex( body );
		const QPersistentModelIndex pOldList = tlCollBlockIndex( nif, heldBlock );

		const int nv = merged.verts.size(), nt = merged.tris.size();
		QModelIndex iData = nif->insertNiBlock( "NiTriStripsData" );
		Vector3 centre;
		for ( const Vector3 & v : merged.verts )
			centre += v;
		centre /= float( nv );
		float radius = 0.0f;
		for ( const Vector3 & v : merged.verts )
			radius = std::max( radius, ( v - centre ).length() );

		nif->set<int>( iData, "Num Vertices", nv );
		nif->set<int>( iData, "Has Vertices", 1 );
		nif->updateArraySize( iData, "Vertices" );
		nif->setArray<Vector3>( iData, "Vertices", merged.verts );
		if ( QModelIndex iBound = nif->getIndex( iData, "Bounding Sphere" ); iBound.isValid() ) {
			nif->set<Vector3>( iBound, "Center", centre );
			nif->set<float>( iBound, "Radius", radius );
		}
		// one strip per triangle, which is what the create spell writes too
		nif->set<int>( iData, "Num Triangles", nt );
		nif->set<int>( iData, "Num Strips", nt );
		nif->set<int>( iData, "Has Points", 1 );
		QModelIndex iLengths = nif->getIndex( iData, "Strip Lengths" );
		QModelIndex iPoints = nif->getIndex( iData, "Points" );
		if ( iLengths.isValid() && iPoints.isValid() ) {
			nif->updateArraySize( iLengths );
			nif->updateArraySize( iPoints );
			for ( int t = 0; t < nt; t++ ) {
				nif->set<int>( nif->getIndex( iLengths, t ), 3 );
				QModelIndex iStrip = nif->getIndex( iPoints, t );
				nif->updateArraySize( iStrip );
				const Triangle & tri = merged.tris.at( t );
				nif->setArray<quint16>( iStrip,
					{ quint16( tri[0] ), quint16( tri[1] ), quint16( tri[2] ) } );
			}
		}

		QModelIndex iStrips = nif->insertNiBlock( "bhkNiTriStripsShape" );
		nif->set<float>( iStrips, "Radius", 0.1f );
		nif->set<uint>( iStrips, "Num Strips Data", 1 );
		nif->updateArraySize( iStrips, "Strips Data" );
		nif->setLink( nif->getIndex( nif->getIndex( iStrips, "Strips Data" ), 0 ),
			nif->getBlockNumber( iData ) );
		nif->set<uint>( iStrips, "Num Filters", 1 );
		nif->updateArraySize( iStrips, "Filters" );

		/* THE BODY TAKES THE NEW SHAPE FIRST, then the old arrangement goes.
		 *
		 * In that order and through persistent indices, because removing a branch
		 * renumbers everything after it — the lesson the body-targeted drop was
		 * taught earlier today. Removing the LIST takes its sub-shapes with it,
		 * which is the whole old arrangement in one call.
		 */
		const QPersistentModelIndex pStrips = iStrips;
		if ( pBody.isValid() )
			nif->setLink( QModelIndex( pBody ), "Shape",
				nif->getBlockNumber( QModelIndex( pStrips ) ) );
		if ( pOldList.isValid() )
			spRemoveBranch().castIfApplicable( nif, QModelIndex( pOldList ) );
	} );
	return QString();
}


//! Compile one editable collision body into an FO4 hknp packfile (defined below the panel)
QModelIndex tlCompileCollision( NifModel * nif, QWidget * parent,
	const QModelIndex & object, bool confirmed );

namespace {

enum CollisionRoles {
	ObjectBlockRole = Qt::UserRole + 1,
	NodeBlockRole,
	SystemBlockRole,
	BodyBlockRole,
	ShapeBlockRole,
	BodyIdRole,
	CompiledRole,
	ShapeIndexRole
};

static QString basicCollisionLayerName( quint32 layer )
{
	switch ( layer ) {
	case 1: return QObject::tr( "STATIC (1)" );
	case 10: return QObject::tr( "PROPS (10)" );
	case 31: return QObject::tr( "STAIRHELPER (31)" );
	case 56: return QObject::tr( "CHARBUMPER (56)" );
	default: return QObject::tr( "Layer %1" ).arg( layer );
	}
}

static QString materialCrcText( quint32 crc )
{
	if ( !crc ) return QObject::tr( "None" );
	QString hex = QStringLiteral( "%1" ).arg( crc, 8, 16, QLatin1Char( '0' ) ).toUpper();
	/* "Unnamed", not "Unknown": this is a perfectly good material ID that
	 * nif.xml's Fallout4HavokMaterial list has no name for, and calling it
	 * unknown makes a healthy file look broken.
	 *
	 * Measured over 150 FO4 files with compiled collision (architecture,
	 * SetDressing, Furniture): 250 shape materials, 27 distinct IDs, only 72%
	 * of which resolve. The most common material in that sample - 0xFCB37EA0,
	 * 52 uses - is itself unnamed. So this is a hole in the name table, not a
	 * decode failure: the same scan found the BODY material to be 0 on all 157
	 * bodies, which is why the shape's own ID is what reaches here.
	 *
	 * The + button beside the Material combo names one permanently.
	 */
	return QObject::tr( "Unnamed (0x%1)" ).arg( hex );
}

class CollisionTreeItem final : public QTreeWidgetItem
{
public:
	using QTreeWidgetItem::QTreeWidgetItem;
	bool operator<( const QTreeWidgetItem & other ) const override
	{
		int column = treeWidget() ? treeWidget()->sortColumn() : 0;
		if ( column == 4 )
			return text( column ).section( QLatin1Char( ' ' ), 0, 0 ).toDouble()
				< other.text( column ).section( QLatin1Char( ' ' ), 0, 0 ).toDouble();
		return QString::localeAwareCompare( text( column ), other.text( column ) ) < 0;
	}
};

/* The Blender number-field gesture used to be reimplemented here as WwNumberField.
 *
 * It is now ui/widgets/wwnumberfield.h, shared with every other panel.
 * Its own comment claimed it was "the same Blender-style scrub field used by
 * the transform operator panel". It was not: it silently dropped both
 * editingFinished emissions, so these fields never committed the way the
 * Move field does. Retiring it restores them.
 */

/*! A drop-down with a search box at the top.
 *
 *  The collision layer list is 57 rows and the material list is longer; picking
 *  from either by scrolling is the slow way to do a thing you already know the
 *  name of. Qt's own answer is an editable combo with a completer, which was
 *  tried here and removed: it drew no drop-down arrow under this stylesheet, put
 *  a clear button on a field with no empty state, and showed grey placeholder
 *  text where the current value should be whenever an edit did not match. A
 *  search box that lives INSIDE the drop-down leaves the closed field alone.
 *
 *  \a extraRow is an optional action under the search box, above the list —
 *  "add a custom material", which is a thing you do while looking at the list
 *  and finding it is not in there.
 *
 *  No Q_OBJECT: this file has no moc pass, so the callback is a std::function
 *  rather than a signal, and showPopup/hidePopup are plain virtual overrides.
 */
/* CUSTOM COLLISION-BODY PRESETS are authored reusable content, not a window
 * preference. They live in <NifSkope Library>/Collision/Presets.json; the
 * helper imports the old CollisionManager/Presets QSettings groups once. */
static QStringList wwCollisionPresetNames()
{
	QStringList names = WwCollisionLibrary::presets().keys();
	names.sort( Qt::CaseInsensitive );
	return names;
}

static QVariantMap wwCollisionPresetValues( const QString & name )
{
	return WwCollisionLibrary::presets().value( name ).toMap();
}

static bool wwCollisionPresetWrite( const QString & name, const QVariantMap & values )
{
	QVariantMap presets = WwCollisionLibrary::presets();
	presets.insert( name, values );
	return WwCollisionLibrary::writePresets( presets );
}

static bool wwCollisionPresetRemove( const QString & name )
{
	QVariantMap presets = WwCollisionLibrary::presets();
	presets.remove( name );
	return WwCollisionLibrary::writePresets( presets );
}

static bool wwCollisionPresetRename( const QString & from, const QString & to )
{
	if ( from == to || to.isEmpty() )
		return false;
	QVariantMap presets = WwCollisionLibrary::presets();
	const QVariant values = presets.take( from );
	if ( !values.isValid() ) return false;
	presets.insert( to, values );
	return WwCollisionLibrary::writePresets( presets );
}

class WwSearchCombo final : public QComboBox
{
public:
	WwSearchCombo( QWidget * parent, const QString & hint )
		: QComboBox( parent ), placeholder( hint )
	{
		// no Q_OBJECT in this file, so metaObject() still says QComboBox and
		// inherits() cannot see this class. A dynamic property is what anything
		// outside — the harness, a future restyle pass — can recognise it by.
		setProperty( "wwSearchable", true );
	}

	void setExtraRow( const QString & text, const std::function<void()> & run )
	{
		extraText = text;
		extraRun = run;
	}

	/*! Leave the popup open when a row is chosen, so the list can be worked on.
	 *
	 *  Off everywhere else. A layer or a material is a pick-and-go, and closing
	 *  is what you want; a preset list you can also rename is a thing you stay
	 *  in for a moment. Escape, Return or a click outside still close it.
	 */
	void setKeepOpen( bool on ) { keepOpen = on; }

	/*! Double-clicking a row \a can accepts renames it in place.
	 *
	 *  This only works while setKeepOpen is on, and that is not a coincidence.
	 *  A single click emits itemClicked on mouse RELEASE, so a popup that
	 *  closes there is gone before the second press of a double click can
	 *  arrive — the gesture is unreachable, not merely awkward. Staying open is
	 *  what makes select-then-rename possible without putting a
	 *  double-click-interval delay on every ordinary pick.
	 */
	void setRenamer( const std::function<bool( int )> & can,
					 const std::function<void( int, const QString & )> & apply )
	{
		canRename = can;
		applyRename = apply;
	}

	void showPopup() override
	{
		build();
		// itemChanged fires while filling the list too, and the rename handler
		// cannot tell that apart from a user committing an edit.
		populating = true;
		list->clear();
		for ( int row = 0; row < count(); row++ ) {
			auto * item = new QListWidgetItem( itemText( row ), list );
			item->setData( Qt::UserRole, row );
			item->setToolTip( itemData( row, Qt::ToolTipRole ).toString() );
		}
		populating = false;
		find->clear();
		// the count, so a list that scrolls does not read as a list that is short
		find->setPlaceholderText( count() > 0
			? QObject::tr( "%1  —  %2 to choose from" ).arg( placeholder ).arg( count() )
			: placeholder );
		list->setCurrentRow( std::max( 0, currentIndex() ) );
		list->scrollToItem( list->currentItem(), QAbstractItemView::PositionAtCenter );

		/* Sized to what is in it, between four rows and twelve.
		 *
		 * It was briefly as tall as the screen allowed, which was an overreaction
		 * to a short list that turned out to be the wrong game's table rather
		 * than a sizing problem. Fifty-seven layers do not need fifty-seven rows
		 * on screen when there is a search box above them and a scroll bar beside
		 * them; five presets should not sit in a box built for fifty.
		 */
		const QScreen * screen = QGuiApplication::screenAt( mapToGlobal( QPoint() ) );
		if ( !screen ) screen = QGuiApplication::primaryScreen();
		const QRect fits = screen ? screen->availableGeometry() : QRect( 0, 0, 1280, 800 );
		const int rowHeight = std::max( 16, list->sizeHintForRow( 0 ) );
		const int rows = std::clamp( count(), 4, 12 );
		QRect where( mapToGlobal( QPoint( 0, height() ) ),
			QSize( std::max( width(), 300 ),
				std::min( rowHeight * rows + chromeHeight(), int( fits.height() * 0.8 ) ) ) );
		if ( where.bottom() > fits.bottom() )
			where.moveTop( mapToGlobal( QPoint() ).y() - where.height() );
		where.moveLeft( std::clamp( where.left(), fits.left(), fits.right() - where.width() ) );
		where.moveTop( std::clamp( where.top(), fits.top(), fits.bottom() - where.height() ) );
		pop->setGeometry( where );
		pop->show();
		find->setFocus();
	}

	void hidePopup() override
	{
		if ( pop )
			pop->hide();
		QComboBox::hidePopup();
	}

protected:
	bool eventFilter( QObject * watched, QEvent * event ) override
	{
		if ( watched == pop && event->type() == QEvent::MouseButtonPress ) {
			const QPoint at = static_cast<QMouseEvent *>( event )->globalPosition().toPoint();
			if ( !pop->frameGeometry().contains( at ) ) {
				pop->hide();
				return true;
			}
		}
		if ( watched == find && event->type() == QEvent::KeyPress ) {
			auto * key = static_cast<QKeyEvent *>( event );
			switch ( key->key() ) {
			case Qt::Key_Escape:
				pop->hide();
				return true;
			case Qt::Key_Up:
			case Qt::Key_Down:
			case Qt::Key_PageUp:
			case Qt::Key_PageDown:
				// arrows belong to the list even while the box has focus
				QCoreApplication::sendEvent( list, key );
				return true;
			case Qt::Key_Return:
			case Qt::Key_Enter:
				choose( list->currentItem() );
				return true;
			default:
				break;
			}
		}
		return QComboBox::eventFilter( watched, event );
	}

private:
	//! Everything in the popup that is not list rows: margins, the search box,
	//! and the add-a-custom row when there is one.
	int chromeHeight() const
	{
		return 12 + ( find ? find->sizeHint().height() : 24 )
			+ ( extraText.isEmpty() ? 0 : 30 ) + 8;
	}

	void choose( QListWidgetItem * item )
	{
		if ( !item || item->isHidden() )
			return;
		setCurrentIndex( item->data( Qt::UserRole ).toInt() );
		if ( !keepOpen )
			pop->hide();
	}

	void build()
	{
		if ( pop )
			return;
		pop = new QFrame( this, Qt::Popup );
		pop->setFrameShape( QFrame::StyledPanel );
		pop->setStyleSheet( QStringLiteral(
			"QFrame { background:%1; border:1px solid %2; }"
			"QListWidget { background:%1; color:%3; border:none; }"
			// the row under the cursor lights up, distinct from the current one:
			// needs setMouseTracking below, or ::item:hover never matches
			"QListWidget::item:hover { background:%6; color:%5; }"
			"QListWidget::item:selected { background:%4; color:%5; }"
			"QLineEdit { background:%6; color:%3; border:none; border-radius:3px; padding:3px 6px; }"
			"QPushButton { background:%6; color:%3; border:none; border-radius:3px; padding:4px; text-align:left; }"
			"QPushButton:hover { background:%4; }" )
			.arg( wwSkinColor( "bgCard" ), wwSkinColor( "borderStrong" ), wwSkinColor( "text" ),
				  wwSkinColor( "bgBtnHover" ), wwSkinColor( "textBright" ), wwSkinColor( "bgInput" ) ) );
		auto * lay = new QVBoxLayout( pop );
		lay->setContentsMargins( 6, 6, 6, 6 );
		lay->setSpacing( 4 );
		find = new QLineEdit( pop );
		find->setPlaceholderText( placeholder );
		find->setClearButtonEnabled( true );
		lay->addWidget( find );
		if ( !extraText.isEmpty() ) {
			auto * extra = new QPushButton( extraText, pop );
			lay->addWidget( extra );
			QObject::connect( extra, &QPushButton::clicked, pop, [this]() {
				pop->hide();
				if ( extraRun ) extraRun();
			} );
		}
		list = new QListWidget( pop );
		list->setUniformItemSizes( true );
		/* Without this the ::item:hover rule above never matches. An item view
		 * only gets move events while a button is down unless it is asked to
		 * track, so the row under the cursor would light up only while dragging
		 * — which is not hovering.
		 */
		list->setMouseTracking( true );
		list->viewport()->setMouseTracking( true );
		lay->addWidget( list, 1 );
		QObject::connect( find, &QLineEdit::textChanged, pop, [this]( const QString & text ) {
			int firstShown = -1;
			for ( int i = 0; i < list->count(); i++ ) {
				const bool hit = text.isEmpty()
					|| list->item( i )->text().contains( text, Qt::CaseInsensitive );
				list->item( i )->setHidden( !hit );
				if ( hit && firstShown < 0 )
					firstShown = i;
			}
			// so Return runs the top hit rather than whatever was selected before
			if ( firstShown >= 0 )
				list->setCurrentRow( firstShown );
		} );
		QObject::connect( list, &QListWidget::itemClicked, pop,
			[this]( QListWidgetItem * item ) { choose( item ); } );
		/* Double click renames, where the owner allows it.
		 *
		 * The editable flag is set per edit and taken straight back off, so a
		 * slow second click on an already-current row cannot start an edit
		 * nobody asked for — QAbstractItemView opens a persistent editor on
		 * click for anything carrying ItemIsEditable.
		 */
		QObject::connect( list, &QListWidget::itemDoubleClicked, pop,
			[this]( QListWidgetItem * item ) {
				if ( !item || !canRename || !canRename( item->data( Qt::UserRole ).toInt() ) )
					return;
				item->setFlags( item->flags() | Qt::ItemIsEditable );
				list->editItem( item );
			} );
		QObject::connect( list, &QListWidget::itemChanged, pop,
			[this]( QListWidgetItem * item ) {
				if ( populating || renaming || !item || !applyRename )
					return;
				item->setFlags( item->flags() & ~Qt::ItemIsEditable );
				const int row = item->data( Qt::UserRole ).toInt();
				if ( !canRename || !canRename( row ) )
					return;
				const QString name = item->text().trimmed();
				if ( name.isEmpty() ) {
					// an empty name is not a rename, it is a lost preset
					renaming = true;
					item->setText( itemText( row ) );
					renaming = false;
					return;
				}
				renaming = true;
				applyRename( row, name );
				renaming = false;
			} );
		pop->installEventFilter( this );
		find->installEventFilter( this );
	}

	QString placeholder, extraText;
	std::function<void()> extraRun;
	std::function<bool( int )> canRename;
	std::function<void( int, const QString & )> applyRename;
	bool keepOpen = false;
	bool populating = false;
	bool renaming = false;
	QFrame * pop = nullptr;
	QLineEdit * find = nullptr;
	QListWidget * list = nullptr;
};

//! The payload a shape row carries. Private to this tree, so a row dropped
//! anywhere else is ignored rather than half-understood.
static const QLatin1String CollisionShapeDragMime( "application/x-nifskope-collisionshape" );

/*! The Collision Manager's inventory, and a drop target for its own rows.
 *
 *  A body's shapes sit under it, and until now the only way to get a shape from
 *  one body to another was to delete it and build another. Dragging the row is
 *  the obvious gesture; the body it would land in lights up while the pointer is
 *  over it, the same way the Block List lights the row a block would drop into.
 *
 *  Everything about WHAT a row is and what moving means is handed in by the
 *  panel, so this class knows about drags and nothing about collision.
 */
class CollisionInventoryTree final : public QTreeWidget
{
public:
	explicit CollisionInventoryTree( QWidget * parent ) : QTreeWidget( parent ) {}

	std::function<qint32( QTreeWidgetItem * )> shapeOfRow;
	std::function<qint32( QTreeWidgetItem * )> bodyOfRow;
	std::function<QString( qint32 shape, qint32 body )> refusalFor;
	std::function<void( qint32 shape, qint32 body )> moveShapeToBody;

	//! WW_COLLDROP_TEST: begin where Qt's routing ends. QApplication::notify puts
	//! drag events through the drag manager, so a synthetic one reaches neither
	//! event() nor any filter — measured at zero on the Block List.
	bool wwDeliverDragEvent( QEvent * event )
	{
		switch ( event->type() ) {
		case QEvent::DragEnter:
			dragEnterEvent( static_cast<QDragEnterEvent *>( event ) );
			return true;
		case QEvent::DragMove:
			dragMoveEvent( static_cast<QDragMoveEvent *>( event ) );
			return true;
		case QEvent::Drop:
			dropEvent( static_cast<QDropEvent *>( event ) );
			return true;
		default:
			return false;
		}
	}

	//! The row a drop at this point would land in, or -1. Shared with the harness
	//! so what it aims at is what the drop resolves.
	qint32 bodyUnder( const QPoint & pos ) const
	{
		QTreeWidgetItem * item = itemAt( pos );
		return item && bodyOfRow ? bodyOfRow( item ) : -1;
	}

	//! Light the body under this viewport point, or nothing. Public so a drag
	//! over the PANEL — a mesh coming in from the Block List — lights the body it
	//! would join, the same as a shape dragged within the tree.
	void wwHighlightBodyAt( const QPoint & viewportPos ) { highlight( itemAt( viewportPos ) ); }
	void wwClearHighlight() { clearHighlight(); }
	//! The shape a payload carries, or -1. Public so the Block List can ask.
	qint32 wwShapeInPayload( const QMimeData * mime ) const { return draggedShape( mime ); }

	QMimeData * shapePayload( qint32 shape ) const
	{
		auto * mime = new QMimeData;
		QByteArray payload;
		{
			QDataStream ds( &payload, QIODevice::WriteOnly );
			ds << quint64( quintptr( this ) ) << shape;
		}
		mime->setData( CollisionShapeDragMime, payload );
		return mime;
	}

protected:
	void startDrag( Qt::DropActions actions ) override final
	{
		QTreeWidgetItem * item = currentItem();
		const qint32 shape = item && shapeOfRow ? shapeOfRow( item ) : -1;
		if ( shape < 0 || !( item->flags() & Qt::ItemIsDragEnabled ) ) {
			QTreeWidget::startDrag( actions );
			return;
		}
		auto * drag = new QDrag( this );
		drag->setMimeData( shapePayload( shape ) );
		/* COPY TOO, or the Block List can never take it. Dropping a shape there
		 * turns it into a BSTriShape, which the Block List quite rightly answers
		 * as a Copy — and Qt refuses an action the drag does not support, so with
		 * the mask at Move alone the loop-closing direction was unreachable by a
		 * real drag while its checks passed. Move stays the default, which is what
		 * a shape dragged onto another BODY is.
		 */
		drag->exec( Qt::MoveAction | Qt::CopyAction, Qt::MoveAction );
		clearHighlight();
	}

	void dragEnterEvent( QDragEnterEvent * event ) override final { hover( event ); }
	void dragMoveEvent( QDragMoveEvent * event ) override final { hover( event ); }

	void dragLeaveEvent( QDragLeaveEvent * event ) override final
	{
		clearHighlight();
		QTreeWidget::dragLeaveEvent( event );
	}

	/*! A payload this tree does not understand goes to the PANEL instead.
	 *
	 *  The tree is a child widget that accepts drops, so while the pointer is over
	 *  the body rows Qt delivers to the TREE and not to the panel behind it — and
	 *  the tree only knows collision-shape payloads. A mesh dragged from the Block
	 *  List was therefore refused over every body row and accepted only over the
	 *  panel's bare furniture: the exact inverse of where it needs to land, and
	 *  with no body highlighting because the panel never saw the drag.
	 *
	 *  Set by the panel, which maps the position into its own coordinates and runs
	 *  the handler it already had.
	 */
public:
	std::function<bool( QEvent *, const QPoint & )> offerToPanel;

protected:
	void dropEvent( QDropEvent * event ) override final
	{
		const qint32 shape = draggedShape( event->mimeData() );
		const qint32 body = bodyUnder( event->position().toPoint() );
		clearHighlight();
		if ( shape < 0 && offerToPanel
			&& offerToPanel( event, viewport()->mapToGlobal( event->position().toPoint() ) ) )
			return;
		if ( shape < 0 || body < 0 || !refusalFor || !refusalFor( shape, body ).isEmpty()
			|| !moveShapeToBody )
		{
			event->setDropAction( Qt::IgnoreAction );
			event->ignore();
			return;
		}
		event->setDropAction( Qt::MoveAction );
		event->accept();
		moveShapeToBody( shape, body );
	}

private:
	/*! Lit while the pointer is over a body that would take it.
	 *
	 *  ACCEPTED EVEN WHEN IT REFUSES, with the verdict in the ACTION. Ignoring a
	 *  drag event ends the drag over the widget and not one further move arrives
	 *  — so the first position the pointer happens to be at would decide the whole
	 *  gesture, and a drag begins on the row being dragged, which refuses itself.
	 *  That is the trap the Block List's drag died in for four wrong fixes.
	 */
	void hover( QDragMoveEvent * event )
	{
		const qint32 shape = draggedShape( event->mimeData() );
		if ( shape < 0 ) {
			// a mesh from the Block List: the panel behind this tree is what takes
			// those, and it is the one that lights the body under the pointer
			if ( offerToPanel
				&& offerToPanel( event, viewport()->mapToGlobal( event->position().toPoint() ) ) )
				return;
			event->setDropAction( Qt::IgnoreAction );
			event->accept();
			return;
		}
		const QPoint pos = event->position().toPoint();
		const qint32 body = bodyUnder( pos );
		const bool legal = body >= 0 && refusalFor && refusalFor( shape, body ).isEmpty();
		highlight( legal ? itemAt( pos ) : nullptr );
		event->setDropAction( legal ? Qt::MoveAction : Qt::IgnoreAction );
		event->accept();
	}

	//! The BODY row, whichever row the pointer is actually on: dropping onto a
	//! shape means the body holding it, which is what aiming at a group of shapes
	//! looks like it should mean.
	void highlight( QTreeWidgetItem * item )
	{
		QTreeWidgetItem * row = item;
		while ( row && row->parent() )
			row = row->parent();
		if ( row == highlighted )
			return;
		clearHighlight();
		if ( !row )
			return;
		highlighted = row;
		const QBrush lit( QColor( wwSkinColor( "accent" ) ) );
		for ( int c = 0; c < columnCount(); c++ ) {
			highlightWas.append( row->background( c ) );
			row->setBackground( c, lit );
		}
	}

	void clearHighlight()
	{
		if ( !highlighted )
			return;
		for ( int c = 0; c < columnCount() && c < highlightWas.size(); c++ )
			highlighted->setBackground( c, highlightWas.at( c ) );
		highlightWas.clear();
		highlighted = nullptr;
	}

	qint32 draggedShape( const QMimeData * mime ) const
	{
		if ( !mime || !mime->hasFormat( CollisionShapeDragMime ) )
			return -1;
		QByteArray payload = mime->data( CollisionShapeDragMime );
		QDataStream ds( &payload, QIODevice::ReadOnly );
		quint64 fromTree = 0;
		qint32 shape = -1;
		ds >> fromTree;
		if ( fromTree != quint64( quintptr( this ) ) )
			return -1;		// another dock's tree; the numbers mean nothing here
		ds >> shape;
		return shape;
	}

	QTreeWidgetItem * highlighted = nullptr;
	QList<QBrush> highlightWas;
};

class CollisionManagerPanel final : public QWidget
{
public:
	CollisionManagerPanel( NifModel * model, QMainWindow * window, GLView * view, QDockWidget * owner )
		: QWidget( owner ), nif( model ), mw( window ), ogl( view ), dock( owner )
	{
		// meshes can be dragged in from the Block List; see collisionDropSources
		setObjectName( QStringLiteral( "CollisionManagerPanel" ) );
		setAcceptDrops( true );
		buildUi();
		connectModel();
		rebuild();
	}

	/*! WW_COLLDROP_TEST only: begin where Qt's routing ends.
	 *
	 *  QApplication::notify puts drag and drop through the drag manager, so a
	 *  synthetic drag event reaches neither event() nor any filter — measured at
	 *  zero on the block list, which is why NifTreeView carries the same hook.
	 *  What this steps over is `acceptDrops`, the flag Qt gates on before any of
	 *  it is reached, so that is asked for on its own.
	 */
	bool wwDeliverDragEvent( QEvent * event )
	{
		switch ( event->type() ) {
		case QEvent::DragEnter:
			dragEnterEvent( static_cast<QDragEnterEvent *>( event ) );
			return true;
		case QEvent::DragMove:
			dragMoveEvent( static_cast<QDragMoveEvent *>( event ) );
			return true;
		case QEvent::Drop:
			dropEvent( static_cast<QDropEvent *>( event ) );
			return true;
		default:
			return false;
		}
	}

protected:
	/*! DRAG A MESH IN HERE TO GIVE IT COLLISION.
	 *
	 *  The Block List's drag already carries the blocks it picked up, and this
	 *  panel already knows how to make collision out of a selection — so the drop
	 *  is only an aiming device: it points the existing Create at what was
	 *  dragged. It runs the SAME thing the shape popup's Create button runs, at
	 *  the shape type the panel is showing, which is why a drop is never a
	 *  surprise: whatever that popup says it will make is what you get. Convex
	 *  and Mesh still open their preview, so the expensive two keep their
	 *  confirmation step.
	 *
	 *  Several meshes at once make one body EACH, which is what
	 *  castCollisionOverSelection already does for a multi-selection and what
	 *  collision_per_shape.sh covers.
	 */
	void dragEnterEvent( QDragEnterEvent * event ) override final { offerCollisionDrop( event ); }
	void dragMoveEvent( QDragMoveEvent * event ) override final { offerCollisionDrop( event ); }

	void dropEvent( QDropEvent * event ) override final
	{
		const QList<qint32> sources = collisionDropSources( event->mimeData() );
		const qint32 intoBody = bodyUnderPanelPoint( event->position().toPoint() );
		if ( tree )
			tree->wwClearHighlight();
		if ( sources.isEmpty() || !createShapeNow ) {
			event->ignore();
			return;
		}
		event->setDropAction( Qt::CopyAction );
		event->accept();

		/* THE CURRENT BLOCK ONLY IF IT HAS TO CHANGE. The create path reads
		 * currentSource(), and the drag came out of the Block List, so the
		 * current row is normally already one of the blocks being dropped —
		 * selecting again would collapse the very multi-selection that makes this
		 * one body per mesh.
		 */
		if ( auto * w = dynamic_cast<NifSkope *>( mw ); w && nif ) {
			const qint32 current = nif->getBlockNumber( w->currentNifIndex() );
			if ( !sources.contains( current ) )
				w->select( nif->getBlockIndex( sources.first() ) );
		}
		// what the spells read; castCollisionOverSelection restores it on the way out
		setBlockListSelection( sources );

		/* NO BODY IN THE FILE? SAY SO, AND OFFER TO MAKE ONE PROPERLY.
		 *
		 * Dropping meshes here used to create a body silently, taking layer, mass,
		 * motion system and the rest from whatever the panel happened to be set to
		 * — settings a drag says nothing about and that are tedious to correct
		 * afterwards. When there is no body at all, that is a decision worth
		 * making on purpose.
		 *
		 * The meshes are parked and go in once the body exists, so answering the
		 * popup finishes the drop rather than replacing it.
		 */
		bool anyBody = false;
		for ( int b = 0; b < nif->getBlockCount() && !anyBody; b++ )
			if ( nif->blockInherits( nif->getBlockIndex( b ), "bhkRigidBody" ) )
				anyBody = true;
		/* NOT UNDER THE DROP HARNESS, which has nobody at the keyboard.
		 *
		 * This question and the popup behind it arrived after collision_drop.sh was
		 * written, and its fixture starts with no body at all — so its first real
		 * drop opened a modal question and waited for a click that was never
		 * coming. The harness ran four checks and hung, every run, and the log just
		 * stopped: no crash, no error, the process alive and pumping. What it cost
		 * to find is the reason for this comment.
		 *
		 * Under WW_COLLDROP_TEST it takes the SAME FLOW with the question and the
		 * popup left out: the shapes are parked and the popup's own Create action
		 * runs directly. Skipping the whole branch instead — the first thing tried —
		 * skipped the body with it, and every count below read zero.
		 *
		 * So the prompt and the popup are not covered by that harness; they want a
		 * driven-dialog test of their own. What is covered is everything after them.
		 */
		const bool harness = qEnvironmentVariableIsSet( "WW_COLLDROP_TEST" );
		if ( !anyBody && harness && createBodyNow ) {
			pendingDropShapes.clear();
			for ( const qint32 block : sources ) {
				const QModelIndex idx = nif->getBlockIndex( block );
				if ( idx.isValid() )
					pendingDropShapes.append( QPersistentModelIndex( idx ) );
			}
			createBodyNow();
			return;
		}
		if ( !anyBody && !harness ) {
			const QMessageBox::StandardButton answer = QMessageBox::warning( this,
				tr( "No collision body" ),
				tr( "This file has no collision body, and a shape has to live in one.\n\n"
					"Create a collision body now? Its layer, material and physics are set in "
					"the panel that opens; the %n mesh(es) you dropped go into it once it "
					"exists.", "", int( sources.size() ) ),
				QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok );
			if ( answer != QMessageBox::Ok )
				return;
			pendingDropShapes.clear();
			for ( const qint32 block : sources ) {
				const QModelIndex idx = nif->getBlockIndex( block );
				if ( idx.isValid() )
					pendingDropShapes.append( QPersistentModelIndex( idx ) );
			}
			// the SAME placement the button uses: clamped to the screen, flipped
			// above when there is no room below. Hand-rolling it here put the panel
			// half off the bottom with Create under the taskbar.
			openPopupUnder( createPopup, createButton );
			return;
		}

		if ( QStatusBar * bar = mw ? mw->statusBar() : nullptr )
			bar->showMessage( sources.size() == 1
				? tr( "Making collision from %1." ).arg( nif->itemName( nif->getBlockIndex( sources.first() ) ) )
				: tr( "Making collision from %1 blocks, one body each." ).arg( sources.size() ), 5000 );

		/* DROPPED ON A BODY MEANS INTO THAT BODY.
		 *
		 * Create makes the mesh a body of its own wherever it lands, so joining an
		 * existing one is that plus a move: the new shape goes into the body the
		 * pointer was over and the body Create just made is taken away again. The
		 * shapes it makes are the same either way — this only decides where they
		 * hang.
		 *
		 * Only for the shapes Create makes outright. Convex and Mesh hand off to
		 * the live preview, which returns long after this does, so there is
		 * nothing to move yet when the drop ends: those keep their own body and
		 * the status bar says so rather than quietly doing something else.
		 */
		/* PERSISTENT INDICES, BECAUSE THE CREATE MOVES THE FURNITURE UNDER US.
		 *
		 * Creating collision CONSUMES the source mesh, and removing a block
		 * renumbers every block after it — so a body number read before the create
		 * names something else after it. This shipped with plain numbers and the
		 * harness's body-targeted drop is what found it: the shape was moved into
		 * whatever had slid into the target's row, bodies that had merely been
		 * renumbered were taken for newly created ones, and the tidy-up that
		 * followed carried the new shape off with them. Three bodies in, three
		 * bodies out, no new shape anywhere — and every count still plausible.
		 *
		 * Same reason castCollisionOverSelection holds its selection this way.
		 */
		const QPersistentModelIndex target = nif->getBlockIndex( intoBody );
		QList<QPersistentModelIndex> stood;
		for ( int b = 0; b < nif->getBlockCount(); b++ )
			if ( nif->blockInherits( nif->getBlockIndex( b ), "bhkRigidBody" ) )
				stood.append( QPersistentModelIndex( nif->getBlockIndex( b ) ) );

		createShapeNow();

		if ( intoBody < 0 || !target.isValid() )
			return;
		QSet<qint32> already;
		for ( const QPersistentModelIndex & body : std::as_const( stood ) )
			if ( body.isValid() )
				already.insert( nif->getBlockNumber( QModelIndex( body ) ) );
		const qint32 into = nif->getBlockNumber( QModelIndex( target ) );
		QList<QPersistentModelIndex> made;
		for ( int b = 0; b < nif->getBlockCount(); b++ )
			if ( nif->blockInherits( nif->getBlockIndex( b ), "bhkRigidBody" )
				&& !already.contains( b ) && b != into )
				made.append( QPersistentModelIndex( nif->getBlockIndex( b ) ) );
		if ( made.isEmpty() ) {
			if ( QStatusBar * bar = mw ? mw->statusBar() : nullptr )
				bar->showMessage( tr( "The preview decides where that one lands; it will get "
					"its own body." ), 5000 );
			return;
		}
		QString trouble;
		nifSnapshotOp( nif, tr( "Drop collision into body" ), [&]() {
			// every number re-read each time round: the move inserts a list block
			// and the removal below takes a branch out, and both renumber
			for ( const QPersistentModelIndex & body : std::as_const( made ) ) {
				if ( !body.isValid() || !target.isValid() )
					continue;
				const qint32 shape = nif->getLink( QModelIndex( body ), "Shape" );
				if ( shape < 0 )
					continue;
				const QString refusal = tlMoveCollisionShape( nif, shape,
					nif->getBlockNumber( QModelIndex( target ) ) );
				if ( !refusal.isEmpty() ) {
					trouble = refusal;
					continue;
				}
				// and the body Create made, now holding nothing, goes with its
				// collision object — the node it hung off is left alone
				const qint32 emptied = nif->getBlockNumber( QModelIndex( body ) );
				for ( int b = 0; b < nif->getBlockCount(); b++ ) {
					const QModelIndex object = nif->getBlockIndex( b );
					if ( nif->blockInherits( object, "bhkCollisionObject" )
						&& nif->getLink( object, "Body" ) == emptied )
					{
						spRemoveBranch().castIfApplicable( nif, object );
						break;
					}
				}
			}
		} );
		if ( QStatusBar * bar = mw ? mw->statusBar() : nullptr )
			bar->showMessage( trouble.isEmpty() && target.isValid()
				? tr( "Added to %1." ).arg( nif->itemName( QModelIndex( target ) ) )
				: trouble, 5000 );
		queueRebuild();
	}

public:
	/*! Is the create hook actually wired? A null one makes every drop accept the
	 *  payload, say nothing, and do nothing — indistinguishable from a create that
	 *  ran and produced no collision, which is a whole evening's difference.
	 */
	bool hasCreateHook() const { return bool( createShapeNow ); }

	//! The body under a point in the PANEL's coordinates, or -1 — the inventory
	//! is a child of this widget, so a drag over the panel has to be mapped in.
	//! Public because the drop harness has to be able to ask the aiming device
	//! itself rather than work the mapping out a second time.
	qint32 bodyUnderPanelPoint( const QPoint & panelPos ) const
	{
		if ( !tree || !tree->viewport()->isVisible() )
			return -1;
		const QPoint at = tree->viewport()->mapFrom( const_cast<CollisionManagerPanel *>( this ), panelPos );
		if ( !tree->viewport()->rect().contains( at ) )
			return -1;
		return tree->bodyUnder( at );
	}

private:
	//! Accept only what the create spells would actually take, so the no-drop
	//! cursor is the honest answer for anything else.
	void offerCollisionDrop( QDragMoveEvent * event )
	{
		if ( collisionDropSources( event->mimeData() ).isEmpty() ) {
			// accepted, IGNORE action: refusing the event outright ends the drag
			// over this widget and no further move arrives — the same trap the
			// block list's own drop handling documents.
			if ( tree )
				tree->wwClearHighlight();
			event->setDropAction( Qt::IgnoreAction );
			event->accept();
			return;
		}
		/* THE BODY IT WOULD JOIN LIGHTS UP, and nothing lights when the pointer is
		 * over the panel's own furniture — which is the difference between "into
		 * that body" and "a body of its own", and there is no other way to tell
		 * the two apart while dragging.
		 */
		if ( tree ) {
			const QPoint at = tree->viewport()->mapFrom( this, event->position().toPoint() );
			if ( tree->viewport()->rect().contains( at ) )
				tree->wwHighlightBodyAt( at );
			else
				tree->wwClearHighlight();
		}
		event->setDropAction( Qt::CopyAction );
		event->accept();
	}

	/*! Has this block geometry to make collision out of? Asked of the FILE.
	 *
	 *  NOT the create spell's own `isApplicable`, which was the first thing tried
	 *  here and is wrong for a drop: it asks the SCENE whether the block has
	 *  vertices and triangles, so until a frame has been drawn with that mesh in
	 *  it the answer is no — measured, with the scene, its renderer and the node
	 *  itself all present and the spell still refusing. Whether a mesh can be
	 *  dropped must not depend on whether the viewport has got round to it, or
	 *  the gesture would refuse on a collapsed viewport and on a file that has
	 *  only just opened.
	 *
	 *  A NiNode is deliberately NOT taken, though the spell would: dropping a
	 *  node means every mesh under it, which is a much bigger thing than the
	 *  gesture looks and is what the Create button is for.
	 */
	bool blockHasGeometry( const QModelIndex & idx ) const
	{
		if ( !nif || !idx.isValid() || nif->getBSVersion() == 0 )
			return false;
		if ( !nif->blockInherits( idx, { "BSGeometry", "BSTriShape", "NiTriBasedGeom" } ) )
			return false;
		// BSTriShape carries its own counts; NiGeometry keeps them in its Data,
		// which is the same split wwBlockSummary reads them through
		QModelIndex counts = idx;
		if ( !nif->getIndex( counts, "Num Vertices" ).isValid() ) {
			const int data = nif->getLink( idx, "Data" );
			if ( data < 0 )
				return false;
			counts = nif->getBlockIndex( data );
		}
		return nif->get<int>( counts, "Num Vertices" ) > 0
			&& nif->get<int>( counts, "Num Triangles" ) > 0;
	}

	//! The blocks in a Block List drag that collision can be made from.
	QList<qint32> collisionDropSources( const QMimeData * mime ) const
	{
		QList<qint32> sources;
		auto * w = dynamic_cast<NifSkope *>( mw );
		if ( !w || !nif || !mime )
			return sources;
		for ( const qint32 block : w->blockListDragPayload( mime ) )
			if ( blockHasGeometry( nif->getBlockIndex( block ) ) )
				sources.append( block );
		return sources;
	}

	//! The shape popup's Create, so a drop and the button cannot drift apart.
	std::function<void()> createShapeNow;
	//! What the create popup's own Create button runs. See createShapeNow.
	std::function<void()> createBodyNow;

	/* A DROP PARKED UNTIL THERE IS A BODY TO PUT IT IN.
	 *
	 * Dropping meshes on a file with no collision body at all used to make one
	 * silently, with whatever settings the panel happened to hold — layer, mass,
	 * motion system, all of it decided for you by a gesture that said nothing
	 * about bodies. The meshes wait here instead while the Create Collision Body
	 * popup is answered, and go in once it is.
	 *
	 * Persistent indices: creating a body inserts blocks and renumbers.
	 */
	QList<QPersistentModelIndex> pendingDropShapes;
	//! True while createBody runs, so hiding its popup can tell "confirmed" from
	//! "dismissed" and drop a parked drop that was never going to happen.
	bool creatingBodyNow = false;

	/*! Open a popup just under the widget that summons it, clamped to the screen.
	 *
	 *  Not centred on the display: the panel is docked at one edge and the thing
	 *  you are about to operate on is in the viewport beside it, so the middle of
	 *  the screen is the one place your eyes are not.
	 *
	 *  A MEMBER, because the mesh drop opens this same popup and the placement it
	 *  hand-rolled instead put the panel half off the bottom of the screen with
	 *  the Create button below the taskbar — unreachable, on the one path where
	 *  the popup is the only way to finish what you started. This clamps, and
	 *  flips above when there is no room below.
	 */
	void openPopupUnder( QFrame * popup, QWidget * under )
	{
		if ( !popup || !under )
			return;
		popup->adjustSize();
		QRect where( under->mapToGlobal( QPoint( 0, under->height() + 2 ) ), popup->sizeHint() );
		where.setWidth( std::max( where.width(), under->width() ) );
		const QScreen * screen = QGuiApplication::screenAt( where.topLeft() );
		if ( !screen ) screen = QGuiApplication::primaryScreen();
		if ( screen ) {
			const QRect fits = screen->availableGeometry();
			if ( where.bottom() > fits.bottom() )		// no room below: go above
				where.moveTop( under->mapToGlobal( QPoint() ).y() - where.height() - 2 );
			where.moveLeft( std::clamp( where.left(), fits.left(), fits.right() - where.width() ) );
			where.moveTop( std::clamp( where.top(), fits.top(), fits.bottom() - where.height() ) );
		}
		popup->setGeometry( where );
		popup->show();
		popup->raise();
	}

	NifModel * nif = nullptr;
	QMainWindow * mw = nullptr;
	GLView * ogl = nullptr;
	QDockWidget * dock = nullptr;
	CollisionInventoryTree * tree = nullptr;
	QLabel * summary = nullptr;
	QLabel * inventoryHeader = nullptr;
	QLabel * physicsHint = nullptr;
	QGroupBox * physicsGroup = nullptr;
	QWidget * physicsEditorBody = nullptr;
	QWidget * compiledSummaryWidget = nullptr;
	QWidget * emptyPhysicsWidget = nullptr;
	QLabel * compiledSummaryLabel = nullptr;
	PhysicsSimPanel * testPanel = nullptr;
	QWidget * advancedSection = nullptr;
	QWidget * advancedBody = nullptr;
	QDoubleSpinBox * mass = nullptr;
	//! held as a member so the compiled/editable sync can grey it with the rest
	QPushButton * massFromMaterialButton = nullptr;
	QDoubleSpinBox * friction = nullptr;
	QDoubleSpinBox * restitution = nullptr;
	QDoubleSpinBox * linearDamping = nullptr;
	QDoubleSpinBox * angularDamping = nullptr;
	QDoubleSpinBox * maxLinearVelocity = nullptr;
	QDoubleSpinBox * maxAngularVelocity = nullptr;
	QComboBox * layer = nullptr;
	QComboBox * material = nullptr;
	QComboBox * motionSystem = nullptr;
	QComboBox * qualityType = nullptr;
	QComboBox * solverDeactivation = nullptr;
	QComboBox * deactivatorType = nullptr;
	QCheckBox * keyframed = nullptr;
	QCheckBox * linkedGroup = nullptr;
	QCheckBox * collisionWithinGroup = nullptr;
	QCheckBox * wind = nullptr;
	QCheckBox * phantom = nullptr;
	QCheckBox * shapePhantom = nullptr;
	QSpinBox * filterGroup = nullptr;
	QDoubleSpinBox * centerX = nullptr;
	QDoubleSpinBox * centerY = nullptr;
	QDoubleSpinBox * centerZ = nullptr;
	QDoubleSpinBox * inertiaX = nullptr;
	QDoubleSpinBox * inertiaY = nullptr;
	QDoubleSpinBox * inertiaZ = nullptr;
	QDoubleSpinBox * penetrationDepth = nullptr;
	QToolButton * primaryCollisionAction = nullptr;
	QAction * decompileSelectedAction = nullptr;
	QAction * compileSelectedAction = nullptr;
	QToolButton * createButton = nullptr;			//!< Create Collision Body
	QToolButton * createShapeButton = nullptr;		//!< Create Collision Shape
	//! Wrapper carrying the shape button's tooltip. A disabled widget gets no
	//! mouse events, so a tooltip on the button itself would never appear —
	//! which is exactly when the explanation is needed.
	QWidget * createShapeHost = nullptr;
	QFrame * createPopup = nullptr;					//!< body settings
	QFrame * shapePopup = nullptr;					//!< shape type and material
	QComboBox * createDeactivator = nullptr;
	QDoubleSpinBox * createPenetration = nullptr;
	//! The create popup's copy of the body settings. Named like the editor's
	//! below it because they are the same eleven fields, seeding instead of editing.
	//! A member, not a buildUi local, because its list is version-specific and
	//! has to be refilled when a file loads. See populatePhysicsEnums.
	WwSearchCombo * createMaterialCombo = nullptr;
	QComboBox * createLayerCombo = nullptr;
	QComboBox * createMotion = nullptr;
	QComboBox * createQuality = nullptr;
	QComboBox * createSolver = nullptr;
	QDoubleSpinBox * createMass = nullptr;
	QDoubleSpinBox * createFriction = nullptr;
	QDoubleSpinBox * createRestitution = nullptr;
	QDoubleSpinBox * createLinDamp = nullptr;
	QDoubleSpinBox * createAngDamp = nullptr;
	QDoubleSpinBox * createMaxLinVel = nullptr;
	QDoubleSpinBox * createMaxAngVel = nullptr;
	QFrame * previewPanel = nullptr;
	QWidget * previewBody = nullptr;
	QComboBox * previewMethod = nullptr;
	QDoubleSpinBox * previewRatio = nullptr;
	QDoubleSpinBox * previewPrecision = nullptr;
	QDoubleSpinBox * previewThreshold = nullptr;
	QSpinBox * previewMaxHulls = nullptr;
	QLabel * previewStats = nullptr;
	QLabel * previewMethodLabel = nullptr;
	QLabel * previewPrecisionLabel = nullptr;
	QLabel * previewThresholdLabel = nullptr;
	QLabel * previewMaxHullsLabel = nullptr;
	QToolButton * previewTitle = nullptr;
	QTimer * previewTimer = nullptr;
	QPersistentModelIndex previewSource;
	int previewKind = 0; // 0 convex, 1 mesh/decimate
	bool updating = false;
	bool rebuildQueued = false;
	//! set while this dock is the one driving the selection, so the echo back
	//! through currentNifIndexChanged does not re-enter and fight the click
	bool syncingSelection = false;
	QSet<int> expandedObjects;

	QModelIndex blockIndex( int block ) const { return tlCollBlockIndex( nif, block ); }

	QString blockType( int block ) const
	{
		QModelIndex i = blockIndex( block );
		return i.isValid() ? nif->itemName( i ) : QString();
	}

	/*! Fallout 4's material table. Not "whichever game the header suggests".
	 *
	 *  This was a ladder walking the BS version down through Fallout 76, Skyrim
	 *  and Fallout 3 into Oblivion. A BS version of 0 — what you get before a
	 *  file is open, and what this program's own new document had — reached the
	 *  bottom rung, so collision was authored out of Oblivion's 32 materials
	 *  where Fallout 4 has 157. Nothing on screen said so: the names are
	 *  plausible words, Stone and Metal and Water, and only a tooltip's
	 *  OB_HAV_MAT_ prefix gave it away.
	 *
	 *  Making the FALLBACK Fallout 4 was not enough and was the wrong shape of
	 *  fix. This is a Fallout 4 program — the handoff says so, and the other
	 *  games' paths are inherited and unmaintained — so there is one table and
	 *  no ladder to fall off. A Skyrim or Oblivion mesh opened here shows
	 *  Fallout 4 names against its values, which is the trade this fork already
	 *  makes everywhere else.
	 */
	QString materialEnumType() const
	{
		return QStringLiteral( "Fallout4HavokMaterial" );
	}

	QVariantMap customMaterials() const
	{
		return WwCollisionLibrary::customMaterials();
	}

	QString knownMaterialText( quint32 crc ) const
	{
		const auto & options = NifValue::enumOptionData( materialEnumType() ).o;
		auto it = options.constFind( crc );
		if ( it == options.cend() ) return QString();
		// Keep the CRC/internal identifier as the stored value, but present the
		// exact labels used by Bethesda's FO4 3ds Max exporter. nif.xml contains
		// several CK-style or later-added labels (ActorArmoredCrab,
		// MaterialPlasticBin...) that do not match the authoring dropdown.
		if ( materialEnumType() == QLatin1String( "Fallout4HavokMaterial" ) ) {
			switch ( crc ) {
				case 0u: return QStringLiteral( "NullMaterial" );
				case 186875565u: return QStringLiteral( "Generic" );
				case 3824112521u: return QStringLiteral( "ActorGeneric" );
				case 4288341836u: return QStringLiteral( "ActorCrabArmored" );
				case 2377431615u: return QStringLiteral( "ActorGhost" );
				case 1395526401u: return QStringLiteral( "ActorInsect" );
				case 1317396055u: return QStringLiteral( "ActorMetal" );
				case 1526052914u: return QStringLiteral( "ActorMetalLarge" );
				case 1994095519u: return QStringLiteral( "ActorMetalSmall" );
				case 2215730879u: return QStringLiteral( "ActorSkeleton" );
				case 1423343525u: return QStringLiteral( "ActorSkin" );
				case 1124183901u: return QStringLiteral( "ActorSkinLarge" );
				case 1865379056u: return QStringLiteral( "ActorSkinSmall" );
				case 748236019u: return QStringLiteral( "ArmorHeavy" );
				case 1035078235u: return QStringLiteral( "ArmorLight" );
				case 504699171u: return QStringLiteral( "Arrow" );
				case 2474963416u: return QStringLiteral( "Basket" );
				case 2605587041u: return QStringLiteral( "Bone" );
				case 3936451629u: return QStringLiteral( "Book" );
				case 493553910u: return QStringLiteral( "Bottle" );
				case 172611777u: return QStringLiteral( "BottleSmall" );
				case 4050795553u: return QStringLiteral( "Brick" );
				case 4028041609u: return QStringLiteral( "Carpet" );
				case 885278459u: return QStringLiteral( "CeramicMedium" );
				case 1998647255u: return QStringLiteral( "Chain" );
				case 3839073443u: return QStringLiteral( "Cloth" );
				case 630687820u: return QStringLiteral( "ClothCushion" );
				case 1949124673u: return QStringLiteral( "Coin" );
				case 911716378u: return QStringLiteral( "Concrete" );
				case 3106094762u: return QStringLiteral( "Dirt" );
				case 1340314491u: return QStringLiteral( "DirtStairs" );
				case 3739830338u: return QStringLiteral( "Glass" );
				case 2534329404u: return QStringLiteral( "GlassStairs" );
				case 1848600814u: return QStringLiteral( "Grass" );
				case 2926148967u: return QStringLiteral( "GrassStairs" );
				case 428587608u: return QStringLiteral( "Gravel" );
				case 873356572u: return QStringLiteral( "Ice" );
				case 668408902u: return QStringLiteral( "Insect" );
				case 2900281814u: return QStringLiteral( "Meat" );
				case 104858580u: return QStringLiteral( "Metal" );
				case 2050332274u: return QStringLiteral( "MetalBarrel" );
				case 4094939507u: return QStringLiteral( "MetalHeavy" );
				case 2993325253u: return QStringLiteral( "MetalHollow" );
				case 3845715931u: return QStringLiteral( "MetalLight" );
				case 706373264u: return QStringLiteral( "MetalSolid" );
				case 1486385281u: return QStringLiteral( "Mud" );
				case 2974920155u: return QStringLiteral( "Organic" );
				case 3028667025u: return QStringLiteral( "OtherParent" );
				case 2285410059u: return QStringLiteral( "Paper" );
				case 2113564165u: return QStringLiteral( "Plastic" );
				case 4013367152u: return QStringLiteral( "PotsPans" );
				case 2168343821u: return QStringLiteral( "Sand" );
				case 3504249533u: return QStringLiteral( "Skeleton" );
				case 591247106u: return QStringLiteral( "Skin" );
				case 2925203401u: return QStringLiteral( "ShieldHeavy" );
				case 3211798881u: return QStringLiteral( "ShieldLight" );
				case 398949039u: return QStringLiteral( "Snow" );
				case 950426180u: return QStringLiteral( "SnowStairs" );
				case 3741512247u: return QStringLiteral( "Stone" );
				case 1782067423u: return QStringLiteral( "StoneAsStairs" );
				case 2276150428u: return QStringLiteral( "StoneBoulderLarge" );
				case 3251579835u: return QStringLiteral( "StoneBoulderMedium" );
				case 2877656881u: return QStringLiteral( "StoneBoulderSmall" );
				case 1413383033u: return QStringLiteral( "StoneBroken" );
				case 811073006u: return QStringLiteral( "StoneBrokenStairs" );
				case 2253994566u: return QStringLiteral( "StoneHeavy" );
				case 2519395573u: return QStringLiteral( "StoneStairs" );
				case 1024582599u: return QStringLiteral( "Water" );
				case 2459239824u: return QStringLiteral( "WaterPuddle" );
				case 3934839107u: return QStringLiteral( "Web" );
				case 26782757u: return QStringLiteral( "WeaponAxe1Hand" );
				case 3284711549u: return QStringLiteral( "WeaponBlade1Hand" );
				case 361241712u: return QStringLiteral( "WeaponBlade1HandSmall" );
				case 694643242u: return QStringLiteral( "WeaponBlade2Hand" );
				case 4207714651u: return QStringLiteral( "WeaponBlunt1Hand" );
				case 3178027915u: return QStringLiteral( "WeaponBlunt2Hand" );
				case 238888479u: return QStringLiteral( "WeaponBowsStaves" );
				case 4146539321u: return QStringLiteral( "WeaponPistol" );
				case 1201612612u: return QStringLiteral( "WeaponRifle" );
				case 500811281u: return QStringLiteral( "Wood" );
				case 3403586657u: return QStringLiteral( "WoodBarrel" );
				case 3793849624u: return QStringLiteral( "WoodAsStairs" );
				case 3481395209u: return QStringLiteral( "WoodHeavy" );
				case 3735733921u: return QStringLiteral( "WoodLight" );
				case 1879074862u: return QStringLiteral( "WoodStairs" );
				default: break;
			}
		}
		QString text = it.value().second.trimmed();
		return text.isEmpty() ? it.value().first : text;
	}

	QString materialToolTip( quint32 crc ) const
	{
		const auto & options = NifValue::enumOptionData( materialEnumType() ).o;
		auto it = options.constFind( crc );
		QString hex = QStringLiteral( "%1" ).arg( crc, 8, 16, QLatin1Char( '0' ) ).toUpper();
		if ( it == options.cend() ) {
			// say that the DATA is fine and only the name is missing, and point at
			// the one control that fixes it
			return tr( "Unnamed material\nCRC: 0x%1 (%2)\n\n"
				"A valid material ID that nif.xml has no name for - the collision "
				"data is fine. Use + beside the Material box to name it." )
				.arg( hex ).arg( crc );
		}
		return tr( "%1\nCRC: 0x%2 (%3)" ).arg( it.value().first, hex ).arg( crc );
	}

	QString materialText( quint32 crc ) const
	{
		QString name = knownMaterialText( crc );
		if ( !name.isEmpty() ) return name;
		const QVariantMap custom = customMaterials();
		for ( auto it = custom.cbegin(); it != custom.cend(); ++it ) {
			if ( it.value().toUInt() == crc ) return it.key();
		}
		return materialCrcText( crc );
	}

	//! Fallout 4's layers, for the same reason as materialEnumType.
	QString layerEnumType() const
	{
		return QStringLiteral( "Fallout4Layer" );
	}

	QString collisionLayerName( quint32 value ) const
	{
		QString name = collisionLayerLabel( value );
		if ( !name.isEmpty() ) return QStringLiteral( "%1 (%2)" ).arg( name ).arg( value );
		return basicCollisionLayerName( value );
	}

	QString collisionLayerLabel( quint32 value ) const
	{
		if ( layerEnumType() == QLatin1String( "Fallout4Layer" ) ) {
			static const QStringList labels = {
				QStringLiteral( "Unidentified" ), QStringLiteral( "Static" ), QStringLiteral( "Anim Static" ),
				QStringLiteral( "Transparent" ), QStringLiteral( "Clutter" ), QStringLiteral( "Weapon" ),
				QStringLiteral( "Projectile" ), QStringLiteral( "Spell" ), QStringLiteral( "Biped" ),
				QStringLiteral( "Tree" ), QStringLiteral( "Prop" ), QStringLiteral( "Water" ),
				QStringLiteral( "Trigger" ), QStringLiteral( "Terrain" ), QStringLiteral( "Trap" ),
				QStringLiteral( "NonCollidable" ), QStringLiteral( "CloudTrap" ), QStringLiteral( "Ground" ),
				QStringLiteral( "Portal" ), QStringLiteral( "Small Debris" ), QStringLiteral( "Large Debris" ),
				QStringLiteral( "Acoustic Space" ), QStringLiteral( "ActorZone" ), QStringLiteral( "ProjectileZone" ),
				QStringLiteral( "GasTrap" ), QStringLiteral( "ShellCasing" ), QStringLiteral( "Transparent Small" ),
				QStringLiteral( "Invisible Wall" ), QStringLiteral( "Transparent Small Anim" ), QStringLiteral( "Clutter Large" ),
				QStringLiteral( "Character Controller" ), QStringLiteral( "Stair Helper" ), QStringLiteral( "Dead Bip" ),
				QStringLiteral( "Biped No CC" ), QStringLiteral( "AvoidBox" ), QStringLiteral( "CollisionBox" ),
				QStringLiteral( "Camera Sphere" ), QStringLiteral( "Door Detection" ), QStringLiteral( "Cone Projectile" ),
				QStringLiteral( "Camera Pick" ), QStringLiteral( "Item Pick" ), QStringLiteral( "Line of Sight" ),
				QStringLiteral( "Path Pick" ), QStringLiteral( "Custom Pick 1" ), QStringLiteral( "FX Collider" ),
				QStringLiteral( "Spell Explosion" ), QStringLiteral( "Dropping Pick" ), QStringLiteral( "Dead Actor Zone" ),
				QStringLiteral( "Falling Trap" ), QStringLiteral( "NavMesh Cut" ), QStringLiteral( "Critter" ),
				QStringLiteral( "spellTrigger" ), QStringLiteral( "Living And Dead Actors" ), QStringLiteral( "Detection" ),
				QStringLiteral( "Trap Trigger" ), QStringLiteral( "Clutter NoNavCut" ), QStringLiteral( "CharBumper" )
			};
			if ( value < quint32( labels.size() ) ) return labels.at( int( value ) );
		}
		QString name = NifValue::enumOptionName( layerEnumType(), value );
		if ( name.contains( QLatin1Char( '_' ) ) ) name = name.section( QLatin1Char( '_' ), 1 );
		return name;
	}

	static QString titleCaseEnum( QString name )
	{
		name.replace( QLatin1Char( '_' ), QLatin1Char( ' ' ) );
		QStringList words = name.toLower().split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
		for ( QString & word : words ) if ( !word.isEmpty() ) word[0] = word[0].toUpper();
		return words.join( QLatin1Char( ' ' ) );
	}

	void populatePhysicsEnums()
	{
		if ( !layer || !material || !motionSystem || !qualityType || !solverDeactivation || !deactivatorType )
			return;
		QSignalBlocker bl( layer ), bm( material ), bmo( motionSystem ), bq( qualityType ),
			bs( solverDeactivation ), bd( deactivatorType );
		layer->clear(); material->clear(); motionSystem->clear(); qualityType->clear();
		solverDeactivation->clear(); deactivatorType->clear();
		const auto & layerOptions = NifValue::enumOptionData( layerEnumType() ).o;
		QSet<quint32> addedLayers;
		// FO4's authoring list is a contiguous 0..56 table. Add it explicitly so
		// useful entries such as Stair Helper remain available even when an older
		// nif.xml enum table is incomplete.
		if ( layerEnumType() == QLatin1String( "Fallout4Layer" )
			|| layerEnumType() == QLatin1String( "Fallout76Layer" ) ) {
			for ( quint32 value = 0; value <= 56; value++ ) {
				layer->addItem( QStringLiteral( "%1 (%2)" ).arg( collisionLayerLabel( value ) ).arg( value ), value );
				addedLayers.insert( value );
			}
		}
		for ( auto it = layerOptions.cbegin(); it != layerOptions.cend(); ++it ) {
			if ( addedLayers.contains( it.key() ) ) continue;
			QString name = collisionLayerLabel( it.key() );
			layer->addItem( QStringLiteral( "%1 (%2)" ).arg( name ).arg( it.key() ), it.key() );
		}

		QMap<QString, QPair<quint32, QString>> sortedMaterials;
		const auto & materialOptions = NifValue::enumOptionData( materialEnumType() ).o;
		for ( auto it = materialOptions.cbegin(); it != materialOptions.cend(); ++it ) {
			QString name = knownMaterialText( it.key() );
			sortedMaterials.insert( name, qMakePair( it.key(), it.value().first ) );
		}
		for ( auto it = sortedMaterials.cbegin(); it != sortedMaterials.cend(); ++it ) {
			material->addItem( it.key(), it.value().first );
			int row = material->count() - 1;
			material->setItemData( row, materialToolTip( it.value().first ), Qt::ToolTipRole );
			material->setItemData( row, it.value().second, Qt::UserRole + 1 );
		}
		const QVariantMap custom = customMaterials();
		for ( auto it = custom.cbegin(); it != custom.cend(); ++it ) {
			if ( material->findData( it.value().toUInt() ) < 0 )
				material->addItem( it.key(), it.value().toUInt() );
		}

		const auto addEnum = []( QComboBox * combo, const QString & type, const QString & prefix ) {
			const auto & options = NifValue::enumOptionData( type ).o;
			for ( auto it = options.cbegin(); it != options.cend(); ++it ) {
				QString name = it.value().first;
				if ( name.startsWith( prefix ) ) name.remove( 0, prefix.size() );
				combo->addItem( QStringLiteral( "%1 (%2)" ).arg( titleCaseEnum( name ) ).arg( it.key() ), it.key() );
			}
		};
		addEnum( motionSystem, QStringLiteral( "hkMotionType" ), QStringLiteral( "MO_SYS_" ) );
		addEnum( qualityType, QStringLiteral( "hkQualityType" ), QStringLiteral( "MO_QUAL_" ) );
		addEnum( solverDeactivation, QStringLiteral( "hkSolverDeactivation" ), QStringLiteral( "SOLVER_DEACTIVATION_" ) );
		addEnum( deactivatorType, QStringLiteral( "hkDeactivatorType" ), QStringLiteral( "DEACTIVATOR_" ) );

		/* The create popup's copies, from the same tables.
		 *
		 * They have to be refilled here and not once at construction: the enums
		 * are version-specific, and the panel is built before a file is loaded —
		 * which is exactly why this function exists and is called again on
		 * modelReset. A create-side layer list left at whatever the first guess
		 * was would offer Skyrim's layers for a Fallout 4 file.
		 */
		if ( createLayerCombo && createMotion && createQuality && createSolver ) {
			QSignalBlocker bcl( createLayerCombo ), bcm( createMotion ),
				bcq( createQuality ), bcs( createSolver );
			createLayerCombo->clear(); createMotion->clear();
			createQuality->clear(); createSolver->clear();
			for ( int i = 0; i < layer->count(); i++ )
				createLayerCombo->addItem( layer->itemText( i ), layer->itemData( i ) );
			addEnum( createMotion, QStringLiteral( "hkMotionType" ), QStringLiteral( "MO_SYS_" ) );
			addEnum( createQuality, QStringLiteral( "hkQualityType" ), QStringLiteral( "MO_QUAL_" ) );
			addEnum( createSolver, QStringLiteral( "hkSolverDeactivation" ),
				QStringLiteral( "SOLVER_DEACTIVATION_" ) );
			if ( createDeactivator ) {
				QSignalBlocker bcd( createDeactivator );
				createDeactivator->clear();
				addEnum( createDeactivator, QStringLiteral( "hkDeactivatorType" ),
					QStringLiteral( "DEACTIVATOR_" ) );
			}
		}
		/* The create-side material list, built here for the same reason.
		 *
		 * It stores the NAME as the item data, where the editor's stores the
		 * numeric value: CollisionManager/Create/Material holds a name, so that
		 * a custom material named in a BGSM keeps working when nif.xml has never
		 * heard of it. The two conventions are opposite on purpose; do not
		 * "tidy" one into the other.
		 */
		if ( createMaterialCombo ) {
			QSignalBlocker bcmat( createMaterialCombo );
			createMaterialCombo->clear();
			for ( int i = 0; i < material->count(); i++ ) {
				const quint32 value = material->itemData( i ).toUInt();
				createMaterialCombo->addItem( material->itemText( i ),
					material->itemData( i, Qt::UserRole + 1 ) );
				const int row = createMaterialCombo->count() - 1;
				createMaterialCombo->setItemData( row, value, Qt::UserRole + 1 );
				createMaterialCombo->setItemData( row, materialToolTip( value ), Qt::ToolTipRole );
			}
			const QVariantMap createCustomMaterials = customMaterials();
			for ( auto it = createCustomMaterials.cbegin(); it != createCustomMaterials.cend(); ++it ) {
				if ( createMaterialCombo->findData( it.value().toUInt(), Qt::UserRole + 1 ) >= 0 )
					continue;
				createMaterialCombo->addItem( it.key(), QStringLiteral( "0x%1" )
					.arg( it.value().toUInt(), 8, 16, QLatin1Char( '0' ) ) );
				const int row = createMaterialCombo->count() - 1;
				createMaterialCombo->setItemData( row, it.value().toUInt(), Qt::UserRole + 1 );
				createMaterialCombo->setItemData( row, materialToolTip( it.value().toUInt() ), Qt::ToolTipRole );
			}
		}
	}

	/*! Fill the create popup's fields from what is stored, preset as the fallback.
	 *
	 *  A member and not a lambda in buildUi because it has to run AFTER
	 *  populatePhysicsEnums has actually filled the combos, and again whenever it
	 *  refills them. The first version called it during construction, where
	 *  populatePhysicsEnums returns early — the physics editor it guards on does
	 *  not exist yet — so every combo was empty, currentData() came back invalid,
	 *  and the layer written for new collision was 0. Unidentified: the one value
	 *  the layer repair spell exists to find.
	 */
	/*! Create Collision Shape is only reachable once there is a body to give the
	 *  shape to, and the greyed button has to say which dead end you are at.
	 *
	 *  Two different ones with two different answers: no body in the file at all,
	 *  or bodies exist but none is selected. A single "unavailable" would leave
	 *  the second case looking like the first.
	 *
	 *  The text goes on the WRAPPER. A disabled widget receives no mouse events,
	 *  so Qt never sends it a ToolTip event and setToolTip on the button itself
	 *  displays nothing — silently, in exactly the case the explanation is for.
	 */
	/*! The editable body the BLOCK LIST selection stands for, if any.
	 *
	 *  A body is reachable from three different selections and all three are
	 *  things people actually click: the bhkRigidBody itself, the
	 *  bhkCollisionObject that owns it, or the NiNode the object hangs off.
	 *  Walk to the body from whichever it is.
	 *
	 *  Ends on a bhkRigidBody test rather than trusting the walk, so a compiled
	 *  or otherwise unexpected body does not come back as editable.
	 */
	QModelIndex selectedBlockListBody()
	{
		if ( !nif )
			return QModelIndex();
		const QModelIndex block = nif->getBlockIndex( currentSource() );
		if ( !block.isValid() )
			return QModelIndex();
		QModelIndex body;
		if ( nif->blockInherits( block, "bhkRigidBody" ) )
			body = block;
		else if ( nif->blockInherits( block, "bhkNiCollisionObject" ) )
			body = blockIndex( nif->getLink( block, "Body" ) );
		else if ( nif->blockInherits( block, "NiAVObject" ) ) {
			const QModelIndex object = blockIndex( nif->getLink( block, "Collision Object" ) );
			if ( object.isValid() )
				body = blockIndex( nif->getLink( object, "Body" ) );
		}
		return ( body.isValid() && nif->blockInherits( body, "bhkRigidBody" ) )
			? body : QModelIndex();
	}

	/*! Re-parent this body's node under the block chosen in the BLOCK LIST.
	 *
	 *  The operation and its refusals live in wwReparentBlocks (blocks.cpp),
	 *  shared with the block list's own drag-and-drop. They used to live here,
	 *  and a second copy with its own idea of what is legal is exactly what that
	 *  feature would have grown.
	 *
	 *  KeepLocal: the LOCAL transform is left alone, so a body moves in world
	 *  space when the new parent sits elsewhere. That is right for attaching
	 *  collision to a bone — which is what this button is for — and the block
	 *  list's plain drop is the world-preserving one.
	 *
	 *  The two checks kept here are the ones about this panel rather than about
	 *  the blocks: a row with no node behind it, and nothing picked in the block
	 *  list to be the parent. Neither is something wwReparentRefusal can see.
	 */
	void reparentFromBlockList( QTreeWidgetItem * item )
	{
		if ( !nif || !item )
			return;
		const int nodeBlock = item->data( 0, NodeBlockRole ).toInt();
		const QModelIndex node = blockIndex( nodeBlock );
		const QModelIndex parent = nif->getBlockIndex( currentSource() );
		if ( !node.isValid() ) {
			QMessageBox::information( this, tr( "Set Parent" ),
				tr( "This row has no node to re-parent." ) );
			return;
		}
		if ( !parent.isValid() ) {
			QMessageBox::information( this, tr( "Set Parent" ),
				tr( "Select the new parent in the block list first, and make it a NiNode — "
					"only a NiNode carries children." ) );
			return;
		}
		QStringList refusals;
		const int moved = wwReparentBlocks( nif, { nodeBlock }, nif->getBlockNumber( parent ),
			WwReparentMode::KeepLocal, &refusals );
		if ( moved == 0 ) {
			QMessageBox::information( this, tr( "Set Parent" ),
				refusals.value( 0, tr( "Nothing to re-parent." ) ) );
			return;
		}
		queueRebuild();
	}

	void updateShapeButtonState()
	{
		if ( !createShapeButton || !createShapeHost )
			return;
		QTreeWidgetItem * item = tree ? tree->currentItem() : nullptr;
		const bool anyBody = tree && tree->topLevelItemCount() > 0;
		const bool compiled = item && item->data( 0, CompiledRole ).toBool();
		/* A BODY PICKED IN THE BLOCK LIST COUNTS TOO.
		 *
		 * The gate read this panel's own list and nothing else, so selecting a
		 * bhkRigidBody in the block list — the obvious way to reach one when
		 * this dock is not the thing you are looking at — left the button
		 * greyed out with a tooltip telling you to select a body, which is
		 * exactly what you had just done.
		 *
		 * The creation path never had this problem: it casts the shape spell
		 * against currentSource(), the block list index. So only the enable
		 * test was wrong, and the two now agree about what counts.
		 */
		const bool fromBlockList = selectedBlockListBody().isValid();
		const bool ready = ( item && !compiled ) || fromBlockList;
		createShapeButton->setEnabled( ready );
		createShapeHost->setToolTip( ready
			? tr( "Add a shape to the selected collision body" )
			: !anyBody ? tr( "Create a collision body first" )
			: compiled ? tr( "The selected body is compiled — decompile it to add shapes" )
			: tr( "Select a collision body — here, or in the block list" ) );
	}

	/*! The body values the create fields are showing, shaped as a preset.
	 *
	 *  The KEY SET comes from tlCollisionPresetDefaults rather than being typed
	 *  out here, so a saved preset always holds exactly what the apply path
	 *  reads back. Add a field to the body later and presets pick it up instead
	 *  of silently lacking it.
	 *
	 *  Read out of the settings rather than off the widgets because
	 *  saveCreationSettings has just written them there and it already knows
	 *  every widget's type. A second transcription is a second thing to get
	 *  wrong.
	 */
	QVariantMap createFieldValues() const
	{
		QSettings s;
		QVariantMap out;
		const QVariantMap keys = tlCollisionPresetDefaults( 1 );
		for ( auto it = keys.cbegin(); it != keys.cend(); ++it )
			out[it.key()] =
				s.value( QStringLiteral( "CollisionManager/Create/" ) + it.key(), it.value() );
		return out;
	}

	void loadCreateFields()
	{
		if ( !createLayerCombo || !createMotion || !createQuality || !createSolver )
			return;
		QSettings s;
		const QVariantMap d = tlCollisionPresetDefaults(
			s.value( "CollisionManager/Create/Preset", 1 ).toInt() );
		auto stored = [&]( const QString & key ) {
			return s.value( QStringLiteral( "CollisionManager/Create/" ) + key, d.value( key ) );
		};
		QSignalBlocker b0( createLayerCombo ), b1( createMotion ), b2( createQuality ),
			b3( createSolver ), b4( createMass ), b5( createFriction ), b6( createRestitution ),
			b7( createLinDamp ), b8( createAngDamp ), b9( createMaxLinVel ), b10( createMaxAngVel );
		/* Zero is INVALID in all three of these tables — MO_SYS_INVALID,
		 * MO_QUAL_INVALID, SOLVER_DEACTIVATION_INVALID — so it is never something
		 * anyone chose. It is what got written by the build whose create combos
		 * were read before they had any rows in them, and it is sitting in the
		 * settings of everyone who ran it. Treat it as unset and take the preset's.
		 */
		auto enumValue = [&]( const QString & key ) {
			const quint32 v = stored( key ).toUInt();
			return v ? v : d.value( key ).toUInt();
		};
		/* The material, by the name that was stored.
		 *
		 * By name and not by value because that is what the setting holds — a
		 * custom material can outlive nif.xml's knowledge of it. A name matching
		 * no row is added as one rather than dropped: the field is not a text box
		 * any more, so there is nowhere else for it to be shown.
		 */
		if ( createMaterialCombo ) {
			QSignalBlocker bmat( createMaterialCombo );
			const QString name = s.value( "CollisionManager/Create/Material",
				QStringLiteral( "MaterialMetalSolid" ) ).toString().trimmed();
			int row = createMaterialCombo->findData( name );
			bool numeric = false;
			const quint32 asValue = name.toUInt( &numeric, 0 );
			if ( row < 0 && numeric )
				row = createMaterialCombo->findData( asValue, Qt::UserRole + 1 );
			if ( row < 0 && !name.isEmpty() ) {
				createMaterialCombo->addItem( name, name );
				row = createMaterialCombo->count() - 1;
			}
			if ( row >= 0 )
				createMaterialCombo->setCurrentIndex( row );
		}
		selectComboValue( createLayerCombo, enumValue( QStringLiteral( "Layer" ) ) );
		selectComboValue( createMotion, enumValue( QStringLiteral( "MotionSystem" ) ) );
		selectComboValue( createQuality, enumValue( QStringLiteral( "QualityType" ) ) );
		selectComboValue( createSolver, enumValue( QStringLiteral( "SolverDeactivation" ) ) );
		createMass->setValue( stored( QStringLiteral( "Mass" ) ).toDouble() );
		createFriction->setValue( stored( QStringLiteral( "Friction" ) ).toDouble() );
		createRestitution->setValue( stored( QStringLiteral( "Restitution" ) ).toDouble() );
		createLinDamp->setValue( stored( QStringLiteral( "LinearDamping" ) ).toDouble() );
		createAngDamp->setValue( stored( QStringLiteral( "AngularDamping" ) ).toDouble() );
		createMaxLinVel->setValue( stored( QStringLiteral( "MaxLinearVelocity" ) ).toDouble() );
		createMaxAngVel->setValue( stored( QStringLiteral( "MaxAngularVelocity" ) ).toDouble() );
		if ( createDeactivator ) {
			QSignalBlocker bd( createDeactivator );
			selectComboValue( createDeactivator, enumValue( QStringLiteral( "DeactivatorType" ) ) );
		}
		if ( createPenetration ) {
			QSignalBlocker bp( createPenetration );
			createPenetration->setValue( stored( QStringLiteral( "PenetrationDepth" ) ).toDouble() );
		}
	}

	static void selectComboValue( QComboBox * combo, quint32 value, const QString & fallback = QString() )
	{
		int row = combo->findData( value );
		if ( row < 0 ) {
			combo->addItem( fallback.isEmpty() ? QObject::tr( "Unknown (%1)" ).arg( value ) : fallback, value );
			row = combo->count() - 1;
		}
		combo->setCurrentIndex( row );
	}

	int ownerNode( const QModelIndex & object ) const
	{
		int target = nif->getLink( object, "Target" );
		if ( nif->isValidBlockNumber( target ) )
			return target;
		return nif->getParent( nif->getBlockNumber( object ) );
	}

	/*! What this collision's owner node is, as a bone.
	 *
	 * Two independent sources, because the two file kinds answer it differently.
	 * A skinned mesh has a real skin to consult, so skeletonAnalyse() says
	 * whether the node deforms anything. A skeleton.nif has no skin at all -
	 * every node would come back "not a bone" - but its collision hangs off a
	 * bhkRagdollSystem, and that block existing IS the statement that these
	 * bodies are the bone collision. Hence the fallback: no skin evidence plus a
	 * ragdoll system means ragdoll bone.
	 */
	QString boneRole( const QHash<int, QString> & boneRoles, int node, int systemBlock ) const
	{ return tlCollBoneRole( nif, boneRoles, node, systemBlock ); }
	QHash<int, QString> collectBoneRoles() const { return tlCollBoneRoles( nif ); }
	QString nodeName( int block ) const { return tlCollNodeName( nif, block ); }

	/*! What the body's node hangs under.
	 *
	 *  Column 0 is the node that owns the collision object; this is that node's
	 *  scene-graph parent. Two bodies on identically-named nodes are told apart
	 *  by nothing else in this list, and it is the thing you have to know before
	 *  re-parenting one.
	 */
	QString parentNodeName( int nodeBlock ) const
	{
		if ( !nif || nodeBlock < 0 )
			return QString();
		const int p = nif->getParent( nodeBlock );
		return p < 0 ? tr( "— (root)" ) : nodeName( p );
	}

	QString compiledShapeSummary( const HknpSystem & sys, quint32 bodyId,
		quint32 * material, int * verts, int * tris ) const
	{
		int count = 0;
		QString one;
		for ( const HknpShape & s : sys.shapes ) {
			if ( s.bodyId >= 0 && quint32( s.bodyId ) != bodyId )
				continue;
			count++;
			*verts += s.verts.size();
			*tris += s.tris.size();
			if ( !*material && s.materialCRC )
				*material = s.materialCRC;
			one = compiledShapeText( s );
		}
		if ( count > 1 )
			return tr( "Compound (%1)" ).arg( count );
		return count == 1 ? one : tr( "No decompiled shape" );
	}

	QString compiledShapeText( const HknpShape & shape ) const
	{
		if ( shape.primType == 1 )
			return tr( "Sphere" );
		if ( shape.primType == 2 )
			return tr( "Capsule" );
		if ( shape.isConvex )
			return tr( "Hull (%1 v)" ).arg( shape.verts.size() );
		return tr( "Mesh (%1 tri)" ).arg( shape.tris.size() );
	}

	/*! Fill the Bone cell.
	 *
	 * Column 6 is the LOGICAL index; the header moves it next to Node visually
	 * (see the tree setup). Appending rather than inserting keeps every other
	 * column index in this file unchanged - there are 42 literal ones.
	 */
	void setBoneCell( QTreeWidgetItem * item, const QString & role ) const
	{
		if ( role.isEmpty() )
			return;
		item->setText( 6, role );
		item->setForeground( 6, QColor( wwSkinColor( "accentText" ) ) );
	}

	void setItemRoles( QTreeWidgetItem * item, int objectBlock, int nodeBlock,
		int systemBlock, int bodyBlock, int shapeBlock, quint32 bodyId,
		bool compiled, int shapeIndex = -1 ) const
	{
		item->setData( 0, ObjectBlockRole, objectBlock );
		item->setData( 0, NodeBlockRole, nodeBlock );
		item->setData( 0, SystemBlockRole, systemBlock );
		item->setData( 0, BodyBlockRole, bodyBlock );
		item->setData( 0, ShapeBlockRole, shapeBlock );
		item->setData( 0, BodyIdRole, bodyId );
		item->setData( 0, CompiledRole, compiled );
		item->setData( 0, ShapeIndexRole, shapeIndex );
		/* DRAGGABLE, IF IT IS AN EDITABLE SHAPE ROW.
		 *
		 * QAbstractItemView will not enter DraggingState without
		 * Qt::ItemIsDragEnabled on the pressed item, so startDrag() is never
		 * called and the gesture does nothing whatever else is in place — which is
		 * exactly how the Block List's drag first shipped, with a green harness
		 * over a dead feature. Set here because this is the one call both the
		 * compiled and the editable population paths make.
		 *
		 * Child rows only: a top-level row is the body, and a body is what a shape
		 * is dropped ON rather than something to drag.
		 */
		const bool draggable = !compiled && shapeBlock >= 0 && item->parent();
		item->setFlags( draggable ? ( item->flags() | Qt::ItemIsDragEnabled )
			: ( item->flags() & ~Qt::ItemIsDragEnabled ) );
		/* PARENT, column 7. Set here rather than at the two population sites so
		 * the compiled and editable paths cannot drift — they already build
		 * their rows separately and this is the one call both make.
		 *
		 * Top-level rows only: a shape row's parent is the body row directly
		 * above it, which the indentation already says.
		 */
		if ( !item->parent() )
			item->setText( 7, parentNodeName( nodeBlock ) );
	}

	int firstLeafShape( int shape, int depth = 0 ) const
	{
		return tlCollFirstLeafShape( nif, shape, depth );
	}


	void appendEditableMesh( int shapeBlock, CollisionMesh & out,
		const Matrix4 & transform = Matrix4(), int depth = 0 ) const
	{
		tlCollAppendEditableMesh( nif, shapeBlock, out, transform, depth );
	}

	CollisionMesh selectedCollisionMesh() const
	{
		CollisionMesh out; QTreeWidgetItem * item = tree->currentItem(); if ( !item ) return out;
		if ( item->data( 0, CompiledRole ).toBool() ) {
			QModelIndex sysBlock = blockIndex( item->data( 0, SystemBlockRole ).toInt() );
			HknpSystem sys = hknpDecode( nif->get<QByteArray>( sysBlock, "Binary Data" ) );
			int selectedShape = item->data( 0, ShapeIndexRole ).toInt(); quint32 bodyId = item->data( 0, BodyIdRole ).toUInt();
			for ( int i = 0; i < sys.shapes.size(); i++ ) {
				const HknpShape & s = sys.shapes.at( i );
				if ( selectedShape >= 0 ? i != selectedShape : ( s.bodyId >= 0 && quint32( s.bodyId ) != bodyId ) ) continue;
				QVector<Vector3> v; for ( const Vector3 & p : s.verts ) v.append( s.transformed( p ) );
				appendMesh( out, v, s.tris, Matrix4(), 69.99125f, s.materialCRC );
			}
		} else {
			appendEditableMesh( item->data( 0, ShapeBlockRole ).toInt(), out );
		}
		return out;
	}

	static double meshVolumeGameUnits( const CollisionMesh & mesh )
	{
		if ( mesh.verts.size() < 4 || mesh.tris.size() < 4 )
			return 0.0;
		Vector3 centre;
		for ( const Vector3 & v : mesh.verts )
			centre += v;
		centre /= float( mesh.verts.size() );
		double volume = 0.0;
		for ( const Triangle & t : mesh.tris ) {
			if ( t[0] >= mesh.verts.size() || t[1] >= mesh.verts.size() || t[2] >= mesh.verts.size() ) continue;
			Vector3 a = mesh.verts[t[0]] - centre;
			Vector3 b = mesh.verts[t[1]] - centre;
			Vector3 c = mesh.verts[t[2]] - centre;
			volume += std::fabs( double( Vector3::dotproduct( a, Vector3::crossproduct( b, c ) ) ) ) / 6.0;
		}
		return volume;
	}

	QString editableShapeSummary( int rootShape, quint32 * material, int * verts, int * tris ) const
	{
		if ( !nif->isValidBlockNumber( rootShape ) )
			return tr( "Missing shape" );
		QModelIndex root = blockIndex( rootShape );
		QString rt = nif->itemName( root );
		if ( rt == QLatin1String( "bhkListShape" ) || rt == QLatin1String( "bhkConvexListShape" ) ) {
			QVector<qint32> links = nif->getLinkArray( root, "Sub Shapes" );
			if ( !links.isEmpty() ) {
				int leaf = firstLeafShape( links.first() );
				if ( leaf >= 0 )
					*material = nif->get<quint32>( blockIndex( leaf ), "Material" );
			}
			return tr( "Compound (%1)" ).arg( links.size() );
		}

		int leaf = firstLeafShape( rootShape );
		QModelIndex i = blockIndex( leaf );
		QString t = i.isValid() ? nif->itemName( i ) : rt;
		if ( i.isValid() )
			*material = nif->get<quint32>( i, "Material" );
		if ( t == QLatin1String( "bhkBoxShape" ) ) return tr( "Box" );
		if ( t == QLatin1String( "bhkSphereShape" ) ) return tr( "Sphere" );
		if ( t == QLatin1String( "bhkCapsuleShape" ) ) return tr( "Capsule" );
		if ( t == QLatin1String( "bhkConvexVerticesShape" ) ) {
			*verts = nif->get<uint>( i, "Num Vertices" );
			return tr( "Hull (%1 v)" ).arg( *verts );
		}
		if ( t == QLatin1String( "bhkNiTriStripsShape" ) ) {
			for ( qint32 d : nif->getLinkArray( i, "Strips Data" ) ) {
				QModelIndex di = blockIndex( d );
				*verts += nif->get<int>( di, "Num Vertices" );
				*tris += nif->get<int>( di, "Num Triangles" );
			}
			return tr( "Mesh (%1 tri)" ).arg( *tris );
		}
		return t.isEmpty() ? rt : t;
	}

	void appendEditableShapeItems( QTreeWidgetItem * parent, int shapeBlock,
		int objectBlock, int nodeBlock, int bodyBlock, quint32 collLayer,
		int & totalVerts, int & totalTris, int depth = 0 )
	{
		if ( !nif->isValidBlockNumber( shapeBlock ) || depth > 16 )
			return;
		QModelIndex shape = blockIndex( shapeBlock );
		QString type = nif->itemName( shape );
		if ( type.endsWith( QLatin1String( "ListShape" ) ) ) {
			for ( qint32 child : nif->getLinkArray( shape, "Sub Shapes" ) )
				appendEditableShapeItems( parent, child, objectBlock, nodeBlock, bodyBlock,
					collLayer, totalVerts, totalTris, depth + 1 );
			return;
		}
		if ( type == QLatin1String( "bhkMoppBvTreeShape" )
			 || type == QLatin1String( "bhkTransformShape" )
			 || type == QLatin1String( "bhkConvexTransformShape" ) ) {
			appendEditableShapeItems( parent, nif->getLink( shape, "Shape" ), objectBlock,
				nodeBlock, bodyBlock, collLayer, totalVerts, totalTris, depth + 1 );
			return;
		}

		quint32 material = 0;
		int verts = 0, tris = 0;
		QString label = editableShapeSummary( shapeBlock, &material, &verts, &tris );
		auto * child = new CollisionTreeItem( parent );
		child->setText( 0, tr( "Shape %1" ).arg( parent->childCount() ) );
		child->setText( 1, label );
		child->setText( 2, collisionLayerName( collLayer ) );
		child->setText( 3, materialText( material ) );
		child->setToolTip( 3, materialToolTip( material ) );
		child->setText( 5, tr( "EDITABLE" ) );
		child->setForeground( 5, QColor( 90, 169, 230 ) );
		setItemRoles( child, objectBlock, nodeBlock, -1, bodyBlock, shapeBlock, 0, false );
		totalVerts += verts;
		totalTris += tris;
	}

	void addCompiled( int objectBlock, const QHash<int, QString> & boneRoles,
		int & totalVerts, int & totalTris, int & totalBytes )
	{
		QModelIndex object = blockIndex( objectBlock );
		int systemBlock = nif->getLink( object, "Data" );
		QModelIndex system = blockIndex( systemBlock );
		quint32 bodyId = nif->get<quint32>( object, "Body ID" );
		int node = ownerNode( object );
		QByteArray bytes = system.isValid() ? nif->get<QByteArray>( system, "Binary Data" ) : QByteArray();
		// A skeleton points every bone's collision object at ONE ragdoll system,
		// so this ran the full packfile decode once per bone (41x on a brahmin).
		// The cache is keyed on the data, so shared systems decode once.
		const HknpSystem & sys = hknpDecodeCached( bytes );
		quint32 mat = 0, collLayer = 1;
		int nv = 0, nt = 0;
		QString shape = sys.valid ? compiledShapeSummary( sys, bodyId, &mat, &nv, &nt ) : tr( "Invalid packfile" );
		float m = 0.0f;
		if ( int( bodyId ) < sys.bodyPhys.size() ) {
			collLayer = sys.bodyPhys.at( int( bodyId ) ).layer;
			// this body's own mass, not the system's first body's
			m = sys.dynamic ? sys.bodyPhys.at( int( bodyId ) ).mass : 0.0f;
		} else if ( sys.dynamic ) {
			m = sys.mass;
		}

		auto * item = new CollisionTreeItem( tree );
		item->setText( 0, nodeName( node ) );
		item->setText( 1, shape );
		item->setText( 2, collisionLayerName( collLayer ) );
		item->setText( 3, materialText( mat ) );
		item->setToolTip( 3, materialToolTip( mat ) );
		item->setText( 4, tr( "%1 kg" ).arg( m, 0, 'f', 1 ) );
		item->setText( 5, nif->isNiBlock( system, "bhkRagdollSystem" )
			? tr( "RAGDOLL" ) : tr( "COMPILED" ) );
		item->setForeground( 5, QColor( 226, 165, 61 ) );
		setBoneCell( item, boneRole( boneRoles, node, systemBlock ) );
		setItemRoles( item, objectBlock, node, systemBlock, -1, -1, bodyId, true );
		item->setToolTip( 1, sys.valid ? QString() : sys.error );
		if ( nt > 500 || bytes.size() > 128 * 1024 ) {
			item->setIcon( 1, style()->standardIcon( QStyle::SP_MessageBoxWarning ) );
			item->setForeground( 1, QColor( 232, 169, 63 ) );
			item->setToolTip( 1, tr( "%1\nBudget warning: %2 triangles, %3 packfile bytes. Consider decimating this collision." )
				.arg( item->toolTip( 1 ) ).arg( nt ).arg( bytes.size() ).trimmed() );
		}
		int shapeNumber = 0;
		for ( int shapeIndex = 0; shapeIndex < sys.shapes.size(); shapeIndex++ ) {
			const HknpShape & s = sys.shapes.at( shapeIndex );
			if ( s.bodyId >= 0 && quint32( s.bodyId ) != bodyId )
				continue;
			auto * child = new CollisionTreeItem( item );
			child->setText( 0, tr( "Shape %1" ).arg( ++shapeNumber ) );
			child->setText( 1, compiledShapeText( s ) );
			child->setText( 2, collisionLayerName( collLayer ) );
			child->setText( 3, materialText( s.materialCRC ) );
			child->setToolTip( 3, materialToolTip( s.materialCRC ) );
			child->setText( 5, tr( "COMPILED" ) );
			child->setForeground( 5, QColor( 226, 165, 61 ) );
			if ( s.tris.size() > 500 ) {
				child->setIcon( 1, style()->standardIcon( QStyle::SP_MessageBoxWarning ) );
				child->setForeground( 1, QColor( 232, 169, 63 ) );
				child->setToolTip( 1, tr( "%1 triangles; consider decimating this collision shape." ).arg( s.tris.size() ) );
			}
			setItemRoles( child, objectBlock, node, systemBlock, -1, -1,
				bodyId, true, shapeIndex );
		}
		item->setExpanded( expandedObjects.contains( objectBlock ) );
		totalVerts += nv;
		totalTris += nt;
		totalBytes += bytes.size();
	}

	void addEditable( int objectBlock, const QHash<int, QString> & boneRoles,
		int & totalVerts, int & totalTris )
	{
		QModelIndex object = blockIndex( objectBlock );
		int bodyBlock = nif->getLink( object, "Body" );
		QModelIndex body = blockIndex( bodyBlock );
		int shapeBlock = body.isValid() ? nif->getLink( body, "Shape" ) : -1;
		int node = ownerNode( object );
		QModelIndex info = body.isValid() ? nif->getIndex( body, "Rigid Body Info" ) : QModelIndex();
		QModelIndex filter = bhkGetHavokFilter( nif, info );
		quint32 collLayer = filter.isValid() ? nif->get<quint32>( filter, "Layer" ) : 1;
		float m = info.isValid() ? nif->get<float>( info, "Mass" ) : 0.0f;
		if ( collLayer == 0 ) collLayer = ( info.isValid() && nif->get<quint32>( info, "Motion System" ) == 3 ) ? 10u : 1u;
		quint32 mat = 0;
		int nv = 0, nt = 0;
		QString shape = editableShapeSummary( shapeBlock, &mat, &nv, &nt );

		auto * item = new CollisionTreeItem( tree );
		item->setText( 0, nodeName( node ) );
		item->setText( 1, shape );
		item->setText( 2, collisionLayerName( collLayer ) );
		item->setText( 3, materialText( mat ) );
		item->setToolTip( 3, materialToolTip( mat ) );
		item->setText( 4, tr( "%1 kg" ).arg( m, 0, 'f', 1 ) );
		item->setText( 5, tr( "EDITABLE" ) );
		item->setForeground( 5, QColor( 90, 169, 230 ) );
		setBoneCell( item, boneRole( boneRoles, node, -1 ) );
		if ( nt > 500 ) {
			item->setIcon( 1, style()->standardIcon( QStyle::SP_MessageBoxWarning ) );
			item->setForeground( 1, QColor( 232, 169, 63 ) );
			item->setToolTip( 1, tr( "%1 triangles; consider using Optimize Source Mesh." ).arg( nt ) );
		}
		setItemRoles( item, objectBlock, node, -1, bodyBlock, shapeBlock, 0, false );
		appendEditableShapeItems( item, shapeBlock, objectBlock, node, bodyBlock,
			collLayer, totalVerts, totalTris );
		item->setExpanded( expandedObjects.contains( objectBlock ) );
	}

	void rebuild()
	{
		if ( updating || !nif )
			return;
		updating = true;
		int selectedBlock = tree->currentItem() ? tree->currentItem()->data( 0, ObjectBlockRole ).toInt() : -1;
		tree->clear();
		int compiled = 0, editable = 0, totalVerts = 0, totalTris = 0, totalBytes = 0;
		QSet<int> systems;
		const QHash<int, QString> boneRoles = collectBoneRoles();
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );
			if ( nif->blockInherits( i, "bhkNPCollisionObject" ) ) {
				systems.insert( nif->getLink( i, "Data" ) );
				addCompiled( b, boneRoles, totalVerts, totalTris, totalBytes );
				compiled++;
			} else if ( nif->blockInherits( i, "bhkCollisionObject" ) ) {
				addEditable( b, boneRoles, totalVerts, totalTris );
				editable++;
			}
		}
		inventoryHeader->setText( tr( "Collision in file  -  %1 system(s), %2 body/bodies" )
			.arg( systems.size() + editable ).arg( compiled + editable ) );
		for ( int r = 0; r < tree->topLevelItemCount(); r++ ) {
			QTreeWidgetItem * item = tree->topLevelItem( r );
			if ( item->data( 0, ObjectBlockRole ).toInt() == selectedBlock ) {
				tree->setCurrentItem( item );
				break;
			}
		}
		QString budget = tr( "%1 editable | %2 compiled | %3 verts | %4 tris | %5 packfile bytes" )
			.arg( editable ).arg( compiled ).arg( totalVerts ).arg( totalTris ).arg( totalBytes );
		bool overBudget = totalTris > 2000 || totalBytes > 256 * 1024;
		if ( overBudget ) budget += tr( " | Budget warning - decimation recommended" );
		summary->setText( budget );
		summary->setStyleSheet( overBudget
			? QStringLiteral( "QLabel { color:%1; padding:3px; }" ).arg( wwSkinColor( "accentText" ) )
			: QStringLiteral( "QLabel { color: %1; padding: 3px; }" ).arg( wwSkinColor( "textMuted" ) ) );
		updating = false;
		refreshSimPins();
		updateDetails();
		// the list just changed, so whether a body exists to add a shape to may have
		updateShapeButtonState();
	}

	/*! Put each body's PIN on its own row in the tree.
	 *
	 * The simulator carried a second list of the same bodies purely to hang a
	 * checkbox off, one panel below a tree that already showed every one of them
	 * with its bone, shape, layer, material, mass and state. The only thing that
	 * list added was the checkbox, so the checkbox came here instead.
	 *
	 * Only rows belonging to the system actually being simulated get one: the
	 * others have no body to pin, and a checkbox that does nothing is worse than
	 * no checkbox.
	 */
	void refreshSimPins()
	{
		if ( !ogl )
			return;
		PhysicsPreview & pv = ogl->physicsSim();
		const bool live = pv.active();
		const int sysBlock = pv.systemBlock();
		// itemChanged fires on every setCheckState below, so the handler has to
		// be told this is us and not the user
		const bool wasUpdating = updating;
		updating = true;
		for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
			QTreeWidgetItem * item = tree->topLevelItem( i );
			bool ok = false;
			const int body = item->data( 0, BodyIdRole ).toInt( &ok );
			const bool mine = live && ok && item->data( 0, SystemBlockRole ).toInt() == sysBlock
				&& body >= 0 && body < pv.bodyCount();
			if ( !mine ) {
				item->setData( 0, Qt::CheckStateRole, QVariant() );
				continue;
			}
			item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
			item->setCheckState( 0, pv.sim().bodies().at( body ).pinned
				? Qt::Checked : Qt::Unchecked );
			item->setToolTip( 0, tr( "Tick to pin this body where it is. The same pin the "
									 "right mouse button sets in the viewport." ) );
		}
		updating = wasUpdating;
	}

	/*! Highlight the row that speaks for this block, if any.
	 *
	 * Matches on the collision object, its shape, and its OWNER NODE - the last
	 * is the one that makes bone -> collision work, since selecting a bone
	 * selects a node block that no collision role otherwise mentions. A shape
	 * child wins over its parent so that clicking a decoded shape block lands on
	 * the shape row rather than the body row.
	 */
	void selectRowForBlock( int block )
	{
		if ( block < 0 )
			return;
		QTreeWidgetItem * match = nullptr;
		for ( int r = 0; r < tree->topLevelItemCount() && !match; r++ ) {
			QTreeWidgetItem * top = tree->topLevelItem( r );
			for ( int c = 0; c < top->childCount(); c++ ) {
				QTreeWidgetItem * child = top->child( c );
				if ( child->data( 0, ShapeBlockRole ).toInt() == block ) { match = child; break; }
			}
			if ( match )
				break;
			if ( top->data( 0, ObjectBlockRole ).toInt() == block
				|| top->data( 0, NodeBlockRole ).toInt() == block
				|| top->data( 0, BodyBlockRole ).toInt() == block
				|| top->data( 0, ShapeBlockRole ).toInt() == block )
				match = top;
		}
		if ( !match || match == tree->currentItem() )
			return;
		syncingSelection = true;
		tree->setCurrentItem( match );
		tree->scrollToItem( match );
		syncingSelection = false;
	}

	void queueRebuild()
	{
		if ( rebuildQueued )
			return;
		rebuildQueued = true;
		QTimer::singleShot( 0, this, [this]() {
			rebuildQueued = false;
			if ( dock->isVisible() )
				rebuild();
		} );
	}

	void updateDetails()
	{
		QTreeWidgetItem * item = tree->currentItem();
		bool have = item != nullptr;
		bool compiled = have && item->data( 0, CompiledRole ).toBool();
		decompileSelectedAction->setEnabled( compiled );
		compileSelectedAction->setEnabled( have && !compiled );
		primaryCollisionAction->setDefaultAction( compiled ? decompileSelectedAction : compileSelectedAction );
		primaryCollisionAction->setEnabled( have );
		/* The editor shows for a COMPILED body too, mostly greyed.
		 *
		 * It used to be hidden outright, which said "there is nothing here" when
		 * what is true is "two of these are editable and the rest are read-outs".
		 * The greyed rows are worth seeing: several of them are the decode's
		 * substitutions rather than stored values, and a user comparing a
		 * compiled body against an editable one has no other way to tell.
		 */
		physicsEditorBody->setVisible( have );
		compiledSummaryWidget->setVisible( have && compiled );
		emptyPhysicsWidget->setVisible( !have );
		advancedSection->setVisible( have && !compiled );
		mass->setEnabled( have && !compiled );
		friction->setEnabled( have && !compiled );
		restitution->setEnabled( have && !compiled );
		layer->setEnabled( have && !compiled );
		material->setEnabled( have && !compiled );
		// it computes and writes MASS, which a compiled body does not store as an
		// editable value - so it belongs with the read-only set, not offering a
		// dialog that refuses once the user has filled it in
		massFromMaterialButton->setEnabled( have && !compiled );
		const QList<QWidget *> physicsEditors = { linearDamping, angularDamping,
			maxLinearVelocity, maxAngularVelocity, motionSystem, qualityType, solverDeactivation,
			deactivatorType, keyframed, linkedGroup, collisionWithinGroup, wind, filterGroup,
			centerX, centerY, centerZ, inertiaX, inertiaY, inertiaZ, penetrationDepth };
		for ( QWidget * editor : physicsEditors )
			editor->setEnabled( have && !compiled );
		if ( !have ) {
			physicsGroup->setTitle( tr( "Selected collision properties" ) );
			physicsHint->setText( tr( "Select a collision row to inspect or edit it." ) );
			return;
		}

		physicsGroup->setTitle( tr( "Selected collision properties - %1" ).arg( item->text( 0 ) ) );
		QSignalBlocker bm( mass ), bf( friction ), br( restitution ), bl( layer );
		QSignalBlocker bld( linearDamping ), bad( angularDamping ), bml( maxLinearVelocity ),
			bma( maxAngularVelocity ), bmo( motionSystem ), bq( qualityType ), bs( solverDeactivation ),
			bmat( material ), bdt( deactivatorType ), bk( keyframed ), blg( linkedGroup ),
			bcg( collisionWithinGroup ), bw( wind ), bfg( filterGroup ), bcx( centerX ), bcy( centerY ),
			bcz( centerZ ), bix( inertiaX ), biy( inertiaY ), biz( inertiaZ ), bpd( penetrationDepth );
		if ( compiled ) {
			int systemBlock = item->data( 0, SystemBlockRole ).toInt();
			quint32 bodyId = item->data( 0, BodyIdRole ).toUInt();
			QModelIndex system = blockIndex( systemBlock );
			// name the QByteArray: the cached decode keys on the data, and passing a
			// temporary trips -Wdangling-reference even though the returned
			// reference points into the cache rather than into the argument
			const QByteArray systemBytes = nif->get<QByteArray>( system, "Binary Data" );
			const HknpSystem & sys = hknpDecodeCached( systemBytes );
			HknpBodyPhys phys = int( bodyId ) < sys.bodyPhys.size() ? sys.bodyPhys.at( int( bodyId ) ) : HknpBodyPhys();
			// per-body: on a ragdoll every bone has its own mass and damping
			mass->setValue( phys.mass );
			friction->setValue( phys.friction );
			restitution->setValue( phys.restitution );
			selectComboValue( layer, phys.layer );
			linearDamping->setValue( phys.linDamping );
			angularDamping->setValue( phys.angDamping );
			maxLinearVelocity->setValue( phys.maxLinVelocity );
			maxAngularVelocity->setValue( phys.maxAngVelocity );
			selectComboValue( motionSystem, phys.hasMotion ? 3u : 5u );
			selectComboValue( qualityType, phys.hasMotion ? 4u : 0u );
			selectComboValue( solverDeactivation, phys.hasMotion ? 2u : 1u );
			selectComboValue( deactivatorType, 1u );
			keyframed->setChecked( false ); linkedGroup->setChecked( phys.filterFlags & 0x80u );
			collisionWithinGroup->setChecked( !( phys.filterFlags & 0x40u ) ); wind->setChecked( false );
			filterGroup->setValue( phys.filterGroup );
			centerX->setValue( phys.position[0] ); centerY->setValue( phys.position[1] ); centerZ->setValue( phys.position[2] );
			// the packfile holds inverse inertia; this field shows the real tensor
			auto trueI = []( float v ) { return ( v > 1.0e-12f ) ? 1.0f / v : 0.0f; };
			inertiaX->setValue( trueI( phys.invInertia[0] ) );
			inertiaY->setValue( trueI( phys.invInertia[1] ) );
			inertiaZ->setValue( trueI( phys.invInertia[2] ) );
			penetrationDepth->setValue( 0.15 );
			quint32 materialValue = 0;
			int shapeIndex = item->data( 0, ShapeIndexRole ).toInt();
			if ( shapeIndex >= 0 && shapeIndex < sys.shapes.size() ) {
				materialValue = sys.shapes.at( shapeIndex ).materialCRC;
			} else {
				for ( const HknpShape & shape : sys.shapes ) {
					if ( shape.bodyId < 0 || quint32( shape.bodyId ) == bodyId ) { materialValue = shape.materialCRC; break; }
				}
			}
			selectComboValue( material, materialValue, materialText( materialValue ) );

			/* Now that the body is decoded, re-enable the two things that are
			 * really STORED on it. Everything else above stays grey because it
			 * is a substitution — Motion System and Quality Type come from
			 * hasMotion, Penetration Depth is the literal 0.15, Keyframed and
			 * Wind are always false — and writing a substitution back would
			 * invent data the file never had.
			 */
			friction->setEnabled( true );
			restitution->setEnabled( true );
			/* The filter only when it is stored. A layer of 0 is real, and the
			 * decode substitutes 1 or 10 so the row reads usefully; offering
			 * that substitution as an editable value would write it in.
			 */
			const bool storedFilter = phys.hasStoredFilter;
			layer->setEnabled( storedFilter );
			linkedGroup->setEnabled( storedFilter );
			collisionWithinGroup->setEnabled( storedFilter );
			filterGroup->setEnabled( storedFilter );
			physicsHint->setText( storedFilter
				? tr( "Friction, restitution and the collision filter edit this body inside the "
					"compiled system. Everything else here is read-only." )
				: tr( "Friction and restitution edit this body inside the compiled system. This "
					"body stores no collision filter, so the layer shown is a default." ) );

			compiledSummaryLabel->setText( tr( "<b>%1 - %2</b><br>%3 | %4 | %5 | friction %6 | restitution %7" )
				.arg( item->text( 0 ), item->text( 1 ), item->text( 2 ), materialText( materialValue ), item->text( 4 ) )
				.arg( phys.friction, 0, 'f', 3 ).arg( phys.restitution, 0, 'f', 3 ) );
		} else {
			QModelIndex body = blockIndex( item->data( 0, BodyBlockRole ).toInt() );
			QModelIndex info = nif->getIndex( body, "Rigid Body Info" );
			QModelIndex filter = bhkGetHavokFilter( nif, info );
			mass->setValue( nif->get<float>( info, "Mass" ) );
			friction->setValue( nif->get<float>( info, "Friction" ) );
			restitution->setValue( nif->get<float>( info, "Restitution" ) );
			quint32 editableLayer = nif->get<quint32>( filter, "Layer" );
			if ( editableLayer == 0 ) editableLayer = nif->get<quint32>( info, "Motion System" ) == 3 ? 10u : 1u;
			selectComboValue( layer, editableLayer );
			linearDamping->setValue( nif->get<float>( info, "Linear Damping" ) );
			angularDamping->setValue( nif->get<float>( info, "Angular Damping" ) );
			maxLinearVelocity->setValue( nif->get<float>( info, "Max Linear Velocity" ) );
			maxAngularVelocity->setValue( nif->get<float>( info, "Max Angular Velocity" ) );
			selectComboValue( motionSystem, nif->get<quint32>( info, "Motion System" ) );
			selectComboValue( qualityType, nif->get<quint32>( info, "Quality Type" ) );
			selectComboValue( solverDeactivation, nif->get<quint32>( info, "Solver Deactivation" ) );
			selectComboValue( deactivatorType, nif->get<quint32>( info, "Deactivator Type" ) );
			quint32 filterFlags = nif->get<quint32>( filter, "Flags" );
			linkedGroup->setChecked( filterFlags & 0x80u );
			collisionWithinGroup->setChecked( !( filterFlags & 0x40u ) );
			filterGroup->setValue( nif->get<quint32>( filter, "Group" ) );
			keyframed->setChecked( nif->get<quint32>( info, "Motion System" ) == 6u );
			wind->setChecked( nif->get<quint32>( body, "Body Flags" ) & 1u );
			Vector4 center = nif->get<Vector4>( info, "Center" );
			centerX->setValue( center[0] ); centerY->setValue( center[1] ); centerZ->setValue( center[2] );
			QModelIndex inertia = nif->getIndex( info, "Inertia Tensor" );
			inertiaX->setValue( nif->get<float>( inertia, "m11" ) ); inertiaY->setValue( nif->get<float>( inertia, "m22" ) ); inertiaZ->setValue( nif->get<float>( inertia, "m33" ) );
			penetrationDepth->setValue( nif->get<float>( info, "Penetration Depth" ) );
			int shapeBlock = firstLeafShape( item->data( 0, ShapeBlockRole ).toInt() );
			QModelIndex shape = blockIndex( shapeBlock );
			selectComboValue( material, shape.isValid() ? nif->get<quint32>( shape, "Material" ) : 0 );
			physicsHint->setText( tr( "Values live-edit the selected bhkRigidBody." ) );
		}
	}

	/*! Edit a COMPILED body in place, without decompiling it.
	 *
	 *  hknpEncodeSystem reproduces 810 of 822 stock FO4 packfiles byte for byte,
	 *  and until now it was reachable only from the CLI's own round-trip
	 *  self-test. The one production write went through hknpEncodeCompressedMesh,
	 *  which flattens a system to a single static body with one triangle mesh —
	 *  so changing a compiled body's friction meant Decompile, edit, Compile, and
	 *  losing every other body, the compounds, the primitives, the constraints
	 *  and the ragdoll skeleton on the way.
	 *
	 *  Nothing is decompiled here. The system is decoded, ONE modelled field is
	 *  changed, and it is encoded again — so every opaque region the decoder
	 *  carries verbatim goes back exactly as it came.
	 *
	 *  ONLY THE FIELDS THAT ARE REALLY STORED. Most of what the compiled display
	 *  shows is a substitution: Motion System and Quality Type are derived from
	 *  hasMotion, Penetration Depth is the literal 0.15, Keyframed and Wind are
	 *  always false. Writing those back would invent data. Friction and
	 *  restitution are stored (as float16, so an untouched value re-encodes
	 *  exactly), and the collision filter is stored only when hasStoredFilter —
	 *  a layer of 0 is real, and the decode substitutes 1 or 10 so the user sees
	 *  something useful, which must not then be written back as though it were
	 *  the file's own value.
	 *
	 *  THE GUARD IS A BYTE COMPARISON, not a proxy. Before any edit is applied,
	 *  the untouched decode is re-encoded and compared with the bytes on disk. If
	 *  the system is one of the twelve that does not reproduce exactly, the edit
	 *  is refused rather than silently rewriting the other differences too.
	 */
	void applyCompiledPhysics()
	{
		QTreeWidgetItem * item = tree->currentItem();
		if ( updating || !item || !item->data( 0, CompiledRole ).toBool() )
			return;
		const int systemBlock = item->data( 0, SystemBlockRole ).toInt();
		const quint32 bodyId = item->data( 0, BodyIdRole ).toUInt();
		const QModelIndex system = blockIndex( systemBlock );
		if ( !system.isValid() )
			return;
		const QByteArray systemBytes = nif->get<QByteArray>( system, "Binary Data" );
		HknpSystem sys = hknpDecodeCached( systemBytes );		// a copy: this one gets edited
		if ( !sys.valid || int( bodyId ) >= sys.bodyPhys.size() )
			return;

		// does this system reproduce at all? if not, an edit cannot be isolated
		QString err;
		const QByteArray probe = hknpEncodeSystem( sys, &err );
		if ( probe != systemBytes ) {
			physicsHint->setText( tr( "This system does not re-encode byte-for-byte, so an edit "
				"cannot be applied without rewriting other differences too." ) );
			updating = true;
			updateDetails();		// put the widgets back to what is in the file
			updating = false;
			return;
		}

		HknpBodyPhys & b = sys.bodyPhys[int( bodyId )];
		const float wantFriction = float( friction->value() );
		const float wantRestitution = float( restitution->value() );
		b.friction = wantFriction;
		b.restitution = wantRestitution;
		if ( b.hasStoredFilter ) {
			quint32 packed = b.packedFilter & ~0x000000ffu;
			const QVariant chosen = layer->currentData();
			packed |= ( chosen.isValid() ? chosen.toUInt() : b.layer ) & 0xffu;
			quint32 flags = ( b.packedFilter >> 8 ) & 0xffu;
			flags &= ~0xc0u;
			if ( linkedGroup->isChecked() ) {
				flags |= 0x80u;
				if ( !collisionWithinGroup->isChecked() )
					flags |= 0x40u;
			}
			packed = ( packed & ~0x0000ff00u ) | ( flags << 8 );
			packed = ( packed & 0x0000ffffu ) | ( quint32( filterGroup->value() ) << 16 );
			b.packedFilter = packed;
		}

		const QByteArray bytes = hknpEncodeSystem( sys, &err );
		if ( bytes.isEmpty() ) {
			physicsHint->setText( tr( "The edit could not be encoded: %1" ).arg( err ) );
			return;
		}
		if ( bytes == systemBytes ) {
			physicsHint->setText( tr( "No change." ) );
			return;
		}
		/* Re-decode and check the field actually landed. The self-check the
		 * Compile path uses counts triangles, which cannot fail on a physics
		 * edit; this reads back the value that was asked for.
		 */
		const HknpSystem back = hknpDecode( bytes );
		if ( !back.valid || int( bodyId ) >= back.bodyPhys.size()
			|| std::fabs( back.bodyPhys.at( int( bodyId ) ).friction - wantFriction ) > 1.0e-2f ) {
			physicsHint->setText( tr( "The edit did not survive a re-read and was not applied." ) );
			return;
		}

		nifSnapshotOp( nif, tr( "Edit compiled collision body" ), [&]() {
			nif->set<QByteArray>( QModelIndex( system ), "Binary Data", bytes );
		} );
		physicsHint->setText( tr( "Body %1 updated in place; the rest of the system is unchanged." )
			.arg( bodyId ) );
		queueRebuild();
	}

	void applyLayerSelection( int row )
	{
		if ( updating || row < 0 || !tree->currentItem() )
			return;
		if ( tree->currentItem()->data( 0, CompiledRole ).toBool() ) {
			applyCompiledPhysics();
			return;
		}
		QVariant selected = layer->itemData( row );
		if ( !selected.isValid() )
			return;
		QModelIndex body = blockIndex( tree->currentItem()->data( 0, BodyBlockRole ).toInt() );
		QModelIndex info = nif->getIndex( body, "Rigid Body Info" );
		QModelIndex filter = bhkGetHavokFilter( nif, info );
		QModelIndex layerIndex = nif->getIndex( filter, "Layer" );
		if ( !layerIndex.isValid() )
			return;
		quint32 value = selected.toUInt();
		/* Snapshotted, like every other write in this panel.
		 *
		 * NifModel::set does not push an undo command by itself, so these live
		 * editors were writing straight through the model while Compile, Import
		 * Donor and Apply Safe Fixes in the same panel all went through
		 * nifSnapshotOp. Undo therefore looked like it worked and then reverted
		 * whichever of THOSE ran last, silently keeping the edit made here.
		 *
		 * One entry per edit rather than per pixel, because the spins are
		 * connected to editingFinished and the combos to activation.
		 */
		bool ok = false;
		nifSnapshotOp( nif, tr( "Set collision layer" ), [&]() {
			// both copies: the block's own flattened filter as well as the
			// Rigid Body Info one, or the viewport keeps colouring by the old
			// layer and the file stores two disagreeing values
			ok = bhkSetFilterField( nif, body, QStringLiteral( "Layer" ), value ) > 0;
		} );
		if ( ok ) {
			physicsHint->setText( tr( "Collision layer set to %1." ).arg( collisionLayerName( value ) ) );
			queueRebuild();
		}
	}

	void applyMaterialSelection( int row )
	{
		if ( updating || row < 0 || !tree->currentItem()
			|| tree->currentItem()->data( 0, CompiledRole ).toBool() )
			return;
		QVariant selected = material->itemData( row );
		if ( !selected.isValid() )
			return;
		int shapeBlock = firstLeafShape( tree->currentItem()->data( 0, ShapeBlockRole ).toInt() );
		QModelIndex shape = blockIndex( shapeBlock );
		QModelIndex materialIndex = nif->getIndex( shape, "Material" );
		if ( !materialIndex.isValid() )
			return;
		quint32 value = selected.toUInt();
		bool ok = false;			// undoable — see applyLayerSelection
		nifSnapshotOp( nif, tr( "Set collision material" ), [&]() {
			ok = nif->set<quint32>( materialIndex, value );
		} );
		if ( ok ) {
			physicsHint->setText( tr( "Material set to %1." ).arg( materialText( value ) ) );
			queueRebuild();
		}
	}

	void applyPhysics()
	{
		if ( updating || !tree->currentItem() )
			return;
		// a compiled body is edited in its packfile, not through NIF blocks
		if ( tree->currentItem()->data( 0, CompiledRole ).toBool() ) {
			applyCompiledPhysics();
			return;
		}
		QModelIndex body = blockIndex( tree->currentItem()->data( 0, BodyBlockRole ).toInt() );
		QModelIndex info = nif->getIndex( body, "Rigid Body Info" );
		if ( !info.isValid() )
			return;
		/* READ EVERY WIDGET FIRST, then write. The values must not be read from
		 * inside the nifSnapshotOp lambda.
		 *
		 * nifSnapshotOp serialises the whole model before running its operation,
		 * and that pass is enough to put the panel's own editors back to what is
		 * still in the file — so a lambda that asks `mass->value()` gets the OLD
		 * mass and writes it straight back. Every field here was doing that. The
		 * edit produced a perfectly good undo step containing no change at all,
		 * which is worse than not working, because the undo history says
		 * something happened.
		 *
		 * applyLayerSelection above never had the bug: it takes `quint32 value`
		 * out of the combo before it opens the snapshot. This is that, for the
		 * other twenty fields.
		 *
		 * Found by tests/spells/collision_undo.sh, which sets the mass spin to
		 * 3.5 and reads the model back.
		 */
		const float wantMass = float( mass->value() );
		const float wantFriction = float( friction->value() );
		const float wantRestitution = float( restitution->value() );
		const float wantLinearDamping = float( linearDamping->value() );
		const float wantAngularDamping = float( angularDamping->value() );
		const float wantMaxLinear = float( maxLinearVelocity->value() );
		const float wantMaxAngular = float( maxAngularVelocity->value() );
		const float wantPenetration = float( penetrationDepth->value() );
		const Vector4 wantCenter( float( centerX->value() ), float( centerY->value() ),
								  float( centerZ->value() ), 0.0f );
		const float wantInertiaX = float( inertiaX->value() );
		const float wantInertiaY = float( inertiaY->value() );
		const float wantInertiaZ = float( inertiaZ->value() );
		const quint32 wantSolver = solverDeactivation->currentData().toUInt();
		const quint32 wantDeactivator = deactivatorType->currentData().toUInt();
		const quint32 wantFilterGroup = quint32( filterGroup->value() );
		const bool wantWind = wind->isChecked();
		const bool wantLinkedGroup = linkedGroup->isChecked();
		const bool wantWithinGroup = collisionWithinGroup->isChecked();
		quint32 wantMotion = motionSystem->currentData().toUInt();
		quint32 wantQuality = qualityType->currentData().toUInt();
		if ( keyframed->isChecked() ) { wantMotion = 6u; wantQuality = 2u; }
		else if ( wantMotion == 6u ) {
			wantMotion = wantMass > 0.0f ? 3u : 5u;
			wantQuality = wantMass > 0.0f ? 4u : 0u;
		}

		/* Every field in ONE undo entry — see applyLayerSelection for why these
		 * needed wrapping at all. One entry for the lot rather than one per
		 * field, because a single editingFinished can legitimately rewrite
		 * several of them (keyframed forces Motion System and Quality Type).
		 */
		nifSnapshotOp( nif, tr( "Edit rigid body" ), [&]() {
			QModelIndex filter = bhkGetHavokFilter( nif, info );
			nif->set<float>( info, "Mass", wantMass );
			nif->set<float>( info, "Friction", wantFriction );
			nif->set<float>( info, "Restitution", wantRestitution );
			nif->set<float>( info, "Linear Damping", wantLinearDamping );
			nif->set<float>( info, "Angular Damping", wantAngularDamping );
			nif->set<float>( info, "Max Linear Velocity", wantMaxLinear );
			nif->set<float>( info, "Max Angular Velocity", wantMaxAngular );
			nif->set<quint32>( info, "Motion System", wantMotion );
			nif->set<quint32>( info, "Quality Type", wantQuality );
			nif->set<quint32>( info, "Solver Deactivation", wantSolver );
			nif->set<quint32>( info, "Deactivator Type", wantDeactivator );
			nif->set<float>( info, "Penetration Depth", wantPenetration );
			nif->set<Vector4>( info, "Center", wantCenter );
			QModelIndex inertia = nif->getIndex( info, "Inertia Tensor" );
			if ( inertia.isValid() ) {
				nif->set<float>( inertia, "m11", wantInertiaX );
				nif->set<float>( inertia, "m22", wantInertiaY );
				nif->set<float>( inertia, "m33", wantInertiaZ );
			}
			quint32 bodyFlags = nif->get<quint32>( body, "Body Flags" );
			bodyFlags = wantWind ? ( bodyFlags | 1u ) : ( bodyFlags & ~1u );
			nif->set<quint32>( body, "Body Flags", bodyFlags );
			if ( filter.isValid() ) {
				quint32 flags = nif->get<quint32>( filter, "Flags" ) & ~0xc0u;
				if ( wantLinkedGroup ) {
					flags |= 0x80u;
					if ( !wantWithinGroup ) flags |= 0x40u;
				}
				// both stored copies, same reason as applyLayerSelection
				bhkSetFilterField( nif, body, QStringLiteral( "Flags" ), flags );
				bhkSetFilterField( nif, body, QStringLiteral( "Group" ), wantFilterGroup );
			}
		} );
		if ( ogl ) {
			ogl->flush();
			ogl->update();
		}
		queueRebuild();
	}

	int selectedTargetNode() const
	{
		if ( tree->currentItem() ) {
			int node = tree->currentItem()->data( 0, NodeBlockRole ).toInt();
			if ( nif->isValidBlockNumber( node ) ) return node;
		}
		QModelIndex source = currentSource();
		int block = nif->getBlockNumber( source );
		if ( nif->blockInherits( source, "NiNode" ) ) return block;
		if ( nif->blockInherits( source, { "BSGeometry", "BSTriShape", "NiTriBasedGeom" } ) )
			return nif->getParent( block );
		return -1;
	}

	void collisionToBSTriShape()
	{
		convertCollisionToMesh( selectedCollisionMesh(), selectedTargetNode(), true );
	}

	/*! One shape, back to geometry, under \a targetNode.
	 *
	 *  Split out of the menu route so a shape row dragged into the Block List can
	 *  reach it: the drag names the shape and the node it landed on, where the
	 *  menu asks the tree what is selected. The making of the mesh is the same
	 *  either way, which is the point of the split.
	 *
	 *  \return the empty string on success, or why not. \a modal is for the menu
	 *  route, which has a window to put a message box in front of; a drop says it
	 *  in the status bar instead.
	 */
public:
	QString convertShapeToMesh( qint32 shape, int targetNode )
	{
		CollisionMesh mesh;
		appendEditableMesh( shape, mesh );
		return convertCollisionToMesh( mesh, targetNode, false );
	}

private:
	QString convertCollisionToMesh( CollisionMesh mesh, int targetNode, bool modal )
	{
		if ( mesh.verts.size() < 3 || mesh.tris.isEmpty() ) {
			const QString why = tr( "That collision has no usable geometry." );
			if ( modal )
				QMessageBox::information( this, tr( "Collision to BSTriShape" ), why );
			return why;
		}
		if ( !nif->isValidBlockNumber( targetNode ) ) {
			const QString why = tr( "That collision has no node to put the mesh under." );
			if ( modal )
				QMessageBox::warning( this, tr( "Collision to BSTriShape" ), why );
			return why;
		}
		QVector<Vector3> normals( mesh.verts.size(), Vector3() );
		for ( const Triangle & tri : mesh.tris ) {
			Vector3 n = Vector3::crossproduct( mesh.verts[tri[1]] - mesh.verts[tri[0]], mesh.verts[tri[2]] - mesh.verts[tri[0]] );
			if ( n.length() > 1.0e-8f ) { n.normalize(); normals[tri[0]] += n; normals[tri[1]] += n; normals[tri[2]] += n; }
		}
		for ( Vector3 & n : normals ) { if ( n.length() > 1.0e-8f ) n.normalize(); else n = Vector3( 0, 0, 1 ); }
		QPersistentModelIndex created;
		nifSnapshotOp( nif, tr( "Collision to BSTriShape" ), [&, this]() {
			nif->holdUpdates( true );
			QModelIndex shape = nif->insertNiBlock( "BSTriShape" ); created = shape;
			nif->set<QString>( shape, "Name", QStringLiteral( "COL_PROXY_%1" ).arg( nodeName( targetNode ) ) );
			nif->set<quint32>( shape, "Flags", 14 );
			std::uint64_t vertexDesc = 0x0041B00000650407ULL;
			nif->set<BSVertexDesc>( shape, "Vertex Desc", vertexDesc );
			nif->set<quint32>( shape, "Num Triangles", quint32( mesh.tris.size() ) );
			nif->set<quint32>( shape, "Num Vertices", quint32( mesh.verts.size() ) );
			nif->set<quint32>( shape, "Data Size", quint32( mesh.verts.size() * 28 + mesh.tris.size() * 6 ) );
			nif->setState( BaseModel::Processing );
			QModelIndex vertexData = nif->getIndex( shape, "Vertex Data" ); nif->updateArraySize( vertexData );
			for ( int i = 0; i < mesh.verts.size(); i++ ) {
				QModelIndex vertex = nif->getIndex( vertexData, i );
				nif->set<Vector3>( vertex, "Vertex", mesh.verts.at( i ) );
				nif->set<HalfVector2>( vertex, "UV", HalfVector2( 0.0f, 0.0f ) );
				nif->set<ByteVector3>( vertex, "Normal", ByteVector3( normals.at( i ) ) );
			}
			QModelIndex triangleData = nif->getIndex( shape, "Triangles" ); nif->updateArraySize( triangleData );
			nif->setArray<Triangle>( triangleData, mesh.tris );
			BoundSphere bounds( mesh.verts, true ); bounds.update( nif, shape );
			nif->restoreState(); nif->holdUpdates( false );
			addLink( nif, blockIndex( targetNode ), "Children", nif->getBlockNumber( shape ) );
		} );
		if ( created.isValid() ) if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( QModelIndex( created ) );
		queueRebuild();
		return created.isValid() ? QString() : tr( "The mesh could not be written." );
	}

	static double recommendedDensity( const QString & materialName )
	{
		QString n = materialName.toLower();
		if ( n.contains( "metal" ) || n.contains( "armor" ) || n.contains( "weapon" ) ) return 7800.0;
		if ( n.contains( "stone" ) || n.contains( "concrete" ) || n.contains( "brick" ) ) return 2400.0;
		if ( n.contains( "glass" ) || n.contains( "ceramic" ) ) return 2500.0;
		if ( n.contains( "wood" ) ) return 550.0;
		if ( n.contains( "dirt" ) || n.contains( "sand" ) || n.contains( "gravel" ) ) return 1600.0;
		if ( n.contains( "rubber" ) ) return 1100.0;
		if ( n.contains( "plastic" ) ) return 950.0;
		if ( n.contains( "cloth" ) || n.contains( "paper" ) || n.contains( "carpet" ) ) return 300.0;
		if ( n.contains( "water" ) || n.contains( "organic" ) || n.contains( "skin" ) || n.contains( "meat" ) ) return 1000.0;
		return 1000.0;
	}

	void calculateMassFromMaterial()
	{
		if ( !tree->currentItem() || tree->currentItem()->data( 0, CompiledRole ).toBool() ) {
			QMessageBox::information( this, tr( "Mass from Material" ), tr( "Select editable collision first." ) ); return;
		}
		CollisionMesh mesh = selectedCollisionMesh();
		double gameVolume = meshVolumeGameUnits( mesh );
		double volume = gameVolume / std::pow( 69.99125, 3.0 );
		if ( !std::isfinite( volume ) || volume <= 1.0e-9 ) {
			QMessageBox::warning( this, tr( "Mass from Material" ),
				tr( "Could not derive a usable collision volume. The selected shape may be open, flat, or degenerate." ) ); return;
		}
		QString matName = material->currentText(); double suggested = recommendedDensity( matName ); bool ok = false;
		double density = QInputDialog::getDouble( this, tr( "Mass from Material" ),
			tr( "%1\nCollision volume: %2 m³\nDensity (kg/m³):" ).arg( matName ).arg( volume, 0, 'g', 6 ),
			suggested, 1.0, 50000.0, 1, &ok );
		if ( !ok ) return;
		double derivedMass = volume * density;
		mass->setValue( derivedMass ); applyPhysics();
		physicsHint->setText( tr( "Mass %1 kg = %2 m³ × %3 kg/m³" ).arg( derivedMass, 0, 'g', 6 ).arg( volume, 0, 'g', 6 ).arg( density, 0, 'g', 6 ) );
	}

	static void collectBranchBlocks( const NifModel * model, int block, QList<qint32> & blocks )
	{
		if ( !model->isValidBlockNumber( block ) || blocks.contains( block ) ) return;
		blocks.append( block );
		for ( int child : model->getChildLinks( block ) ) collectBranchBlocks( model, child, blocks );
	}

	void importDonorCollision()
	{
		int targetNode = selectedTargetNode();
		if ( !nif->isValidBlockNumber( targetNode ) ) {
			QMessageBox::information( this, tr( "Import Donor Collision" ), tr( "Select the target NiNode, BSTriShape, or collision row first." ) ); return;
		}
		QString fileName = QFileDialog::getOpenFileName( this, tr( "Choose Donor NIF" ), nif->getFolder(), tr( "NIF files (*.nif)" ) );
		if ( fileName.isEmpty() ) return;
		NifModel donor;
		if ( !donor.loadFromFile( fileName ) ) {
			QMessageBox::warning( this, tr( "Import Donor Collision" ), tr( "Could not load the donor NIF." ) ); return;
		}
		if ( donor.getBSVersion() != nif->getBSVersion() ) {
			QMessageBox::warning( this, tr( "Import Donor Collision" ), tr( "The donor uses a different Bethesda NIF version." ) ); return;
		}
		QStringList labels; QVector<int> objects;
		for ( int b = 0; b < donor.getBlockCount(); b++ ) {
			QModelIndex object = donor.getBlockIndex( b );
			if ( !donor.blockInherits( object, { "bhkCollisionObject", "bhkNPCollisionObject" } ) ) continue;
			int node = donor.getLink( object, "Target" ); if ( !donor.isValidBlockNumber( node ) ) node = donor.getParent( b );
			QString name = donor.isValidBlockNumber( node ) ? donor.get<QString>( donor.getBlockIndex( node ), "Name" ) : QString();
			labels.append( tr( "%1 — %2 [block %3]" ).arg( name.isEmpty() ? tr( "Unnamed" ) : name, donor.itemName( object ) ).arg( b ) );
			objects.append( b );
		}
		if ( objects.isEmpty() ) { QMessageBox::information( this, tr( "Import Donor Collision" ), tr( "The donor NIF contains no collision objects." ) ); return; }
		bool ok = false; QString chosen = QInputDialog::getItem( this, tr( "Import Donor Collision" ), tr( "Collision object:" ), labels, 0, false, &ok );
		if ( !ok ) return;
		int choice = labels.indexOf( chosen );
		if ( choice < 0 ) return;
		int donorObject = objects.at( choice ); QList<qint32> blocks; collectBranchBlocks( &donor, donorObject, blocks );
		QByteArray serialized; QBuffer writeBuffer( &serialized ); writeBuffer.open( QIODevice::WriteOnly ); QDataStream out( &writeBuffer );
		out << int( blocks.size() ); QMap<qint32, qint32> map;
		for ( int i = 0; i < blocks.size(); i++ ) map.insert( blocks.at( i ), nif->getBlockCount() + i );
		for ( int block : blocks ) { out << donor.itemName( donor.getBlockIndex( block ) ); donor.saveIndex( writeBuffer, donor.getBlockIndex( block ) ); }
		writeBuffer.close();
		QPersistentModelIndex imported;
		nifSnapshotOp( nif, tr( "Import donor collision" ), [&, this]() {
			QBuffer readBuffer( &serialized ); readBuffer.open( QIODevice::ReadOnly ); QDataStream in( &readBuffer ); int count = 0; in >> count;
			nif->holdUpdates( true );
			for ( int i = 0; i < count; i++ ) {
				QString type; in >> type; QModelIndex block = nif->insertNiBlock( type );
				if ( i == 0 ) imported = block;
				if ( !nif->loadAndMapLinks( readBuffer, block, map ) ) break;
			}
			nif->holdUpdates( false );
			QModelIndex target = blockIndex( targetNode ); QModelIndex old = blockIndex( nif->getLink( target, "Collision Object" ) );
			if ( old.isValid() ) { spRemoveBranch remove; remove.castIfApplicable( nif, old ); }
			if ( imported.isValid() ) {
				nif->setLink( QModelIndex( imported ), "Target", targetNode );
				nif->setLink( nif->getIndex( target, "Collision Object" ), nif->getBlockNumber( QModelIndex( imported ) ) );
			}
		} );
		if ( imported.isValid() ) if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( QModelIndex( imported ) );
		queueRebuild();
	}

	/*! What a Create acts on: the window's current block, or the selection.
	 *
	 *  The fallback is not belt and braces, it is the drop's only route. A drop
	 *  sets the block-list selection (setBlockListSelection) and asks the window
	 *  to follow, but the window's CURRENT ROW is not guaranteed to have caught
	 *  up by the time the create runs — and when it has not, this returned an
	 *  invalid index, spellSelectionRoots answered {-1}, the target list came out
	 *  empty and the create did nothing at all. Silently: the empty-target message
	 *  is behind a check that a drop never reaches.
	 *
	 *  Found by collision_drop.sh, whose synthesised drag has no real block-list
	 *  row behind it, which is exactly the case a real drag hides.
	 */
	QModelIndex currentSource() const
	{
		if ( auto * w = dynamic_cast<NifSkope *>( mw ) ) {
			const QModelIndex current = w->currentNifIndex();
			if ( current.isValid() )
				return current;
		}
		const QList<qint32> selected = blockListSelectionForSpells();
		if ( !selected.isEmpty() && nif )
			return nif->getBlockIndex( selected.first() );
		return QModelIndex();
	}

	void runSpell( const QString & id, const QModelIndex & index )
	{
		SpellPtr spell = SpellBook::lookup( id );
		if ( !spell ) {
			QMessageBox::warning( this, tr( "Collision Manager" ), tr( "The '%1' action is unavailable." ).arg( id ) );
			return;
		}
		if ( !spell->isApplicable( nif, index ) ) {
			QMessageBox::information( this, tr( "Collision Manager" ), tr( "Select a compatible block for '%1'." ).arg( spell->name() ) );
			return;
		}
		SpellBook book( nif, index );
		book.cast( nif, index, spell );
		queueRebuild();
	}

	void compileSelectedCollision()
	{
		QTreeWidgetItem * item = tree->currentItem();
		if ( !item || item->data( 0, CompiledRole ).toBool() ) {
			QMessageBox::information( this, tr( "Compile Collision" ),
				tr( "Select an editable collision body first." ) );
			return;
		}
		// one implementation, shared with the Compile Collision spell
		const QModelIndex made = tlCompileCollision( nif, this,
			blockIndex( item->data( 0, ObjectBlockRole ).toInt() ), false );
		if ( made.isValid() )
			if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( made );
		queueRebuild();
	}

	void lintCollision()
	{
		int dangling = 0, suspiciousHulls = 0, nearBoxes = 0, nonUniformPrimitives = 0;
		int stairWithoutSlope = 0, visibleWithoutCollision = 0, compiled = 0, editable = 0;
		QVector<int> danglingObjects;
		// block numbers, not filter indices: the repair needs Motion System, which
		// lives on the body (or its Rigid Body Info), never on the filter row
		QVector<int> zeroLayerBodies;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );
			if ( nif->blockInherits( i, "bhkNPCollisionObject" ) ) {
				compiled++;
				if ( !blockIndex( nif->getLink( i, "Data" ) ).isValid() ) {
					dangling++; danglingObjects.append( b );
				}
			} else if ( nif->blockInherits( i, "bhkCollisionObject" ) ) {
				editable++;
				int body = nif->getLink( i, "Body" );
				if ( !blockIndex( body ).isValid() ) {
					dangling++; danglingObjects.append( b );
				}
				int shape = blockIndex( body ).isValid() ? nif->getLink( blockIndex( body ), "Shape" ) : -1;
				int leaf = firstLeafShape( shape );
				QModelIndex li = blockIndex( leaf );
				if ( li.isValid() && nif->isNiBlock( li, "bhkConvexVerticesShape" ) ) {
					QVector<Vector4> vertices = nif->getArray<Vector4>( li, "Vertices" );
					if ( vertices.size() > 64 ) suspiciousHulls++;
					if ( vertices.size() >= 8 ) {
						Vector3 mn( vertices.first()[0], vertices.first()[1], vertices.first()[2] ), mx( mn );
						for ( const Vector4 & v : vertices ) for ( int axis = 0; axis < 3; axis++ ) {
							mn[axis] = std::min( mn[axis], v[axis] ); mx[axis] = std::max( mx[axis], v[axis] );
						}
						float tolerance = std::max( { mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2] } ) * 0.03f + 1.0e-5f;
						bool boxLike = true;
						for ( const Vector4 & v : vertices ) for ( int axis = 0; axis < 3; axis++ )
							if ( std::min( std::fabs( v[axis] - mn[axis] ), std::fabs( v[axis] - mx[axis] ) ) > tolerance ) boxLike = false;
						if ( boxLike ) nearBoxes++;
					}
				}
				QModelIndex info = blockIndex( body ).isValid() ? nif->getIndex( blockIndex( body ), "Rigid Body Info" ) : QModelIndex();
				QModelIndex filter = bhkGetHavokFilter( nif, info );
				quint32 collLayer = filter.isValid() ? nif->get<quint32>( filter, "Layer" ) : 0;
				if ( filter.isValid() && collLayer == 0 ) zeroLayerBodies.append( body );
				if ( collLayer == 31 && li.isValid() && ( nif->isNiBlock( li, "bhkBoxShape" )
						|| nif->isNiBlock( li, "bhkSphereShape" ) || nif->isNiBlock( li, "bhkCapsuleShape" ) ) ) stairWithoutSlope++;
			}
			if ( nif->isNiBlock( i, "bhkTransformShape" ) || nif->isNiBlock( i, "bhkConvexTransformShape" ) ) {
				QModelIndex child = blockIndex( firstLeafShape( nif->getLink( i, "Shape" ) ) );
				if ( child.isValid() && ( nif->isNiBlock( child, "bhkBoxShape" ) || nif->isNiBlock( child, "bhkSphereShape" )
						|| nif->isNiBlock( child, "bhkCapsuleShape" ) ) ) {
					Vector3 trans, scales; Matrix rot; nif->get<Matrix4>( i, "Transform" ).decompose( trans, rot, scales );
					float lo = std::min( { std::fabs( scales[0] ), std::fabs( scales[1] ), std::fabs( scales[2] ) } );
					float hi = std::max( { std::fabs( scales[0] ), std::fabs( scales[1] ), std::fabs( scales[2] ) } );
					if ( hi - lo > std::max( 1.0e-4f, hi * 0.001f ) ) nonUniformPrimitives++;
				}
			}
			if ( nif->blockInherits( i, { "BSGeometry", "BSTriShape", "NiTriBasedGeom" } )
					&& !( nif->get<quint32>( i, "Flags" ) & 1u ) ) {
				bool protectedByCollision = false; int owner = nif->getParent( b );
				for ( int depth = 0; nif->isValidBlockNumber( owner ) && depth < 32; depth++, owner = nif->getParent( owner ) ) {
					QModelIndex node = blockIndex( owner );
					if ( nif->blockInherits( node, "NiNode" ) && nif->isValidBlockNumber( nif->getLink( node, "Collision Object" ) ) ) {
						protectedByCollision = true; break;
					}
				}
				if ( !protectedByCollision ) visibleWithoutCollision++;
			}
		}
		QStringList findings;
		if ( dangling ) findings << tr( "%1 dangling collision reference(s) - safe fix: remove the broken collision object" ).arg( dangling );
		if ( !zeroLayerBodies.isEmpty() ) findings << tr( "%1 collision layer(s) are Unidentified (0) - safe fix: infer Static or Props from motion" ).arg( zeroLayerBodies.size() );
		if ( suspiciousHulls ) findings << tr( "%1 convex hull(s) exceed 64 vertices - optimize the source mesh or rebuild Convex" ).arg( suspiciousHulls );
		if ( nearBoxes ) findings << tr( "%1 hull(s) are box-like - Box collision would be cheaper" ).arg( nearBoxes );
		if ( nonUniformPrimitives ) findings << tr( "%1 primitive collision transform(s) use non-uniform scale" ).arg( nonUniformPrimitives );
		if ( stairWithoutSlope ) findings << tr( "%1 STAIRHELPER body/bodies appear to contain no sloped shape" ).arg( stairWithoutSlope );
		if ( visibleWithoutCollision ) findings << tr( "%1 visible geometry block(s) have no collision on their node hierarchy" ).arg( visibleWithoutCollision );
		if ( compiled && editable ) findings << tr( "File contains mixed compiled and editable collision - finish editing, then compile consistently" );
		if ( findings.isEmpty() ) {
			QMessageBox::information( this, tr( "Check Collision" ), tr( "No collision issues found." ) ); return;
		}
		QMessageBox report( QMessageBox::Warning, tr( "Check Collision" ), findings.join( QStringLiteral( "\n\n" ) ), QMessageBox::NoButton, this );
		auto * fix = report.addButton( tr( "Apply Safe Fixes" ), QMessageBox::AcceptRole );
		report.addButton( QMessageBox::Close ); report.exec();
		if ( report.clickedButton() != fix || ( danglingObjects.isEmpty() && zeroLayerBodies.isEmpty() ) ) return;
		nifSnapshotOp( nif, tr( "Fix collision warnings" ), [&, this]() {
			for ( int body : zeroLayerBodies ) {
				QModelIndex iBody = blockIndex( body );
				if ( !iBody.isValid() ) continue;
				// Motion System sits on the block or inside Rigid Body Info, never on
				// the filter row - reading it off filter.parent() always yielded 0, so
				// this branch could only ever write STATIC, never PROPS
				quint32 motion = nif->get<quint32>( bhkGetRBInfo( nif, iBody, QStringLiteral( "Motion System" ) ) );
				bhkSetFilterField( nif, iBody, QStringLiteral( "Layer" ), motion == 3 ? 10u : 1u );
			}
			std::sort( danglingObjects.begin(), danglingObjects.end(), std::greater<int>() );
			for ( int block : danglingObjects ) if ( nif->isValidBlockNumber( block ) ) {
				spRemoveBranch remove; remove.castIfApplicable( nif, blockIndex( block ) );
			}
		} );
		queueRebuild();
	}

	QFrame * separator()
	{
		auto * line = new QFrame( this );
		line->setFrameShape( QFrame::HLine );
		line->setFrameShadow( QFrame::Sunken );
		return line;
	}

	void positionPreviewPanel()
	{
		if ( !previewPanel || !ogl ) return;
		previewPanel->adjustSize();
		QPoint local( std::max( 10, ogl->width() - previewPanel->width() - 10 ),
			std::max( 10, ogl->height() - previewPanel->height() - 10 ) );
		previewPanel->move( ogl->mapToGlobal( local ) );
	}

	void clearCollisionPreview()
	{
		if ( previewTimer ) previewTimer->stop();
		if ( ogl ) ogl->clearCollisionPreview();
		previewSource = QPersistentModelIndex();
		if ( previewPanel ) previewPanel->hide();
	}

	void updatePreviewControlVisibility()
	{
		if ( !previewPanel ) return;
		const bool convex = ( previewKind == 0 );
		const bool decomp = convex && previewMethod->currentIndex() == 1;
		const QList<QWidget *> convexControls = { previewMethodLabel, previewMethod, previewPrecisionLabel, previewPrecision };
		const QList<QWidget *> decompositionControls = { previewThresholdLabel, previewThreshold, previewMaxHullsLabel, previewMaxHulls };
		for ( QWidget * widget : convexControls ) widget->setVisible( convex );
		for ( QWidget * widget : decompositionControls ) widget->setVisible( decomp );
		QString title = convex ? tr( "Collision - Convex Preview" ) : tr( "Collision - Mesh Optimization" );
		previewPanel->setProperty( "titleText", title );
		previewTitle->setText( ( previewPanel->property( "collapsed" ).toBool()
			? QStringLiteral( "˃  " ) : QStringLiteral( "˅  " ) ) + title );
		positionPreviewPanel();
	}

	void rebuildCollisionPreview()
	{
		if ( !previewPanel || !previewPanel->isVisible() || !previewSource.isValid() ) return;
		QVector<Vector3> soup;
		QString statistics;
		bool ok = tlBuildCollisionPreview( nif, QModelIndex( previewSource ), previewKind,
			float( previewRatio->value() / 100.0 ), previewMethod->currentIndex() == 1,
			float( previewPrecision->value() ), float( previewThreshold->value() ),
			previewMaxHulls->value(), soup, statistics );
		previewStats->setText( statistics );
		previewStats->setStyleSheet( ok ? QStringLiteral( "color:%1" ).arg( wwSkinColor( "textMuted" ) )
										: QStringLiteral( "color:%1" ).arg( wwSkinColor( "danger" ) ) );
		if ( ok ) ogl->setCollisionPreview( soup ); else ogl->clearCollisionPreview();
		positionPreviewPanel();
	}

	void ensurePreviewPanel()
	{
		if ( previewPanel ) return;
		previewPanel = new QFrame( mw, Qt::Tool | Qt::FramelessWindowHint );
		previewPanel->setObjectName( QStringLiteral( "CollisionOperatorPanel" ) );
		previewPanel->setFrameShape( QFrame::StyledPanel );
		previewPanel->setAutoFillBackground( true );
		previewPanel->setAttribute( Qt::WA_ShowWithoutActivating );
		previewPanel->setProperty( "collapsed", false );
		previewPanel->setStyleSheet( QStringLiteral(
			"QFrame#CollisionOperatorPanel { background:%1; border:1px solid %2; }"
			"QLabel { color:%3; background:transparent; }"
			"QToolButton { color:%3; background:transparent; border:none; }"
			"QToolButton:hover { color:%4; }"
			"QPushButton { background:%5; color:%3; border:none; border-radius:3px; padding:4px 14px; }"
			"QPushButton:hover { background:%6; }"
			"QPushButton:pressed { background:%7; }"
			"QComboBox { background:%8; color:%3; border:none; border-radius:3px; padding:2px 6px; }"
			"QSpinBox { background:%5; color:%3; border:none; border-radius:3px; padding:2px 6px; }" )
			.arg( wwSkinColor( "bgCard" ), wwSkinColor( "borderStrong" ), wwSkinColor( "text" ),
				  wwSkinColor( "textBright" ), wwSkinColor( "bgInput" ), wwSkinColor( "bgBtnHover" ),
				  wwSkinColor( "bgBtnDown" ), wwSkinColor( "bgPanel" ) ) );

		auto * outer = new QVBoxLayout( previewPanel );
		outer->setContentsMargins( 10, 8, 10, 8 ); outer->setSpacing( 4 );
		auto * header = new QHBoxLayout;
		previewTitle = new QToolButton( previewPanel );
		previewTitle->setAutoRaise( true );
		QFont titleFont = previewTitle->font(); titleFont.setBold( true ); previewTitle->setFont( titleFont );
		auto * close = new QToolButton( previewPanel ); close->setText( QStringLiteral( "✕" ) ); close->setAutoRaise( true );
		header->addWidget( previewTitle ); header->addStretch(); header->addWidget( close ); outer->addLayout( header );
		previewBody = new QWidget( previewPanel );
		auto * bodyLayout = new QVBoxLayout( previewBody ); bodyLayout->setContentsMargins( 0, 0, 0, 0 ); bodyLayout->setSpacing( 4 );
		auto * grid = new QGridLayout; grid->setContentsMargins( 0, 0, 0, 0 ); grid->setHorizontalSpacing( 8 ); grid->setVerticalSpacing( 3 );
		previewRatio = new WwNumberField( previewBody );
		previewRatio->setRange( 1.0, 100.0 ); previewRatio->setDecimals( 0 ); previewRatio->setSuffix( QStringLiteral( "%" ) );
		previewRatio->setSingleStep( 1.0 ); previewRatio->setKeyboardTracking( false ); previewRatio->setMinimumWidth( 150 );
		previewRatio->setToolTip( tr( "Drag left/right to change the triangle percentage; Shift-drag for fine control" ) );
		previewMethod = new QComboBox( previewBody ); previewMethod->addItems( { tr( "Single Hull (qhull)" ), tr( "Decomposition (CoACD)" ) } );
		// the same field chrome as the scrub fields it sits between, now that this
		// panel is where the convex method is chosen rather than a second copy of it
		wwMatchFieldStyle( previewMethod );
		previewPrecision = new WwNumberField( previewBody );
		previewPrecision->setRange( 0.0, 5.0 ); previewPrecision->setDecimals( 3 ); previewPrecision->setSingleStep( 0.01 ); previewPrecision->setKeyboardTracking( false );
		previewPrecision->setToolTip( tr( "Drag left/right to change hull precision; Shift-drag for fine control" ) );
		previewThreshold = new WwNumberField( previewBody );
		previewThreshold->setRange( 0.01, 1.0 ); previewThreshold->setDecimals( 3 ); previewThreshold->setSingleStep( 0.01 ); previewThreshold->setKeyboardTracking( false );
		previewThreshold->setToolTip( tr( "Drag left/right to change the decomposition threshold; Shift-drag for fine control" ) );
		previewMaxHulls = new QSpinBox( previewBody ); previewMaxHulls->setRange( 1, 256 ); previewMaxHulls->setKeyboardTracking( false );
		previewMethodLabel = new QLabel( tr( "Method" ), previewBody );
		previewPrecisionLabel = new QLabel( tr( "Hull precision" ), previewBody );
		previewThresholdLabel = new QLabel( tr( "Threshold" ), previewBody );
		previewMaxHullsLabel = new QLabel( tr( "Max hulls" ), previewBody );
		grid->addWidget( new QLabel( tr( "Triangles" ), previewBody ), 0, 0 ); grid->addWidget( previewRatio, 0, 1 );
		grid->addWidget( previewMethodLabel, 1, 0 ); grid->addWidget( previewMethod, 1, 1 );
		grid->addWidget( previewPrecisionLabel, 2, 0 ); grid->addWidget( previewPrecision, 2, 1 );
		grid->addWidget( previewThresholdLabel, 3, 0 ); grid->addWidget( previewThreshold, 3, 1 );
		grid->addWidget( previewMaxHullsLabel, 4, 0 ); grid->addWidget( previewMaxHulls, 4, 1 );
		bodyLayout->addLayout( grid );
		previewStats = new QLabel( previewBody ); previewStats->setWordWrap( true ); bodyLayout->addWidget( previewStats );
		auto * buttons = new QHBoxLayout;
		auto * cancel = new QPushButton( tr( "Cancel" ), previewBody );
		auto * apply = new QPushButton( tr( "Apply" ), previewBody );
		buttons->addWidget( cancel ); buttons->addWidget( apply ); bodyLayout->addLayout( buttons );
		outer->addWidget( previewBody );

		previewTimer = new QTimer( this ); previewTimer->setSingleShot( true ); previewTimer->setInterval( 80 );
		auto queuePreview = [this]() {
			updatePreviewControlVisibility();
			// Throttle expensive mesh rebuilding while a scrub gesture is active,
			// instead of postponing the preview until the drag has stopped.
			if ( !previewTimer->isActive() ) previewTimer->start();
		};
		connect( previewTimer, &QTimer::timeout, this, [this]() { rebuildCollisionPreview(); } );
		connect( previewRatio, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [queuePreview]( double ) { queuePreview(); } );
		connect( previewPrecision, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [queuePreview]( double ) { queuePreview(); } );
		connect( previewThreshold, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [queuePreview]( double ) { queuePreview(); } );
		connect( previewMaxHulls, qOverload<int>( &QSpinBox::valueChanged ), this, [queuePreview]( int ) { queuePreview(); } );
		connect( previewMethod, qOverload<int>( &QComboBox::currentIndexChanged ), this, [queuePreview]( int ) { queuePreview(); } );
		connect( previewTitle, &QToolButton::clicked, this, [this]() {
			bool collapsed = !previewPanel->property( "collapsed" ).toBool();
			previewPanel->setProperty( "collapsed", collapsed );
			previewBody->setVisible( !collapsed );
			updatePreviewControlVisibility();
			previewPanel->adjustSize();
			previewPanel->resize( previewPanel->sizeHint() );
			positionPreviewPanel();
		} );
		connect( close, &QToolButton::clicked, this, [this]() { clearCollisionPreview(); } );
		connect( cancel, &QPushButton::clicked, this, [this]() { clearCollisionPreview(); } );
		connect( apply, &QPushButton::clicked, this, [this]() {
			if ( !previewSource.isValid() ) return;
			QModelIndex result = tlCommitCollisionPreview( nif, QModelIndex( previewSource ), previewKind,
				float( previewRatio->value() / 100.0 ), previewMethod->currentIndex() == 1,
				float( previewPrecision->value() ), float( previewThreshold->value() ), previewMaxHulls->value() );
			ogl->clearCollisionPreview(); previewPanel->hide(); previewSource = QPersistentModelIndex();
			if ( result.isValid() ) if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( result );
			queueRebuild();
		} );
		connect( ogl, &QWindow::widthChanged, this, [this]( int ) { if ( previewPanel->isVisible() ) positionPreviewPanel(); } );
		connect( ogl, &QWindow::heightChanged, this, [this]( int ) { if ( previewPanel->isVisible() ) positionPreviewPanel(); } );
	}

	void showCollisionPreview( int kind, double initialRatio, bool decomposition = false )
	{
		QModelIndex source = currentSource();
		if ( !source.isValid() ) {
			QMessageBox::information( this, tr( "Collision Preview" ), tr( "Select a BSTriShape or NiNode first." ) );
			return;
		}
		ensurePreviewPanel();
		for ( const char * name : { "GizmoRedoPanel", "OperatorRedoPanel", "BoxSelectRedoPanel" } )
			if ( QFrame * panel = mw->findChild<QFrame *>( QLatin1String( name ) ); panel ) panel->hide();
		previewKind = kind;
		previewSource = source;
		QSettings settings;
		const QList<QObject *> controls = { previewRatio, previewMethod, previewPrecision, previewThreshold, previewMaxHulls };
		for ( QObject * control : controls ) control->blockSignals( true );
		previewRatio->setValue( std::clamp( initialRatio, 1.0, 100.0 ) );
		previewMethod->setCurrentIndex( decomposition ? 1 : 0 );
		previewPrecision->setValue( settings.value( "CollisionManager/Preview/Precision", 0.25 ).toDouble() );
		previewThreshold->setValue( settings.value( "CollisionManager/Preview/Threshold", 0.05 ).toDouble() );
		previewMaxHulls->setValue( settings.value( "CollisionManager/Preview/MaxHulls", 16 ).toInt() );
		for ( QObject * control : controls ) control->blockSignals( false );
		previewPanel->show(); previewPanel->raise();
		updatePreviewControlVisibility();
		rebuildCollisionPreview();
	}

	void buildUi()
	{
		auto * root = new QVBoxLayout( this );
		root->setContentsMargins( 6, 6, 6, 6 );
		root->setSpacing( 6 );

		/* The display row moved to the viewport's Overlays menu, where the rest of
		 * "what the viewport draws on top of the model" already lives.
		 *
		 * Show collision / Colour by / Solid / X-ray / Only / Labels are viewport
		 * state, not collision authoring — none of them change the file. Show
		 * collision was outright a second face for ui->aShowCollision, which is in
		 * the Overlays menu and always has been, so the dock carried a duplicate
		 * of a toggle two clicks away. Six controls and a whole row leave the
		 * panel, and the settings keys are untouched, so what is stored and what
		 * reads it are the same as before. See wwBuildCollisionOverlayMenu.
		 */
		QSettings settings;

		inventoryHeader = new QLabel( tr( "Collision in file" ), this );
		inventoryHeader->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
		root->addWidget( inventoryHeader );

		tree = new CollisionInventoryTree( this );
		tree->setObjectName( QStringLiteral( "CollisionInventoryTree" ) );
		tree->setColumnCount( 8 );
		tree->setHeaderLabels( { tr( "Node" ), tr( "Shape" ), tr( "Layer" ), tr( "Material" ),
			tr( "Mass" ), tr( "State" ), tr( "Bone" ), tr( "Parent" ) } );
		tree->setRootIsDecorated( true );
		tree->setAlternatingRowColors( true );
		tree->setSelectionMode( QAbstractItemView::SingleSelection );
		tree->setContextMenuPolicy( Qt::CustomContextMenu );

		/* SHAPES MOVE BETWEEN BODIES BY DRAGGING THEM.
		 *
		 * setDropIndicatorShown(false) for the same reason the Block List turns it
		 * off: Qt draws that indicator from QAbstractItemView::dragMoveEvent, which
		 * these overrides never reach. The lit body row is the feedback.
		 *
		 * The tree knows about drags and nothing about collision; what a row IS,
		 * and what moving one means, is handed in here.
		 */
		tree->setDragEnabled( true );
		tree->viewport()->setAcceptDrops( true );
		tree->setAcceptDrops( true );
		tree->setDropIndicatorShown( false );
		tree->setDragDropMode( QAbstractItemView::DragDrop );
		tree->shapeOfRow = []( QTreeWidgetItem * item ) -> qint32 {
			// child rows only: a top-level row is the body
			if ( !item || !item->parent() )
				return -1;
			const QVariant v = item->data( 0, ShapeBlockRole );
			return v.isValid() ? v.toInt() : -1;
		};
		tree->bodyOfRow = []( QTreeWidgetItem * item ) -> qint32 {
			QTreeWidgetItem * row = item;
			while ( row && row->parent() )
				row = row->parent();
			if ( !row )
				return -1;
			const QVariant v = row->data( 0, BodyBlockRole );
			return v.isValid() ? v.toInt() : -1;
		};
		tree->refusalFor = [this]( qint32 shape, qint32 body ) {
			return tlMoveCollisionShapeRefusal( nif, shape, body );
		};

		/* A MESH DRAGGED OVER THE ROWS REACHES THIS PANEL.
		 *
		 * The tree covers the body rows and accepts drops, so Qt hands it the drag
		 * whenever the pointer is over one — and it only understands shape rows.
		 * A mesh from the Block List was refused over every body and accepted only
		 * over the panel's bare furniture below the tree, which is the inverse of
		 * where it has to land, with no body lit because this panel never saw it.
		 *
		 * The event is re-made at this widget's coordinates and run through the
		 * handlers that were already here, so the body targeting and the highlight
		 * are the same ones, reached from the place the pointer actually is.
		 */
		tree->offerToPanel = [this]( QEvent * event, const QPoint & globalPos ) -> bool {
			const QPointF at( mapFromGlobal( globalPos ) );
			if ( event->type() == QEvent::Drop ) {
				auto * from = static_cast<QDropEvent *>( event );
				QDropEvent here( at, from->possibleActions(), from->mimeData(),
					from->buttons(), from->modifiers() );
				dropEvent( &here );
				if ( !here.isAccepted() )
					return false;
				from->setDropAction( here.dropAction() );
				from->accept();
				return true;
			}
			auto * from = static_cast<QDragMoveEvent *>( event );
			QDragMoveEvent here( at.toPoint(), from->possibleActions(), from->mimeData(),
				from->buttons(), from->modifiers() );
			here.setAccepted( false );
			offerCollisionDrop( &here );
			// IgnoreAction means "not mine" — let the tree give its own answer
			if ( !here.isAccepted() || here.dropAction() == Qt::IgnoreAction )
				return false;
			from->setDropAction( here.dropAction() );
			from->accept();
			return true;
		};
		tree->moveShapeToBody = [this]( qint32 shape, qint32 body ) {
			QString refusal;
			nifSnapshotOp( nif, tr( "Move collision shape" ), [&]() {
				refusal = tlMoveCollisionShape( nif, shape, body );
			} );
			if ( QStatusBar * bar = mw ? mw->statusBar() : nullptr )
				bar->showMessage( refusal.isEmpty()
					? tr( "Moved %1 into %2." ).arg( nif->itemName( blockIndex( shape ) ),
						nif->itemName( blockIndex( body ) ) )
					: refusal, 5000 );
			queueRebuild();
		};
		/* Every column sizes to its own text; the view scrolls when the dock is
		 * narrower than the total.
		 *
		 * Material used to be Stretch, which only gets the space the other columns
		 * leave over - in a docked panel that is nearly nothing, so the material
		 * name sat permanently clipped to "U...". Mass and State had no mode set
		 * at all and stayed at the default section width. And since
		 * ResizeToContents and Stretch both disable interactive resizing, the
		 * columns could not be dragged wider either - clipped AND frozen.
		 */
		for ( int c = 0; c < tree->columnCount(); c++ )
			tree->header()->setSectionResizeMode( c, QHeaderView::ResizeToContents );
		// or the last column stretches instead of fitting its text
		tree->header()->setStretchLastSection( false );
		// Bone is logical column 6 but belongs beside Node; moving the visual
		// section leaves the 42 literal column indices in this file alone.
		tree->header()->moveSection( 6, 1 );
		tree->setMinimumHeight( 150 );
		tree->setStyleSheet( wwSelectionTreeQss() );
		int sortColumn = std::clamp( settings.value( "CollisionManager/SortColumn", 0 ).toInt(), 0, 5 );
		Qt::SortOrder initialOrder = settings.value( "CollisionManager/SortOrder", int( Qt::AscendingOrder ) ).toInt()
			? Qt::DescendingOrder : Qt::AscendingOrder;
		tree->setSortingEnabled( true );
		tree->sortItems( sortColumn, initialOrder );
		connect( tree->header(), &QHeaderView::sortIndicatorChanged, this,
			[]( int column, Qt::SortOrder order ) {
				QSettings s; s.setValue( "CollisionManager/SortColumn", column ); s.setValue( "CollisionManager/SortOrder", int( order ) );
			} );
		root->addWidget( tree, 1 );

		auto * browserActions = new QHBoxLayout;
		decompileSelectedAction = new QAction( tr( "Decompile Selected" ), this );
		compileSelectedAction = new QAction( tr( "Compile Selected" ), this );
		compileSelectedAction->setToolTip( tr( "Compile the selected editable collision into an FO4 hknp packfile" ) );
		auto * decompileAllAction = new QAction( tr( "Decompile All" ), this );
		primaryCollisionAction = new QToolButton( this );
		primaryCollisionAction->setToolButtonStyle( Qt::ToolButtonTextOnly );
		primaryCollisionAction->setPopupMode( QToolButton::MenuButtonPopup );
		primaryCollisionAction->setDefaultAction( decompileSelectedAction );
		auto * primaryMenu = new QMenu( primaryCollisionAction );
		primaryMenu->addAction( decompileSelectedAction );
		primaryMenu->addAction( decompileAllAction );
		primaryMenu->addSeparator();
		primaryMenu->addAction( compileSelectedAction );
		primaryCollisionAction->setMenu( primaryMenu );
		primaryCollisionAction->setEnabled( false );

		/* Four buttons became one.
		 *
		 * Decompile / Compile / Import Donor all act on the row you have selected
		 * in the list directly above them, and the list has had a right-click menu
		 * offering the same operations the whole time — so the buttons were a
		 * second way to do what pointing at the thing already does, taking a
		 * permanent row to do it. Check Collision is file-wide and stays a button
		 * with the other file-wide entries under More.
		 *
		 * The actions themselves stay: the primary split button's menu is built
		 * from them, and so is the row menu, so both routes run one implementation.
		 */
		auto * more = new QToolButton( this );
		more->setText( tr( "More..." ) );
		more->setStyleSheet( wwBoxedButtonQss( QStringLiteral( "3px 8px" ) ) );
		more->setPopupMode( QToolButton::InstantPopup );
		auto * moreMenu = new QMenu( more );
		auto * lintAction = moreMenu->addAction( tr( "Check Collision" ) );
		lintAction->setToolTip( tr( "Report dangling, suspicious or mis-layered collision across the file" ) );
		moreMenu->addAction( decompileAllAction );
		auto * reverseAction = moreMenu->addAction( tr( "Create Editable Mesh Copy" ) );
		reverseAction->setToolTip( tr( "Create a visible BSTriShape proxy from the selected collision" ) );
		auto * refreshAction = moreMenu->addAction( tr( "Refresh" ) );
		more->setMenu( moreMenu );
		moreMenu->setToolTipsVisible( true );
		browserActions->addWidget( primaryCollisionAction );
		browserActions->addStretch();
		browserActions->addWidget( more );
		root->addLayout( browserActions );
		root->addWidget( separator() );

		/* Untitled, because the switch directly above it already says "Collision
		 * Creation" — the group repeated the name of the tab that reveals it, one
		 * row apart, and with the group down to a single button the frame was
		 * most of what was left of it.
		 */
		auto * createGroup = new QGroupBox( this );
		auto * createLayout = new QGridLayout( createGroup );
		/* The source hint is gone with the row it sat on. It was fixed text that
		 * never changed — an instruction, permanently, on a panel you have by
		 * then already used. What is actually selected is now on the button.
		 */
		const QStringList shapeNames = {
			tr( "Box" ), tr( "Sphere" ), tr( "Capsule" ), tr( "Convex" ), tr( "Mesh" )
		};
		QSettings createSettings;
		/* Kept as MODELS, never shown. Both carry data the menu needs — the
		 * preset's stable ids, the material list with its CRC values and tooltips
		 * — and rebuilding either as a bare list would be duplicating the loader
		 * above it.
		 */
		auto * preset = new WwSearchCombo( createGroup, tr( "Search presets" ) );
		// both are reparented into the popup below and shown there
		// Stable data IDs preserve settings written by the original three-item
		// list while allowing the authoring-oriented display order below.
		preset->addItem( tr( "Static" ), 0 );
		preset->addItem( tr( "Anim Static" ), 3 );
		preset->addItem( tr( "Prop" ), 1 );
		preset->addItem( tr( "Stairhelper" ), 4 );
		preset->addItem( tr( "Custom" ), 2 );
		/* Saved presets after the built-ins, carrying their NAME as item data.
		 *
		 * Built-ins carry an int and customs a QString, and every read tests
		 * which it got. That is what keeps the existing
		 * CollisionManager/Create/Preset setting meaning exactly what it always
		 * meant, instead of overloading it with ids that would collide the
		 * moment two machines saved different presets.
		 */
		for ( const QString & name : wwCollisionPresetNames() )
			preset->addItem( name, name );
		const QString savedPresetName =
			createSettings.value( "CollisionManager/Create/PresetName" ).toString();
		int savedPresetRow = savedPresetName.isEmpty() ? -1 : preset->findData( savedPresetName );
		if ( savedPresetRow < 0 )
			savedPresetRow = preset->findData(
				createSettings.value( "CollisionManager/Create/Preset", 1 ).toInt() );
		preset->setCurrentIndex( savedPresetRow >= 0 ? savedPresetRow : preset->findData( 1 ) );
		/* No save button. Every menu choice writes through immediately, so there
		 * is nothing left for it to do — and left behind as a parented widget
		 * with no layout cell, Qt put it at 0,0 of the group, on top of the
		 * group's own title.
		 */
		/* NOT editable, and that was the whole bug.
		 *
		 * An editable QComboBox is a line edit with a list behind it: clicking it
		 * puts a cursor in the text instead of opening anything, so the field
		 * offered typing and nothing else — no vanilla list, no search box, no
		 * way to reach "add a custom material". Making it a WwSearchCombo did not
		 * help while this line survived, because showPopup() is not what a click
		 * on an editable combo calls. The search box lives in the drop-down now,
		 * which is the whole reason the closed field does not need to be a text
		 * box.
		 */
		/* Empty here on purpose. The list is FILLED by populatePhysicsEnums.
		 *
		 * Which materials exist depends on the game, and materialEnumType() reads
		 * getBSVersion() — which is 0 while the panel is being built, because the
		 * panel exists before any file is opened. Filling it here got Oblivion's
		 * 32 materials, permanently, on a Fallout 4 file with 157. Same shape as
		 * the layer combo: it is a member so the refill on modelReset can reach
		 * it, and the stored selection is applied by loadCreateFields afterwards.
		 */
		createMaterialCombo = new WwSearchCombo( createGroup, tr( "Search materials" ) );
		createMaterialCombo->setObjectName( QStringLiteral( "CollisionCreateMaterial" ) );
		auto * materialEdit = createMaterialCombo;
		// reparented into the popup below, where it stays a searchable combo

		/* ONE BUTTON, and a popup with everything in it at once.
		 *
		 * The group was five shape buttons, a convex-method combo, a preset combo
		 * and its save button, an expander hiding a material picker and a Replace
		 * tick, an Optimize button and a Create button — eleven controls, of which
		 * the method line applies to one shape, Optimize to two, and the rest are
		 * defaults you set once and forget. All of it permanently on screen, in a
		 * docked panel that is mostly the list above it.
		 *
		 * A POPUP PANEL, NOT A MENU. The first version put the settings in nested
		 * submenus off a split button, which buries them: you cannot see the
		 * preset and the material at the same time, and reading one costs two
		 * hovers. A panel shows the whole decision at once, which is what the
		 * permanent group did well and the only thing it did well. Nothing here
		 * is on screen unless it was asked for.
		 *
		 * It is also the only place the material picker can stay searchable — a
		 * combo inside a QMenu only gets the keys the menu chooses to forward,
		 * and click-to-focus through a QWidgetAction is the flakiest interaction
		 * in Qt. In a plain popup frame it is an ordinary widget again.
		 */
		/* TWO BUTTONS, IN THE ORDER THE BLOCKS NEST.
		 *
		 * Create Collision Body makes the bhkCollisionObject and bhkRigidBody and
		 * everything they carry, which is all of the physics. Create Collision
		 * Shape makes a shape and hands it to a body that already exists. That is
		 * not an arbitrary split: bhkRigidBodyCInfo holds mass, friction,
		 * restitution, both dampings, both velocities, motion, quality, solver,
		 * deactivator, penetration and the layer, and a shape holds its Material,
		 * its geometry and nothing else. One button doing both had to pretend the
		 * body settings belonged to the shape being made — and with Replace off,
		 * where the shape joins an existing body, they were a straight lie.
		 */
		const QString buttonQss = QStringLiteral(
			"QToolButton { background:%1; color:%2; border:1px solid %3; border-radius:3px;"
			" padding:5px 10px; }"
			"QToolButton:hover { background:%4; color:%5; }"
			"QToolButton:pressed { background:%6; }"
			"QToolButton:disabled { color:%7; border-color:%8; }" )
			.arg( wwSkinColor( "bgBtn" ), wwSkinColor( "text" ), wwSkinColor( "border" ),
				  wwSkinColor( "bgBtnHover" ), wwSkinColor( "textBright" ),
				  wwSkinColor( "bgBtnDown" ), wwSkinColor( "textMuted" ), wwSkinColor( "bgAlt" ) );

		createButton = new QToolButton( createGroup );
		createButton->setObjectName( QStringLiteral( "CollisionCreateBodyButton" ) );
		createButton->setText( tr( "Create Collision Body…" ) );
		createButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
		createButton->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
		createButton->setMinimumHeight( 30 );
		createButton->setToolTip( tr( "Make the rigid body and everything it carries. It has no shape until you give it one." ) );
		createButton->setStyleSheet( buttonQss );

		createShapeButton = new QToolButton( createGroup );
		createShapeButton->setObjectName( QStringLiteral( "CollisionCreateShapeButton" ) );
		createShapeButton->setText( tr( "Create Collision Shape…" ) );
		createShapeButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
		createShapeButton->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
		createShapeButton->setMinimumHeight( 30 );
		createShapeButton->setStyleSheet( buttonQss );
		/* The tooltip goes on a WRAPPER, not on the button.
		 *
		 * A disabled widget receives no mouse events, so Qt never delivers it a
		 * ToolTip event and setToolTip on it shows nothing at all — which is
		 * precisely the case this text exists for. The wrapper stays enabled and
		 * carries the explanation while the button inside it is greyed.
		 */
		createShapeHost = new QWidget( createGroup );
		auto * shapeHostLayout = new QHBoxLayout( createShapeHost );
		shapeHostLayout->setContentsMargins( 0, 0, 0, 0 );
		shapeHostLayout->addWidget( createShapeButton );

		createPopup = new QFrame( this, Qt::Popup );
		createPopup->setObjectName( QStringLiteral( "CollisionCreatePopup" ) );
		createPopup->setFrameShape( QFrame::StyledPanel );
		createPopup->setStyleSheet( QStringLiteral(
			"QFrame#CollisionCreatePopup { background:%1; border:1px solid %2; }"
			"QLabel { color:%3; background:transparent; }"
			"QCheckBox { color:%3; background:transparent; }" )
			.arg( wwSkinColor( "bgCard" ), wwSkinColor( "borderStrong" ), wwSkinColor( "text" ) ) );
		auto * popupLayout = new QVBoxLayout( createPopup );
		popupLayout->setContentsMargins( 10, 10, 10, 10 );
		popupLayout->setSpacing( 6 );

		/* The shape popup, its own frame. Everything a SHAPE holds and nothing
		 * else: which shape, and its Material. The physics is one block up.
		 */
		shapePopup = new QFrame( this, Qt::Popup );
		shapePopup->setObjectName( QStringLiteral( "CollisionShapePopup" ) );
		shapePopup->setFrameShape( QFrame::StyledPanel );
		const QString popupQss = QStringLiteral(
			"QFrame { background:%1; border:1px solid %2; }"
			"QLabel { color:%3; background:transparent; }"
			"QCheckBox { color:%3; background:transparent; }"
			// checked is the selection blue with white on it, matching the
			// Creation/Simulation switch below and the Block List's selected row
			"QToolButton { border: 1px solid %4; border-radius: 3px; padding: 3px; background: %5; color:%3; }"
			"QToolButton:hover { background: %6; }"
			"QToolButton:checked { border-color: %7; color: %8; background: %7; }" )
			.arg( wwSkinColor( "bgCard" ), wwSkinColor( "borderStrong" ), wwSkinColor( "text" ),
				  wwSkinColor( "border" ), wwSkinColor( "bgBtn" ), wwSkinColor( "bgBtnHover" ),
				  wwSkinColor( "selBgActive" ), wwSkinColor( "textBright" ) );
		shapePopup->setStyleSheet( popupQss );
		createPopup->setStyleSheet( popupQss );
		auto * shapeLayout = new QVBoxLayout( shapePopup );
		shapeLayout->setContentsMargins( 10, 10, 10, 10 );
		shapeLayout->setSpacing( 6 );

		auto * shapeHeading = new QLabel( tr( "Collision Shape" ), shapePopup );
		shapeHeading->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
		shapeLayout->addWidget( shapeHeading );
		auto * shapeRow = new QHBoxLayout;
		shapeRow->setSpacing( 4 );
		auto * shapeGroup = new QButtonGroup( shapePopup );
		shapeGroup->setExclusive( true );
		const QStringList shapeHints = {
			tr( "Smallest oriented box around the source" ),
			tr( "Smallest sphere around the source" ),
			tr( "Capsule along the source's longest axis" ),
			tr( "Convex hull, or a decomposition into several" ),
			tr( "The triangles of the selected meshes, optionally decimated" )
		};
		for ( int mode = 0; mode < shapeNames.size(); mode++ ) {
			auto * b = new QToolButton( shapePopup );
			b->setText( shapeNames.at( mode ) );
			b->setToolTip( shapeHints.at( mode ) );
			b->setCheckable( true );
			b->setMinimumSize( 58, 30 );
			b->setToolButtonStyle( Qt::ToolButtonTextOnly );
			shapeGroup->addButton( b, mode );
			shapeRow->addWidget( b, 1 );
		}
		shapeLayout->addLayout( shapeRow );

		auto * popupForm = new QGridLayout;
		popupForm->setContentsMargins( 0, 0, 0, 0 );
		popupForm->setHorizontalSpacing( 8 );
		popupForm->setVerticalSpacing( 4 );
		popupForm->setColumnStretch( 1, 1 );
		/* Convex method and the triangle percentage are NOT here: they belong to
		 * the live preview those two shapes open, which is where the geometry
		 * they change is on screen.
		 *
		 * Material goes with the SHAPE, everything else with the BODY. That is
		 * not a layout preference — bhkSphereRepShape carries Material and the
		 * geometry, bhkRigidBodyCInfo carries all the physics, so this split is
		 * the block structure drawn as two panels.
		 */
		preset->setParent( createPopup );
		preset->show();
		materialEdit->setParent( shapePopup );
		materialEdit->show();
		/* Search box, then "add a custom one", then the vanilla list.
		 *
		 * FO4 takes any material name — collisionCreateMaterial hashes an unknown
		 * one the way the Bethesda exporter does, lowercase CRC32, and remembers
		 * it — so naming a new one has to be reachable. It was an editable combo,
		 * which cost the drop-down its arrow, put a clear button on a field with
		 * no empty state, and left grey placeholder text where the current
		 * material should be whenever the typed text matched nothing. Adding one
		 * is a thing you do while looking at the list and finding it is not in
		 * there, so it is a row in the list's own popup.
		 */
		materialEdit->setExtraRow( tr( "+   Add a custom material…" ), [this, materialEdit]() {
			bool ok = false;
			const QString name = QInputDialog::getText( this, tr( "Add Custom Collision Material" ),
				tr( "Material name:" ), QLineEdit::Normal, QString(), &ok ).trimmed();
			if ( !ok || name.isEmpty() )
				return;
			const QString valueText = QInputDialog::getText( this, tr( "Add Custom Collision Material" ),
				tr( "Numeric value (decimal or 0x hex), or blank to hash the name:" ),
				QLineEdit::Normal, QString(), &ok ).trimmed();
			if ( !ok )
				return;
			quint32 value = 0;
			if ( valueText.isEmpty() ) {
				// the exporter's own rule, so a name typed here and the same name
				// in a BGSM come out as the same material
				for ( QChar c : name )
					hashFunctionCRC32( value, static_cast<unsigned char>( c.toLower().unicode() ) );
			} else {
				bool numeric = false;
				value = valueText.toUInt( &numeric, 0 );
				if ( !numeric ) {
					QMessageBox::warning( this, tr( "Custom Collision Material" ),
						tr( "'%1' is not a valid 32-bit material value." ).arg( valueText ) );
					return;
				}
			}
			QVariantMap custom = WwCollisionLibrary::customMaterials();
			custom.insert( name, value );
			if ( !WwCollisionLibrary::writeCustomMaterials( custom ) ) {
				QMessageBox::warning( this, tr( "Custom Collision Material" ),
					tr( "Could not write %1." ).arg( WwLibrary::featureFile(
						QStringLiteral( "Collision" ),
						QStringLiteral( "CustomMaterials.json" ) ) ) );
				return;
			}
			const QString stored = QStringLiteral( "0x%1" ).arg( value, 8, 16, QLatin1Char( '0' ) );
			int row = materialEdit->findData( stored );
			if ( row < 0 ) {
				materialEdit->addItem( name, stored );
				row = materialEdit->count() - 1;
				materialEdit->setItemData( row, value, Qt::UserRole + 1 );
			}
			materialEdit->setCurrentIndex( row );
		} );
		auto * replace = new QCheckBox( tr( "Replace existing shape" ), createPopup );
		replace->setChecked( createSettings.value( "CollisionManager/Create/ReplaceShape", false ).toBool() );
		replace->setToolTip( tr( "Off combines the new shape with what the body already has" ) );
		/* KEEP THE SOURCE MESH. Off by default, which is the long-standing
		 * behaviour: creating collision from a mesh consumes it. The advice was
		 * "duplicate it first if you want it kept", which is a footgun a
		 * checkbox removes. Read in collisionConsumeSource (havok.cpp), the one
		 * function every create path funnels through.
		 */
		auto * keepMesh = new QCheckBox( tr( "Keep the source mesh" ), createPopup );
		keepMesh->setChecked( createSettings.value( "CollisionManager/Create/KeepMesh", false ).toBool() );
		keepMesh->setToolTip( tr( "Off consumes the mesh, turning it into the collision shape" ) );

		/* And the body settings, which used not to be here at all.
		 *
		 * The preset was the only way to reach any of them, so authoring a body
		 * with a different mass or layer meant creating it wrong and correcting it
		 * in the editor below. These are the same eleven fields that editor shows;
		 * the preset now fills them in rather than being the only thing consulted.
		 */
		auto * createLayerSearch = new WwSearchCombo( createPopup, tr( "Search collision layers" ) );
		createLayerCombo = createLayerSearch;
		createLayerCombo->setObjectName( QStringLiteral( "CollisionCreateLayer" ) );
		createMotion = new QComboBox( createPopup );
		createMotion->setObjectName( QStringLiteral( "CollisionCreateMotion" ) );
		createQuality = new QComboBox( createPopup );
		createSolver = new QComboBox( createPopup );
		auto spin = [this]( double lo, double hi, int decimals ) {
			auto * s = new QDoubleSpinBox( createPopup );
			s->setRange( lo, hi );
			s->setDecimals( decimals );
			s->setKeyboardTracking( false );
			return s;
		};
		createMass = spin( 0.0, 1000000.0, 3 );
		createFriction = spin( 0.0, 10.0, 3 );
		createRestitution = spin( 0.0, 1.0, 3 );
		createLinDamp = spin( 0.0, 100.0, 3 );
		createAngDamp = spin( 0.0, 100.0, 3 );
		createMaxLinVel = spin( 0.0, 1000000.0, 2 );
		createMaxAngVel = spin( 0.0, 1000000.0, 2 );
		// the two the editor had and the create side did not, now that the body
		// is authored here rather than corrected afterwards
		createDeactivator = new QComboBox( createPopup );
		createPenetration = spin( 0.0, 1000.0, 3 );

		/* The Blender scrub field, like every other number in the program.
		 *
		 * These were plain QDoubleSpinBoxes with Qt's stepper arrows, six inches
		 * above the identical seven fields in the body editor — which do get
		 * wwMakeScrubField — so the same quantity was two different species of
		 * control depending on which half of the panel you were in. There is one
		 * number field in this fork and this is it.
		 */
		for ( QDoubleSpinBox * s : { createMass, createFriction, createRestitution,
				createLinDamp, createAngDamp, createMaxLinVel, createMaxAngVel,
				createPenetration } )
			wwMakeScrubField( s );

		for ( QComboBox * c : QList<QComboBox *>{ preset, materialEdit, createLayerCombo,
				createMotion, createQuality, createSolver, createDeactivator } )
			wwMatchFieldStyle( c );

		/* One column, and the labels say the whole word.
		 *
		 * Two columns fitted more on screen and made every row a guess about
		 * which label owned which field — and it forced the abbreviations, so
		 * "Max ang. vel." sat beside "Solver deact." and neither was a phrase
		 * anyone says. A popup has the height to spend.
		 */
		auto * bodyHeading = new QLabel( tr( "Collision Body" ), createPopup );
		bodyHeading->setStyleSheet( QStringLiteral( "QLabel { font-weight: 600; }" ) );
		popupLayout->addWidget( bodyHeading );

		int row = 0;
		auto addRow = [&]( const QString & label, QWidget * field ) {
			popupForm->addWidget( new QLabel( label, createPopup ), row, 0 );
			popupForm->addWidget( field, row++, 1 );
		};
		/* PRESET ROW: the menu, then + and −, which is Blender's arrangement.
		 *
		 * + saves what the fields below currently say; − removes the saved
		 * preset that is selected. Built-ins cannot be removed, so − greys out
		 * on them — and its explanation goes on a WRAPPER, because a disabled
		 * widget receives no mouse events, Qt never sends it a ToolTip event
		 * and setToolTip on it shows nothing. Silently, in exactly the case
		 * where the explanation is the point. Same pattern as createShapeHost.
		 */
		auto * presetHost = new QWidget( createPopup );
		auto * presetRow = new QHBoxLayout( presetHost );
		presetRow->setContentsMargins( 0, 0, 0, 0 );
		presetRow->setSpacing( 4 );
		presetRow->addWidget( preset, 1 );
		auto * presetAdd = new QToolButton( presetHost );
		presetAdd->setText( QStringLiteral( "+" ) );
		presetAdd->setToolTip( tr( "Save these values as a new preset" ) );
		presetRow->addWidget( presetAdd, 0 );
		auto * presetRemoveHost = new QWidget( presetHost );
		auto * presetRemoveLayout = new QHBoxLayout( presetRemoveHost );
		presetRemoveLayout->setContentsMargins( 0, 0, 0, 0 );
		auto * presetRemove = new QToolButton( presetRemoveHost );
		presetRemove->setText( QStringLiteral( "−" ) );
		presetRemoveLayout->addWidget( presetRemove );
		presetRow->addWidget( presetRemoveHost, 0 );
		addRow( tr( "Preset" ), presetHost );
		addRow( tr( "Collision layer" ), createLayerCombo );
		addRow( tr( "Motion system" ), createMotion );
		addRow( tr( "Quality type" ), createQuality );
		addRow( tr( "Solver deactivation" ), createSolver );
		addRow( tr( "Deactivator type" ), createDeactivator );
		addRow( tr( "Mass" ), createMass );
		addRow( tr( "Friction" ), createFriction );
		addRow( tr( "Restitution" ), createRestitution );
		addRow( tr( "Linear damping" ), createLinDamp );
		addRow( tr( "Angular damping" ), createAngDamp );
		addRow( tr( "Maximum linear velocity" ), createMaxLinVel );
		addRow( tr( "Maximum angular velocity" ), createMaxAngVel );
		addRow( tr( "Allowed penetration" ), createPenetration );
		popupLayout->addLayout( popupForm );

		// Material and Replace are the shape's, so they sit in the shape popup
		auto * shapeForm = new QGridLayout;
		shapeForm->setContentsMargins( 0, 0, 0, 0 );
		shapeForm->setHorizontalSpacing( 8 );
		shapeForm->setVerticalSpacing( 4 );
		shapeForm->setColumnStretch( 1, 1 );
		shapeForm->addWidget( new QLabel( tr( "Material" ), shapePopup ), 0, 0 );
		shapeForm->addWidget( materialEdit, 0, 1 );
		shapeForm->addWidget( replace, 1, 0, 1, 2 );
		shapeForm->addWidget( keepMesh, 2, 0, 1, 2 );
		shapeLayout->addLayout( shapeForm );

		auto * shapeButtons = new QHBoxLayout;
		auto * shapeCreate = new QPushButton( tr( "Create" ), shapePopup );
		shapeCreate->setDefault( true );
		shapeButtons->addStretch();
		shapeButtons->addWidget( shapeCreate );
		shapeLayout->addLayout( shapeButtons );

		auto * popupButtons = new QHBoxLayout;
		auto * popupCreate = new QPushButton( tr( "Create" ), createPopup );
		popupCreate->setDefault( true );
		popupButtons->addStretch();
		popupButtons->addWidget( popupCreate );
		popupLayout->addLayout( popupButtons );

		// nothing left in the form is shape-specific: the two settings that were
		// have moved to the preview that Convex and Mesh open
		auto showForShape = []() {};

		shapeGroup->button( std::clamp(
			createSettings.value( "CollisionManager/Create/Shape", 3 ).toInt(), 0, 4 ) )->setChecked( true );
		showForShape();
		// fields are filled by loadCreateFields() once populatePhysicsEnums has
		// run for real -- see the call after the physics editor is built

		createLayout->addWidget( createButton, 0, 0, 1, 2 );
		createLayout->addWidget( createShapeHost, 1, 0, 1, 2 );
		/* Create and Test share the bottom of the panel, one at a time.
		 *
		 * They are the two things you do with collision once you can see it --
		 * author it, then throw something at it -- and they are never wanted at
		 * the same moment. Side by side they would each get half the width and the
		 * panel would be twice as tall as a dock usefully is; stacked, whichever
		 * one you are not using is scrolled past on every trip. A switch costs one
		 * row and removes both problems.
		 */
		auto * bottomSwitch = new QWidget( this );
		auto * bottomSwitchLayout = new QHBoxLayout( bottomSwitch );
		bottomSwitchLayout->setContentsMargins( 0, 0, 0, 0 );
		bottomSwitchLayout->setSpacing( 0 );
		auto * bottomGroup = new QButtonGroup( bottomSwitch );
		bottomGroup->setExclusive( true );
		for ( int i = 0; i < 2; i++ ) {
			auto * b = new QToolButton( bottomSwitch );
			b->setProperty( i == 0 ? "wwSegmentFirst" : "wwSegmentLast", true );
			b->setText( i == 0 ? tr( "Collision Creation" ) : tr( "Collision Simulation" ) );
			b->setToolTip( i == 0
				? tr( "Build collision for the selected mesh" )
				: tr( "Simulate this file's ragdoll: drop it, drag it, shoot at it" ) );
			b->setCheckable( true );
			b->setMinimumHeight( 26 );
			b->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
			bottomGroup->addButton( b, i );
			bottomSwitchLayout->addWidget( b, 1 );
		}
		/* Its own copy of the segmented-control styling.
		 *
		 * It used to borrow createGroup's, which was set for the five shape
		 * buttons that lived in that group — when they went into a menu the
		 * group stopped carrying a stylesheet, and this switch silently lost the
		 * fill that says which of the two halves you are on. The one thing it
		 * must do is show which is selected, and nothing was left doing it.
		 *
		 * Selected is the selection blue with white on it, not the amber plate:
		 * orange text on an orange outline reads as a warning — the colour this
		 * skin uses for invalid material paths and missing textures — when all
		 * it means is "you are on this tab". Blue is what selection looks like
		 * everywhere else here, and in Blender.
		 */
		bottomSwitch->setStyleSheet( wwSegmentedToolButtonQss() );
		root->addWidget( bottomSwitch );
		root->addWidget( createGroup );

		/* The simulator, in its full form. The toolbar dropdown carries the same
		 * class in its Essentials mode -- one implementation in two sizes, so the
		 * two cannot come to disagree about what a control does.
		 */
		testPanel = new PhysicsSimPanel( ogl, nif, PhysicsSimPanel::Mode::Full,
			nullptr, this );
		testPanel->setObjectName( QStringLiteral( "CollisionTestPanel" ) );
		root->addWidget( testPanel );

		connect( bottomGroup, &QButtonGroup::idClicked, this,
			[this, createGroup]( int id ) {
				createGroup->setVisible( id == 0 );
				testPanel->setVisible( id == 1 );
				if ( id == 1 ) {
					testPanel->sync();
					refreshSimPins();
				}
				QSettings().setValue( "CollisionManager/BottomSection", id );
			} );
		// remembered, because which of the two you want is a property of the job
		// you are doing rather than of the session
		const int bottomSection = QSettings().value( "CollisionManager/BottomSection", 0 ).toInt() == 1 ? 1 : 0;
		bottomGroup->button( bottomSection )->setChecked( true );
		createGroup->setVisible( bottomSection == 0 );
		testPanel->setVisible( bottomSection == 1 );

		physicsGroup = new QGroupBox( tr( "Selected collision properties" ), this );
		auto * physicsOuter = new QVBoxLayout( physicsGroup );
		emptyPhysicsWidget = new QLabel( tr( "Select a collision row to inspect or edit it." ), physicsGroup );
		compiledSummaryWidget = new QWidget( physicsGroup );
		auto * compiledSummaryLayout = new QHBoxLayout( compiledSummaryWidget );
		compiledSummaryLayout->setContentsMargins( 0, 0, 0, 0 );
		compiledSummaryLabel = new QLabel( compiledSummaryWidget );
		compiledSummaryLabel->setWordWrap( true );
		auto * decompilePhysicsButton = new QPushButton( tr( "Decompile to Edit Physics" ), compiledSummaryWidget );
		compiledSummaryLayout->addWidget( compiledSummaryLabel, 1 );
		compiledSummaryLayout->addWidget( decompilePhysicsButton );
		physicsEditorBody = new QWidget( physicsGroup );
		auto * form = new QGridLayout( physicsEditorBody );
		form->setContentsMargins( 0, 0, 0, 0 );
		form->setColumnStretch( 1, 1 );
		form->setColumnStretch( 3, 1 );
		mass = new QDoubleSpinBox( physicsGroup );
		mass->setRange( 0.0, 1000000.0 ); mass->setDecimals( 3 );
		/* Named so findChild can reach them. The live editors write through
		 * nifSnapshotOp and there was no way to verify that from outside this
		 * class — every control is a private member. See WW_COLLUNDO_TEST.
		 */
		mass->setObjectName( QStringLiteral( "CollisionMassSpin" ) );
		friction = new QDoubleSpinBox( physicsGroup );
		friction->setObjectName( QStringLiteral( "CollisionFrictionSpin" ) );
		friction->setRange( 0.0, 10.0 ); friction->setDecimals( 3 );
		restitution = new QDoubleSpinBox( physicsGroup );
		restitution->setRange( 0.0, 1.0 ); restitution->setDecimals( 3 );
		layer = new QComboBox( physicsGroup );
		layer->setObjectName( QStringLiteral( "CollisionLayerCombo" ) );
		material = new QComboBox( physicsGroup );
		material->setObjectName( QStringLiteral( "CollisionMaterialCombo" ) );
		/* Plain pickers, like Motion and Quality beside them.
		 *
		 * These two were editable, for type-to-search over ~57 layers and ~30
		 * materials. It cost more than it bought. An editable QComboBox draws no
		 * drop-down arrow under this stylesheet, so the two controls at the top of
		 * the form did not look like the four below them and did not look
		 * clickable; the clear button put an X on a field that has no empty state
		 * -- a body always has a layer; and an unmatched edit left the field
		 * showing grey placeholder text where the body's actual material should
		 * be, which reads as "no material" on a body that has one.
		 *
		 * Non-editable still type-ahead: Qt jumps to an item on its first letters.
		 */
		layer->setMaxVisibleItems( 15 );
		material->setMaxVisibleItems( 15 );
		layer->view()->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
		material->view()->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
		/* The selectors sit in the same grid as the number fields and were
		 * visibly a different species of control - Qt's frame, its own
		 * background, a native arrow. Same tokens now, so a row of pickers and a
		 * row of typed values read as one set.
		 */
		for ( QComboBox * c : { layer, material, motionSystem, qualityType,
				solverDeactivation, deactivatorType } )
			wwMatchFieldStyle( c );
		linearDamping = new QDoubleSpinBox( physicsGroup );
		linearDamping->setRange( 0.0, 100.0 ); linearDamping->setDecimals( 3 );
		angularDamping = new QDoubleSpinBox( physicsGroup );
		angularDamping->setRange( 0.0, 100.0 ); angularDamping->setDecimals( 3 );
		maxLinearVelocity = new QDoubleSpinBox( physicsGroup );
		maxLinearVelocity->setRange( 0.0, 1000000.0 ); maxLinearVelocity->setDecimals( 2 );
		maxAngularVelocity = new QDoubleSpinBox( physicsGroup );
		maxAngularVelocity->setRange( 0.0, 1000000.0 ); maxAngularVelocity->setDecimals( 2 );
		motionSystem = new QComboBox( physicsGroup );
		qualityType = new QComboBox( physicsGroup );
		solverDeactivation = new QComboBox( physicsGroup );
		deactivatorType = new QComboBox( physicsGroup );
		populatePhysicsEnums();
		// now that the combos have rows, the create popup can show real values
		loadCreateFields();
		int dynamicRow = motionSystem->findData( 3 );
		if ( dynamicRow >= 0 ) motionSystem->setItemText( dynamicRow, tr( "Dynamic (3)" ) );
		int staticRow = motionSystem->findData( 5 );
		if ( staticRow >= 0 ) motionSystem->setItemText( staticRow, tr( "Static (5)" ) );
		/* The + that named a custom material is gone with it. It was a second way
		 * to do something the Create group already does: type a name into
		 * "Material for new collision" and collisionCreateMaterial hashes it the
		 * way the Bethesda exporter does and remembers it, after which it is in
		 * this list like any other. One route, on the side that creates things.
		 */
		physicsHint = new QLabel( tr( "Physics values appear here." ), physicsGroup );
		physicsHint->setWordWrap( true );
		auto addPhysicsRow = [form, this]( int row, const QString & leftLabel, QWidget * left,
			const QString & rightLabel, QWidget * right ) {
			form->addWidget( new QLabel( leftLabel, physicsGroup ), row, 0 );
			form->addWidget( left, row, 1 );
			form->addWidget( new QLabel( rightLabel, physicsGroup ), row, 2 );
			form->addWidget( right, row, 3 );
		};
		addPhysicsRow( 0, tr( "Collision layer" ), layer,
			tr( "Material of this body" ), material );
		addPhysicsRow( 1, tr( "Mass" ), mass, tr( "Motion" ), motionSystem );
		addPhysicsRow( 2, tr( "Friction" ), friction, tr( "Quality" ), qualityType );
		addPhysicsRow( 3, tr( "Restitution" ), restitution, tr( "Solver deact." ), solverDeactivation );
		addPhysicsRow( 4, tr( "Lin. damping" ), linearDamping, tr( "Max lin. vel." ), maxLinearVelocity );
		addPhysicsRow( 5, tr( "Ang. damping" ), angularDamping, tr( "Max ang. vel." ), maxAngularVelocity );
		auto * massFromMaterial = massFromMaterialButton
			= new QPushButton( tr( "Mass from Material..." ), physicsGroup );
		massFromMaterial->setToolTip( tr( "Calculate mass from collision volume and a suggested material density" ) );
		form->addWidget( massFromMaterial, 6, 0, 1, 4 );
		form->addWidget( physicsHint, 7, 0, 1, 4 );
		physicsOuter->addWidget( emptyPhysicsWidget );
		physicsOuter->addWidget( compiledSummaryWidget );
		physicsOuter->addWidget( physicsEditorBody );
		emptyPhysicsWidget->show();
		compiledSummaryWidget->hide();
		physicsEditorBody->hide();
		root->addWidget( physicsGroup );

		advancedSection = new QWidget( this );
		auto * advancedOuter = new QVBoxLayout( advancedSection );
		advancedOuter->setContentsMargins( 0, 0, 0, 0 );
		auto * advancedToggle = new QToolButton( advancedSection );
		advancedToggle->setText( tr( "Advanced physics and collision group" ) );
		advancedToggle->setCheckable( true );
		advancedToggle->setChecked( settings.value( "CollisionManager/AdvancedExpanded", false ).toBool() );
		advancedToggle->setArrowType( advancedToggle->isChecked() ? Qt::DownArrow : Qt::RightArrow );
		advancedToggle->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
		advancedBody = new QWidget( advancedSection );
		auto * advancedGroup = advancedBody;
		auto * advanced = new QGridLayout( advancedBody );
		advanced->setContentsMargins( 6, 3, 6, 3 );
		advancedOuter->addWidget( advancedToggle, 0, Qt::AlignLeft );
		advancedOuter->addWidget( advancedBody );
		advancedBody->setVisible( advancedToggle->isChecked() );
		connect( advancedToggle, &QToolButton::toggled, this, [advancedToggle, this]( bool expanded ) {
			advancedToggle->setArrowType( expanded ? Qt::DownArrow : Qt::RightArrow );
			advancedBody->setVisible( expanded );
			QSettings().setValue( "CollisionManager/AdvancedExpanded", expanded );
		} );
		keyframed = new QCheckBox( tr( "Keyframed" ), advancedGroup );
		linkedGroup = new QCheckBox( tr( "Linked Group" ), advancedGroup );
		collisionWithinGroup = new QCheckBox( tr( "Collision within Group" ), advancedGroup );
		wind = new QCheckBox( tr( "Wind" ), advancedGroup );
		phantom = new QCheckBox( tr( "Phantom" ), advancedGroup );
		shapePhantom = new QCheckBox( tr( "Shape Phantom" ), advancedGroup );
		phantom->setEnabled( false ); shapePhantom->setEnabled( false );
		phantom->setToolTip( tr( "Requires conversion to a bhkPCollisionObject phantom graph; compiled hknp phantom encoding is not validated yet" ) );
		shapePhantom->setToolTip( tr( "Requires bhkSPCollisionObject and bhkSimpleShapePhantom encoding" ) );
		filterGroup = new QSpinBox( advancedGroup ); filterGroup->setRange( 0, 65535 );
		penetrationDepth = new QDoubleSpinBox( advancedGroup ); penetrationDepth->setRange( 0.0, 100000.0 ); penetrationDepth->setDecimals( 4 );
		centerX = new QDoubleSpinBox( advancedGroup ); centerY = new QDoubleSpinBox( advancedGroup ); centerZ = new QDoubleSpinBox( advancedGroup );
		inertiaX = new QDoubleSpinBox( advancedGroup ); inertiaY = new QDoubleSpinBox( advancedGroup ); inertiaZ = new QDoubleSpinBox( advancedGroup );
		for ( QDoubleSpinBox * spin : { centerX, centerY, centerZ } ) { spin->setRange( -1000000.0, 1000000.0 ); spin->setDecimals( 4 ); }
		for ( QDoubleSpinBox * spin : { inertiaX, inertiaY, inertiaZ } ) { spin->setRange( 0.0, 1000000000.0 ); spin->setDecimals( 5 ); }

		/* Give every body field the number-field gesture, and a step that suits
		 * what it measures.
		 *
		 * The step is not decoration here: the shared field scrubs by
		 * singleStep, so a field left at Qt's default 1.0 would move mass by a
		 * kilo per ten pixels and friction by a whole unit over its 0..10 range.
		 * Setting it also fixes the arrow clicks and the keyboard arrows, which
		 * until now stepped by a different amount than the drag did.
		 */
		mass->setSingleStep( 1.0 );
		friction->setSingleStep( 0.01 );
		restitution->setSingleStep( 0.01 );
		linearDamping->setSingleStep( 0.01 );
		angularDamping->setSingleStep( 0.01 );
		maxLinearVelocity->setSingleStep( 1.0 );
		maxAngularVelocity->setSingleStep( 1.0 );
		penetrationDepth->setSingleStep( 0.001 );
		for ( QDoubleSpinBox * spin : { centerX, centerY, centerZ } )
			spin->setSingleStep( 0.1 );
		for ( QDoubleSpinBox * spin : { inertiaX, inertiaY, inertiaZ } )
			spin->setSingleStep( 100.0 );
		for ( QDoubleSpinBox * spin : { mass, friction, restitution, linearDamping,
				angularDamping, maxLinearVelocity, maxAngularVelocity, penetrationDepth,
				centerX, centerY, centerZ, inertiaX, inertiaY, inertiaZ } )
			wwMakeScrubField( spin );
		// Filter group is 16 packed bits, not a quantity: every intermediate
		// value during a drag would be a real write of a meaningless mask.
		wwNeverScrub( filterGroup );
		wwMakeScrubField( previewMaxHulls );
		auto * centerRow = new QWidget( advancedGroup ); auto * centerLayout = new QHBoxLayout( centerRow ); centerLayout->setContentsMargins( 0, 0, 0, 0 );
		centerLayout->addWidget( new QLabel( tr( "X" ), centerRow ) ); centerLayout->addWidget( centerX ); centerLayout->addWidget( new QLabel( tr( "Y" ), centerRow ) ); centerLayout->addWidget( centerY ); centerLayout->addWidget( new QLabel( tr( "Z" ), centerRow ) ); centerLayout->addWidget( centerZ );
		auto * inertiaRow = new QWidget( advancedGroup ); auto * inertiaLayout = new QHBoxLayout( inertiaRow ); inertiaLayout->setContentsMargins( 0, 0, 0, 0 );
		inertiaLayout->addWidget( new QLabel( tr( "X" ), inertiaRow ) ); inertiaLayout->addWidget( inertiaX ); inertiaLayout->addWidget( new QLabel( tr( "Y" ), inertiaRow ) ); inertiaLayout->addWidget( inertiaY ); inertiaLayout->addWidget( new QLabel( tr( "Z" ), inertiaRow ) ); inertiaLayout->addWidget( inertiaZ );
		advanced->addWidget( keyframed, 0, 0 ); advanced->addWidget( linkedGroup, 0, 1 );
		advanced->addWidget( collisionWithinGroup, 1, 0 ); advanced->addWidget( wind, 1, 1 );
		advanced->addWidget( phantom, 2, 0 ); advanced->addWidget( shapePhantom, 2, 1 );
		advanced->addWidget( new QLabel( tr( "Collision Filter Group" ), advancedGroup ), 3, 0 ); advanced->addWidget( filterGroup, 3, 1 );
		advanced->addWidget( new QLabel( tr( "C.O.M. (local)" ), advancedGroup ), 4, 0 ); advanced->addWidget( centerRow, 4, 1 );
		advanced->addWidget( new QLabel( tr( "Inertia tensor diagonal" ), advancedGroup ), 5, 0 ); advanced->addWidget( inertiaRow, 5, 1 );
		advanced->addWidget( new QLabel( tr( "Allowed penetration" ), advancedGroup ), 6, 0 ); advanced->addWidget( penetrationDepth, 6, 1 );
		advanced->addWidget( new QLabel( tr( "Deactivator Type" ), advancedGroup ), 7, 0 ); advanced->addWidget( deactivatorType, 7, 1 );
		root->addWidget( advancedSection );

		summary = new QLabel( this );
		summary->setWordWrap( true );
		summary->setStyleSheet( QStringLiteral( "QLabel { color: %1; padding: 3px; }" ).arg( wwSkinColor( "textMuted" ) ) );
		root->addWidget( summary );

		connect( tree, &QTreeWidget::currentItemChanged, this, [this]( QTreeWidgetItem * current ) {
			updateDetails();
			updateShapeButtonState();
			if ( current ) {
				if ( auto * w = dynamic_cast<NifSkope *>( mw ) ) {
					int block = current->parent() && current->data( 0, ShapeBlockRole ).toInt() >= 0
						? current->data( 0, ShapeBlockRole ).toInt()
						: current->data( 0, ObjectBlockRole ).toInt();
					syncingSelection = true;
					w->select( blockIndex( block ) );
					syncingSelection = false;
				}
			}
		} );
		connect( tree, &QTreeWidget::customContextMenuRequested, this,
			[this, shapeGroup]( const QPoint & pos ) {
			QTreeWidgetItem * item = tree->itemAt( pos );
			/* NO EARLY RETURN ON EMPTY SPACE.
			 *
			 * Creating is the first thing you do in an empty file, and an empty
			 * file is exactly when there is no row to right-click. Bailing out
			 * when itemAt() missed made the menu unreachable in the one state
			 * that needs it most. Everything below that needs a row is disabled
			 * rather than absent, so the menu reads the same either way — and
			 * `expand` in particular used to dereference item unconditionally.
			 */
			if ( item )
				tree->setCurrentItem( item );
			const bool compiled = item && item->data( 0, CompiledRole ).toBool();
			// the row menu can create, so the shape gate has to be current
			updateShapeButtonState();
			QMenu menu( this );
			QAction * newBody = menu.addAction( tr( "Create Collision Body…" ) );
			newBody->setToolTip( tr( "On the node selected in the block list" ) );
			QAction * newShape = menu.addAction( tr( "Create Collision Shape…" ) );
			newShape->setEnabled( createShapeButton && createShapeButton->isEnabled() );
			QAction * meshToColl = menu.addAction( tr( "Mesh to Collision…" ) );
			meshToColl->setToolTip(
				tr( "Turn the meshes selected in the block list into collision on this body" ) );
			meshToColl->setEnabled( createShapeButton && createShapeButton->isEnabled() );
			menu.addSeparator();
			QAction * decompile = menu.addAction( tr( "Decompile Selected" ) );
			decompile->setEnabled( compiled );
			QAction * decompileAllAction = menu.addAction( tr( "Decompile All" ) );
			QAction * compileAction = menu.addAction( tr( "Compile Selected" ) );
			compileAction->setEnabled( item && !compiled );
			// moved off a permanent button: it targets the row that was clicked,
			// which is exactly what a row menu is for
			QAction * importDonor = menu.addAction( tr( "Import Donor..." ) );
			importDonor->setToolTip( tr( "Copy collision from another NIF onto this target node" ) );
			importDonor->setEnabled( item != nullptr );
			menu.addSeparator();
			QAction * check = menu.addAction( tr( "Check Collision" ) );
			QAction * makeRenderProxy = menu.addAction( tr( "Create Editable Mesh Copy" ) );
			makeRenderProxy->setEnabled( item != nullptr );
			QAction * massFromMaterialAction = menu.addAction( tr( "Mass from Material..." ) );
			massFromMaterialAction->setEnabled( item && !compiled );
			/* MERGE, on the body that holds them.
			 *
			 * A box, a sphere and a mesh in one body are three shapes the engine
			 * tests separately; merged they are one. Offered on the BODY rather
			 * than on a shape selection because a body is what owns an
			 * arrangement — and because the tree is single-select, so "the
			 * selected shapes" would have been one shape.
			 */
			const qint32 mergeBody = item
				? ( item->parent() ? item->parent() : item )->data( 0, BodyBlockRole ).toInt()
				: -1;
			const QString mergeRefusal = tlMergeBodyShapesRefusal( nif, mergeBody );
			QAction * mergeShapes = menu.addAction( tr( "Merge Shapes into One Mesh" ) );
			mergeShapes->setEnabled( item && mergeRefusal.isEmpty() );
			mergeShapes->setToolTip( mergeRefusal.isEmpty()
				? tr( "Replace everything this body holds with a single mesh shape" )
				: mergeRefusal );

			QAction * expand = menu.addAction( item && item->isExpanded()
				? tr( "Collapse Shapes" ) : tr( "Expand Shapes" ) );
			expand->setEnabled( item && item->childCount() > 0 );
			QAction * selectBlock = menu.addAction( tr( "Select in Block List" ) );
			selectBlock->setEnabled( item != nullptr );
			QAction * setParent = menu.addAction( tr( "Set Parent from Block List" ) );
			setParent->setToolTip(
				tr( "Move this body's node under the block selected in the block list" ) );
			// bodies only: a shape row's parent is the body above it
			setParent->setEnabled( item && !item->parent() );
			menu.addSeparator();
			QAction * copyMaterial = menu.addAction( tr( "Copy Material Name and Value" ) );
			QAction * useMaterial = menu.addAction( tr( "Use Material for New Collision" ) );
			QAction * refreshAction = menu.addAction( tr( "Refresh" ) );
			QAction * chosen = menu.exec( tree->viewport()->mapToGlobal( pos ) );
			/* Through the BUTTONS, not a second copy of the create logic.
			 *
			 * They already open the popup positioned under themselves and carry
			 * the enable rules, so routing the menu through them keeps exactly
			 * one authoring path. A duplicate here is how the create popup and
			 * the panel came to disagree about the preset in the first place.
			 */
			if ( chosen == newBody ) {
				if ( createButton )
					createButton->click();
			} else if ( chosen == newShape ) {
				if ( createShapeButton )
					createShapeButton->click();
			} else if ( chosen == meshToColl ) {
				// Mesh is shape mode 4. Checking the button drives the same
				// idToggled path a click on it would, so showForShape() and the
				// settings write both happen — setting the key directly here
				// would leave the popup showing the previous shape.
				if ( QAbstractButton * meshBtn = shapeGroup->button( 4 ) )
					meshBtn->setChecked( true );
				if ( createShapeButton )
					createShapeButton->click();
			} else if ( chosen == setParent ) {
				reparentFromBlockList( item );
			} else if ( chosen == decompile )
				runSpell( QStringLiteral( "Havok/Decompile Compiled Collision" ),
					blockIndex( item->data( 0, ObjectBlockRole ).toInt() ) );
			else if ( chosen == decompileAllAction )
				runSpell( QStringLiteral( "Havok/Decompile All Compiled Collision" ), QModelIndex() );
			else if ( chosen == compileAction ) compileSelectedCollision();
			else if ( chosen == importDonor ) importDonorCollision();
			else if ( chosen == check ) lintCollision();
			else if ( chosen == makeRenderProxy ) collisionToBSTriShape();
			else if ( chosen == massFromMaterialAction ) calculateMassFromMaterial();
			else if ( chosen == mergeShapes ) {
				int verts = 0, was = 0;
				const QString trouble = tlMergeBodyShapes( nif, mergeBody, &verts, &was );
				if ( QStatusBar * bar = mw ? mw->statusBar() : nullptr )
					bar->showMessage( trouble.isEmpty()
						? tr( "Merged %1 shapes into one mesh, %2 vertices." ).arg( was ).arg( verts )
						: trouble, 5000 );
				queueRebuild();
			}
			else if ( chosen == expand ) item->setExpanded( !item->isExpanded() );
			else if ( chosen == selectBlock ) {
				if ( auto * w = dynamic_cast<NifSkope *>( mw ) ) {
					int block = item->parent() && item->data( 0, ShapeBlockRole ).toInt() >= 0
						? item->data( 0, ShapeBlockRole ).toInt() : item->data( 0, ObjectBlockRole ).toInt();
					w->select( blockIndex( block ) );
				}
			} else if ( chosen == copyMaterial ) {
				quint32 value = material->currentData().toUInt();
				QGuiApplication::clipboard()->setText( QStringLiteral( "%1 (0x%2)" )
					.arg( material->currentText() )
					.arg( QStringLiteral( "%1" ).arg( value, 8, 16, QLatin1Char( '0' ) ).toUpper() ) );
			} else if ( chosen == useMaterial ) {
				quint32 value = material->currentData().toUInt();
				QSettings s; s.setValue( "CollisionManager/Create/Material",
					QStringLiteral( "0x%1" ).arg( value, 8, 16, QLatin1Char( '0' ) ) );
			} else if ( chosen == refreshAction ) rebuild();
		} );
		/* Ticking a row pins that body. Guarded by `updating`, which
		 * refreshSimPins() raises while it writes the check states -- without it
		 * every refresh would read as a user click and toggle what it just set.
		 */
		connect( tree, &QTreeWidget::itemChanged, this, [this]( QTreeWidgetItem * item, int column ) {
			if ( updating || column != 0 || !ogl || !item )
				return;
			PhysicsPreview & pv = ogl->physicsSim();
			bool ok = false;
			const int body = item->data( 0, BodyIdRole ).toInt( &ok );
			if ( !ok || !pv.active() || body < 0 || body >= pv.bodyCount() )
				return;
			if ( item->data( 0, SystemBlockRole ).toInt() != pv.systemBlock() )
				return;
			pv.sim().setPinned( body, item->checkState( 0 ) == Qt::Checked );
			ogl->update();
		} );
		// entering or leaving the mode changes whether the pins mean anything
		connect( ogl, &GLView::physicsSimModeChanged, this,
			[this]( bool ) { refreshSimPins(); } );

		connect( tree, &QTreeWidget::itemExpanded, this, [this]( QTreeWidgetItem * item ) {
			if ( !item->parent() ) expandedObjects.insert( item->data( 0, ObjectBlockRole ).toInt() );
		} );
		connect( tree, &QTreeWidget::itemCollapsed, this, [this]( QTreeWidgetItem * item ) {
			if ( !item->parent() ) expandedObjects.remove( item->data( 0, ObjectBlockRole ).toInt() );
		} );
		connect( decompileSelectedAction, &QAction::triggered, this, [this]() {
			if ( tree->currentItem() )
				runSpell( QStringLiteral( "Havok/Decompile Compiled Collision" ),
					blockIndex( tree->currentItem()->data( 0, ObjectBlockRole ).toInt() ) );
		} );
		connect( decompileAllAction, &QAction::triggered, this, [this]() {
			runSpell( QStringLiteral( "Havok/Decompile All Compiled Collision" ), QModelIndex() );
		} );
		connect( compileSelectedAction, &QAction::triggered, this, [this]() { compileSelectedCollision(); } );
		connect( decompilePhysicsButton, &QPushButton::clicked, decompileSelectedAction, &QAction::trigger );
		connect( refreshAction, &QAction::triggered, this, [this]() { rebuild(); } );
		connect( lintAction, &QAction::triggered, this, [this]() { lintCollision(); } );
		connect( reverseAction, &QAction::triggered, this, [this]() { collisionToBSTriShape(); } );
		connect( massFromMaterial, &QPushButton::clicked, this, [this]() { calculateMassFromMaterial(); } );
		/* Every menu choice writes through immediately.
		 *
		 * The save button that used to do this is gone, and with it the state
		 * where the panel showed one preset and the next Create used another
		 * because nobody pressed save. A menu you ticked is a decision; asking
		 * for it to be confirmed by a second control was the panel not believing
		 * the first one.
		 */
		auto saveCreationSettings = [this, preset, materialEdit, replace, keepMesh, shapeGroup]() {
			QSettings settings;
			settings.setValue( "CollisionManager/Create/Shape", std::clamp( shapeGroup->checkedId(), 0, 4 ) );
			// A saved preset carries its NAME here. toInt() on a QString is 0,
			// which is Static, so writing it unconditionally would quietly
			// rewrite the built-in fallback every time a custom was selected.
			if ( preset->currentData().typeId() != QMetaType::QString )
				settings.setValue( "CollisionManager/Create/Preset", preset->currentData().toInt() );
			// the fields ARE the settings now; the preset only fills them in
			settings.setValue( "CollisionManager/Create/Layer", createLayerCombo->currentData().toUInt() );
			settings.setValue( "CollisionManager/Create/MotionSystem", createMotion->currentData().toUInt() );
			settings.setValue( "CollisionManager/Create/QualityType", createQuality->currentData().toUInt() );
			settings.setValue( "CollisionManager/Create/SolverDeactivation", createSolver->currentData().toUInt() );
			settings.setValue( "CollisionManager/Create/Mass", float( createMass->value() ) );
			settings.setValue( "CollisionManager/Create/Friction", float( createFriction->value() ) );
			settings.setValue( "CollisionManager/Create/Restitution", float( createRestitution->value() ) );
			settings.setValue( "CollisionManager/Create/LinearDamping", float( createLinDamp->value() ) );
			settings.setValue( "CollisionManager/Create/AngularDamping", float( createAngDamp->value() ) );
			settings.setValue( "CollisionManager/Create/MaxLinearVelocity", float( createMaxLinVel->value() ) );
			settings.setValue( "CollisionManager/Create/MaxAngularVelocity", float( createMaxAngVel->value() ) );
			settings.setValue( "CollisionManager/Create/DeactivatorType", createDeactivator->currentData().toUInt() );
			settings.setValue( "CollisionManager/Create/PenetrationDepth", float( createPenetration->value() ) );
			QString materialValue = materialEdit->currentText().trimmed();
			int materialRow = materialEdit->findText( materialValue, Qt::MatchFixedString );
			if ( materialRow >= 0 ) materialValue = materialEdit->itemData( materialRow ).toString();
			settings.setValue( "CollisionManager/Create/Material", materialValue );
			settings.setValue( "CollisionManager/Create/ReplaceShape", replace->isChecked() );
			settings.setValue( "CollisionManager/Create/KeepMesh", keepMesh->isChecked() );
			// ConvexMethod is the preview's now; it writes it on Apply
			settings.beginGroup( "Spells/Havok/Create Convex Shapes" );
			settings.setValue( "Replace Shape", replace->isChecked() );
			settings.endGroup();
		};
		connect( shapeGroup, &QButtonGroup::idToggled, this,
			[saveCreationSettings, showForShape]( int, bool checked ) {
				if ( checked ) { showForShape(); saveCreationSettings(); }
			} );
		/* Picking a preset FILLS the fields, then saves what they now say.
		 *
		 * In that order, and it matters: the preset is a shortcut for eleven
		 * values, not a twelfth value that overrides them. Loading first means
		 * what gets stored is exactly what the panel is showing, so there is no
		 * arrangement of the two where the file disagrees with the fields.
		 */
		auto updatePresetButtons = [this, preset, presetRemove, presetRemoveHost]() {
			const bool custom = preset->currentData().typeId() == QMetaType::QString;
			presetRemove->setEnabled( custom );
			presetRemoveHost->setToolTip( custom
				? tr( "Remove the saved preset '%1'" ).arg( preset->currentText() )
				: tr( "Built-in presets cannot be removed" ) );
		};
		updatePresetButtons();
		connect( preset, qOverload<int>( &QComboBox::currentIndexChanged ), this,
			[this, saveCreationSettings, updatePresetButtons, preset]( int ) {
				/* A saved preset is applied by the SAME three lines as a
				 * built-in — only where the map comes from differs. That is the
				 * whole reason presets are stored in this shape.
				 */
				const QVariant chosen = preset->currentData();
				QSettings s;
				QVariantMap d;
				if ( chosen.typeId() == QMetaType::QString ) {
					s.setValue( "CollisionManager/Create/PresetName", chosen.toString() );
					d = wwCollisionPresetValues( chosen.toString() );
				} else {
					s.remove( "CollisionManager/Create/PresetName" );
					s.setValue( "CollisionManager/Create/Preset", chosen.toInt() );
					d = tlCollisionPresetDefaults( chosen.toInt() );
				}
				for ( auto it = d.cbegin(); it != d.cend(); ++it )
					s.setValue( QStringLiteral( "CollisionManager/Create/" ) + it.key(), it.value() );
				loadCreateFields();
				saveCreationSettings();
				updatePresetButtons();
			} );
		connect( presetAdd, &QToolButton::clicked, this,
			[this, preset, saveCreationSettings, updatePresetButtons]() {
				// snapshot what the panel is SHOWING, which needs the fields
				// flushed to settings first — createFieldValues reads them back
				saveCreationSettings();
				bool ok = false;
				const QString name = QInputDialog::getText( this, tr( "Save Collision Preset" ),
					tr( "Preset name" ), QLineEdit::Normal,
					tr( "Preset %1" ).arg( wwCollisionPresetNames().size() + 1 ), &ok ).trimmed();
				if ( !ok || name.isEmpty() )
					return;
				const bool existed = preset->findData( name ) >= 0;
				if ( !existed && preset->findText( name, Qt::MatchFixedString ) >= 0 ) {
					QMessageBox::information( this, tr( "Save Collision Preset" ),
						tr( "'%1' is a built-in preset. Choose another name." ).arg( name ) );
					return;
				}
				if ( existed && QMessageBox::question( this, tr( "Save Collision Preset" ),
						tr( "Replace the saved preset '%1'?" ).arg( name ) ) != QMessageBox::Yes )
					return;
				if ( !wwCollisionPresetWrite( name, createFieldValues() ) ) {
					QMessageBox::warning( this, tr( "Save Collision Preset" ),
						tr( "Could not write %1." ).arg( WwLibrary::featureFile(
							QStringLiteral( "Collision" ), QStringLiteral( "Presets.json" ) ) ) );
					return;
				}
				QSettings().setValue( "CollisionManager/Create/PresetName", name );
				int row = preset->findData( name );
				if ( row < 0 ) {
					preset->addItem( name, name );
					row = preset->count() - 1;
				}
				preset->setCurrentIndex( row );
				updatePresetButtons();
			} );
		connect( presetRemove, &QToolButton::clicked, this,
			[this, preset, updatePresetButtons]() {
				const QVariant chosen = preset->currentData();
				if ( chosen.typeId() != QMetaType::QString )
					return;
				const QString name = chosen.toString();
				if ( QMessageBox::question( this, tr( "Remove Collision Preset" ),
						tr( "Remove the saved preset '%1'?" ).arg( name ) ) != QMessageBox::Yes )
					return;
				if ( !wwCollisionPresetRemove( name ) ) {
					QMessageBox::warning( this, tr( "Remove Collision Preset" ),
						tr( "Could not write %1." ).arg( WwLibrary::featureFile(
							QStringLiteral( "Collision" ), QStringLiteral( "Presets.json" ) ) ) );
					return;
				}
				const int row = preset->findData( name );
				if ( row >= 0 )
					preset->removeItem( row );
				QSettings().remove( "CollisionManager/Create/PresetName" );
				const int fallback = preset->findData( 1 );
				preset->setCurrentIndex( fallback >= 0 ? fallback : 0 );
				updatePresetButtons();
			} );
		/* Select on one click, rename on two — and the popup stays open, which
		 * is what makes the second gesture reachable at all. See setRenamer.
		 */
		preset->setKeepOpen( true );
		preset->setRenamer(
			[preset]( int row ) {
				return preset->itemData( row ).typeId() == QMetaType::QString;
			},
			[this, preset, updatePresetButtons]( int row, const QString & name ) {
				const QString from = preset->itemData( row ).toString();
				if ( from == name )
					return;
				if ( preset->findData( name ) >= 0
					 || preset->findText( name, Qt::MatchFixedString ) >= 0 ) {
					QMessageBox::information( this, tr( "Rename Collision Preset" ),
						tr( "'%1' is already taken." ).arg( name ) );
					return;
				}
				if ( !wwCollisionPresetRename( from, name ) ) {
					QMessageBox::warning( this, tr( "Rename Collision Preset" ),
						tr( "Could not write %1." ).arg( WwLibrary::featureFile(
							QStringLiteral( "Collision" ), QStringLiteral( "Presets.json" ) ) ) );
					return;
				}
				preset->setItemText( row, name );
				preset->setItemData( row, name );
				if ( preset->currentIndex() == row )
					QSettings().setValue( "CollisionManager/Create/PresetName", name );
				updatePresetButtons();
			} );
		for ( QComboBox * c : QList<QComboBox *>{ materialEdit, createLayerCombo, createMotion,
				createQuality, createSolver, createDeactivator } )
			connect( c, qOverload<int>( &QComboBox::currentIndexChanged ), this,
				[saveCreationSettings]( int ) { saveCreationSettings(); } );
		// no editTextChanged: the field is not editable any more. A custom
		// material is named through the popup's own "add" row, which appends a
		// real item and selects it, so currentIndexChanged above carries it.
		for ( QDoubleSpinBox * s : { createMass, createFriction, createRestitution, createLinDamp,
				createAngDamp, createMaxLinVel, createMaxAngVel, createPenetration } )
			connect( s, &QDoubleSpinBox::editingFinished, this,
				[saveCreationSettings]() { saveCreationSettings(); } );
		connect( replace, &QCheckBox::toggled, this,
			[saveCreationSettings]( bool ) { saveCreationSettings(); } );
		connect( keepMesh, &QCheckBox::toggled, this,
			[saveCreationSettings]( bool ) { saveCreationSettings(); } );

		/* Create Collision Body: the object, the body, and every physics value.
		 *
		 * One per selected mesh, which is the same rule the shape create follows
		 * and for the same reason — a NiAVObject holds one Collision Object, so
		 * two bodies means two nodes.
		 */
		auto createBody = [this, saveCreationSettings]() {
			/* TAKEN BEFORE THE POPUP HIDES. A drop with nothing to drop onto parks
			 * its meshes here and opens this popup; hiding the popup is what clears
			 * a parked drop that was dismissed instead of confirmed, so the claim
			 * has to happen first or the drop would be thrown away by its own
			 * Create button.
			 */
			const QList<QPersistentModelIndex> parked = pendingDropShapes;
			pendingDropShapes.clear();
			creatingBodyNow = true;
			saveCreationSettings();
			if ( createPopup ) createPopup->hide();
			if ( !nif )
				return;
			const QModelIndex source = currentSource();
			QList<QPersistentModelIndex> targets;
			for ( const qint32 block : spellSelectionRoots( nif, source ) ) {
				const QModelIndex index = blockIndex( block );
				if ( index.isValid() )
					targets.append( QPersistentModelIndex( index ) );
			}
			if ( targets.isEmpty() ) {
				QMessageBox::information( this, tr( "Create Collision Body" ),
					tr( "Select a node or a mesh to put the body on." ) );
				return;
			}
			const bool ownNode = targets.size() > 1;
			QModelIndex made;
			QList<int> unusable;
			QList<int> alreadyHad;
			int freshBodies = 0;
			nifSnapshotOp( nif, tr( "Create collision body" ), [&]() {
				for ( const QPersistentModelIndex & target : targets ) {
					// a target that went stale is REPORTED, not skipped: a silent
					// continue here is indistinguishable from a broken button, which
					// is the complaint the two messages below already exist to answer
					if ( !target.isValid() ) {
						unusable << -1;
						continue;
					}
					const QModelIndex node = tlCollisionAttachNode( nif, QModelIndex( target ), ownNode );
					if ( !node.isValid() ) {
						unusable << nif->getBlockNumber( QModelIndex( target ) );
						continue;
					}
					// tlCreateCollisionBody returns the body EXISTING OR NEW, so
					// this has to be asked before the call or the answer is
					// always yes. See the alreadyHad message below.
					const bool had = blockIndex( nif->getLink( node, "Collision Object" ) ).isValid();
					const QModelIndex body = tlCreateCollisionBody( nif, node );
					if ( body.isValid() ) {
						made = body;
						if ( had )
							alreadyHad << nif->getBlockNumber( node );
						else
							freshBodies++;
					} else {
						unusable << nif->getBlockNumber( QModelIndex( target ) );
					}
				}
			} );
			/* THE SECOND BODY THAT LOOKS LIKE A NO-OP.
			 *
			 * tlCreateCollisionBody returns the body EXISTING OR NEW: a
			 * NiAVObject holds exactly one Collision Object, so a second one on
			 * the same node is not a thing that can be made. It handed back the
			 * body already there, the selection moved to a row that was already
			 * selected, and the button appeared to do nothing whatsoever —
			 * reported, accurately, as "nothing happens". The valid return is
			 * why the not-usable message below never covered this.
			 *
			 * Both ways out get named, because neither is guessable from a
			 * refusal: shapes go on the body you already have, and a genuinely
			 * separate body needs a node of its own.
			 */
			if ( freshBodies == 0 && !alreadyHad.isEmpty() ) {
				QStringList numbers;
				for ( int block : alreadyHad )
					numbers << QString::number( block );
				QMessageBox::information( this, tr( "Create Collision Body" ),
					tr( "Block(s) %1 already have a collision body, and a block can hold "
						"only one.\n\nTo add geometry to it, use Create Collision Shape on "
						"that body. For a separate body, put the mesh under its own NiNode "
						"first." ).arg( numbers.join( QStringLiteral( ", " ) ) ) );
			}

			/* SAY SO WHEN THE SELECTION CANNOT TAKE ONE.
			 *
			 * The empty case above has always had a message. A selection that
			 * exists but cannot hold a body did not: tlCollisionAttachNode
			 * returned an invalid index, the loop skipped the block, and the
			 * button appeared to do nothing at all. Nothing in the panel, the
			 * viewport or the block list changed, so the only reading available
			 * was that the feature was broken.
			 *
			 * Named by block number rather than counted, because "1 block was
			 * skipped" does not tell you which of a multi-selection to fix.
			 */
			if ( !unusable.isEmpty() ) {
				QStringList numbers;
				for ( int block : unusable )
					numbers << QString::number( block );
				const QString detail =
					tr( "Block(s) %1 cannot hold a collision body. A body attaches to a "
						"NiNode — select a node, or a mesh that sits under one." )
					.arg( numbers.join( QStringLiteral( ", " ) ) );
				QMessageBox::information( this, tr( "Create Collision Body" ),
					made.isValid()
						? tr( "Part of the selection was skipped.\n\n%1" ).arg( detail )
						: detail );
			}
			if ( made.isValid() )
				if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) window->select( made );
			queueRebuild();
			creatingBodyNow = false;

			/* AND NOW THE DROP THAT WAS WAITING FOR THIS BODY.
			 *
			 * The meshes were parked when they were dropped on a file with nothing
			 * to put them in. The body exists now, so they go through the ordinary
			 * path: make the shapes, then move each one into the body that was just
			 * created — which is what dropping ON a body does, and the reason that
			 * code is shared rather than repeated here.
			 */
			if ( parked.isEmpty() || !made.isValid() || !createShapeNow )
				return;
			// tlCreateCollisionBody hands back the body, so `made` IS the target
			const QPersistentModelIndex intoBody = made;
			QList<qint32> blocks;
			for ( const QPersistentModelIndex & shape : parked )
				if ( shape.isValid() )
					blocks << nif->getBlockNumber( QModelIndex( shape ) );
			if ( blocks.isEmpty() || !intoBody.isValid() )
				return;

			if ( auto * window = dynamic_cast<NifSkope *>( mw ) )
				window->select( nif->getBlockIndex( blocks.first() ) );
			setBlockListSelection( blocks );

			QSet<qint32> before;
			for ( int b = 0; b < nif->getBlockCount(); b++ )
				if ( nif->blockInherits( nif->getBlockIndex( b ), "bhkRigidBody" ) )
					before.insert( b );
			QList<QPersistentModelIndex> had;
			for ( const qint32 b : std::as_const( before ) )
				had.append( QPersistentModelIndex( nif->getBlockIndex( b ) ) );

			createShapeNow();

			QSet<qint32> stillThere;
			for ( const QPersistentModelIndex & body : std::as_const( had ) )
				if ( body.isValid() )
					stillThere.insert( nif->getBlockNumber( QModelIndex( body ) ) );
			const qint32 target = nif->getBlockNumber( QModelIndex( intoBody ) );
			QList<QPersistentModelIndex> spare;
			for ( int b = 0; b < nif->getBlockCount(); b++ )
				if ( nif->blockInherits( nif->getBlockIndex( b ), "bhkRigidBody" )
					&& !stillThere.contains( b ) && b != target )
					spare.append( QPersistentModelIndex( nif->getBlockIndex( b ) ) );

			nifSnapshotOp( nif, tr( "Drop collision into new body" ), [&]() {
				for ( const QPersistentModelIndex & body : std::as_const( spare ) ) {
					if ( !body.isValid() || !intoBody.isValid() )
						continue;
					const qint32 shape = nif->getLink( QModelIndex( body ), "Shape" );
					if ( shape < 0 )
						continue;
					if ( !tlMoveCollisionShape( nif, shape,
						nif->getBlockNumber( QModelIndex( intoBody ) ) ).isEmpty() )
						continue;
					const qint32 emptied = nif->getBlockNumber( QModelIndex( body ) );
					for ( int b = 0; b < nif->getBlockCount(); b++ ) {
						const QModelIndex object = nif->getBlockIndex( b );
						if ( nif->blockInherits( object, "bhkCollisionObject" )
							&& nif->getLink( object, "Body" ) == emptied )
						{
							spRemoveBranch().castIfApplicable( nif, object );
							break;
						}
					}
				}
			} );
			if ( QStatusBar * bar = mw ? mw->statusBar() : nullptr )
				bar->showMessage( tr( "Made a body and put %1 dropped mesh(es) in it." )
					.arg( blocks.size() ), 5000 );
			queueRebuild();
		};

		auto createCollision = [this, shapeGroup, saveCreationSettings]() {
			saveCreationSettings();
			if ( shapePopup ) shapePopup->hide();
			const int mode = std::clamp( shapeGroup->checkedId(), 0, 4 );
			/* Convex and Mesh hand off to the live preview, which is where their
			 * parameters are. It opens at the last percentage and method it was
			 * left on rather than resetting to a full-density hull every time.
			 */
			QSettings previewState;
			const double keep = std::clamp(
				previewState.value( "CollisionManager/Preview/Ratio", 1.0 ).toDouble() * 100.0, 1.0, 100.0 );
			const bool decomposition =
				previewState.value( "CollisionManager/Preview/Decomposition", false ).toBool();
			if ( mode == 3 ) { showCollisionPreview( 0, keep, decomposition ); return; }
			if ( mode == 4 ) { showCollisionPreview( 1, keep ); return; }
			QString spellId;
			if ( mode == 0 ) spellId = QStringLiteral( "Havok/Create Box Collision" );
			else if ( mode == 1 ) spellId = QStringLiteral( "Havok/Create Sphere Collision" );
			else if ( mode == 2 ) spellId = QStringLiteral( "Havok/Create Capsule Collision" );
			runSpell( spellId, currentSource() );
		};
		/* The button opens the popup, just under itself and clamped to the screen.
		 *
		 * Not centred on the display, which was the first idea: the panel is
		 * docked at one edge and the thing you are about to operate on is in the
		 * viewport beside it, so a popup in the middle of the screen is the one
		 * place your eyes are not. Same reasoning as the search menu opening
		 * where the right-click was.
		 */
		// a member now, not a lambda: the mesh drop has to open the same popup and
		// hand-rolling the placement there put it half off the bottom of the screen
		// with the Create button out of reach. See openPopupUnder.
		connect( createButton, &QToolButton::clicked, this,
			[this]() { openPopupUnder( createPopup, createButton ); } );
		connect( createShapeButton, &QToolButton::clicked, this,
			[this]() { openPopupUnder( shapePopup, createShapeButton ); } );
		connect( popupCreate, &QPushButton::clicked, this, [createBody]() { createBody(); } );
		// the same action, reachable without the popup: the drop harness has nobody
		// to click Create, and skipping the popup entirely skipped the BODY with it
		createBodyNow = createBody;
		connect( shapeCreate, &QPushButton::clicked, this, [createCollision]() { createCollision(); } );
		// and a mesh dropped on the panel runs exactly this, so the two cannot
		// drift apart — see dropEvent
		createShapeNow = createCollision;
		for ( QDoubleSpinBox * spin : { mass, friction, restitution, linearDamping,
			angularDamping, maxLinearVelocity, maxAngularVelocity, centerX, centerY, centerZ,
			inertiaX, inertiaY, inertiaZ, penetrationDepth } )
			connect( spin, &QDoubleSpinBox::editingFinished, this, [this]() { applyPhysics(); } );
		connect( layer, qOverload<int>( &QComboBox::currentIndexChanged ), this,
			[this]( int row ) { applyLayerSelection( row ); } );
		connect( material, qOverload<int>( &QComboBox::currentIndexChanged ), this,
			[this]( int row ) { applyMaterialSelection( row ); } );
		for ( QComboBox * combo : { motionSystem, qualityType, solverDeactivation, deactivatorType } )
			connect( combo, qOverload<int>( &QComboBox::currentIndexChanged ), this,
				[this]( int ) { applyPhysics(); } );
		for ( QCheckBox * check : { keyframed, linkedGroup, collisionWithinGroup, wind } )
			connect( check, &QCheckBox::toggled, this, [this]( bool ) { applyPhysics(); } );
		connect( filterGroup, &QSpinBox::editingFinished, this, [this]() { applyPhysics(); } );
		connect( linkedGroup, &QCheckBox::toggled, collisionWithinGroup, &QWidget::setEnabled );
	}

	void connectModel()
	{
		connect( nif, &QAbstractItemModel::modelReset, this, [this]() {
			// The manager is constructed before a file is loaded. Rebuild all
			// version-specific enums now that BS Version is known.
			populatePhysicsEnums();
			loadCreateFields();
			queueRebuild();
		} );
		connect( nif, &QAbstractItemModel::rowsInserted, this,
			[this]( const QModelIndex &, int, int ) { queueRebuild(); } );
		connect( nif, &QAbstractItemModel::rowsRemoved, this,
			[this]( const QModelIndex &, int, int ) { queueRebuild(); } );
		connect( nif, &QAbstractItemModel::dataChanged, this,
			[this]( const QModelIndex &, const QModelIndex &, const QList<int> & ) { queueRebuild(); } );
		connect( nif, &NifModel::linksChanged, this, [this]() { queueRebuild(); } );
		connect( dock, &QDockWidget::visibilityChanged, this, [this]( bool visible ) {
			if ( visible ) rebuild(); else clearCollisionPreview();
		} );
		if ( auto * window = dynamic_cast<NifSkope *>( mw ) ) {
			connect( window, &NifSkope::completeLoading, this, [this]() { clearCollisionPreview(); } );
			// Selecting a bone anywhere - Skeleton Manager, block list, viewport -
			// highlights that bone's collision body here. The bone is the node the
			// collision object targets, which is already on every row.
			connect( window, &NifSkope::currentNifIndexChanged, this,
				[this]( const QModelIndex & index ) {
					if ( syncingSelection || updating || !dock->isVisible() || !nif )
						return;
					selectRowForBlock( nif->getBlockNumber( nif->getBlockIndex( index ) ) );
						// Create Collision Shape accepts a body chosen over there now,
						// so its enable test has to re-run when the BLOCK LIST moves
						// and not only when this panel's own list does.
						updateShapeButtonState();
				} );
		}
	}
};

} // namespace

/*! WW_COLLDROP_TEST: hand a drag or drop event to the Collision Manager panel.
 *
 *  The panel lives in this file with no header, so the harness cannot name its
 *  type — it hands in the window and this finds it. `dynamic_cast` rather than
 *  qobject_cast or findChild: the class carries no Q_OBJECT, and both of those
 *  go through the meta-object.
 *
 *  With a null \a event it answers whether the panel is there and accepting
 *  drops at all. That is the flag Qt gates on before any handler is reached, and
 *  the one thing a delivered event steps over — a panel with acceptDrops false
 *  would pass every check below it while doing nothing whatsoever for a real
 *  drag, which is exactly how the block list's drag first shipped.
 */
bool wwDeliverCollisionDrop( QMainWindow * mw, QEvent * event )
{
	if ( !mw )
		return false;
	for ( QWidget * widget : mw->findChildren<QWidget *>() ) {
		auto * panel = dynamic_cast<CollisionManagerPanel *>( widget );
		if ( !panel )
			continue;
		return event ? panel->wwDeliverDragEvent( event ) : panel->acceptDrops();
	}
	return false;
}

/*! WW_COLLDROP_TEST: the body under a point in the PANEL's coordinates, or -1.
 *
 *  This is the aiming device itself — the one call that decides "into that body"
 *  rather than "a body of its own" — so a harness has to be able to ask it, and
 *  ask the PANEL rather than work the mapping out a second time. A harness that
 *  computed its own answer would agree with a broken panel.
 */
//! WW_COLLDROP_TEST: is the panel's create hook wired? See hasCreateHook.
bool wwCollisionCreateWired( QMainWindow * mw )
{
	if ( !mw )
		return false;
	for ( QWidget * widget : mw->findChildren<QWidget *>() )
		if ( auto * panel = dynamic_cast<CollisionManagerPanel *>( widget ) )
			return panel->hasCreateHook();
	return false;
}

qint32 wwCollisionBodyAtPanelPoint( QMainWindow * mw, const QPoint & panelPos )
{
	if ( !mw )
		return -1;
	for ( QWidget * widget : mw->findChildren<QWidget *>() )
		if ( auto * panel = dynamic_cast<CollisionManagerPanel *>( widget ) )
			return panel->bodyUnderPanelPoint( panelPos );
	return -1;
}

//! The inventory tree, for a harness that cannot name its type.
static CollisionInventoryTree * wwCollisionTree( QMainWindow * mw )
{
	if ( !mw )
		return nullptr;
	for ( QWidget * widget : mw->findChildren<QWidget *>() )
		if ( auto * tree = dynamic_cast<CollisionInventoryTree *>( widget ) )
			return tree;
	return nullptr;
}

//! WW_COLLDROP_TEST: as wwDeliverCollisionDrop, for the inventory tree. A null
//! event asks whether it is taking drops at all — the flag Qt gates on, and the
//! one thing delivering an event directly steps over.
bool wwDeliverCollisionTreeDrag( QMainWindow * mw, QEvent * event )
{
	CollisionInventoryTree * tree = wwCollisionTree( mw );
	if ( !tree )
		return false;
	return event ? tree->wwDeliverDragEvent( event )
		: ( tree->dragEnabled() && tree->viewport()->acceptDrops() );
}

//! WW_COLLDROP_TEST: the payload a dragged shape row carries, built by the tree
//! itself so the harness cannot invent a second idea of the format.
QMimeData * wwCollisionShapePayload( QMainWindow * mw, qint32 shape )
{
	CollisionInventoryTree * tree = wwCollisionTree( mw );
	return tree ? tree->shapePayload( shape ) : nullptr;
}

/*! The shape a Collision Manager drag is carrying, or -1.
 *
 *  So the Block List can take a shape row without knowing the payload's format —
 *  which stays private to the tree, the same way the block payload's format stays
 *  private to the Block List. Each side reads the other's only through a call.
 */
qint32 wwCollisionShapeInDrag( QMainWindow * mw, const QMimeData * mime )
{
	CollisionInventoryTree * tree = wwCollisionTree( mw );
	return tree ? tree->wwShapeInPayload( mime ) : -1;
}

/*! Turn that shape into a BSTriShape under \a node — the loop closing.
 *
 *  A mesh dragged into the Collision Manager becomes collision; the same shape
 *  dragged back into the Block List becomes geometry again. Both directions go
 *  through the code that was already there: the conversion is the Collision
 *  Manager's own Collision to BSTriShape, told which shape and which node
 *  instead of asking the tree what is selected.
 *
 *  \return the empty string on success, or why not.
 */
QString wwCollisionShapeToMesh( QMainWindow * mw, qint32 shape, qint32 node )
{
	if ( !mw )
		return QObject::tr( "No window." );
	for ( QWidget * widget : mw->findChildren<QWidget *>() )
		if ( auto * panel = dynamic_cast<CollisionManagerPanel *>( widget ) )
			return panel->convertShapeToMesh( shape, node );
	return QObject::tr( "The Collision Manager is not open." );
}

/*! Compile one editable collision body into an FO4 hknp packfile.
 *
 *  Hoisted out of CollisionManagerPanel so it can be a spell. It was reachable
 *  only from a running dock, which is why it wrote layer Static/0/0 into every
 *  packfile it produced for as long as it did: nothing could execute it from a
 *  test.
 *
 *  \param object the bhkCollisionObject to compile
 *  \param confirmed the caller has already put the one-way-trip question to the
 *                   user, so do not ask again (SpellBook::cast does)
 *  \return the new bhkNPCollisionObject, or an invalid index if nothing was done
 */
QModelIndex tlCompileCollision( NifModel * nif, QWidget * parent,
	const QModelIndex & object, bool confirmed )
{
	if ( !nif || !nif->blockInherits( object, "bhkCollisionObject" ) )
		return QModelIndex();
	const int objectBlock = nif->getBlockNumber( object );
	const int bodyBlock = nif->getLink( object, "Body" );
	/* Target first, then the parent -- ownerNode()'s rule, because a collision
	 * object without a Target belongs to whatever holds it.
	 */
	const int nodeBlock = nif->isValidBlockNumber( nif->getLink( object, "Target" ) )
		? nif->getLink( object, "Target" ) : nif->getParent( objectBlock );
	QModelIndex body = tlCollBlockIndex( nif, bodyBlock ), node = tlCollBlockIndex( nif, nodeBlock );
	/* Every refusal below has to reach a headless caller as a message, not as a
	 * dialog: -no-gui builds a QCoreApplication, where constructing a QMessageBox
	 * aborts the process. The bone question already tested for this; the four
	 * failure paths did not, so `nifskope-cli cast` died on any file they had an
	 * opinion about -- which is most of what a compile sweep would test.
	 */
	const bool interactive = qobject_cast<QApplication *>( QCoreApplication::instance() ) != nullptr;
	auto refuse = [&]( const QString & text ) {
		if ( interactive )
			QMessageBox::warning( parent, QObject::tr( "Compile Collision" ), text );
		else
			qWarning().noquote() << text;
		return QModelIndex();
	};
	if ( !body.isValid() || !node.isValid() )
		return refuse( QObject::tr( "The selected collision has a broken body or target node." ) );
	// Compile writes ONE static body as a compressed mesh in a
	// bhkPhysicsSystem. That is right for world collision and wrong for a
	// bone: it triangulates the capsule and cannot restore a ragdoll (see the
	// Decompile warning). Bone collision is exactly where this would be a
	// one-way trip, so ask first and default to Cancel.
	/* Skipped when a SpellBook already asked -- destructiveWarning() carries the
	 * same paragraph, and two dialogs for one decision is what Spell::confirmedByBook
	 * exists to prevent. Also skipped with no QApplication: -no-gui builds a plain
	 * QCoreApplication, where a QMessageBox cannot be constructed.
	 */
	const QString role = ( confirmed || !qobject_cast<QApplication *>( QCoreApplication::instance() ) )
		? QString() : tlCollBoneRole( nif, tlCollBoneRoles( nif ), nodeBlock, -1 );
	if ( !role.isEmpty()
		&& QMessageBox::warning( parent, QObject::tr( "Compile Collision" ),
			// same correction as the Decompile warning in havok.cpp: the
			// constraints and skeleton DO decode and re-encode byte for byte.
			// What is missing is any NIF representation to carry them across.
			QObject::tr( "%1 is a bone (%2).\n\n"
				"Compile writes a single static body as a triangle mesh in a "
				"bhkPhysicsSystem. It cannot write bone collision, and it cannot rebuild "
				"a ragdoll - nothing carries the joint constraints or the ragdoll skeleton "
				"into NIF blocks, so a ragdoll this came from stays lost.\n\n"
				"Compile anyway?" ).arg( tlCollNodeName( nif, nodeBlock ), role ),
			QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Yes )
		return QModelIndex();

	CollisionMesh mesh; int rootShape = nif->getLink( body, "Shape" ); tlCollAppendEditableMesh( nif, rootShape, mesh );
	/* Strip triangles the 11-bit quantization will flatten anyway. The sphere
	 * and capsule tessellations close their poles with slivers whose float area
	 * is tiny but nonzero -- a plain zero-area test passes them, and then the
	 * packfile's grid collapses them into the degenerate triangles the vanilla
	 * corpus never carries (a compiled basketball read back 16).
	 *
	 * The threshold is a QUARTER step of the whole mesh's extent, and the
	 * quarter is measured, not chosen: sections quantize against their own
	 * smaller domains, so a triangle thin at the global scale can carry real
	 * area in its section -- TransitStation01 holds exactly four, at 0.55-0.88
	 * global steps but 1.3-2.1 of their sections' steps, and a full-step filter
	 * dropped them (a real hole, however thin). Nothing legitimate in the
	 * corpus sits below half a global step; the pole slivers sit at ~0.
	 */
	if ( !mesh.verts.isEmpty() ) {
		Vector3 mn = mesh.verts.first(), mx = mn;
		for ( const Vector3 & v : std::as_const( mesh.verts ) )
			for ( int a = 0; a < 3; a++ ) { mn[a] = std::min( mn[a], v[a] ); mx[a] = std::max( mx[a], v[a] ); }
		const Vector3 ext = mx - mn;
		const float step = std::max( { ext[0], ext[1], ext[2] } ) / 2047.0f * 0.25f;
		// the material list is parallel to the triangles, so it is filtered with
		// them -- dropping a sliver without dropping its material would slide
		// every later triangle onto the wrong one
		QVector<Triangle> kept; kept.reserve( mesh.tris.size() );
		QVector<quint32> keptMaterial; keptMaterial.reserve( mesh.triMaterial.size() );
		for ( qsizetype i = 0; i < mesh.tris.size(); i++ ) {
			const Triangle & t = mesh.tris.at( i );
			if ( t[0] >= mesh.verts.size() || t[1] >= mesh.verts.size() || t[2] >= mesh.verts.size() )
				continue;
			const Vector3 e1 = mesh.verts.at( t[1] ) - mesh.verts.at( t[0] );
			const Vector3 e2 = mesh.verts.at( t[2] ) - mesh.verts.at( t[0] );
			const Vector3 e3 = mesh.verts.at( t[2] ) - mesh.verts.at( t[1] );
			const float area2 = Vector3::crossproduct( e1, e2 ).length();
			const float base = std::max( { e1.length(), e2.length(), e3.length() } );
			if ( area2 >= 1.0e-12f && base > 0.0f && area2 / base >= step ) {
				kept.append( t );
				if ( i < mesh.triMaterial.size() )
					keptMaterial.append( mesh.triMaterial.at( i ) );
			}
		}
		mesh.tris = kept; mesh.triMaterial = keptMaterial;
	}
	HknpEncodeInput input; input.verts.reserve( mesh.verts.size() );
	for ( const Vector3 & v : mesh.verts ) input.verts.append( v / 69.99125f );
	input.tris = mesh.tris;
	// per-triangle materials, so a body made of parts keeps all of them; the
	// writer folds them into the shape's material table and the CMSD run table
	input.triMaterial = mesh.triMaterial;
	QModelIndex info = nif->getIndex( body, "Rigid Body Info" ); QModelIndex filter = bhkGetHavokFilter( nif, info );
	quint32 sourceMotion = info.isValid() ? nif->get<quint32>( info, "Motion System" ) : 5u;
	QStringList unsupported;
	/* 6 is MO_SYS_KEYFRAMED and IS supported now: the body takes a motion index
	 * and an inertia record with no dyn_motion behind it, which is how vanilla
	 * writes every animated door, gate and pushable. This guard named the case it
	 * was refusing, and refusing it is what made Compile turn every door into a
	 * static that could not open.
	 */
	if ( sourceMotion != 3u && sourceMotion != 5u && sourceMotion != 6u )
		unsupported << QObject::tr( "Motion/Keyframed mode %1" ).arg( sourceMotion );
	quint32 expectedQuality = sourceMotion == 3u ? 4u : 0u, expectedSolver = sourceMotion == 3u ? 2u : 1u;
	if ( info.isValid() && nif->get<quint32>( info, "Quality Type" ) != expectedQuality ) unsupported << QObject::tr( "Quality Type" );
	if ( info.isValid() && nif->get<quint32>( info, "Solver Deactivation" ) != expectedSolver ) unsupported << QObject::tr( "Solver Deactivation" );
	if ( nif->get<quint32>( body, "Body Flags" ) & 1u ) unsupported << QObject::tr( "Wind" );
	if ( info.isValid() && nif->get<quint32>( info, "Deactivator Type" ) != 1u ) unsupported << QObject::tr( "Deactivator Type" );
	if ( info.isValid() && std::fabs( nif->get<float>( info, "Penetration Depth" ) - 0.15f ) > 1.0e-5f ) unsupported << QObject::tr( "Allowed Penetration" );
	if ( !unsupported.isEmpty() )
		return refuse( QObject::tr( "These settings are valid on editable collision but their FO4 hknp byte layout is not validated yet, so Compile will not discard them:\n\n%1" )
			.arg( unsupported.join( QStringLiteral( "\n" ) ) ) );
	input.layer = filter.isValid() ? nif->get<quint32>( filter, "Layer" ) : 1u;
	input.filterFlags = filter.isValid() ? quint8( nif->get<quint32>( filter, "Flags" ) ) : 0u;
	input.filterGroup = filter.isValid() ? quint16( nif->get<quint32>( filter, "Group" ) ) : 0u;
	input.mass = info.isValid() ? nif->get<float>( info, "Mass" ) : 0.0f;
	input.dynamic = sourceMotion == 3u;
	/* Motion System 6 is MO_SYS_KEYFRAMED: a body the game MOVES without
	 * simulating it -- a door, a gate, a pushable car. It takes a motion index
	 * and an inertia record but no dyn_motion, and Decompile marks it so, because
	 * collapsing it to a static is what stopped every door in Concord opening.
	 */
	input.keyframed = sourceMotion == 6u;
	/* Body Flags bit 1 -- a DYNAMIC body that the engine converts to keyframed
	 * once, as it enters the world. Decompile puts it there; see HknpBodyFlag.
	 * Bit 0 is the Wind convention, refused above.
	 */
	input.addKeyframed = ( nif->get<quint32>( body, "Body Flags" ) & 2u ) != 0u;
	input.raiseTriggerEvents = ( nif->get<quint32>( body, "Body Flags" ) & 4u ) != 0u;
	input.friction = info.isValid() ? nif->get<float>( info, "Friction" ) : 0.5f;
	input.restitution = info.isValid() ? nif->get<float>( info, "Restitution" ) : 0.4f;
	input.gravityFactor = info.isValid() ? nif->get<float>( info, "Gravity Factor" ) : 1.0f;
	input.maxLinVelocity = info.isValid() ? nif->get<float>( info, "Max Linear Velocity" ) : 104.375f;
	input.maxAngVelocity = info.isValid() ? nif->get<float>( info, "Max Angular Velocity" ) : 31.57f;
	input.linDamping = info.isValid() ? nif->get<float>( info, "Linear Damping" ) : 0.1f;
	input.angDamping = info.isValid() ? nif->get<float>( info, "Angular Damping" ) : 0.05f;
	if ( info.isValid() ) {
		Vector4 center = nif->get<Vector4>( info, "Center" ); input.center = Vector3( center[0], center[1], center[2] );
		input.orientation = nif->get<Quat>( info, "Rotation" );
		QModelIndex inertia = nif->getIndex( info, "Inertia Tensor" );
		input.inertia = Vector3( nif->get<float>( inertia, "m11" ), nif->get<float>( inertia, "m22" ), nif->get<float>( inertia, "m33" ) );
	}
	QModelIndex materialShape = tlCollBlockIndex( nif, tlCollFirstLeafShape( nif, rootShape ) );
	input.materialCRC = materialShape.isValid() ? nif->get<quint32>( materialShape, "Material" ) : 0u;
	/* A convex source compiles to convex shapes, the way Elric does — measured on
	 * 1,500 SetDressing files, where the compiled class follows the SOURCE and not
	 * the motion type. Empty means this path could not write it, and the mesh
	 * below is what every convex source used to get.
	 */
	QString error; QByteArray bytes = tlCollCompileConvex( nif, rootShape, input, &error );
	const bool convexPath = !bytes.isEmpty();
	if ( bytes.isEmpty() ) {
		/* Empty with an error set means the source WAS convex and the writer could
		 * not finish it — a silent drop to the mesh would hide a real defect
		 * behind a worse shape, so say it happened. Empty with no error is the
		 * ordinary "not a convex source", which needs no comment.
		 */
		if ( !error.isEmpty() )
			qWarning().noquote() << QObject::tr( "Convex compile declined, falling back to a mesh:" ) << error;
		error.clear();
		bytes = hknpEncodeCompressedMesh( input, &error );
	}
	if ( bytes.isEmpty() )
		return refuse( error );
	HknpSystem roundTrip = hknpDecode( bytes );
	if ( !roundTrip.valid || roundTrip.shapes.isEmpty() )
		return refuse( QObject::tr( "The generated packfile failed NifSkope's round-trip check: %1" ).arg( roundTrip.error ) );
	if ( convexPath ) {
		/* A convex shape has no triangle count to check against — the mesh path's
		 * invariant — so the check is that the hull came back a hull: convex, with
		 * the vertices and faces that were written.
		 */
		const HknpShape & got = roundTrip.shapes.first();
		const bool primitive = got.primType == 1 || got.primType == 2;
		if ( !got.isConvex || ( !primitive && ( got.verts.size() < 4 || got.faces.isEmpty() ) ) )
			return refuse( QObject::tr( "The compiled convex shape did not read back as one — the packfile was not written." ) );
	} else {
		if ( roundTrip.shapes.first().tris.isEmpty() )
			return refuse( QObject::tr( "The generated packfile failed NifSkope's round-trip check: %1" ).arg( roundTrip.error ) );
		// section boundaries duplicate verts, so only the tri count is stable
		if ( roundTrip.shapes.first().tris.size() != input.tris.size() )
			return refuse( QObject::tr( "Round-trip triangle count mismatch (%1 encoded, %2 decoded) — the packfile was not written." )
				.arg( input.tris.size() ).arg( roundTrip.shapes.first().tris.size() ) );
	}
	QPersistentModelIndex oldObject = tlCollBlockIndex( nif, objectBlock ), target = node;
	QPersistentModelIndex newObject, newSystem;
	nifSnapshotOp( nif, QObject::tr( "Compile collision" ), [&]() {
		nif->holdUpdates( true );
		newSystem = nif->insertNiBlock( "bhkPhysicsSystem" );
		nif->set<QByteArray>( QModelIndex( newSystem ), "Binary Data", bytes );
		newObject = nif->insertNiBlock( "bhkNPCollisionObject" );
		nif->setLink( QModelIndex( newObject ), "Target", nodeBlock );
		nif->setLink( QModelIndex( newObject ), "Data", nif->getBlockNumber( QModelIndex( newSystem ) ) );
		nif->set<quint32>( QModelIndex( newObject ), "Body ID", 0u );
		nif->holdUpdates( false );
		/* Remove EXACTLY this branch.
		 *
		 * spRemoveBranch::cast consults the published Block List selection
		 * (spellSelectionRoots in blocks.cpp) and removes every root in it when
		 * the clicked block is one of several. That is right for Ctrl+Delete and
		 * catastrophic here: from the Block List, right-clicking one of five
		 * selected blocks would delete all five branches, silently, inside a
		 * snapshot labelled "Compile collision". Harmless while this lived in the
		 * dock, because the dock drives a single-row selection -- so the hoist is
		 * what makes it reachable, and the hoist has to bring the fix with it.
		 */
		if ( oldObject.isValid() ) {
			const QList<qint32> saved = blockListSelectionForSpells();
			setBlockListSelection( QList<qint32>() );
			spRemoveBranch remove;
			remove.castIfApplicable( nif, QModelIndex( oldObject ) );
			setBlockListSelection( saved );
		}
		if ( target.isValid() && newObject.isValid() )
			nif->setLink( nif->getIndex( QModelIndex( target ), "Collision Object" ), nif->getBlockNumber( QModelIndex( newObject ) ) );
		// inside the lambda on purpose: nifSnapshotOp serialises `after` the moment
		// op() returns, so a write placed below it would be missing from the redo
		wwEnsureRootBSXFlags( nif, BSXF_Havok );
	} );
	// the dock rebuilds itself off the model signals; a spell's caller selects
	// whatever cast() returns
	return QModelIndex( newObject );
}

QDockWidget * tlCreateCollisionManagerDock( NifModel * nif, QMainWindow * mw, GLView * ogl )
{
	auto * dock = new QDockWidget( QObject::tr( "Collision Manager" ), mw );
	dock->setObjectName( QStringLiteral( "CollisionManagerDock" ) );
	dock->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea );
	auto * scroll = new QScrollArea( dock );
	scroll->setObjectName( QStringLiteral( "CollisionManagerScrollArea" ) );
	scroll->setWidgetResizable( true );
	scroll->setFrameShape( QFrame::NoFrame );
	scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	scroll->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
	auto * panel = new CollisionManagerPanel( nif, mw, ogl, dock );
	/* CONTENT WIDE ENOUGH FOR ITS COLUMNS; DOCK FREE TO FOLD.
	 *
	 * The list has seven — Node, Bone, Shape, Layer, Material, Mass, State —
	 * and nothing here claimed any width, so the dock opened at whatever the
	 * main-window layout felt like giving it. Folded to about 290px that is
	 * Node/Bone/Shape/Layer and a horizontal scroll bar, with Material and Mass
	 * — two of the ones you actually author — off the end and no sign that they
	 * exist.
	 *
	 * The panel keeps the width that fits those columns, but the scroll area no
	 * longer inherits it. Folding the dock now reveals its horizontal scrollbar
	 * instead of forcing the entire main-window dock column to remain 640px wide.
	 *
	 * 560 fitted seven columns exactly; Parent made eight and the scroll bar
	 * came back, so this is re-measured rather than nudged.
	 */
	panel->setMinimumWidth( 640 );
	scroll->setWidget( panel );
	dock->setWidget( scroll );
	mw->addDockWidget( Qt::RightDockWidgetArea, dock );
	dock->hide();
	return dock;
}


/*! The collision lint, whole file, reported through logMessage.
 *
 *  The Collision Manager's Check button has done these tests for a while, but it
 *  ends in a QMessageBox and it counts rather than locates — "3 dangling collision
 *  reference(s)" tells you the file is wrong and not which block to open. Both
 *  make it useless to the Unfuck panel, which casts checks with an invalid index,
 *  drains logMessage, and offers Go to on whatever block a finding names.
 *
 *  So this reports ONE FINDING PER BLOCK, with the block number, which is the
 *  part that had to be rewritten rather than hoisted. The detection is the same
 *  walk, over the same helpers, now at file scope.
 *
 *  SEVERITY IS MEASURED, NOT ASSUMED — see spCheckAllMaterials in meshtools.cpp
 *  for how that went the first time. A dangling reference is a broken file. A
 *  box-like hull or an uncollided visible mesh is an observation: effect meshes
 *  and decorative geometry legitimately have no collision, so reporting those as
 *  warnings would put amber rows on most of the FO4 effects tree.
 */
class spCheckCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Collision Problems" ); }
	QString page() const override final { return Spell::tr( "Error Checking" ); }
	bool constant() const override final { return true; }
	bool checker() const override final { return true; }
	static QString message() { return Spell::tr( "Collision problems were found." ); }

	// cheap: SpellBook::checkers() is cast per-file by the bulk directory scan
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return !index.isValid() && nif && nif->getBlockCount() > 0;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & ) override final
	{
		auto say = [nif]( int b, const QString & what, QMessageBox::Icon lvl ) {
			nif->logMessage( message(), Spell::tr( "[%1] %2" ).arg( b ).arg( what ), lvl );
		};

		int compiled = 0, editable = 0;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex i = nif->getBlockIndex( b );

			if ( nif->blockInherits( i, "bhkNPCollisionObject" ) ) {
				compiled++;
				if ( !tlCollBlockIndex( nif, nif->getLink( i, "Data" ) ).isValid() )
					say( b, Spell::tr( "Compiled collision object's Data reference is missing." ),
						QMessageBox::Critical );
			} else if ( nif->blockInherits( i, "bhkCollisionObject" ) ) {
				editable++;
				const int body = nif->getLink( i, "Body" );
				const QModelIndex bi = tlCollBlockIndex( nif, body );
				if ( !bi.isValid() ) {
					say( b, Spell::tr( "Collision object's Body reference is missing." ),
						QMessageBox::Critical );
					continue;			// nothing below can be read without the body
				}

				const int leaf = tlCollFirstLeafShape( nif, nif->getLink( bi, "Shape" ) );
				const QModelIndex li = tlCollBlockIndex( nif, leaf );
				if ( li.isValid() && nif->isNiBlock( li, "bhkConvexVerticesShape" ) ) {
					const QVector<Vector4> vertices = nif->getArray<Vector4>( li, "Vertices" );
					if ( vertices.size() > 64 )
						say( leaf, Spell::tr( "Convex hull has %1 vertices; Havok handles 64 well." )
							.arg( vertices.size() ), QMessageBox::Warning );
					if ( vertices.size() >= 8 ) {
						Vector3 mn( vertices.first()[0], vertices.first()[1], vertices.first()[2] ), mx( mn );
						for ( const Vector4 & v : vertices )
							for ( int axis = 0; axis < 3; axis++ ) {
								mn[axis] = std::min( mn[axis], v[axis] );
								mx[axis] = std::max( mx[axis], v[axis] );
							}
						const float tolerance =
							std::max( { mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2] } ) * 0.03f + 1.0e-5f;
						bool boxLike = true;
						for ( const Vector4 & v : vertices )
							for ( int axis = 0; axis < 3; axis++ )
								if ( std::min( std::fabs( v[axis] - mn[axis] ),
										std::fabs( v[axis] - mx[axis] ) ) > tolerance )
									boxLike = false;
						if ( boxLike )
							say( leaf, Spell::tr( "Convex hull is box-shaped; a Box collision would be cheaper." ),
								QMessageBox::Information );
					}
				}

				const QModelIndex info = nif->getIndex( bi, "Rigid Body Info" );
				const QModelIndex filter = bhkGetHavokFilter( nif, info );
				const quint32 collLayer = filter.isValid() ? nif->get<quint32>( filter, "Layer" ) : 0;
				if ( filter.isValid() && collLayer == 0 )
					say( nif->getBlockNumber( bi ),
						Spell::tr( "Collision layer is Unidentified (0)." ), QMessageBox::Warning );
				if ( collLayer == 31 && li.isValid()
					&& ( nif->isNiBlock( li, "bhkBoxShape" ) || nif->isNiBlock( li, "bhkSphereShape" )
						|| nif->isNiBlock( li, "bhkCapsuleShape" ) ) )
					say( nif->getBlockNumber( bi ),
						Spell::tr( "STAIRHELPER body contains no sloped shape." ), QMessageBox::Warning );
			}

			if ( nif->isNiBlock( i, "bhkTransformShape" ) || nif->isNiBlock( i, "bhkConvexTransformShape" ) ) {
				const QModelIndex child =
					tlCollBlockIndex( nif, tlCollFirstLeafShape( nif, nif->getLink( i, "Shape" ) ) );
				if ( child.isValid() && ( nif->isNiBlock( child, "bhkBoxShape" )
						|| nif->isNiBlock( child, "bhkSphereShape" )
						|| nif->isNiBlock( child, "bhkCapsuleShape" ) ) ) {
					Vector3 trans, scales; Matrix rot;
					nif->get<Matrix4>( i, "Transform" ).decompose( trans, rot, scales );
					const float lo = std::min( { std::fabs( scales[0] ), std::fabs( scales[1] ), std::fabs( scales[2] ) } );
					const float hi = std::max( { std::fabs( scales[0] ), std::fabs( scales[1] ), std::fabs( scales[2] ) } );
					if ( hi - lo > std::max( 1.0e-4f, hi * 0.001f ) )
						say( b, Spell::tr( "Primitive collision transform uses non-uniform scale; Havok ignores it." ),
							QMessageBox::Warning );
				}
			}

			if ( nif->blockInherits( i, { "BSGeometry", "BSTriShape", "NiTriBasedGeom" } )
					&& !( nif->get<quint32>( i, "Flags" ) & 1u ) ) {
				bool protectedByCollision = false;
				int owner = nif->getParent( b );
				for ( int depth = 0; nif->isValidBlockNumber( owner ) && depth < 32;
						depth++, owner = nif->getParent( owner ) ) {
					const QModelIndex node = tlCollBlockIndex( nif, owner );
					if ( nif->blockInherits( node, "NiNode" )
						&& nif->isValidBlockNumber( nif->getLink( node, "Collision Object" ) ) ) {
						protectedByCollision = true;
						break;
					}
				}
				if ( !protectedByCollision )
					say( b, Spell::tr( "Visible geometry has no collision on its node hierarchy." ),
						QMessageBox::Information );
			}
		}

		// whole-file, so no block to name
		if ( compiled && editable )
			nif->logMessage( message(), Spell::tr(
				"File mixes compiled and editable collision; finish editing, then compile consistently." ),
				QMessageBox::Warning );

		return QModelIndex();
	}
};

REGISTER_SPELL( spCheckCollision )


/*! The two repairs the Collision Manager's Check button offers, as spells.
 *
 *  They existed only inside CollisionManagerPanel::lintCollision, behind a modal
 *  whose "Apply Safe Fixes" button applied BOTH to the WHOLE FILE at once. That
 *  is not something the Unfuck panel can offer against one finding, and it is
 *  not what a user clicking one row means.
 *
 *  Split into two per-block spells, so a finding that names block 12 repairs
 *  block 12 and nothing else. The whole-file pass in the dock is unchanged.
 */
class spFixCollisionLayer final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Set Collision Layer from Motion" ); }
	QString group() const override { return Spell::tr( "Fix" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }
	QString hint() const override final
	{
		return Spell::tr( "Replaces an Unidentified (0) collision layer with Props for a "
			"keyframed body, or Static for anything else." );
	}

	/*! Rigid Body Info of a body whose collision layer is Unidentified, or invalid.
	 *
	 *  Returns the INFO, not the filter, and the caller asks bhkGetHavokFilter
	 *  for the filter separately. That is not a stylistic choice: on everything
	 *  from Skyrim on, the filter rows are flattened INTO Rigid Body Info, so the
	 *  filter index and the info index are the same QModelIndex. The version that
	 *  returned the filter alone then read Motion System from `filter.parent()`
	 *  — which is Rigid Body Info when the row is nested, and the bhkRigidBody
	 *  block itself when it is flattened. Two different fields, silently.
	 */
	static QModelIndex zeroLayerInfo( const NifModel * nif, const QModelIndex & index )
	{
		if ( !nif )
			return QModelIndex();
		const QModelIndex body = nif->getBlockIndex( index );
		if ( !body.isValid() || !nif->blockInherits( body, "bhkRigidBody" ) )
			return QModelIndex();
		const QModelIndex info = nif->getIndex( body, "Rigid Body Info" );
		if ( !info.isValid() )
			return QModelIndex();
		const QModelIndex filter = bhkGetHavokFilter( nif, info );
		if ( !filter.isValid() || nif->get<quint32>( filter, "Layer" ) != 0 )
			return QModelIndex();
		return info;
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return zeroLayerInfo( nif, index ).isValid();
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		const QModelIndex info = zeroLayerInfo( nif, index );
		if ( !info.isValid() )
			return index;
		/* Motion System 3 is MO_SYS_KEYFRAMED: something the game moves, which
		 * belongs on Props (10). Everything else is scenery, which is Static (1).
		 * Same rule the dock's safe-fix pass uses -- this is that code, not a
		 * second opinion about it.
		 */
		const quint32 motion = nif->get<quint32>( info, "Motion System" );
		const QModelIndex body = nif->getBlockIndex( index );
		nifSnapshotOp( nif, Spell::tr( "Set collision layer" ), [&]() {
			// both copies of the filter, not just the Rigid Body Info one
			bhkSetFilterField( nif, body, QStringLiteral( "Layer" ), motion == 3 ? 10u : 1u );
		} );
		return index;
	}
};

REGISTER_SPELL( spFixCollisionLayer )

class spRemoveBrokenCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Remove Broken Collision Object" ); }
	QString group() const override { return Spell::tr( "Fix" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }
	QString hint() const override final
	{
		return Spell::tr( "Deletes a collision object whose Body or Data block is missing. "
			"It cannot collide with anything as it stands." );
	}

	/* Destructive, and it says so if cast from a menu. The Unfuck panel's per-row
	 * button calls cast() directly and therefore does not confirm -- that button
	 * is labelled with what it does, the row above it names the block, and
	 * runFixAt snapshots the model so Ctrl+Z takes it back.
	 */
	bool destructive() const override final { return true; }
	QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const override final
	{
		return Spell::tr( "Remove collision object [%1]? Its %2 block is missing, so it collides "
			"with nothing." ).arg( nif->getBlockNumber( index ) )
			.arg( nif->blockInherits( nif->getBlockIndex( index ), "bhkNPCollisionObject" )
				? QStringLiteral( "Data" ) : QStringLiteral( "Body" ) );
	}

	static bool isBroken( const NifModel * nif, const QModelIndex & index )
	{
		if ( !nif )
			return false;
		const QModelIndex i = nif->getBlockIndex( index );
		if ( nif->blockInherits( i, "bhkNPCollisionObject" ) )
			return !nif->isValidBlockNumber( nif->getLink( i, "Data" ) );
		if ( nif->blockInherits( i, "bhkCollisionObject" ) )
			return !nif->isValidBlockNumber( nif->getLink( i, "Body" ) );
		return false;
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		return isBroken( nif, index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		spRemoveBranch remove;
		remove.castIfApplicable( nif, nif->getBlockIndex( index ) );
		return QModelIndex();
	}
};

REGISTER_SPELL( spRemoveBrokenCollision )


/*! Compile Collision, from the Block List rather than only from the dock.
 *
 *  Decompile has been a spell for a long time; Compile was a private member of
 *  CollisionManagerPanel, so the Collision group in the context menu was a
 *  one-way trip. The stronger reason to move it is that a private dock member
 *  cannot be tested: this function wrote layer Static/0/0 into every packfile it
 *  produced for as long as it did, and it survived because nothing outside a
 *  running dock could execute it.
 */
class spCompileCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Compile Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }
	QString group() const override final { return Spell::tr( "Collision" ); }
	QString hint() const override final
	{
		return Spell::tr( "Rewrite this editable collision as a single static body: "
			"one triangle mesh in a compiled bhkPhysicsSystem." );
	}

	//! The bhkCollisionObject a click means: the object, its owner node, or its body.
	static QModelIndex targetObject( const NifModel * nif, const QModelIndex & index )
	{
		if ( !nif || !index.isValid() )
			return QModelIndex();
		QModelIndex iBlock = nif->getBlockIndex( index );
		if ( nif->blockInherits( iBlock, "bhkRigidBody" ) ) {
			const int body = nif->getBlockNumber( iBlock );
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				const QModelIndex i = nif->getBlockIndex( b );
				if ( nif->blockInherits( i, "bhkCollisionObject" ) && nif->getLink( i, "Body" ) == body )
					return i;
			}
			return QModelIndex();
		}
		if ( nif->blockInherits( iBlock, "NiAVObject" ) )
			iBlock = nif->getBlockIndex( nif->getLink( iBlock, "Collision Object" ) );
		return nif->blockInherits( iBlock, "bhkCollisionObject" ) ? iBlock : QModelIndex();
	}

	/* Destroys the editable shapes and cannot be reversed: a box, a sphere or a
	 * capsule does not come back out of a triangle mesh. The dock's own button
	 * never had a confirmation for this -- becoming a spell is what gives it one.
	 */
	bool destructive() const override final { return true; }
	QString destructiveWarning( NifModel * nif, const QModelIndex & index ) const override final
	{
		const QModelIndex object = targetObject( nif, index );
		const int objectBlock = nif->getBlockNumber( object );
		const int nodeBlock = nif->isValidBlockNumber( nif->getLink( object, "Target" ) )
			? nif->getLink( object, "Target" ) : nif->getParent( objectBlock );
		QString text = Spell::tr(
			"Compile the collision on %1 into a single static body: one triangle mesh "
			"in a bhkPhysicsSystem.\n\n"
			"This deletes the editable collision under [%2] %3. A box, a sphere or a "
			"capsule cannot be rebuilt from a triangle mesh, and constraints are not "
			"written into the packfile at all, so nothing decompiles them back." )
			.arg( tlCollNodeName( nif, nodeBlock ) )
			.arg( objectBlock ).arg( nif->itemName( object ) );
		const QString role = tlCollBoneRole( nif, tlCollBoneRoles( nif ), nodeBlock, -1 );
		if ( !role.isEmpty() )
			text += Spell::tr( "\n\n%1 is a bone (%2). Compile cannot write bone collision and "
				"cannot rebuild a ragdoll: nothing carries the joint constraints or the ragdoll "
				"skeleton into NIF blocks, so a ragdoll this came from stays lost." )
				.arg( tlCollNodeName( nif, nodeBlock ), role );
		return text;
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		/* FALLOUT 4 ONLY. hknpEncodeCompressedMesh writes an hk_2014.1.0-r1
		 * packfile with FO4 class names into a bhkPhysicsSystem. The dock never
		 * needed this test, because its tree only lists what the open file
		 * contains; a Block List entry would offer itself on a Skyrim
		 * bhkCollisionObject and write FO4 bytes into it.
		 */
		if ( !nif || nif->getBSVersion() != 130 )
			return false;
		const QModelIndex object = targetObject( nif, index );
		if ( !object.isValid() || nif->blockInherits( object, "bhkNPCollisionObject" ) )
			return false;
		const QModelIndex body = tlCollBlockIndex( nif, nif->getLink( object, "Body" ) );
		const int objectBlock = nif->getBlockNumber( object );
		const int nodeBlock = nif->isValidBlockNumber( nif->getLink( object, "Target" ) )
			? nif->getLink( object, "Target" ) : nif->getParent( objectBlock );
		if ( !body.isValid() || !tlCollBlockIndex( nif, nodeBlock ).isValid() )
			return false;
		/* Structural gate only. Whether the shape yields usable geometry is
		 * answered by building it, and building a convex hull is far too much
		 * work for a function that runs for every spell on every right-click.
		 */
		const QModelIndex leaf = tlCollBlockIndex( nif,
			tlCollFirstLeafShape( nif, nif->getLink( body, "Shape" ) ) );
		return leaf.isValid() && nif->blockInherits( leaf,
			{ "bhkBoxShape", "bhkSphereShape", "bhkCapsuleShape",
			  "bhkConvexVerticesShape", "bhkNiTriStripsShape" } );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		// confirmedByBook: SpellBook::cast has already asked, and destructiveWarning
		// carries the bone paragraph, so the inner gate must not ask a second time
		const QModelIndex made = tlCompileCollision( nif, nullptr,
			targetObject( nif, index ), Spell::confirmedByBook() );
		return made.isValid() ? made : index;
	}
};

REGISTER_SPELL( spCompileCollision )

/*! Merge every compiled bhkPhysicsSystem in the file into ONE.
 *
 * Compile writes one system per body because it compiles one body at a time.
 * Vanilla never does that: of 1,334 corpus files carrying collision, **1,334
 * hold exactly one** `hknpPhysicsSystemData`, and the several
 * bhkNPCollisionObjects that share it name their own body through Body ID. A NIF
 * with two systems is a thing the engine has never been asked to load, and
 * OfficeFileCabinet01 -- two systems, an animated drawer -- crashed inside
 * `bhkNPCollisionObject::CreateInstance` on 2026-08-22.
 *
 * This is index arithmetic, not byte surgery. Each system decodes; its shapes
 * and compounds move into one list with their child indices rebased; its bodies
 * join the body array; and the result re-encodes through `hknpEncodeSystem`,
 * which already reproduces vanilla systems byte for byte -- including a
 * multi-body one, which is what a ragdoll is. Nothing here touches a writer.
 *
 * The motion index is the part that is not a straight offset. `cinfo +0x0c`
 * indexes BOTH dyn_motion and dyn_inertia, so bodies that have one are numbered
 * in order across the merged system, and a body without stays 0x7fffffff. The
 * two array COUNTS need not agree: a keyframed body takes an inertia slot and no
 * motion record, which is exactly the cabinet -- one keyframed drawer and one
 * static shell give inertia 1, motion 0, body 1 at 0x7fffffff, and that is
 * vanilla's own layout for that file.
 */
bool tlCollMergePhysicsSystems( NifModel * nif, QString * error )
{
	auto fail = [&]( const QString & text ) { if ( error ) *error = text; return false; };
	if ( !nif )
		return fail( QObject::tr( "No file." ) );

	// every compiled collision object, in block order, with the system it names
	QVector<int> objects, named;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->blockInherits( i, "bhkNPCollisionObject" ) )
			continue;
		const int data = nif->getLink( i, "Data" );
		if ( nif->isValidBlockNumber( data ) ) { objects.append( b ); named.append( data ); }
	}
	QVector<int> parts;
	for ( int s : std::as_const( named ) )
		if ( !parts.contains( s ) )
			parts.append( s );
	if ( parts.size() < 2 )
		return true;    // already the shape vanilla ships

	HknpSystem merged;
	merged.rootClassName = QStringLiteral( "hknpPhysicsSystemData" );
	QVector<int> bodyBase;        // where each part's bodies landed
	int motionSlots = 0, motionArr = 0;
	for ( int s : std::as_const( parts ) ) {
		const HknpSystem part = hknpDecode( nif->get<QByteArray>( nif->getBlockIndex( s ), "Binary Data" ) );
		if ( !part.valid || part.bodyPhys.isEmpty() )
			return fail( QObject::tr( "Block %1 did not decode as a physics system: %2" ).arg( s ).arg( part.error ) );
		const int shapeBase = int( merged.shapes.size() );
		bodyBase.append( int( merged.bodyPhys.size() ) );
		for ( HknpShape sh : part.shapes ) {
			sh.bodyId = ( sh.bodyId < 0 ? 0 : sh.bodyId ) + bodyBase.last();
			merged.shapes.append( sh );
		}
		for ( HknpCompound c : part.compounds ) {
			c.bodyId = ( c.bodyId < 0 ? 0 : c.bodyId ) + bodyBase.last();
			for ( int & ch : c.children )
				ch += shapeBase;
			merged.compounds.append( c );
		}
		for ( HknpBodyPhys ph : part.bodyPhys ) {
			if ( ph.hasMotion ) {
				ph.motionIndex = motionSlots++;
				// a part that carried a real dyn_motion record needs the merged
				// motion array to reach its slot; a keyframed one does not
				if ( part.motionCount > 0 )
					motionArr = motionSlots;
			} else {
				ph.motionIndex = -1;
			}
			merged.bodyPhys.append( ph );
		}
		/* The shape list at +0x60 holds ONE ENTRY PER BODY, and the entry is a
		 * BODY INDEX -- the assembler reads it as one and calls the variable
		 * `body`. Offsetting these by the SHAPE base is what made it refuse a
		 * railing with "Shape list entry 4 names no body".
		 */
		if ( part.shapeListOrder.size() == part.bodyPhys.size() ) {
			for ( int k : part.shapeListOrder )
				merged.shapeListOrder.append( k + bodyBase.last() );
		} else {
			for ( qsizetype k = 0; k < part.bodyPhys.size(); k++ )
				merged.shapeListOrder.append( int( bodyBase.last() + k ) );
		}
		merged.dynamic = merged.dynamic || part.dynamic;
	}
	merged.inertiaCount = motionSlots;
	merged.motionCount = motionArr;
	/* One entry per BODY. Rebuilding it one-per-SHAPE is what made the assembler
	 * refuse a railing with "Shape list entry 4 names no body": five shapes, four
	 * bodies, five entries.
	 */
	if ( merged.shapeListOrder.size() != merged.bodyPhys.size() )
		return fail( QObject::tr( "Merged %1 shape-list entries for %2 bodies." )
			.arg( merged.shapeListOrder.size() ).arg( merged.bodyPhys.size() ) );

	QString err;
	const QByteArray bytes = hknpEncodeSystem( merged, &err );
	if ( bytes.isEmpty() )
		return fail( QObject::tr( "The merged system would not encode: %1" ).arg( err ) );
	const HknpSystem back = hknpDecode( bytes );
	if ( !back.valid || back.bodyPhys.size() != merged.bodyPhys.size() )
		return fail( QObject::tr( "The merged system did not read back (%1 bodies of %2): %3" )
			.arg( back.bodyPhys.size() ).arg( merged.bodyPhys.size() ).arg( back.error ) );

	const int keep = parts.first();
	nifSnapshotOp( nif, QObject::tr( "Merge physics systems" ), [&]() {
		nif->holdUpdates( true );
		nif->set<QByteArray>( nif->getBlockIndex( keep ), "Binary Data", bytes );
		for ( qsizetype j = 0; j < objects.size(); j++ ) {
			const QModelIndex obj = nif->getBlockIndex( objects.at( j ) );
			const int part = int( parts.indexOf( named.at( j ) ) );
			const int was = int( nif->get<quint32>( obj, "Body ID" ) );
			nif->setLink( obj, QStringLiteral( "Data" ), keep );
			nif->set<quint32>( obj, "Body ID", quint32( bodyBase.at( part ) + was ) );
		}
		// drop what nothing names any more, highest first so the numbers below stay put
		QVector<int> dead;
		for ( int s : std::as_const( parts ) )
			if ( s != keep )
				dead.append( s );
		std::sort( dead.begin(), dead.end(), std::greater<int>() );
		for ( int s : std::as_const( dead ) )
			nif->removeNiBlock( s );
		nif->holdUpdates( false );
	} );
	return true;
}

/*! The same as a spell, so the rebuild pipeline and the Block List can both run it.
 */
class spMergePhysicsSystems final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Merge Physics Systems" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }
	QString group() const override final { return Spell::tr( "Collision" ); }
	QString hint() const override final
	{
		return Spell::tr( "Put every compiled body in ONE bhkPhysicsSystem, the way vanilla "
			"does -- 1,334 of 1,334 corpus files carry exactly one." );
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		if ( !nif || index.isValid() )
			return false;      // a file-wide operation, like Decompile All
		int seen = 0;
		QVector<int> parts;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			const QModelIndex i = nif->getBlockIndex( b );
			if ( !nif->blockInherits( i, "bhkNPCollisionObject" ) )
				continue;
			const int data = nif->getLink( i, "Data" );
			if ( nif->isValidBlockNumber( data ) && !parts.contains( data ) ) { parts.append( data ); seen++; }
		}
		return seen > 1;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QString error;
		if ( !tlCollMergePhysicsSystems( nif, &error ) && !error.isEmpty() )
			qWarning().noquote() << QObject::tr( "Merge Physics Systems:" ) << error;
		return index;
	}
};

REGISTER_SPELL( spMergePhysicsSystems )

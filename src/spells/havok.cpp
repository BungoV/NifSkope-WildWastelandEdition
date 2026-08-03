#include "spellbook.h"

#include "gl/glshape.h"
#include "gl/gltools.h"
#include "gl/hknpdecode.h"
#include "glview.h"
#include "nifskope.h"
#include "nifsnapshot.h"
#include "spells/blocks.h"
#include "data/nifvalue.h"

#include "lib/coacd.h"
#include "libfo76utils/src/common.hpp"
#include "lib/nvtristripwrapper.h"
#include "lib/qhull.h"
#include "meshoptimizer/src/meshoptimizer.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMap>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

#include <algorithm> // std::sort
#include <cfloat>

// Brief description is deliberately not autolinked to class Spell
/*! \file havok.cpp
 * \brief Havok spells
 *
 * All classes here inherit from the Spell class.
 */

/*! For Havok coordinate transforms: game units per Havok unit.
 *
 *  Was a flat 7.0, which made the Fallout 4 factor (this × 10) 70.0 while every
 *  other place in the program that converts Havok units — gl/gltools.cpp's
 *  bhkScale, physics/physicspreview.h, the CLI — uses 1/1.42875 × 100 =
 *  69.99125. One codebase, two answers to the same question, 0.013 % apart:
 *  small enough that nothing ever looked wrong, and large enough that a shape
 *  built here and a shape decoded there did not agree to the last bit.
 */
static const float havokConst = 1.0f / 1.42875f * 10.0f;

static quint32 collisionCreateMaterial( const NifModel * nif );
static void applyCollisionBodySettings( NifModel * nif, const QModelIndex & rigidBody );

//! Creates a convex hull using Qhull
class spCreateCVS final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Create Convex Shapes" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	static bool hasGeometryData( const Node * node )
	{
		if ( !node )
			return false;
		if ( const Shape * s = dynamic_cast< const Shape * >( node ); s )
			return !( s->verts.isEmpty() || s->triangles.isEmpty() );
		const auto &	c = node->getChildren().list();
		for ( auto n : c ) {
			if ( hasGeometryData( n ) )
				return true;
		}
		return false;
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		if ( !( nif && nif->getBSVersion() > 0 && index.isValid() ) )
			return false;
		if ( !nif->blockInherits( index, { "BSGeometry", "BSTriShape", "NiNode", "NiTriBasedGeom" } ) )
			return false;
		Scene * scene = nullptr;
		if ( NifSkope * w = dynamic_cast< NifSkope * >( const_cast< NifModel * >( nif )->getWindow() ); w ) {
			if ( GLView * ogl = w->getGLView(); ogl )
				scene = ogl->getScene();
		}
		if ( !scene || !scene->renderer )
			return false;
		QModelIndex iBlock = nif->getBlockIndex( index );
		if ( auto node = scene->getNode( nif, iBlock ); node )
			return hasGeometryData( node );
		return false;
	}

	struct MeshData {
		QVector<Vector3> verts;
		QVector<unsigned int> indices;
		void removeDuplicateVertices();
	};

	static float getHavokScale( const NifModel * nif );
	static void getShapeData( QVector<MeshData> & meshes, NifModel * nif, const Node * node, int rootNode );
	static void createConvexShapes( QVector<MeshData> & meshes, CoACD & coacd );
	static void addLabel( QBoxLayout * parent, const QString & l );
	static QDoubleSpinBox * addSpinBox( QBoxLayout * parent, const QString & l,
										double v, double minVal, double maxVal, int nDigits );
	static QSpinBox * addSpinBox( QBoxLayout * parent, const QString & l, int v, int minVal, int maxVal );
	static QCheckBox * addCheckBox( QBoxLayout * parent, const QString & l, bool v );
	static QComboBox * addComboBox( QBoxLayout * parent, const QString & l, int v, const QStringList & itemList );
	static bool settingsDialog( CoACD & coacd, float & precision, float & radius, float & simplifyMaxError,
								bool & replaceShape, bool & enableCoACD );

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex	iBlock = nif->getBlockIndex( index );
		QModelIndex	iParent = iBlock;
		if ( !nif->blockInherits( iParent, "NiNode" ) ) {
			iParent = nif->getBlockIndex( nif->getParent( iBlock ) );
			if ( !( iParent.isValid() && nif->blockInherits( iParent, "NiNode" ) ) )
				return index;
		}
		QVector<MeshData>	meshes;
		{
			Scene * scene = nullptr;
			if ( NifSkope * w = dynamic_cast< NifSkope * >( nif->getWindow() ); w ) {
				if ( GLView * ogl = w->getGLView(); ogl )
					scene = ogl->getScene();
			}
			if ( !scene || !scene->renderer )
				return index;
			if ( auto node = scene->getNode( nif, iBlock ); node )
				getShapeData( meshes, nif, node, nif->getBlockNumber( iParent ) );
		}
		if ( meshes.isEmpty() )
			return index;
		meshes.first().removeDuplicateVertices();
		if ( meshes.constFirst().verts.isEmpty() || meshes.constFirst().indices.isEmpty() )
			return index;

		CoACD	coacd;
		float	precision = 0.25f;
		float	radius = 0.05f;
		float	simplifyMaxError = 0.0f;
		bool	replaceShape = false;
		bool	enableCoACD = false;
		QSettings previewSettings;
		const bool quickCreate = previewSettings.value( "CollisionManager/Preview/QuickCreate", false ).toBool();
		if ( quickCreate ) {
			precision = previewSettings.value( "CollisionManager/Preview/Precision", 0.25f ).toFloat();
			radius = previewSettings.value( "CollisionManager/Preview/Radius", 0.05f ).toFloat();
			replaceShape = previewSettings.value( "CollisionManager/Create/Replace", true ).toBool();
			enableCoACD = previewSettings.value( "CollisionManager/Preview/Decomposition", false ).toBool();
			coacd.threshold = previewSettings.value( "CollisionManager/Preview/Threshold", 0.05 ).toDouble();
			coacd.maxConvexHull = previewSettings.value( "CollisionManager/Preview/MaxHulls", 16 ).toInt();
			coacd.merge = ( coacd.maxConvexHull > 0 );
		} else if ( !settingsDialog( coacd, precision, radius, simplifyMaxError, replaceShape, enableCoACD ) ) {
			return index;
		}

		if ( quickCreate ) {
			float ratio = std::clamp( previewSettings.value( "CollisionManager/Preview/Ratio", 1.0 ).toFloat(), 0.01f, 1.0f );
			if ( ratio < 0.999f ) {
				auto & m = meshes.first();
				QVector<unsigned int> newIndices( m.indices.size() );
				size_t target = std::max<size_t>( 3, size_t( ( m.indices.size() / 3 ) * ratio ) * 3 );
				size_t n = meshopt_simplify( newIndices.data(), m.indices.constData(), size_t( m.indices.size() ),
					&( m.verts.constFirst()[0] ), size_t( m.verts.size() ), sizeof( Vector3 ),
					target, 0.02f, meshopt_SimplifyLockBorder, nullptr );
				newIndices.resize( qsizetype( n ) );
				m.indices = newIndices;
				m.removeDuplicateVertices();
			}
		}

		if ( simplifyMaxError >= 0.00005f ) {
			auto &	m = meshes.first();
			QVector<unsigned int>	newIndices;
			newIndices.resize( m.indices.size() );
			size_t	n = meshopt_simplify( newIndices.data(), m.indices.constData(), size_t( m.indices.size() ),
											&( m.verts.constFirst()[0] ), size_t( m.verts.size() ), sizeof( Vector3 ),
											12, simplifyMaxError, meshopt_SimplifyLockBorder, nullptr );
			newIndices.resize( qsizetype( n ) );
			m.indices = newIndices;
			m.removeDuplicateVertices();
		}

		if ( enableCoACD )
			createConvexShapes( meshes, coacd );

		if ( meshes.size() > 1 )
			nif->setState( BaseModel::Processing );
		QModelIndex	rigidBody = index;
		// For each shape: make a convex hull or box shape from it
		for ( const MeshData & m : meshes ) {
			QModelIndex	iCVS;
			qsizetype	cvsVertCount = 0;
			qsizetype	cvsNormCount = 0;
			if ( enableCoACD && coacd.apxMode == 1 && m.verts.size() == 8 ) {	// CoACD approximation mode = box
				FloatVector4	boundsMin = FloatVector4( m.verts.constFirst() );
				FloatVector4	boundsMax = boundsMin;
				for ( qsizetype i = 1; i < 8; i++ ) {
					FloatVector4	v = FloatVector4( m.verts.at( i ) );
					boundsMin.minValues( v );
					boundsMax.maxValues( v );
				}
				FloatVector4	boundsCenter( ( boundsMin + boundsMax ) * 0.5f );
				FloatVector4	boundsDims( ( boundsMax - boundsMin ) * 0.5f );
				iCVS = nif->insertNiBlock( "bhkBoxShape" );
				nif->set<Vector3>( iCVS, "Dimensions", Vector3( boundsDims ) );
				// radius is always 0.1?
				// TODO: Figure out if radius is not arbitrarily set in vanilla NIFs
				nif->set<float>( iCVS, "Radius", radius );
				{
					QModelIndex	i = nif->insertNiBlock( "bhkTransformShape" );
					nif->setLink( i, "Shape", nif->getBlockNumber( iCVS ) );
					iCVS = i;
				}
				nif->set<Matrix4>( iCVS, "Transform", Transform( Vector3( boundsCenter ), 1.0f ).toMatrix4() );

			} else {									// CoACD disabled or approximation mode = convex hull
				/* those will be filled with the CVS data */
				QVector<Vector4> convex_verts, convex_norms;

				// to store results
				QVector<Vector4> hullVerts, hullNorms;

				compute_convex_hull( m.verts, hullVerts, hullNorms, precision / getHavokScale( nif ) );

				// sort and remove duplicate vertices
				{
					QMap<Vector4, bool>	sortedVerts;
					for ( Vector4 vert : hullVerts )
						sortedVerts.insert( vert, false );
					for ( auto i = sortedVerts.constBegin(); i != sortedVerts.constEnd(); i++ )
						convex_verts.append( i.key() );
				}
				if ( cvsVertCount = convex_verts.size(); cvsVertCount < 4 )
					continue;

				// sort and remove duplicate normals
				{
					QMap<Vector4, bool>	sortedNorms;
					for ( Vector4 norm : hullNorms )
						sortedNorms.insert( norm, false );
					for ( auto i = sortedNorms.constBegin(); i != sortedNorms.constEnd(); i++ )
						convex_norms.append( i.key() );
				}
				cvsNormCount = convex_norms.size();

				/* create the CVS block */
				iCVS = nif->insertNiBlock( "bhkConvexVerticesShape" );

				/* set CVS verts */
				nif->set<uint>( iCVS, "Num Vertices", convex_verts.count() );
				nif->updateArraySize( iCVS, "Vertices" );
				nif->setArray<Vector4>( iCVS, "Vertices", convex_verts );

				/* set CVS norms */
				nif->set<uint>( iCVS, "Num Normals", convex_norms.count() );
				nif->updateArraySize( iCVS, "Normals" );
				nif->setArray<Vector4>( iCVS, "Normals", convex_norms );
			}

			// radius is always 0.1?
			// TODO: Figure out if radius is not arbitrarily set in vanilla NIFs
			nif->set<float>( iCVS, "Radius", radius );
			QModelIndex materialShape = iCVS;
			if ( nif->blockInherits( materialShape, "bhkTransformShape" ) )
				materialShape = nif->getBlockIndex( nif->getLink( materialShape, "Shape" ) );
			if ( quint32 material = collisionCreateMaterial( nif ); material && materialShape.isValid() )
				nif->set<quint32>( materialShape, "Material", material );

			QModelIndex collisionLink = nif->getIndex( iParent, "Collision Object" );
			QModelIndex collisionObject = nif->getBlockIndex( nif->getLink( collisionLink ) );

			// create bhkCollisionObject
			if ( !collisionObject.isValid() ) {
				collisionObject = nif->insertNiBlock( "bhkCollisionObject" );

				nif->setLink( collisionLink, nif->getBlockNumber( collisionObject ) );
				nif->setLink( collisionObject, "Target", nif->getBlockNumber( iParent ) );
			}

			QModelIndex rigidBodyLink = nif->getIndex( collisionObject, "Body" );
			rigidBody = nif->getBlockIndex( nif->getLink( rigidBodyLink ) );

			// create bhkRigidBody
			if ( !rigidBody.isValid() ) {
				rigidBody = nif->insertNiBlock( "bhkRigidBody" );

				nif->setLink( rigidBodyLink, nif->getBlockNumber( rigidBody ) );
			}
			applyCollisionBodySettings( nif, rigidBody );

			QPersistentModelIndex shapeLink = nif->getIndex( rigidBody, "Shape" );
			QPersistentModelIndex shape = nif->getBlockIndex( nif->getLink( shapeLink ) );

			if ( replaceShape && shape.isValid() ) {
				replaceShape = false;
				nif->setLink( shapeLink, -1 );
				// Remove all old shapes
				spRemoveBranch().castIfApplicable( nif, shape );
				shape = QModelIndex();
			}

			QVector<qint32> shapeLinks;
			bool replace = true;
			if ( shape.isValid() ) {
				shapeLinks = { nif->getBlockNumber( shape ) };

				QString questionTitle = tr( "Create List Shape" );
				QString questionBody = tr( "This collision object already has a shape. Combine into a list shape? 'No' will replace the shape." );

				bool isListShape = false;
				if ( nif->blockInherits( shape, "bhkListShape" ) ) {
					isListShape = true;
					questionTitle = tr( "Add to List Shape" );
					questionBody = tr( "This collision object already has a list shape. Add to list shape? 'No' will replace the list shape." );
					shapeLinks = nif->getLinkArray( shape, "Sub Shapes" );
				}

				int response = QMessageBox::Yes;
				if ( meshes.size() < 2 ) {
					response = QMessageBox::question( nullptr, questionTitle, questionBody,
														QMessageBox::Yes, QMessageBox::No );
				}
				if ( response == QMessageBox::Yes ) {
					QModelIndex iListShape = shape;
					if ( !isListShape ) {
						iListShape = nif->insertNiBlock( "bhkListShape" );
						nif->setLink( shapeLink, nif->getBlockNumber( iListShape ) );
					}

					shapeLinks << nif->getBlockNumber( iCVS );
					nif->set<uint>( iListShape, "Num Sub Shapes", shapeLinks.size() );
					nif->updateArraySize( iListShape, "Sub Shapes" );
					nif->setLinkArray( iListShape, "Sub Shapes", shapeLinks );
					nif->set<uint>( iListShape, "Num Filters", shapeLinks.size() );
					nif->updateArraySize( iListShape, "Filters" );
					replace = false;
				}
			}

			if ( replace ) {
				// Replace link
				nif->setLink( shapeLink, nif->getBlockNumber( iCVS ) );
				// Remove all old shapes
				spRemoveBranch().castIfApplicable( nif, shape );
			}

			if ( cvsVertCount > 0 ) {
				Message::append( nullptr, Spell::tr( "Create Convex Shapes" ),
									Spell::tr( "Created hull with %1 vertices, %2 normals" )
									.arg( cvsVertCount ).arg( cvsNormCount ), QMessageBox::Information );
			}
		}

		if ( meshes.size() > 1 )
			nif->restoreState();

		return rigidBody;
	}
};

void spCreateCVS::MeshData::removeDuplicateVertices()
{
	QMap<Vector3, unsigned int>	vertSet;
	QVector<unsigned int>	vertMap;
	vertMap.resize( verts.size() );
	unsigned int	n = 0;
	for ( const Vector3 & v : verts ) {
		qsizetype	j = qsizetype( &v - &(verts.constFirst()) );
		if ( auto i = vertSet.constFind( v ); i != vertSet.constEnd() ) {
			vertMap[j] = i.value();
			continue;
		}
		vertSet.insert( v, n );
		vertMap[j] = n;
		verts[n] = v;
		n++;
	}
	verts.resize( qsizetype( n ) );
	QVector<unsigned int>	newIndices;
	newIndices.reserve( indices.size() );
	for ( qsizetype i = 0; ( i + 3 ) <= indices.size(); i = i + 3 ) {
		unsigned int	v0 = indices.at( i );
		unsigned int	v1 = indices.at( i + 1 );
		unsigned int	v2 = indices.at( i + 2 );
		if ( qsizetype( std::max( std::max( v0, v1 ), v2 ) ) >= vertMap.size() )
			continue;
		v0 = vertMap.at( v0 );
		v1 = vertMap.at( v1 );
		v2 = vertMap.at( v2 );
		if ( v0 == v1 || v0 == v2 || v1 == v2 )
			continue;
		newIndices.append( v0 );
		newIndices.append( v1 );
		newIndices.append( v2 );
	}
	indices = newIndices;
}

float spCreateCVS::getHavokScale( const NifModel * nif )
{
	if ( nif->getBSVersion() >= 170 )
		return 1.0f;
	if ( nif->checkVersion( 0x14020007, 0x14020007 ) && nif->getUserVersion() >= 12 )
		return havokConst * 10.0f;
	return havokConst;
}

void spCreateCVS::getShapeData( QVector<MeshData> & meshes, NifModel * nif, const Node * node, int rootNode )
{
	if ( !node )
		return;
	const Shape *	s = dynamic_cast< const Shape * >( node );
	if ( !s ) {
		const auto &	c = node->getChildren().list();
		for ( auto n : c )
			getShapeData( meshes, nif, n, rootNode );
		return;
	}

	if ( s->verts.isEmpty() || s->triangles.isEmpty() )
		return;

	if ( meshes.isEmpty() )
		meshes.resize( 1 );
	MeshData &	m = meshes.last();
	qsizetype	vertexOffs = m.verts.size();
	qsizetype	indicesOffs = m.indices.size();
	m.verts.resize( vertexOffs + s->verts.size() );
	const Transform &	trans = s->localTrans( rootNode );
	Vector3 *	vp = m.verts.data() + vertexOffs;
	float	havokScale = getHavokScale( nif );
	for ( const Vector3 & v : s->verts ) {
		*vp = trans * v / havokScale;
		vp++;
	}
	m.indices.resize( indicesOffs + ( s->triangles.size() * 3 ) );
	unsigned int *	tp = m.indices.data() + indicesOffs;
	for ( const Triangle & t : s->triangles ) {
		tp[0] = (unsigned int) vertexOffs + t[0];
		tp[1] = (unsigned int) vertexOffs + t[1];
		tp[2] = (unsigned int) vertexOffs + t[2];
		tp = tp + 3;
	}
}

void spCreateCVS::createConvexShapes( QVector<MeshData> & meshes, CoACD & coacd )
{
	if ( meshes.isEmpty() )
		return;
	const MeshData &	m = meshes.constFirst();
	if ( m.verts.isEmpty() || m.indices.isEmpty() )
		return;

	CoACD::Mesh	coacdInput;
	qsizetype	numVerts = m.verts.size();
	qsizetype	numIndices = m.indices.size();
	qsizetype	numTriangles = numIndices / 3;
	coacdInput.vertices.resize( size_t( numVerts ) );
	coacdInput.indices.resize( size_t( numTriangles ) );
	for ( qsizetype i = 0; i < numVerts; i++ ) {
		coacdInput.vertices[i][0] = m.verts.at( i )[0];
		coacdInput.vertices[i][1] = m.verts.at( i )[1];
		coacdInput.vertices[i][2] = m.verts.at( i )[2];
	}
	for ( qsizetype i = 0; i < numTriangles; i++ ) {
		coacdInput.indices[i][0] = int( m.indices.at( i * 3 ) );
		coacdInput.indices[i][1] = int( m.indices.at( i * 3 + 1 ) );
		coacdInput.indices[i][2] = int( m.indices.at( i * 3 + 2 ) );
	}

	std::vector< CoACD::Mesh >	coacdOutput = coacd.processMesh( coacdInput );
	if ( coacdOutput.empty() )
		return;

	meshes.clear();
	meshes.resize( qsizetype( coacdOutput.size() ) );
	for ( size_t j = 0; j < coacdOutput.size(); j++ ) {
		const CoACD::Mesh &	o = coacdOutput[j];
		MeshData &	p = meshes[j];
		numVerts = qsizetype( o.vertices.size() );
		numTriangles = qsizetype( o.indices.size() );
		numIndices = numTriangles * 3;
		p.verts.resize( numVerts );
		p.indices.resize( numIndices );
		Vector3 *	vp = p.verts.data();
		for ( qsizetype i = 0; i < numVerts; i++ ) {
			*vp = Vector3( float( o.vertices[i][0] ), float( o.vertices[i][1] ), float( o.vertices[i][2] ) );
			vp++;
		}
		unsigned int *	tp = p.indices.data();
		for ( qsizetype i = 0; i < numTriangles; i++ ) {
			tp[0] = (unsigned int) o.indices[i][0];
			tp[1] = (unsigned int) o.indices[i][1];
			tp[2] = (unsigned int) o.indices[i][2];
			tp = tp + 3;
		}
	}
}

void spCreateCVS::addLabel( QBoxLayout * parent, const QString & l )
{
	parent->addWidget( new QLabel( l ) );
}

QDoubleSpinBox * spCreateCVS::addSpinBox( QBoxLayout * parent, const QString & l,
											double v, double minVal, double maxVal, int nDigits )
{
	QHBoxLayout *	hbox = new QHBoxLayout;
	parent->addLayout( hbox );
	QDoubleSpinBox *	o = new QDoubleSpinBox;
	o->setAccelerated( true );
	o->setRange( minVal, maxVal );
	o->setDecimals( nDigits );
	o->setSingleStep( std::pow( 10.0, double( 1 - nDigits ) ) );
	o->setValue( v );
	hbox->addWidget( new QLabel( l ) );
	hbox->addWidget( o );
	return o;
}

QSpinBox * spCreateCVS::addSpinBox( QBoxLayout * parent, const QString & l, int v, int minVal, int maxVal )
{
	QHBoxLayout *	hbox = new QHBoxLayout;
	parent->addLayout( hbox );
	QSpinBox *	o = new QSpinBox;
	o->setAccelerated( true );
	o->setRange( minVal, maxVal );
	o->setSingleStep( 1 );
	o->setValue( v );
	hbox->addWidget( new QLabel( l ) );
	hbox->addWidget( o );
	return o;
}

QCheckBox * spCreateCVS::addCheckBox( QBoxLayout * parent, const QString & l, bool v )
{
	QCheckBox *	o = new QCheckBox( l );
	o->setChecked( v );
	parent->addWidget( o );
	return o;
}

QComboBox * spCreateCVS::addComboBox( QBoxLayout * parent, const QString & l, int v, const QStringList & itemList )
{
	QHBoxLayout *	hbox = new QHBoxLayout;
	parent->addLayout( hbox );
	QComboBox *	o = new QComboBox;
	o->addItems( itemList );
	o->setCurrentIndex( v );
	hbox->addWidget( new QLabel( l ) );
	hbox->addWidget( o );
	return o;
}

bool spCreateCVS::settingsDialog( CoACD & coacd, float & precision, float & radius, float & simplifyMaxError,
									bool & replaceShape, bool & enableCoACD )
{
	{
		QSettings	settings;
		settings.beginGroup( "Spells/Havok/Create Convex Shapes" );
		precision = settings.value( "Precision", 0.25f ).toFloat();
		radius = settings.value( "Radius", 0.05f ).toFloat();
		simplifyMaxError = settings.value( "Simplify Max Error", 0.0f ).toFloat();
		replaceShape = settings.value( "Replace Shape", false ).toBool();
		enableCoACD = settings.value( "Enable CoACD", false ).toBool();
		coacd.loadSettings( settings );
		settings.endGroup();
	}

	// ask for precision
	QDialog dlg;
	QVBoxLayout * vbox = new QVBoxLayout;
	dlg.setLayout( vbox );

	addLabel( vbox, Spell::tr( "Enter the maximum Qhull roundoff error to use (in NIF units)" ) );
	addLabel( vbox, Spell::tr( "Larger values will give a less precise but better performing hull" ) );
	auto precSpin = addSpinBox( vbox, QString(), precision, 0.0, 5.0, 3 );
	auto spnSimplifyMaxError = addSpinBox( vbox, Spell::tr( "Pre-Simplify Max Error (relative to mesh size)" ),
											simplifyMaxError, 0.0, 0.5, 4 );
	auto chkReplaceShape = addCheckBox( vbox, Spell::tr( "Replace Existing Collision Shapes" ), replaceShape );
	auto spnRadius = addSpinBox( vbox, Spell::tr( "Collision Radius (in Havok units)" ), radius, 0.0, 0.5, 4 );

	addLabel( vbox, QString() );

	auto chkEnableCoACD = addCheckBox( vbox, Spell::tr( "Enable CoACD" ), enableCoACD );

	auto spnThreshold = addSpinBox( vbox, Spell::tr( "Threshold" ), coacd.threshold, 0.01, 1.0, 3 );
	auto spnMaxConvexHull = addSpinBox( vbox, Spell::tr( "Max Convex Hull" ), coacd.maxConvexHull, -1, 256 );
	auto chkPreprocessMode = addComboBox( vbox, Spell::tr( "Preprocess Mode" ), coacd.preprocessMode,
											{ Spell::tr( "Auto" ), Spell::tr( "On" ), Spell::tr( "Off" ) } );
	auto spnPrepResolution = addSpinBox( vbox, Spell::tr( "Preprocess Resolution" ), coacd.prepResolution, 10, 1000 );
	auto spnSampleResolution = addSpinBox( vbox, Spell::tr( "Sample Resolution" ), coacd.sampleResolution, 100, 20000 );
	auto spnMCTSNodes = addSpinBox( vbox, Spell::tr( "MCTS Nodes" ), coacd.mctsNodes, 5, 100 );
	auto spnMCTSIteration = addSpinBox( vbox, Spell::tr( "MCTS Iteration" ), coacd.mctsIteration, 10, 1000 );
	auto spnMCTSMaxDepth = addSpinBox( vbox, Spell::tr( "MCTS Max Depth" ), coacd.mctsMaxDepth, 1, 5 );
	auto chkPCA = addCheckBox( vbox, Spell::tr( "Use PCA" ), coacd.pca );
	auto spnMaxCHVertex = addSpinBox( vbox, Spell::tr( "Max Convex Hull Vertex" ), coacd.maxCHVertex, -1, 4096 );
	auto spnExtrudeMargin = addSpinBox( vbox, Spell::tr( "Extrude Margin (in Havok units)" ), coacd.extrudeMargin,
										0.0, 1.0, 4 );
	auto chkApxMode = addComboBox( vbox, Spell::tr( "Approximation Mode" ), coacd.apxMode,
									{ Spell::tr( "Convex Hull" ), Spell::tr( "Box" ) } );
	auto spnSeed = addSpinBox( vbox, Spell::tr( "Random Seed" ), coacd.seed, 0, 0x7FFFFFFF );

	addLabel( vbox, QString() );

	QHBoxLayout * hbox = new QHBoxLayout;
	vbox->addLayout( hbox );

	QPushButton * ok = new QPushButton;
	ok->setText( Spell::tr( "Ok" ) );
	hbox->addWidget( ok );

	QPushButton * cancel = new QPushButton;
	cancel->setText( Spell::tr( "Cancel" ) );
	hbox->addWidget( cancel );

	QObject::connect( ok, &QPushButton::clicked, &dlg, &QDialog::accept );
	QObject::connect( cancel, &QPushButton::clicked, &dlg, &QDialog::reject );

	if ( dlg.exec() != QDialog::Accepted ) {
		return false;
	}

	precision = float( precSpin->value() );
	radius = float( spnRadius->value() );
	simplifyMaxError = float( spnSimplifyMaxError->value() );
	replaceShape = chkReplaceShape->isChecked();
	enableCoACD = chkEnableCoACD->isChecked();

	QSettings	settings;
	settings.beginGroup( "Spells/Havok/Create Convex Shapes" );
	settings.setValue( "Precision", QVariant( precision ) );
	settings.setValue( "Radius", QVariant( radius ) );
	settings.setValue( "Simplify Max Error", QVariant( simplifyMaxError ) );
	settings.setValue( "Replace Shape", QVariant( replaceShape ) );
	settings.setValue( "Enable CoACD", QVariant( enableCoACD ) );
	if ( enableCoACD ) {
		coacd.threshold = float( spnThreshold->value() );
		coacd.maxConvexHull = spnMaxConvexHull->value();
		coacd.preprocessMode = chkPreprocessMode->currentIndex();
		coacd.prepResolution = spnPrepResolution->value();
		coacd.sampleResolution = spnSampleResolution->value();
		coacd.mctsNodes = spnMCTSNodes->value();
		coacd.mctsIteration = spnMCTSIteration->value();
		coacd.mctsMaxDepth = spnMCTSMaxDepth->value();
		coacd.pca = chkPCA->isChecked();
		coacd.merge = ( coacd.maxConvexHull > 0 );
		coacd.maxCHVertex = spnMaxCHVertex->value();
		coacd.decimate = ( coacd.maxCHVertex > 0 );
		coacd.extrudeMargin = float( spnExtrudeMargin->value() );
		coacd.extrude = ( coacd.extrudeMargin >= 0.00005f );
		coacd.apxMode = chkApxMode->currentIndex();
		coacd.seed = spnSeed->value();
		coacd.saveSettings( settings );
	}
	settings.endGroup();

	return true;
}

static quint32 collisionCreateMaterial( const NifModel * nif )
{
	QSettings settings;
	QString text = settings.value( "CollisionManager/Create/Material", "MaterialMetalSolid" ).toString().trimmed();
	bool ok = false;
	quint32 value = text.toUInt( &ok, 0 );
	if ( !ok ) {
		QString enumType = nif->getBSVersion() >= 170 ? QStringLiteral( "Fallout76HavokMaterial" )
			: nif->getBSVersion() >= 130 ? QStringLiteral( "Fallout4HavokMaterial" )
			: nif->getBSVersion() >= 83 ? QStringLiteral( "SkyrimHavokMaterial" )
			: nif->getBSVersion() > 0 ? QStringLiteral( "Fallout3HavokMaterial" )
			: QStringLiteral( "OblivionHavokMaterial" );
		value = NifValue::enumOptionValue( enumType, text, &ok );
		if ( !ok ) {
			// Accept the exporter-facing label (Concrete) and migrate the old
			// Collision Manager's shortened identifier (StoneConcrete) while
			// preserving the canonical enum value/CRC.
			const auto & options = NifValue::enumOptionData( enumType ).o;
			for ( auto it = options.cbegin(); it != options.cend(); ++it ) {
				QString shortId = it.value().first;
				if ( shortId.startsWith( QLatin1String( "Material" ) ) ) shortId.remove( 0, 8 );
				if ( it.value().second.compare( text, Qt::CaseInsensitive ) == 0
					|| shortId.compare( text, Qt::CaseInsensitive ) == 0 ) {
					value = it.key(); ok = true; break;
				}
			}
		}
	}
	if ( !ok ) {
		const QVariantMap custom = settings.value( "CollisionManager/CustomMaterials" ).toMap();
		auto it = custom.constFind( text );
		if ( it != custom.cend() ) { value = it.value().toUInt(); ok = true; }
	}
	if ( !ok && !text.isEmpty() && nif->getBSVersion() >= 130 ) {
		// Fallout 4/76 permit custom physical-material editor IDs. Match the
		// Bethesda exporter: lowercase CRC32, initial value 0, no final XOR.
		std::uint32_t hash = 0;
		for ( QChar c : text )
			hashFunctionCRC32( hash, static_cast<unsigned char>( c.toLower().unicode() ) );
		value = quint32( hash ); ok = true;
		QVariantMap custom = settings.value( "CollisionManager/CustomMaterials" ).toMap();
		custom.insert( text, value );
		settings.setValue( "CollisionManager/CustomMaterials", custom );
	}
	return ok ? value : 0;
}

static void applyCollisionBodySettings( NifModel * nif, const QModelIndex & rigidBody )
{
	QSettings settings;
	const int layer = settings.value( "CollisionManager/Create/Layer", 10 ).toInt();
	const int preset = settings.value( "CollisionManager/Create/Preset", 1 ).toInt();
	QModelIndex info = nif->getIndex( rigidBody, "Rigid Body Info" );
	if ( !info.isValid() ) return;
	// both stored copies of the filter: the block's own flattened one and the
	// Rigid Body Info copy, or the two disagree and the viewport colours by the
	// one that was never written
	bhkSetFilterField( nif, rigidBody, QStringLiteral( "Layer" ), quint32( layer ) );
	if ( preset == 2 ) return;
	const bool dynamic = ( preset == 1 );
	const bool animated = ( preset == 3 );
	// Static and Stairhelper bodies are fixed; Anim Static is keyframed. Prop
	// keeps the established dynamic/moving defaults.
	nif->set<quint32>( info, "Motion System", dynamic ? 3u : animated ? 6u : 7u );
	nif->set<quint32>( info, "Quality Type", dynamic ? 4u : animated ? 2u : 1u );
	nif->set<quint32>( info, "Solver Deactivation", dynamic ? 2u : 1u );
	nif->set<float>( info, "Mass", dynamic ? 10.0f : 0.0f );
	nif->set<float>( info, "Friction", 0.5f );
	nif->set<float>( info, "Restitution", 0.4f );
}

static QModelIndex attachCollisionShape( NifModel * nif, const QModelIndex & parentNode,
	const QModelIndex & newShape, const QModelIndex & fallback )
{
	QSettings settings;
	const bool replace = settings.value( "CollisionManager/Create/Replace", true ).toBool();
	QModelIndex collisionLink = nif->getIndex( parentNode, "Collision Object" );
	QModelIndex collisionObject = nif->getBlockIndex( nif->getLink( collisionLink ) );
	if ( collisionObject.isValid() && !nif->blockInherits( collisionObject, "bhkCollisionObject" ) ) {
		QMessageBox::warning( nullptr, Spell::tr( "Create Collision" ),
			Spell::tr( "This node has compiled collision. Decompile it before adding editable collision shapes." ) );
		spRemoveBranch().castIfApplicable( nif, newShape );
		return fallback;
	}
	if ( !collisionObject.isValid() ) {
		collisionObject = nif->insertNiBlock( "bhkCollisionObject" );
		nif->setLink( collisionLink, nif->getBlockNumber( collisionObject ) );
		nif->setLink( collisionObject, "Target", nif->getBlockNumber( parentNode ) );
	}
	QModelIndex rigidBody = nif->getBlockIndex( nif->getLink( collisionObject, "Body" ) );
	if ( !rigidBody.isValid() ) {
		rigidBody = nif->insertNiBlock( "bhkRigidBody" );
		nif->setLink( collisionObject, "Body", nif->getBlockNumber( rigidBody ) );
	}

	QModelIndex shapeLink = nif->getIndex( rigidBody, "Shape" );
	QPersistentModelIndex oldShape = nif->getBlockIndex( nif->getLink( shapeLink ) );
	if ( oldShape.isValid() && !replace ) {
		QVector<qint32> shapes;
		QModelIndex listShape = oldShape;
		if ( nif->blockInherits( oldShape, "bhkListShape" ) ) {
			shapes = nif->getLinkArray( oldShape, "Sub Shapes" );
		} else {
			shapes.append( nif->getBlockNumber( oldShape ) );
			listShape = nif->insertNiBlock( "bhkListShape" );
			nif->setLink( shapeLink, nif->getBlockNumber( listShape ) );
		}
		shapes.append( nif->getBlockNumber( newShape ) );
		nif->set<uint>( listShape, "Num Sub Shapes", uint( shapes.size() ) );
		nif->updateArraySize( listShape, "Sub Shapes" );
		nif->setLinkArray( listShape, "Sub Shapes", shapes );
		nif->set<uint>( listShape, "Num Filters", uint( shapes.size() ) );
		nif->updateArraySize( listShape, "Filters" );
	} else {
		nif->setLink( shapeLink, nif->getBlockNumber( newShape ) );
		if ( oldShape.isValid() )
			spRemoveBranch().castIfApplicable( nif, oldShape );
	}

	applyCollisionBodySettings( nif, rigidBody );
	return rigidBody;
}

static bool gatherCollisionMesh( NifModel * nif, const QModelIndex & index,
	QModelIndex & parentNode, spCreateCVS::MeshData & mesh )
{
	QModelIndex block = nif->getBlockIndex( index );
	parentNode = block;
	if ( !nif->blockInherits( parentNode, "NiNode" ) ) {
		parentNode = nif->getBlockIndex( nif->getParent( block ) );
		if ( !( parentNode.isValid() && nif->blockInherits( parentNode, "NiNode" ) ) )
			return false;
	}
	Scene * scene = nullptr;
	if ( NifSkope * w = dynamic_cast<NifSkope *>( nif->getWindow() ); w ) {
		if ( GLView * ogl = w->getGLView(); ogl ) scene = ogl->getScene();
	}
	if ( !scene || !scene->renderer ) return false;
	QVector<spCreateCVS::MeshData> meshes;
	if ( const Node * node = scene->getNode( nif, block ); node )
		spCreateCVS::getShapeData( meshes, nif, node, nif->getBlockNumber( parentNode ) );
	if ( meshes.isEmpty() ) return false;
	mesh = meshes.first();
	mesh.removeDuplicateVertices();
	return mesh.verts.size() >= 3;
}

static void collisionPcaBasis( const QVector<Vector3> & points, Vector3 & center, Vector3 axes[3] )
{
	center = Vector3();
	for ( const Vector3 & p : points ) center += p;
	center /= float( points.size() );
	float a[3][3] = {};
	for ( const Vector3 & p : points ) {
		Vector3 d = p - center;
		for ( int r = 0; r < 3; r++ ) for ( int c = 0; c < 3; c++ ) a[r][c] += d[r] * d[c];
	}
	float v[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	for ( int iteration = 0; iteration < 24; iteration++ ) {
		int p = 0, q = 1;
		if ( std::fabs( a[0][2] ) > std::fabs( a[p][q] ) ) { p = 0; q = 2; }
		if ( std::fabs( a[1][2] ) > std::fabs( a[p][q] ) ) { p = 1; q = 2; }
		if ( std::fabs( a[p][q] ) < 1.0e-8f ) break;
		float angle = 0.5f * std::atan2( 2.0f * a[p][q], a[q][q] - a[p][p] );
		float cs = std::cos( angle ), sn = std::sin( angle );
		for ( int k = 0; k < 3; k++ ) {
			float apk = a[p][k], aqk = a[q][k];
			a[p][k] = cs * apk - sn * aqk; a[q][k] = sn * apk + cs * aqk;
		}
		for ( int k = 0; k < 3; k++ ) {
			float akp = a[k][p], akq = a[k][q];
			a[k][p] = cs * akp - sn * akq; a[k][q] = sn * akp + cs * akq;
			float vkp = v[k][p], vkq = v[k][q];
			v[k][p] = cs * vkp - sn * vkq; v[k][q] = sn * vkp + cs * vkq;
		}
	}
	int order[3] = { 0, 1, 2 };
	std::sort( order, order + 3, [&a]( int x, int y ) { return a[x][x] > a[y][y]; } );
	for ( int i = 0; i < 3; i++ ) axes[i] = Vector3( v[0][order[i]], v[1][order[i]], v[2][order[i]] ).normalize();
	if ( Vector3::dotproduct( Vector3::crossproduct( axes[0], axes[1] ), axes[2] ) < 0.0f ) axes[2] = -axes[2];
}

enum FittedCollisionType { FitBox, FitSphere, FitCapsule };

static QModelIndex createFittedCollision( NifModel * nif, const QModelIndex & index, FittedCollisionType kind )
{
	QModelIndex parent;
	spCreateCVS::MeshData mesh;
	if ( !gatherCollisionMesh( nif, index, parent, mesh ) ) return index;
	Vector3 center, axes[3];
	collisionPcaBasis( mesh.verts, center, axes );
	const quint32 material = collisionCreateMaterial( nif );
	nif->setState( BaseModel::Processing );
	QModelIndex rootShape, materialShape;
	if ( kind == FitBox ) {
		float lo[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
		float hi[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for ( const Vector3 & point : mesh.verts ) for ( int i = 0; i < 3; i++ ) {
			float p = Vector3::dotproduct( point, axes[i] ); lo[i] = std::min( lo[i], p ); hi[i] = std::max( hi[i], p );
		}
		Vector3 boxCenter, dimensions;
		for ( int i = 0; i < 3; i++ ) {
			boxCenter += axes[i] * ( ( lo[i] + hi[i] ) * 0.5f ); dimensions[i] = ( hi[i] - lo[i] ) * 0.5f;
		}
		materialShape = nif->insertNiBlock( "bhkBoxShape" );
		nif->set<Vector3>( materialShape, "Dimensions", dimensions );
		nif->set<float>( materialShape, "Radius", 0.05f );
		Transform transform; transform.translation = boxCenter;
		for ( int r = 0; r < 3; r++ ) for ( int c = 0; c < 3; c++ ) transform.rotation( r, c ) = axes[c][r];
		rootShape = nif->insertNiBlock( "bhkTransformShape" );
		nif->setLink( rootShape, "Shape", nif->getBlockNumber( materialShape ) );
		nif->set<Matrix4>( rootShape, "Transform", transform.toMatrix4() );
	} else if ( kind == FitSphere ) {
		Vector3 a = mesh.verts.first(), b = a;
		for ( const Vector3 & p : mesh.verts ) if ( ( p - a ).squaredLength() > ( b - a ).squaredLength() ) b = p;
		a = b;
		for ( const Vector3 & p : mesh.verts ) if ( ( p - a ).squaredLength() > ( b - a ).squaredLength() ) b = p;
		center = ( a + b ) * 0.5f;
		float radius = ( b - a ).length() * 0.5f;
		for ( const Vector3 & p : mesh.verts ) {
			float distance = ( p - center ).length();
			if ( distance > radius ) { float next = ( radius + distance ) * 0.5f; center += ( p - center ) * ( ( next - radius ) / distance ); radius = next; }
		}
		materialShape = nif->insertNiBlock( "bhkSphereShape" );
		nif->set<float>( materialShape, "Radius", radius );
		rootShape = nif->insertNiBlock( "bhkTransformShape" );
		nif->setLink( rootShape, "Shape", nif->getBlockNumber( materialShape ) );
		nif->set<Matrix4>( rootShape, "Transform", Transform( center, 1.0f ).toMatrix4() );
	} else {
		float lo = FLT_MAX, hi = -FLT_MAX, radius = 0.0f;
		for ( const Vector3 & point : mesh.verts ) {
			Vector3 d = point - center; float t = Vector3::dotproduct( d, axes[0] );
			lo = std::min( lo, t ); hi = std::max( hi, t ); radius = std::max( radius, ( d - axes[0] * t ).length() );
		}
		float a = lo + radius, b = hi - radius;
		if ( a > b ) a = b = ( lo + hi ) * 0.5f;
		materialShape = rootShape = nif->insertNiBlock( "bhkCapsuleShape" );
		nif->set<Vector3>( rootShape, "First Point", center + axes[0] * a );
		nif->set<Vector3>( rootShape, "Second Point", center + axes[0] * b );
		nif->set<float>( rootShape, "Radius", radius ); nif->set<float>( rootShape, "Radius 1", radius ); nif->set<float>( rootShape, "Radius 2", radius );
	}
	if ( material ) nif->set<quint32>( materialShape, "Material", material );
	QModelIndex result = attachCollisionShape( nif, parent, rootShape, index );
		/* Tell the engine the mesh HAS collision. Measured: 22,496 of the
		 * 22,496 stock FO4 meshes carrying a collision object set BSXFlags
		 * bit 1 on the root, with no exceptions — and nothing here wrote it,
		 * so collision created in NifSkope produced meshes the engine
		 * silently ignores. insertNiBlock renumbers, hence the persistent index.
		 */
		quint32 bsxWas = 0, bsxNow = 0;
		QPersistentModelIndex keep( result );
		if ( wwEnsureRootBSXFlags( nif, BSXF_Havok, &bsxWas, &bsxNow ) )
			Message::append( nullptr, Spell::tr( "Create Collision" ),
				Spell::tr( "BSXFlags %1 -> %2: bit 1 (Havok) set, because the file now "
					"carries collision the engine has to be told about" ).arg( bsxWas ).arg( bsxNow ),
				QMessageBox::Information );
		result = QModelIndex( keep );

	nif->restoreState(); nif->updateModel();
	return result;
}

#define DECLARE_FITTED_COLLISION_SPELL( ClassName, DisplayName, Kind ) \
class ClassName final : public Spell { public: \
	QString name() const override final { return Spell::tr( DisplayName ); } \
	QString page() const override final { return Spell::tr( "Havok" ); } \
	QString group() const override final { return Spell::tr( "Collision" ); } \
	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final { spCreateCVS s; return s.isApplicable( nif, index ); } \
	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final { return createFittedCollision( nif, index, Kind ); } \
}; REGISTER_SPELL( ClassName )

DECLARE_FITTED_COLLISION_SPELL( spCreateBoxCollision, "Create Box Collision", FitBox )
DECLARE_FITTED_COLLISION_SPELL( spCreateSphereCollision, "Create Sphere Collision", FitSphere )
DECLARE_FITTED_COLLISION_SPELL( spCreateCapsuleCollision, "Create Capsule Collision", FitCapsule )

/* Create Convex Hull Collision and Create Convex Decomposition Collision used to
 * live here, and they were not two algorithms — they were one spell with a
 * checkbox pre-ticked. Each did
 *
 *     QSettings s; s.beginGroup( "Spells/Havok/Create Convex Shapes" );
 *     s.setValue( "Enable CoACD", <false|true> );
 *
 * before delegating to spCreateCVS. That write is PERSISTENT. Picking one of
 * them once silently reconfigured Create Convex Shapes, the Collision Manager's
 * quick-create, and the other menu entry — for every file thereafter, with
 * nothing in the UI to show it had happened or how to put it back.
 *
 * The choice was already offered honestly: spCreateCVS::settingsDialog has an
 * "Enable CoACD" checkbox, with the whole CoACD parameter block under it. So the
 * two entries were a way to set that checkbox without seeing it. Deleting them
 * removes two of the eight Create rows and costs nothing reachable.
 */

//! Create an accurate editable triangle-mesh collision from rendered geometry.
class spCreateAccurateMeshCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Create Accurate Mesh Collision" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		spCreateCVS geometryCheck;
		return geometryCheck.isApplicable( nif, index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iBlock = nif->getBlockIndex( index );
		QModelIndex iParent = iBlock;
		if ( !nif->blockInherits( iParent, "NiNode" ) ) {
			iParent = nif->getBlockIndex( nif->getParent( iBlock ) );
			if ( !( iParent.isValid() && nif->blockInherits( iParent, "NiNode" ) ) )
				return index;
		}

		Scene * scene = nullptr;
		if ( NifSkope * w = dynamic_cast<NifSkope *>( nif->getWindow() ); w ) {
			if ( GLView * ogl = w->getGLView(); ogl )
				scene = ogl->getScene();
		}
		if ( !scene || !scene->renderer )
			return index;

		QVector<spCreateCVS::MeshData> meshes;
		if ( const Node * node = scene->getNode( nif, iBlock ); node )
			spCreateCVS::getShapeData( meshes, nif, node, nif->getBlockNumber( iParent ) );
		if ( meshes.isEmpty() )
			return index;
		auto & mesh = meshes.first();
		mesh.removeDuplicateVertices();
		QSettings previewSettings;
		if ( previewSettings.value( "CollisionManager/Preview/QuickCreate", false ).toBool() ) {
			float ratio = std::clamp( previewSettings.value( "CollisionManager/Preview/Ratio", 1.0 ).toFloat(), 0.01f, 1.0f );
			if ( ratio < 0.999f && mesh.indices.size() >= 6 ) {
				QVector<unsigned int> newIndices( mesh.indices.size() );
				size_t target = std::max<size_t>( 3, size_t( ( mesh.indices.size() / 3 ) * ratio ) * 3 );
				size_t n = meshopt_simplify( newIndices.data(), mesh.indices.constData(), size_t( mesh.indices.size() ),
					&( mesh.verts.constFirst()[0] ), size_t( mesh.verts.size() ), sizeof( Vector3 ),
					target, 0.02f, meshopt_SimplifyLockBorder, nullptr );
				newIndices.resize( qsizetype( n ) );
				mesh.indices = newIndices;
				mesh.removeDuplicateVertices();
			}
		}
		if ( mesh.verts.size() < 3 || mesh.indices.size() < 3 )
			return index;
		if ( mesh.verts.size() > 65535 ) {
			QMessageBox::warning( nullptr, tr( "Create Accurate Mesh Collision" ),
				tr( "Accurate mesh collision is limited to 65,535 vertices. Decimate the geometry first." ) );
			return index;
		}

		nif->setState( BaseModel::Processing );
		QModelIndex iData = nif->insertNiBlock( "NiTriStripsData" );
		const int nv = int( mesh.verts.size() );
		const int nt = int( mesh.indices.size() / 3 );
		const float scale = spCreateCVS::getHavokScale( nif );
		QVector<Vector3> verts( nv );
		Vector3 center;
		for ( int v = 0; v < nv; v++ ) {
			verts[v] = mesh.verts.at( v ) * scale;
			center += verts[v];
		}
		center /= float( nv );
		float boundRadius = 0.0f;
		for ( const Vector3 & v : verts )
			boundRadius = std::max( boundRadius, ( v - center ).length() );

		nif->set<int>( iData, "Num Vertices", nv );
		nif->set<int>( iData, "Has Vertices", 1 );
		nif->updateArraySize( iData, "Vertices" );
		nif->setArray<Vector3>( iData, "Vertices", verts );
		QModelIndex iBound = nif->getIndex( iData, "Bounding Sphere" );
		if ( iBound.isValid() ) {
			nif->set<Vector3>( iBound, "Center", center );
			nif->set<float>( iBound, "Radius", boundRadius );
		}
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
				const int o = t * 3;
				nif->setArray<quint16>( iStrip, {
					quint16( mesh.indices.at( o ) ), quint16( mesh.indices.at( o + 1 ) ),
					quint16( mesh.indices.at( o + 2 ) ) } );
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
		if ( quint32 material = collisionCreateMaterial( nif ); material )
			nif->set<quint32>( iStrips, "Material", material );
		QModelIndex iMopp = nif->insertNiBlock( "bhkMoppBvTreeShape" );
		nif->setLink( iMopp, "Shape", nif->getBlockNumber( iStrips ) );
		QModelIndex result = attachCollisionShape( nif, iParent, iMopp, index );
		/* Tell the engine the mesh HAS collision. Measured: 22,496 of the
		 * 22,496 stock FO4 meshes carrying a collision object set BSXFlags
		 * bit 1 on the root, with no exceptions — and nothing here wrote it,
		 * so collision created in NifSkope produced meshes the engine
		 * silently ignores. insertNiBlock renumbers, hence the persistent index.
		 */
		quint32 bsxWas = 0, bsxNow = 0;
		QPersistentModelIndex keep( result );
		if ( wwEnsureRootBSXFlags( nif, BSXF_Havok, &bsxWas, &bsxNow ) )
			Message::append( nullptr, Spell::tr( "Create Collision" ),
				Spell::tr( "BSXFlags %1 -> %2: bit 1 (Havok) set, because the file now "
					"carries collision the engine has to be told about" ).arg( bsxWas ).arg( bsxNow ),
				QMessageBox::Information );
		result = QModelIndex( keep );

		nif->restoreState();
		nif->updateModel();
		return result;
	}
};

/* Deliberately NOT registered: reachable through Create Collision… ▸ Accurate
 * Mesh, which is where the choice belongs. As its own top-level row it was the
 * third entry in a Collision submenu that already read "Create Collision…",
 * "Create Convex Shapes" and "Create Accurate Mesh Collision" side by side, two
 * of which the first one only asks you to pick between.
 *
 * spCreateCollision constructs it directly, so the class stays live; only the
 * duplicate menu row goes.
 */
//REGISTER_SPELL( spCreateAccurateMeshCollision )

//! Build temporary world-space collision geometry for the Collision Manager.
//! kind 0 = convex hull/decomposition, kind 1 = accurate/decimated mesh.
bool tlBuildCollisionPreview( NifModel * nif, const QModelIndex & index, int kind,
	float ratio, bool decomposition, float precision, float threshold, int maxHulls,
	QVector<Vector3> & triangleSoup, QString & statistics )
{
	triangleSoup.clear();
	statistics.clear();
	QModelIndex parent;
	spCreateCVS::MeshData mesh;
	if ( !gatherCollisionMesh( nif, index, parent, mesh ) || mesh.indices.size() < 3 ) {
		statistics = Spell::tr( "Select a BSTriShape or NiNode with rendered geometry." );
		return false;
	}
	ratio = std::clamp( ratio, 0.01f, 1.0f );
	if ( ratio < 0.999f && mesh.indices.size() >= 6 ) {
		QVector<unsigned int> simplified( mesh.indices.size() );
		size_t target = std::max<size_t>( 3, size_t( ( mesh.indices.size() / 3 ) * ratio ) * 3 );
		size_t n = meshopt_simplify( simplified.data(), mesh.indices.constData(), size_t( mesh.indices.size() ),
			&( mesh.verts.constFirst()[0] ), size_t( mesh.verts.size() ), sizeof( Vector3 ),
			target, 0.02f, meshopt_SimplifyLockBorder, nullptr );
		simplified.resize( qsizetype( n ) );
		mesh.indices = simplified;
		mesh.removeDuplicateVertices();
	}

	Scene * scene = nullptr;
	if ( NifSkope * w = dynamic_cast<NifSkope *>( nif->getWindow() ); w ) {
		if ( GLView * view = w->getGLView(); view ) scene = view->getScene();
	}
	const Node * parentNode = scene ? scene->getNode( nif, parent ) : nullptr;
	if ( !parentNode ) {
		statistics = Spell::tr( "The source node is not available in the viewport." );
		return false;
	}
	const Transform world = parentNode->worldTrans();
	const float scale = spCreateCVS::getHavokScale( nif );
	auto appendTriangle = [&]( const Vector3 & a, const Vector3 & b, const Vector3 & c ) {
		triangleSoup.append( world * ( a * scale ) );
		triangleSoup.append( world * ( b * scale ) );
		triangleSoup.append( world * ( c * scale ) );
	};

	int hullCount = 0;
	if ( kind == 1 ) {
		for ( qsizetype i = 0; i + 2 < mesh.indices.size(); i += 3 ) {
			unsigned int a = mesh.indices.at( i ), b = mesh.indices.at( i + 1 ), c = mesh.indices.at( i + 2 );
			if ( a < unsigned( mesh.verts.size() ) && b < unsigned( mesh.verts.size() ) && c < unsigned( mesh.verts.size() ) )
				appendTriangle( mesh.verts.at( a ), mesh.verts.at( b ), mesh.verts.at( c ) );
		}
	} else {
		QVector<spCreateCVS::MeshData> pieces;
		pieces.append( mesh );
		if ( decomposition ) {
			CoACD coacd;
			coacd.threshold = std::clamp( double( threshold ), 0.01, 1.0 );
			coacd.maxConvexHull = std::clamp( maxHulls, 1, 256 );
			coacd.merge = true;
			spCreateCVS::createConvexShapes( pieces, coacd );
		}
		for ( const auto & piece : pieces ) {
			QVector<Vector4> hullVerts, hullNorms;
			QVector<Triangle> faces = compute_convex_hull( piece.verts, hullVerts, hullNorms,
				std::max( 0.0f, precision ) / scale );
			if ( faces.isEmpty() ) continue;
			hullCount++;
			for ( const Triangle & face : faces ) {
				if ( face[0] < piece.verts.size() && face[1] < piece.verts.size() && face[2] < piece.verts.size() )
					appendTriangle( piece.verts.at( face[0] ), piece.verts.at( face[1] ), piece.verts.at( face[2] ) );
			}
		}
	}
	if ( triangleSoup.isEmpty() ) {
		statistics = Spell::tr( "No preview geometry could be generated with these settings." );
		return false;
	}
	statistics = kind == 1
		? Spell::tr( "%1 triangles  -  %2% of source" ).arg( triangleSoup.size() / 3 ).arg( int( ratio * 100.0f + 0.5f ) )
		: Spell::tr( "%1 hull(s)  -  %2 preview triangles" ).arg( hullCount ).arg( triangleSoup.size() / 3 );
	return true;
}

QModelIndex tlCommitCollisionPreview( NifModel * nif, const QModelIndex & index, int kind,
	float ratio, bool decomposition, float precision, float threshold, int maxHulls )
{
	QSettings settings;
	settings.setValue( "CollisionManager/Preview/QuickCreate", true );
	settings.setValue( "CollisionManager/Preview/Ratio", std::clamp( ratio, 0.01f, 1.0f ) );
	settings.setValue( "CollisionManager/Preview/Decomposition", decomposition );
	settings.setValue( "CollisionManager/Preview/Precision", precision );
	settings.setValue( "CollisionManager/Preview/Threshold", threshold );
	settings.setValue( "CollisionManager/Preview/MaxHulls", maxHulls );
	QModelIndex result;
	if ( kind == 1 ) {
		spCreateAccurateMeshCollision create;
		result = create.cast( nif, index );
	} else {
		spCreateCVS create;
		result = create.cast( nif, index );
	}
	settings.setValue( "CollisionManager/Preview/QuickCreate", false );
	return result;
}

//! User-facing collision creation entry point. Ask for the fundamental shape
//! choice before opening the chosen workflow's detailed settings.
class spCreateCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Create Collision…" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		spCreateCVS create;
		return create.isApplicable( nif, index );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QMessageBox choice( QMessageBox::Question, tr( "Create Collision" ),
			tr( "Choose how closely the collision should follow the source geometry." ),
			QMessageBox::Cancel );
		QPushButton * accurate = choice.addButton( tr( "Accurate Mesh" ), QMessageBox::AcceptRole );
		QPushButton * convex = choice.addButton( tr( "Convex" ), QMessageBox::ActionRole );
		choice.setDefaultButton( convex );
		choice.exec();
		if ( choice.clickedButton() == accurate ) {
			spCreateAccurateMeshCollision create;
			return create.cast( nif, index );
		}
		if ( choice.clickedButton() != convex )
			return index;
		spCreateCVS create;
		return create.cast( nif, index );
	}
};

REGISTER_SPELL( spCreateCollision )


//! Transforms Havok constraints
class spConstraintHelper final : public Spell
{
public:
	// was "A -> B", which named its arrow and not its effect: it recomputes the
	// B-side pivot and axes from the A side, through both bodies' world transforms
	QString name() const override final { return Spell::tr( "Recompute B Frame from A" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		static QStringList blockNames = {
				  "bhkMalleableConstraint",
				  "bhkBreakableConstraint",
				  "bhkRagdollConstraint",
				  "bhkLimitedHingeConstraint",
				  "bhkHingeConstraint",
				  "bhkPrismaticConstraint"
		};
		return nif && nif->isNiBlock( nif->getBlockIndex( index ), blockNames );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iConstraint = nif->getBlockIndex( index );
		QString name = nif->itemName( iConstraint );

		if ( name == "bhkMalleableConstraint" || name == "bhkBreakableConstraint" ) {
			if ( nif->getIndex( iConstraint, "Ragdoll" ).isValid() ) {
				name = "bhkRagdollConstraint";
			} else if ( nif->getIndex( iConstraint, "Limited Hinge" ).isValid() ) {
				name = "bhkLimitedHingeConstraint";
			} else if ( nif->getIndex( iConstraint, "Hinge" ).isValid() ) {
				name = "bhkHingeConstraint";
			}
		}

		QModelIndex iBodyA = nif->getBlockIndex( nif->getLink( bhkGetEntity( nif, iConstraint, "Entity A" ) ), "bhkRigidBody" );
		QModelIndex iBodyB = nif->getBlockIndex( nif->getLink( bhkGetEntity( nif, iConstraint, "Entity B" ) ), "bhkRigidBody" );

		if ( !iBodyA.isValid() || !iBodyB.isValid() ) {
			Message::warning( nullptr, Spell::tr( "Couldn't find the bodies for this constraint." ) );
			return index;
		}

		Transform transA = bhkBodyTrans( nif, iBodyA );
		Transform transB = bhkBodyTrans( nif, iBodyB );

		QModelIndex iConstraintData;
		if ( name == "bhkLimitedHingeConstraint" ) {
			iConstraintData = nif->getIndex( iConstraint, "Limited Hinge" );
			if ( !iConstraintData.isValid() )
				iConstraintData = iConstraint;
		} else if ( name == "bhkRagdollConstraint" ) {
			iConstraintData = nif->getIndex( iConstraint, "Ragdoll" );
			if ( !iConstraintData.isValid() )
				iConstraintData = iConstraint;
		} else if ( name == "bhkHingeConstraint" ) {
			iConstraintData = nif->getIndex( iConstraint, "Hinge" );
			if ( !iConstraintData.isValid() )
				iConstraintData = iConstraint;
		}

		if ( !iConstraintData.isValid() )
			return index;

		Matrix r1 = transA.rotation;
		Matrix r2 = transB.rotation.inverted();

		Vector3 pivot = Vector3( nif->get<Vector4>( iConstraintData, "Pivot A" ) );
		pivot = transA * pivot;
		pivot = r2 * ( pivot - transB.translation ) / transB.scale;
		nif->set<Vector4>( iConstraintData, "Pivot B", Vector4( pivot ) );

		const char * axisA = nullptr, * axisB = nullptr, * twistA = nullptr, * twistB = nullptr;
		const char * twistA2 = nullptr, * twistB2 = nullptr, * motorA = nullptr, * motorB = nullptr;
		if ( name.endsWith( QLatin1StringView("HingeConstraint") ) ) {
			axisA = "Axis A";
			axisB = "Axis B";
			twistA = "Perp Axis In A1";
			twistB = "Perp Axis In B1";
			twistA2 = "Perp Axis In A2";
			twistB2 = "Perp Axis In B2";
		} else if ( name == "bhkRagdollConstraint" ) {
			axisA = "Plane A";
			axisB = "Plane B";
			twistA = "Twist A";
			twistB = "Twist B";
			motorA = "Motor A";
			motorB = "Motor B";
		}

		if ( !( axisA && axisB && twistA && twistB ) )
			return index;

		auto axis = FloatVector4( nif->get<Vector4>( iConstraintData, axisA ) );
		auto twist = FloatVector4( nif->get<Vector4>( iConstraintData, twistA ) );

		if ( motorA )
			nif->set<Vector4>( iConstraintData, motorA, Vector4( twist.crossProduct3( axis ) ) );

		axis = FloatVector4( r2 * ( r1 * Vector3( axis ) ) );
		nif->set<Vector4>( iConstraintData, axisB, Vector4( axis ) );

		twist = FloatVector4( r2 * ( r1 * Vector3( twist ) ) );
		nif->set<Vector4>( iConstraintData, twistB, Vector4( twist ) );

		if ( motorB )
			nif->set<Vector4>( iConstraintData, motorB, Vector4( twist.crossProduct3( axis ) ) );

		if ( twistA2 && twistB2 ) {
			twist = FloatVector4( r2 * ( r1 * Vector3( nif->get<Vector4>( iConstraintData, twistA2 ) ) ) );
			nif->set<Vector4>( iConstraintData, twistB2, Vector4( twist ) );
		}

		return index;
	}
};

REGISTER_SPELL( spConstraintHelper )

//! Calculates Havok spring lengths
class spStiffSpringHelper final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Calculate Spring Length" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & idx ) override final
	{
		return nif && nif->isNiBlock( nif->getBlockIndex( idx ), "bhkStiffSpringConstraint" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & idx ) override final
	{
		QModelIndex iConstraint = nif->getBlockIndex( idx );
		QModelIndex iSpring = nif->getIndex( iConstraint, "Stiff Spring" );
		if ( !iSpring.isValid() )
			iSpring = iConstraint;

		QModelIndex iBodyA = nif->getBlockIndex( nif->getLink( bhkGetEntity( nif, iConstraint, "Entity A" ) ), "bhkRigidBody" );
		QModelIndex iBodyB = nif->getBlockIndex( nif->getLink( bhkGetEntity( nif, iConstraint, "Entity B" ) ), "bhkRigidBody" );

		if ( !iBodyA.isValid() || !iBodyB.isValid() ) {
			Message::warning( nullptr, Spell::tr( "Couldn't find the bodies for this constraint" ) );
			return idx;
		}

		Transform transA = bhkBodyTrans( nif, iBodyA );
		Transform transB = bhkBodyTrans( nif, iBodyB );

		Vector3 pivotA( nif->get<Vector4>( iSpring, "Pivot A" ) );
		Vector3 pivotB( nif->get<Vector4>( iSpring, "Pivot B" ) );

		float length = ( transA * pivotA - transB * pivotB ).length();

		nif->set<float>( iSpring, "Length", length );

		return nif->getIndex( iSpring, "Length" );
	}
};

REGISTER_SPELL( spStiffSpringHelper )

//! Packs Havok strips
class spPackHavokStrips final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Pack Strips" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & idx ) override final
	{
		return nif->isNiBlock( idx, "bhkNiTriStripsShape" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & iBlock ) override final
	{
		QPersistentModelIndex iShape( iBlock );

		QVector<Vector3> vertices;
		QVector<Triangle> triangles;
		QVector<Vector3> normals;

		for ( const auto lData : nif->getLinkArray( iShape, "Strips Data" ) ) {
			QModelIndex iData = nif->getBlockIndex( lData, "NiTriStripsData" );

			if ( iData.isValid() ) {
				QVector<Vector3> vrts = nif->getArray<Vector3>( iData, "Vertices" );
				QVector<Triangle> tris;
				QVector<Vector3> nrms;

				QModelIndex iPoints = nif->getIndex( iData, "Points" );

				for ( int x = 0; x < nif->rowCount( iPoints ); x++ ) {
					tris += triangulate( nif->getArray<quint16>( nif->getIndex( iPoints, x ) ) );
				}

				QMutableVectorIterator<Triangle> it( tris );

				while ( it.hasNext() ) {
					Triangle & tri = it.next();

					Vector3 a = vrts.value( tri[0] );
					Vector3 b = vrts.value( tri[1] );
					Vector3 c = vrts.value( tri[2] );

					nrms << Vector3::crossproduct( b - a, c - a ).normalize();

					tri[0] += vertices.count();
					tri[1] += vertices.count();
					tri[2] += vertices.count();
				}

				float scale = ( nif->getBSVersion() < 47 ? 0.142875f : 0.0142875f );
				for ( const Vector3& v : vrts ) {
					vertices += v * scale;
				}
				triangles += tris;
				normals += nrms;
			}
		}

		if ( vertices.isEmpty() || triangles.isEmpty() ) {
			Message::warning( nullptr, Spell::tr( "No mesh data was found." ) );
			return iShape;
		}

		QPersistentModelIndex iPackedShape = nif->insertNiBlock( "bhkPackedNiTriStripsShape", nif->getBlockNumber( iShape ) );

		if ( nif->getVersionNumber() <= 0x14000005 ) {	// until 20.0.0.5
			nif->set<int>( iPackedShape, "Num Sub Shapes", 1 );
			QModelIndex iSubShapes = nif->getIndex( iPackedShape, "Sub Shapes" );
			nif->updateArraySize( iSubShapes );
			QModelIndex iSubShape = nif->getIndex( iSubShapes, 0 );
			nif->set<int>( bhkGetHavokFilter( nif, iSubShape ), "Layer", 1 );
			nif->set<int>( iSubShape, "Num Vertices", vertices.count() );
			nif->set<int>( iSubShape, "Material", nif->get<int>( iShape, "Material" ) );
		}
		nif->set<Vector4>( iPackedShape, "Scale", FloatVector4( 1.0f ) );
		nif->set<Vector4>( iPackedShape, "Scale Copy", FloatVector4( 1.0f ) );

		QModelIndex iPackedData = nif->insertNiBlock( "hkPackedNiTriStripsData", nif->getBlockNumber( iPackedShape ) );
		nif->setLink( iPackedShape, "Data", nif->getBlockNumber( iPackedData ) );

		nif->set<int>( iPackedData, "Num Triangles", triangles.count() );
		QModelIndex iTriangles = nif->getIndex( iPackedData, "Triangles" );
		nif->updateArraySize( iTriangles );

		for ( int t = 0; t < triangles.size(); t++ ) {
			nif->set<Triangle>( nif->getIndex( iTriangles, t ), "Triangle", triangles[ t ] );
			if ( nif->getVersionNumber() <= 0x14000005 )
				nif->set<Vector3>( nif->getIndex( iTriangles, t ), "Normal", normals.value( t ) );
		}

		nif->set<int>( iPackedData, "Num Vertices", vertices.count() );
		QModelIndex iVertices = nif->getIndex( iPackedData, "Vertices" );
		nif->updateArraySize( iVertices );
		nif->setArray<Vector3>( iVertices, vertices );

		QMap<qint32, qint32> lnkmap;
		lnkmap.insert( nif->getBlockNumber( iShape ), nif->getBlockNumber( iPackedShape ) );
		nif->mapLinks( lnkmap );

		// *** THIS SOMETIMES CRASHES NIFSKOPE        ***
		// *** UNCOMMENT WHEN BRANCH REMOVER IS FIXED ***
		// See issue #2508255
		spRemoveBranch BranchRemover;
		BranchRemover.castIfApplicable( nif, iShape );

		return iPackedShape;
	}
};

REGISTER_SPELL( spPackHavokStrips )

//! Converts bhkListShape to bhkConvexListShape for FO3
class spConvertListShape final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Convert to bhkConvexListShape" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & idx ) override final
	{
		return nif->isNiBlock( idx, "bhkListShape" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & iBlock ) override final
	{
		QPersistentModelIndex iShape( iBlock );
		QPersistentModelIndex iRigidBody = nif->getBlockIndex( nif->getParent( iShape ) );
		if ( !iRigidBody.isValid() )
			return {};

		auto iCLS = nif->insertNiBlock( "bhkConvexListShape" );

		nif->set<uint>( iCLS, "Num Sub Shapes", nif->get<uint>( iShape, "Num Sub Shapes" ) );
		nif->set<uint>( iCLS, "Material", nif->get<uint>( iShape, "Material" ) );
		nif->updateArraySize( iCLS, "Sub Shapes" );

		nif->setLinkArray( iCLS, "Sub Shapes", nif->getLinkArray( iShape, "Sub Shapes" ) );
		nif->setLinkArray( iShape, "Sub Shapes", {} );
		nif->removeNiBlock( nif->getBlockNumber( iShape ) );

		nif->setLink( iRigidBody, "Shape", nif->getBlockNumber( iCLS ) );

		return iCLS;
	}
};

REGISTER_SPELL( spConvertListShape )

//! Converts bhkConvexListShape to bhkListShape for FNV
class spConvertConvexListShape final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Convert to bhkListShape" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	bool isApplicable( const NifModel * nif, const QModelIndex & idx ) override final
	{
		return nif->isNiBlock( idx, "bhkConvexListShape" );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & iBlock ) override final
	{
		QPersistentModelIndex iShape( iBlock );
		QPersistentModelIndex iRigidBody = nif->getBlockIndex( nif->getParent( iShape ) );
		if ( !iRigidBody.isValid() )
			return {};

		auto iLS = nif->insertNiBlock( "bhkListShape" );

		nif->set<uint>( iLS, "Num Sub Shapes", nif->get<uint>( iShape, "Num Sub Shapes" ) );
		nif->set<uint>( iLS, "Num Filters", nif->get<uint>( iShape, "Num Sub Shapes" ) );
		nif->set<uint>( iLS, "Material", nif->get<uint>( iShape, "Material" ) );
		nif->updateArraySize( iLS, "Sub Shapes" );
		nif->updateArraySize( iLS, "Filters" );

		nif->setLinkArray( iLS, "Sub Shapes", nif->getLinkArray( iShape, "Sub Shapes" ) );
		nif->setLinkArray( iShape, "Sub Shapes", {} );
		nif->removeNiBlock( nif->getBlockNumber( iShape ) );

		nif->setLink( iRigidBody, "Shape", nif->getBlockNumber( iLS ) );

		return iLS;
	}
};

REGISTER_SPELL( spConvertConvexListShape )

//! Decompile FO4 compiled collision (bhkPhysicsSystem) into editable legacy blocks
class spDecodeCompiledCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Decompile Compiled Collision" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	//! Resolve the bhkPhysicsSystem / bhkRagdollSystem from the clicked block
	static QModelIndex systemBlock( const NifModel * nif, const QModelIndex & index )
	{
		QModelIndex iBlock = nif->getBlockIndex( index );
		if ( nif->blockInherits( iBlock, "bhkNPCollisionObject" ) )
			iBlock = nif->getBlockIndex( nif->getLink( iBlock, "Data" ) );
		if ( nif->isNiBlock( iBlock, "bhkPhysicsSystem" ) || nif->isNiBlock( iBlock, "bhkRagdollSystem" ) )
			return iBlock;
		return QModelIndex();
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		QModelIndex iSys = systemBlock( nif, index );
		return iSys.isValid() && nif->get<QByteArray>( iSys, "Binary Data" ).size() > 0x100;
	}

	//! asked about the ragdoll one-way trip already this cast (Decompile All
	//! reuses one decoder instance across every system in the file)
	bool ragdollWarned = false;

	//! Write the decoded Havok material CRC into a shape's HavokMaterial field
	static void setShapeMaterial( NifModel * nif, const QModelIndex & iShape, quint32 crc )
	{
		// resolves through the HavokMaterial compound (same pattern as
		// spConvertListShape); the enum value is the CRC itself
		if ( crc )
			nif->set<quint32>( iShape, "Material", crc );
	}

	//! Build the legacy shape block(s) for one decoded hknp shape
	static qint32 buildShape( NifModel * nif, const HknpShape & shp, float sc )
	{
		if ( shp.primType == 1 ) {
			// sphere: the center (capA) is folded into the transform wrapper
			QModelIndex iSph = nif->insertNiBlock( "bhkSphereShape" );
			nif->set<float>( iSph, "Radius", shp.primRadius );
			setShapeMaterial( nif, iSph, shp.materialCRC );
			return nif->getBlockNumber( iSph );
		}
		if ( shp.primType == 2 ) {
			QModelIndex iCap = nif->insertNiBlock( "bhkCapsuleShape" );
			nif->set<float>( iCap, "Radius", shp.primRadius );
			nif->set<Vector3>( iCap, "First Point", shp.capA );
			nif->set<float>( iCap, "Radius 1", shp.primRadius );
			nif->set<Vector3>( iCap, "Second Point", shp.capB );
			nif->set<float>( iCap, "Radius 2", shp.primRadius );
			setShapeMaterial( nif, iCap, shp.materialCRC );
			return nif->getBlockNumber( iCap );
		}
		if ( shp.isConvex ) {
			// an axis-aligned origin-centered 8-vert / 6-face hull is a box
			if ( shp.verts.size() == 8 && shp.faces.size() == 6 ) {
				Vector3 h;
				bool isBox = true;
				for ( const Vector3 & v : shp.verts ) {
					for ( int c = 0; c < 3; c++ )
						h[c] = std::max( h[c], std::fabs( v[c] ) );
				}
				for ( const Vector3 & v : shp.verts ) {
					for ( int c = 0; c < 3; c++ ) {
						if ( std::fabs( std::fabs( v[c] ) - h[c] ) > 1.0e-4f )
							isBox = false;
					}
				}
				if ( isBox ) {
					QModelIndex iBox = nif->insertNiBlock( "bhkBoxShape" );
					nif->set<Vector3>( iBox, "Dimensions", h );
					nif->set<float>( iBox, "Radius", shp.convexRadius );
					setShapeMaterial( nif, iBox, shp.materialCRC );
					return nif->getBlockNumber( iBox );
				}
			}
			QModelIndex iCVS = nif->insertNiBlock( "bhkConvexVerticesShape" );
			nif->set<float>( iCVS, "Radius", shp.convexRadius );
			QVector<Vector4> verts, norms;
			for ( const Vector3 & v : shp.verts )
				verts.append( Vector4( v[0], v[1], v[2], 0.0f ) );
			for ( const Vector4 & p : shp.planes ) {
				// skip the padding planes Elric appends (zero normal)
				if ( Vector3( p[0], p[1], p[2] ).length() > 0.5f )
					norms.append( p );
			}
			nif->set<uint>( iCVS, "Num Vertices", uint( verts.size() ) );
			nif->updateArraySize( iCVS, "Vertices" );
			nif->setArray<Vector4>( iCVS, "Vertices", verts );
			nif->set<uint>( iCVS, "Num Normals", uint( norms.size() ) );
			nif->updateArraySize( iCVS, "Normals" );
			nif->setArray<Vector4>( iCVS, "Normals", norms );
			setShapeMaterial( nif, iCVS, shp.materialCRC );
			return nif->getBlockNumber( iCVS );
		}

		// triangle mesh: NiTriStripsData (one strip per triangle) under
		// bhkNiTriStripsShape + bhkMoppBvTreeShape - the same chain the 3ds Max
		// exporter emits. The MOPP is left empty: run Update MOPP Code, or let
		// Elric rebuild it when the file is processed again.
		QModelIndex iData = nif->insertNiBlock( "NiTriStripsData" );
		int nv = int( shp.verts.size() );
		int nt = int( shp.tris.size() );
		nif->set<int>( iData, "Num Vertices", nv );
		nif->set<int>( iData, "Has Vertices", 1 );
		nif->updateArraySize( iData, "Vertices" );
		QVector<Vector3> verts( nv );
		Vector3 center;
		for ( int i = 0; i < nv; i++ ) {
			verts[i] = shp.verts.at( i ) * sc;
			center += verts[i];
		}
		if ( nv > 0 )
			center /= float( nv );
		float radius = 0.0f;
		for ( const Vector3 & v : verts )
			radius = std::max( radius, ( v - center ).length() );
		nif->setArray<Vector3>( iData, "Vertices", verts );
		QModelIndex iBound = nif->getIndex( iData, "Bounding Sphere" );
		if ( iBound.isValid() ) {
			nif->set<Vector3>( iBound, "Center", center );
			nif->set<float>( iBound, "Radius", radius );
		}
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
				const Triangle & tri = shp.tris.at( t );
				nif->setArray<quint16>( iStrip, { tri[0], tri[1], tri[2] } );
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
		setShapeMaterial( nif, iStrips, shp.materialCRC );

		QModelIndex iMopp = nif->insertNiBlock( "bhkMoppBvTreeShape" );
		nif->setLink( iMopp, "Shape", nif->getBlockNumber( iStrips ) );
		return nif->getBlockNumber( iMopp );
	}

	//! Internal implementation used by both the one-system and all-system
	//! commands. The public cast() methods provide the single snapshot boundary.
	QModelIndex decodeRaw( NifModel * nif, const QModelIndex & index )
	{
		QModelIndex iSys = systemBlock( nif, index );
		if ( !iSys.isValid() )
			return index;

		// A ragdoll decompiles to its per-bone capsules, but nothing can put it
		// back: the packfile also carries hknpRagdollData, the constraint graph
		// (hkpRagdollConstraintData / hkpLimitedHingeConstraintData /
		// hkpPositionConstraintMotor) and an hkaSkeleton, none of which NifSkope
		// decodes. Say so once, before the destructive part, and default to
		// Cancel. Once per cast so Decompile All asks a single time.
		// (-no-gui builds a plain QCoreApplication, where QMessageBox is invalid;
		// a headless caller named this spell explicitly, so there is nobody to ask)
		if ( nif->isNiBlock( iSys, "bhkRagdollSystem" ) && !ragdollWarned
			&& qobject_cast<QApplication *>( QCoreApplication::instance() ) ) {
			ragdollWarned = true;
			if ( QMessageBox::warning( nullptr, tr( "Decompile Compiled Collision" ),
					/* The warning stands; the REASON it used to give does not.
					 *
					 * It said the joint constraints, the motor and the skeleton copy
					 * "are not decoded". They are, and they re-encode byte for byte —
					 * 742/742 ragdoll constraints, 461/461 limited hinges and 75/75
					 * skeletons over the Fallout 4 mesh tree. What is actually lost
					 * is on the way BACK: Decompile writes no NIF blocks for any of
					 * them, and Compile builds a single static body from triangles,
					 * so nothing carries them across.
					 */
					tr( "This is a ragdoll (bhkRagdollSystem).\n\n"
						"Its collision capsules decompile into editable per-bone bodies, but "
						"NifSkope cannot compile a ragdoll back. The joint constraints, the "
						"constraint motor and the ragdoll's own skeleton copy decode correctly, "
						"and nothing writes them into NIF blocks — so Decompile drops them and "
						"Compile has nothing to rebuild them from.\n\n"
						"To change a body's friction, restitution or collision filter, edit it "
						"in place in the Collision Manager instead: that keeps the whole system "
						"byte for byte.\n\n"
						"Decompile anyway? Keep an unmodified copy of the file." ),
					QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel )
				!= QMessageBox::Yes )
				return index;
		}
		HknpSystem sys = hknpDecode( nif->get<QByteArray>( iSys, "Binary Data" ) );
		if ( !sys.valid ) {
			QMessageBox::warning( nullptr, tr( "Decompile Compiled Collision" ),
				tr( "Could not decompile the Havok data: %1" ).arg( sys.error ) );
			return index;
		}

		// Every collision object referencing this system names ITS body via
		// "Body ID", and that body is placed by that object's node transform
		// (the cinfo transform is only a rest pose - validated on vanilla
		// stair helpers, and the same binding the preview draws with).
		int sysNum = nif->getBlockNumber( iSys );
		QVector<QPair<quint32, int>> refs;	// (body id, node block number)
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex ib = nif->getBlockIndex( b );
			if ( nif->blockInherits( ib, "bhkNPCollisionObject" ) && nif->getLink( ib, "Data" ) == sysNum )
				refs.append( { nif->get<quint32>( ib, "Body ID" ),
					nif->isValidBlockNumber( nif->getLink( ib, "Target" ) )
						? nif->getLink( ib, "Target" ) : nif->getParent( b ) } );
		}

		// This operation inserts, rewires, and removes an entire collision graph.
		// Suppress link/view refreshes until the graph is internally consistent;
		// otherwise the viewport can observe half-built bodies between mutations.
		nif->setState( BaseModel::Loading );
		float sc = havokConst * 10.0f;	// Havok -> game units for FO4

		// built shapes grouped by the body they belong to (shapes no body
		// references, bodyId -1, belong to body 0 - the rule the preview uses)
		QVector<QPair<quint32, QVector<qint32>>> bodyShapes;
		for ( const HknpShape & shp : sys.shapes ) {
			qint32 num = buildShape( nif, shp, sc );
			// a sphere's center folds into the wrapper's translation
			Vector3 extraT;
			if ( shp.primType == 1 )
				extraT = shp.capA;
			Vector3 wrapT = shp.trans + shp.transformed( extraT ) - shp.transformed( Vector3() );
			bool identRot = ( shp.rotRows[0] - Vector3( 1, 0, 0 ) ).length() < 1.0e-6f
							&& ( shp.rotRows[1] - Vector3( 0, 1, 0 ) ).length() < 1.0e-6f
							&& ( shp.rotRows[2] - Vector3( 0, 0, 1 ) ).length() < 1.0e-6f;
			bool identScale = ( shp.scaleVec - Vector3( 1, 1, 1 ) ).length() < 1.0e-6f;
			bool needWrap = ( shp.hasTransform && !( identRot && identScale && shp.trans.length() < 1.0e-6f ) )
							|| extraT.length() > 1.0e-6f;
			if ( num >= 0 && needWrap ) {
				// verbatim NIF Matrix4: rows = scale_i * rotation row i,
				// translation in row 3 (byte-identical to the pre-Elric form)
				QModelIndex iTfm = nif->insertNiBlock(
					shp.isConvex ? "bhkConvexTransformShape" : "bhkTransformShape" );
				nif->setLink( iTfm, "Shape", num );
				float m16[16] = {
					shp.rotRows[0][0] * shp.scaleVec[0], shp.rotRows[0][1] * shp.scaleVec[0],
						shp.rotRows[0][2] * shp.scaleVec[0], 0.0f,
					shp.rotRows[1][0] * shp.scaleVec[1], shp.rotRows[1][1] * shp.scaleVec[1],
						shp.rotRows[1][2] * shp.scaleVec[1], 0.0f,
					shp.rotRows[2][0] * shp.scaleVec[2], shp.rotRows[2][1] * shp.scaleVec[2],
						shp.rotRows[2][2] * shp.scaleVec[2], 0.0f,
					wrapT[0], wrapT[1], wrapT[2], 1.0f
				};
				nif->set<Matrix4>( iTfm, "Transform", Matrix4( m16 ) );
				num = nif->getBlockNumber( iTfm );
			}
			if ( num < 0 )
				continue;
			quint32 id = shp.bodyId >= 0 ? quint32( shp.bodyId ) : 0;
			bool grouped = false;
			for ( auto & bs : bodyShapes ) {
				if ( bs.first == id ) { bs.second.append( num ); grouped = true; break; }
			}
			if ( !grouped )
				bodyShapes.append( { id, { num } } );
		}

		// one bhkRigidBody + bhkCollisionObject per body, attached to that
		// body's own node - the exact inverse of Elric's compile, so the node
		// transform places the body just like the compiled form
		QPersistentModelIndex iRoot;
		for ( const auto & bs : bodyShapes ) {
			qint32 rootShape = bs.second.first();
			if ( bs.second.size() > 1 ) {
				QModelIndex iList = nif->insertNiBlock( "bhkListShape" );
				nif->set<uint>( iList, "Num Sub Shapes", uint( bs.second.size() ) );
				nif->updateArraySize( iList, "Sub Shapes" );
				nif->setLinkArray( iList, "Sub Shapes", bs.second );
				rootShape = nif->getBlockNumber( iList );
			}
			if ( !iRoot.isValid() )
				iRoot = nif->getBlockIndex( rootShape );

			QModelIndex iBody = nif->insertNiBlock( "bhkRigidBody" );
			nif->setLink( iBody, "Shape", rootShape );

			// physics decoded from the packfile (validated against controlled
			// Elric pairs; enum values match what the 3ds Max exporter
			// authors: dynamic props Motion System 3 / Quality 4 / Solver
			// Deactivation 2, statics 5 / 0 / 1)
			QModelIndex iInfo = nif->getIndex( iBody, "Rigid Body Info" );
			if ( iInfo.isValid() ) {
				HknpBodyPhys phys = ( int( bs.first ) < sys.bodyPhys.size() )
									? sys.bodyPhys.at( int( bs.first ) ) : HknpBodyPhys();
				{
					quint32 decodedLayer = phys.layer;
					if ( decodedLayer == 0 ) decodedLayer = ( sys.dynamic && phys.hasMotion ) ? 10u : 1u;
					// both stored copies, so a decompiled body does not come out with the
					// block-level filter left at whatever the template had
					bhkSetFilterField( nif, iBody, QStringLiteral( "Layer" ), decodedLayer );
					bhkSetFilterField( nif, iBody, QStringLiteral( "Flags" ), phys.filterFlags );
					bhkSetFilterField( nif, iBody, QStringLiteral( "Group" ), phys.filterGroup );
				}
				nif->set<float>( iInfo, "Friction", phys.friction );
				nif->set<float>( iInfo, "Restitution", phys.restitution );
				// per-body, not per-system: a ragdoll's bones carry different
				// masses and damping, and taking the system scalars gave every
				// bone bone 0's values
				nif->set<float>( iInfo, "Linear Damping", phys.linDamping );
				nif->set<float>( iInfo, "Angular Damping", phys.angDamping );
				nif->set<float>( iInfo, "Gravity Factor", phys.gravityFactor );
				nif->set<float>( iInfo, "Max Linear Velocity", phys.maxLinVelocity );
				nif->set<float>( iInfo, "Max Angular Velocity", phys.maxAngVelocity );
				nif->set<float>( iInfo, "Penetration Depth", 0.15f );
				nif->set<float>( iInfo, "Time Factor", 1.0f );
				nif->set<quint32>( iInfo, "Collision Response", 1 );	// SIMPLE_CONTACT
				nif->set<quint32>( iInfo, "Process Contact Callback Delay 3", 0xffff );
				nif->set<quint32>( iInfo, "Num Shape Keys in Contact Point", 3 );
				nif->set<quint32>( iInfo, "Deactivator Type", 1 );
				bool dyn = sys.dynamic && phys.hasMotion;
				nif->set<quint32>( iInfo, "Motion System", dyn ? 3u : 5u );
				nif->set<quint32>( iInfo, "Solver Deactivation", dyn ? 2u : 1u );
				nif->set<quint32>( iInfo, "Quality Type", dyn ? 4u : 0u );
				nif->set<float>( iInfo, "Mass", dyn ? phys.mass : 0.0f );
				QModelIndex iInertia = nif->getIndex( iInfo, "Inertia Tensor" );
				if ( dyn && iInertia.isValid() ) {
					/* The packfile stores INVERSE inertia; bhkRigidBody's Inertia
					 * Tensor means the real thing. Convert, so the field a user
					 * reads or edits is the quantity it claims to be.
					 * hknpencode reciprocates on the way back out, leaving the
					 * byte round trip untouched.
					 */
					for ( int k = 0; k < 3; k++ ) {
						const float ii = phys.invInertia[k];
						const char * nm = ( k == 0 ) ? "m11" : ( k == 1 ) ? "m22" : "m33";
						nif->set<float>( iInertia, nm, ( ii > 1.0e-12f ) ? 1.0f / ii : 0.0f );
					}
				}
				if ( dyn ) {
					// cinfo +0x30 is the body position; for a single-body prop
					// whose shape is authored about the origin that is also
					// where bhkRigidBody wants its Center. It is NOT a centre of
					// mass offset -- on a ragdoll it is exactly the bone origin.
					nif->set<Vector4>( iInfo, "Center",
						Vector4( phys.position[0], phys.position[1], phys.position[2], 0.0f ) );
				}
			}

			// the node whose collision object named this body; a body no
			// object names falls back to the first referencing node
			int nodeNum = -1;
			for ( const auto & ref : refs ) {
				if ( ref.first == bs.first ) { nodeNum = ref.second; break; }
			}
			if ( nodeNum < 0 && !refs.isEmpty() )
				nodeNum = refs.first().second;

			QModelIndex iCO = nif->insertNiBlock( "bhkCollisionObject" );
			nif->setLink( iCO, "Body", nif->getBlockNumber( iBody ) );
			QModelIndex iNode = nif->getBlockIndex( nodeNum );
			if ( iNode.isValid() ) {
				nif->setLink( iCO, "Target", nodeNum );
				nif->setLink( nif->getIndex( iNode, "Collision Object" ), nif->getBlockNumber( iCO ) );
			}
		}

		// drop EVERY referencing bhkNPCollisionObject plus the system itself
		// (each node now carries its own decoded collision object). Removing
		// only the first would leave the others with dangling references.
		QVector<int> toRemove;
		toRemove.append( sysNum );
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex ib = nif->getBlockIndex( b );
			if ( nif->blockInherits( ib, "bhkNPCollisionObject" ) && nif->getLink( ib, "Data" ) == sysNum )
				toRemove.append( b );
		}
		// remove high-to-low so lower block numbers stay valid; removeNiBlock
		// updates all link references (dangling ones become -1)
		std::sort( toRemove.begin(), toRemove.end(), []( int a, int b ) { return a > b; } );
		for ( int b : toRemove )
			nif->removeNiBlock( b );
		nif->restoreState();
		nif->updateModel();

		if ( !sys.unknownShapes.isEmpty() ) {
			QMessageBox::warning( nullptr, tr( "Decompile Compiled Collision" ),
				tr( "Some shape types could not be decompiled and were skipped: %1" )
					.arg( sys.unknownShapes.join( QLatin1String( ", " ) ) ) );
		}

		return QModelIndex( iRoot );
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		QPersistentModelIndex result;
		nifSnapshotOp( nif, name(), [&]() { result = decodeRaw( nif, index ); } );
		return QModelIndex( result );
	}
};

REGISTER_SPELL( spDecodeCompiledCollision )

//! Decompile every compiled collision system in the file
class spDecodeAllCompiledCollision final : public Spell
{
public:
	QString name() const override final { return Spell::tr( "Decompile All Compiled Collision" ); }
	QString group() const override { return Spell::tr( "Collision" ); }
	QString page() const override final { return Spell::tr( "Havok" ); }

	/*! A block whose Binary Data is an hknp packfile.
	 *
	 * bhkRagdollSystem has to be here as well as bhkPhysicsSystem: a skeleton's
	 * per-bone collision lives in a ragdoll system, and matching only the physics
	 * class made this command silently do nothing on every skeleton file while
	 * the single-block Decompile (which tests both, havok.cpp systemBlock)
	 * worked. The two paths must agree on what "compiled collision" means.
	 */
	static bool isCompiledSystem( const NifModel * nif, const QModelIndex & iBlock )
	{
		return nif->isNiBlock( iBlock, "bhkPhysicsSystem" )
			|| nif->isNiBlock( iBlock, "bhkRagdollSystem" );
	}

	bool isApplicable( const NifModel * nif, const QModelIndex & index ) override final
	{
		/* Spells menu only. It used to also accept a clicked compiled-collision
		 * block, which put a row on that block's context menu that then ignored
		 * the block entirely and rewrote EVERY system in the file — the one
		 * thing a context menu promises not to do. The single-block
		 * Decompile Compiled Collision is right beside it and does what the
		 * right-click implies.
		 */
		if ( index.isValid() )
			return false;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			if ( isCompiledSystem( nif, nif->getBlockIndex( b ) )
				&& nif->get<QByteArray>( nif->getBlockIndex( b ), "Binary Data" ).size() > 0x100 )
				return true;
		}
		return false;
	}

	QModelIndex cast( NifModel * nif, const QModelIndex & index ) override final
	{
		spDecodeCompiledCollision decoder;
		QPersistentModelIndex result;
		nifSnapshotOp( nif, name(), [&]() {
			// each decode removes / inserts blocks, so rescan after every pass
			for ( bool again = true; again; ) {
				again = false;
				for ( int b = 0; b < nif->getBlockCount(); b++ ) {
					QModelIndex ib = nif->getBlockIndex( b );
					if ( isCompiledSystem( nif, ib ) && decoder.isApplicable( nif, ib ) ) {
						QModelIndex decoded = decoder.decodeRaw( nif, ib );
						if ( !result.isValid() && decoded.isValid() ) result = decoded;
						again = true;
						break;
					}
				}
			}
		} );
		// The right-click invocation index normally points at a system block that
		// was removed above. Returning it leaves SpellBook with a dangling
		// QModelIndex and caused the post-decode heap-corruption crash. Select a
		// newly created persistent block instead.
		Q_UNUSED( index );
		return QModelIndex( result );
	}
};

REGISTER_SPELL( spDecodeAllCompiledCollision )

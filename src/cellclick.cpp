/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellclick.h"

#include "cellpick.h"

#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <cmath>

namespace {

/*! The same full-precision descriptor the cell scene's own shapes carry
 *  (src/cellview.cpp, CELL_VERTEX_DESC).  These vertices are in BLOCK space,
 *  tens of thousands of units out, and a half float cannot hold that. */
constexpr std::uint64_t HL_VERTEX_DESC = 0x0041B00000650407ULL;

constexpr int HL_EDGES = 12;
constexpr int HL_QUADS = HL_EDGES * 2;      //!< two perpendicular blades an edge
constexpr int HL_VERTS = HL_QUADS * 4;      //!< 96
constexpr int HL_TRIS = HL_QUADS * 2;       //!< 48

/*! How thick a highlight bar is, as a fraction of the box's longest side, and
 *  the floor under it in world units.  A fraction alone makes the bars on a
 *  bottle invisible; a constant alone makes the bars on a building a smear. */
constexpr float HL_THICK_FRACTION = 0.012f;
constexpr float HL_THICK_MIN = 1.5f;

int g_block = -1;                   //!< the highlight shape's block number, or -1
float g_origin[3] = { 0, 0, 0 };    //!< the block origin its Translation carries
bool g_enabled = false;
int g_current = -1;

/*! The 96 corners of the twelve edge crosses.  Each edge of the box gets TWO
 *  perpendicular blades, so the highlight reads from any direction instead of
 *  vanishing whenever the camera lines up with a flat quad. */
void fillBars( const float bmin[3], const float bmax[3], float th, Vector3 * out )
{
	int v = 0;
	for ( int k = 0; k < 3; k++ ) {
		const int a = ( k + 1 ) % 3;
		const int b = ( k + 2 ) % 3;
		for ( int corner = 0; corner < 4; corner++ ) {
			float p0[3], p1[3];
			for ( int i = 0; i < 3; i++ )
				p0[i] = bmin[i];
			p0[a] = ( corner & 1 ) ? bmax[a] : bmin[a];
			p0[b] = ( corner & 2 ) ? bmax[b] : bmin[b];
			p0[k] = bmin[k];
			for ( int i = 0; i < 3; i++ )
				p1[i] = p0[i];
			p1[k] = bmax[k];

			// blade 1 spreads along `a`, blade 2 along `b`
			const int spread[2] = { a, b };
			for ( int s = 0; s < 2; s++ ) {
				const int ax = spread[s];
				float q[4][3];
				for ( int i = 0; i < 3; i++ ) {
					q[0][i] = p0[i];
					q[1][i] = p0[i];
					q[2][i] = p1[i];
					q[3][i] = p1[i];
				}
				q[0][ax] -= th;
				q[1][ax] += th;
				q[2][ax] += th;
				q[3][ax] -= th;
				for ( int c = 0; c < 4; c++ )
					out[v++] = Vector3( q[c][0], q[c][1], q[c][2] );
			}
		}
	}
}

//! Write the 96 positions into the shape's vertex array. No block is added or
//! removed: this is a value edit on a shape that already exists.
void writeBars( NifModel * nif, const Vector3 * pts )
{
	if ( !nif || g_block < 0 )
		return;
	const QModelIndex iShape = nif->getBlockIndex( g_block, "BSTriShape" );
	if ( !iShape.isValid() )
		return;
	const QModelIndex iVertexData = nif->getIndex( iShape, "Vertex Data" );
	if ( !iVertexData.isValid() )
		return;

	nif->setState( BaseModel::Processing );
	Vector3 lo( 3.4e38f, 3.4e38f, 3.4e38f ), hi( -3.4e38f, -3.4e38f, -3.4e38f );
	for ( int v = 0; v < HL_VERTS; v++ ) {
		const QModelIndex row = nif->index( v, 0, iVertexData );
		if ( !row.isValid() )
			break;
		nif->set<Vector3>( row, "Vertex", pts[v] );
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = qMin( lo[k], pts[v][k] );
			hi[k] = qMax( hi[k], pts[v][k] );
		}
	}
	const QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
	if ( iBound.isValid() ) {
		const Vector3 c = ( lo + hi ) / 2.0f;
		const Vector3 h = ( hi - lo ) / 2.0f;
		nif->set<Vector3>( iBound, "Center", c );
		nif->set<float>( iBound, "Radius", h.length() );
	}
	nif->restoreState();
}

} // namespace


bool cellPickEnabled()
{
	return g_enabled;
}

void cellPickSetEnabled( bool on )
{
	g_enabled = on;
}

CellPickBus * CellPickBus::instance()
{
	static CellPickBus bus;
	return &bus;
}

int cellPickCurrent()
{
	return g_current;
}

bool cellPickClick( NifModel * nif, const float origin[3], const float direction[3] )
{
	if ( !g_enabled )
		return false;
	const CellPickTable & table = cellPickTable();
	if ( table.size() == 0 )
		return false;

	CellRay ray;
	for ( int k = 0; k < 3; k++ ) {
		ray.o[k] = origin[k];
		ray.d[k] = direction[k];
	}
	int candidates = 0;
	const int hit = table.pick( ray, &candidates, nullptr );

	cellPickSelect( nif, hit, candidates );
	/* A MISS IS STILL A CLICK THIS OWNS.  Returning false on a miss would let
	 * the ordinary block selection run and select the welded bucket under the
	 * cursor, which is a hundred thousand vertices of unrelated references --
	 * the one selection a person clicking in a cell view never wants. */
	return true;
}

bool cellPickSelect( NifModel * nif, int index, int candidates )
{
	/* THE TAIL THE MOUSE ALREADY USED (lane CELLWORK1). This was the last eight
	 * lines of cellPickClick; the Cell workspace's reference list needed the
	 * same eight and copying them would have been two answers to "what is
	 * selected". No master check here -- see cellclick.h for why. */
	const CellPickTable & table = cellPickTable();
	if ( index >= table.size() )
		return false;

	g_current = index;
	if ( index >= 0 ) {
		const CellPickEntry & e = table.at( index );
		cellHighlightShow( nif, e.bmin, e.bmax );
	} else {
		cellHighlightHide( nif );
	}
	emit CellPickBus::instance()->picked( index, candidates );
	return true;
}

void cellPickOrigin( float out[3] )
{
	for ( int k = 0; k < 3; k++ )
		out[k] = g_origin[k];
}

int cellHighlightCreate( NifModel * nif, const QModelIndex & iRoot, const float origin[3] )
{
	g_block = -1;
	g_current = -1;
	for ( int k = 0; k < 3; k++ )
		g_origin[k] = origin[k];
	if ( !nif || !iRoot.isValid() )
		return -1;

	QModelIndex iShape = nif->insertNiBlock( QStringLiteral( "BSTriShape" ) );
	if ( !iShape.isValid() )
		return -1;
	nif->set<QString>( iShape, "Name", QStringLiteral( "!cell pick" ) );
	nif->set<quint32>( iShape, "Flags", 14 );
	nif->set<float>( iShape, "Scale", 1.0f );
	nif->set<Vector3>( iShape, "Translation", Vector3( origin[0], origin[1], origin[2] ) );

	BSVertexDesc desc( HL_VERTEX_DESC );
	desc.SetFlag( VertexFlags::VF_COLORS );
	desc.ResetAttributeOffsets( 130 );
	const std::uint64_t vertexDesc = desc.Value();
	const int stride = int( desc.GetVertexSize() );

	nif->set<BSVertexDesc>( iShape, "Vertex Desc", vertexDesc );
	nif->set<quint32>( iShape, "Num Vertices", quint32( HL_VERTS ) );
	nif->set<quint32>( iShape, "Num Triangles", quint32( HL_TRIS ) );
	nif->set<quint32>( iShape, "Data Size",
		quint32( qint64( HL_VERTS ) * stride + qint64( HL_TRIS ) * 6 ) );

	nif->setState( BaseModel::Processing );
	QModelIndex iVertexData = nif->getIndex( iShape, "Vertex Data" );
	nif->updateArraySize( iVertexData );
	for ( int v = 0; v < HL_VERTS; v++ ) {
		const QModelIndex row = nif->index( v, 0, iVertexData );
		if ( !row.isValid() )
			break;
		nif->set<Vector3>( row, "Vertex", Vector3( 0.0f, 0.0f, 0.0f ) );
		nif->set<HalfVector2>( row, "UV", HalfVector2( Vector2( 0.0f, 0.0f ) ) );
		nif->set<ByteVector3>( row, "Normal", ByteVector3( Vector3( 0.0f, 0.0f, 1.0f ) ) );
		nif->set<ByteVector3>( row, "Tangent", ByteVector3( Vector3( 1.0f, 0.0f, 0.0f ) ) );
		// a zero bitangent is a NaN in the shader's basis and renders BLACK
		nif->set<float>( row, "Bitangent X", 0.0f );
		nif->set<float>( row, "Bitangent Y", 1.0f );
		nif->set<float>( row, "Bitangent Z", 0.0f );
		nif->set<ByteColor4>( row, "Vertex Colors",
			ByteColor4( FloatVector4( 1.0f, 0.78f, 0.18f, 1.0f ) ) );
	}
	QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
	nif->updateArraySize( iTriangles );
	{
		QVector<Triangle> tris;
		tris.reserve( HL_TRIS );
		for ( int q = 0; q < HL_QUADS; q++ ) {
			const quint16 b = quint16( q * 4 );
			tris.append( Triangle( b, quint16( b + 1 ), quint16( b + 2 ) ) );
			tris.append( Triangle( b, quint16( b + 2 ), quint16( b + 3 ) ) );
		}
		nif->setArray<Triangle>( iTriangles, tris );
	}
	nif->restoreState();

	/* EMISSIVE, and with the same `#AARRGGBB` one-texel diffuse the ground uses
	 * (src/cellview.cpp): an EMPTY diffuse slot is not neutral, it binds the
	 * missing-texture magenta under Scene::DoErrorColor (src/gl/renderer.cpp
	 * ~951), which is exactly how this lane's first terrain came out magenta. */
	QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
	nif->set<quint32>( iShader, "Shader Type", 0 );
	nif->set<quint32>( iShader, "Shader Flags 1", 2151677953U );
	nif->set<quint32>( iShader, "Shader Flags 2", 0x25U );
	nif->set<float>( iShader, "Emissive Multiple", 3.0f );
	QModelIndex iTexSet = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
	nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTexSet ) );
	nif->set<uint>( iTexSet, "Num Textures", 10 );
	nif->updateArraySize( iTexSet, "Textures" );
	QModelIndex iArr = nif->getIndex( iTexSet, "Textures" );
	nif->set<QString>( nif->getIndex( iArr, 0 ), QStringLiteral( "#FFFFFFFF" ) );
	nif->set<QString>( nif->getIndex( iArr, 1 ), QStringLiteral( "#FFFF8080n" ) );
	nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );

	addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iShape ) );
	g_block = nif->getBlockNumber( iShape );
	return g_block;
}

void cellHighlightShow( NifModel * nif, const float bmin[3], const float bmax[3] )
{
	if ( !nif || g_block < 0 )
		return;
	float longest = 0.0f;
	for ( int k = 0; k < 3; k++ )
		longest = qMax( longest, bmax[k] - bmin[k] );
	const float th = qMax( HL_THICK_MIN, longest * HL_THICK_FRACTION );

	float lo[3], hi[3];
	for ( int k = 0; k < 3; k++ ) {
		lo[k] = bmin[k] - g_origin[k];
		hi[k] = bmax[k] - g_origin[k];
	}
	Vector3 pts[HL_VERTS];
	fillBars( lo, hi, th, pts );
	writeBars( nif, pts );
}

void cellHighlightHide( NifModel * nif )
{
	if ( !nif || g_block < 0 )
		return;
	Vector3 pts[HL_VERTS];
	for ( int v = 0; v < HL_VERTS; v++ )
		pts[v] = Vector3( 0.0f, 0.0f, 0.0f );
	writeBars( nif, pts );
}

void cellHighlightForget()
{
	g_block = -1;
	g_current = -1;
	emit CellPickBus::instance()->sceneChanged();
}

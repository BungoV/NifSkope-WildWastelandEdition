/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "starterscene.h"

#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QVector>

#include <cmath>

namespace
{

//! One vertex of the generated cube.
struct StarterVert
{
	Vector3 pos, nrm;
	Vector2 uv;
};

/*! Cube with hard normals: 24 vertices, 6 quads, 12 triangles.
 *
 * The same construction as tlMakePrimitive( 1, ... ) in glview.cpp, which is the
 * interactive Add Primitive. Kept separate rather than shared because that one
 * lives inside GLView's translation unit and this has to run without a GL
 * context; if either changes shape, change both.
 */
void starterCube( float size, QVector<StarterVert> & verts, QVector<Triangle> & tris )
{
	const float h = size * 0.5f;
	// The +-Y pair is { 2, 0 }, not { 0, 2 }: each face winds CCW in its own
	// (u, v) plane, so u x v must BE the outward normal. X x Z is -Y, which
	// wound both Y faces inside out - they were backface-culled and the cube
	// showed its own interior through the gap. Z x X is +Y.
	static const int axes[6][2] = { { 0, 1 }, { 0, 1 }, { 2, 0 }, { 2, 0 }, { 1, 2 }, { 1, 2 } };
	static const int naxis[6] = { 2, 2, 1, 1, 0, 0 };
	static const float nsign[6] = { 1, -1, 1, -1, 1, -1 };
	static const float cu[4] = { -1, 1, 1, -1 };
	static const float cv[4] = { -1, -1, 1, 1 };

	auto quad = [&tris]( int a, int b, int c, int d ) {
		tris.append( Triangle( quint16( a ), quint16( b ), quint16( c ) ) );
		tris.append( Triangle( quint16( a ), quint16( c ), quint16( d ) ) );
	};

	for ( int f = 0; f < 6; f++ ) {
		Vector3 n;
		n[naxis[f]] = nsign[f];
		const int u = axes[f][0], v = axes[f][1];
		const int base = verts.size();
		for ( int k = 0; k < 4; k++ ) {
			Vector3 p;
			p[naxis[f]] = nsign[f] * h;
			p[u] = cu[k] * h;
			p[v] = cv[k] * h;
			verts.append( { p, n, Vector2( ( cu[k] + 1 ) * 0.5f, ( 1 - cv[k] ) * 0.5f ) } );
		}
		// outward winding on every face: the negative faces wind the other way
		if ( nsign[f] > 0 )
			quad( base, base + 1, base + 2, base + 3 );
		else
			quad( base, base + 3, base + 2, base + 1 );
	}
}

} // namespace

bool nifCreateStarterScene( NifModel * nif, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( !nif )
		return fail( QStringLiteral( "no model" ) );

	/* Fallout 4 is file version 20.2.0.7, user version 12, BS version 130. The
	 * startup defaults in Settings would give BS 100 (Skyrim SE), and BS version
	 * conditions the layout of BSTriShape's vertex data, so it has to be explicit
	 * rather than left to a user setting other things read.
	 */
	if ( !nif->createNew( 0x14020007, 12, 130 ) )
		return fail( QStringLiteral( "could not create a Fallout 4 document" ) );

	nif->holdUpdates( true );
	QModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
	nif->set<QString>( iRoot, "Name", QStringLiteral( "Scene Root" ) );
	nif->set<quint32>( iRoot, "Flags", 14 );
	nif->set<float>( iRoot, "Scale", 1.0f );
	nif->holdUpdates( false );
	nif->updateModel();

	if ( !iRoot.isValid() )
		return fail( QStringLiteral( "could not create the root node" ) );
	if ( error )
		error->clear();
	return true;
}

bool nifCreateCubeScene( NifModel * nif, float size, QString * error )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( !( size > 0.0f ) )
		return fail( QStringLiteral( "cube size must be positive" ) );
	// the same document the GUI opens with, then a shape put into it - so the
	// fixture and the product cannot drift apart in header or root
	if ( !nifCreateStarterScene( nif, error ) )
		return false;

	QVector<StarterVert> verts;
	QVector<Triangle> tris;
	starterCube( size, verts, tris );

	nif->holdUpdates( true );
	QModelIndex iRoot = nif->getBlockIndex( 0 );
	QModelIndex iShape = nif->insertNiBlock( QStringLiteral( "BSTriShape" ) );
	nif->set<QString>( iShape, "Name", QStringLiteral( "Cube" ) );
	nif->set<quint32>( iShape, "Flags", 14 );
	nif->set<float>( iShape, "Scale", 1.0f );
	// Sitting ON the grid rather than half-sunk through it. The mesh stays
	// symmetric about its own origin, so the node - and the pivot every transform
	// tool uses - is at the cube's centre; only the node is lifted.
	nif->set<Vector3>( iShape, "Translation", Vector3( 0.0f, 0.0f, size * 0.5f ) );

	/* Full-precision vertices with UV, normal and tangent: 28 bytes a vertex.
	 * Same descriptor the collision proxy shape uses (collisiontools.cpp), which
	 * is the one layout in this codebase already known to load and render.
	 * Vanilla statics use 0x0001B00000430205 — the half-float variant, 20 bytes —
	 * and full precision is the friendlier starting point to edit by hand.
	 */
	const std::uint64_t vertexDesc = 0x0041B00000650407ULL;
	const int stride = 28;
	nif->set<BSVertexDesc>( iShape, "Vertex Desc", vertexDesc );
	nif->set<quint32>( iShape, "Num Vertices", quint32( verts.size() ) );
	nif->set<quint32>( iShape, "Num Triangles", quint32( tris.size() ) );
	nif->set<quint32>( iShape, "Data Size", quint32( verts.size() * stride + tris.size() * 6 ) );

	nif->setState( BaseModel::Processing );
	QModelIndex iVertexData = nif->getIndex( iShape, "Vertex Data" );
	nif->updateArraySize( iVertexData );
	for ( int i = 0; i < verts.size(); i++ ) {
		QModelIndex row = nif->getIndex( iVertexData, i );
		nif->set<Vector3>( row, "Vertex", verts.at( i ).pos );
		nif->set<HalfVector2>( row, "UV", HalfVector2( verts.at( i ).uv ) );
		nif->set<ByteVector3>( row, "Normal", ByteVector3( verts.at( i ).nrm ) );
		// a stable perpendicular, the same placeholder Add Primitive writes;
		// Update Tangent Space replaces it once the cube has real UVs
		Vector3 t = Vector3::crossproduct( verts.at( i ).nrm, Vector3( 0, 0, 1 ) );
		if ( t.squaredLength() < 1.0e-6f )
			t = Vector3( 1, 0, 0 );
		t.normalize();
		nif->set<ByteVector3>( row, "Tangent", t );

		/* The bitangent MUST be written, and this is not cosmetic.
		 *
		 * The lighting shader builds a tangent-space basis from normal, tangent
		 * and bitangent and normalizes it. Leaving the bitangent at its zero
		 * default makes that normalize() a normalize(0,0,0) - NaN - and the shape
		 * renders pure BLACK however correct its base map and lights are. That was
		 * the black starter cube, and it is the same failure mode as the 07-17
		 * line defect: a zero that becomes NaN in a shader.
		 *
		 * It only showed up on the freshly-built document. Saving and reloading put
		 * the bitangent through the normbyte encoding, where a stored 0 decodes to
		 * about 0.0039 - tiny, but non-zero, so the basis survives and the same
		 * file loaded from disk rendered correctly. A capture of both showed
		 * exactly (0,0,0) against (0, 0.004, 0.004).
		 */
		const Vector3 b = Vector3::crossproduct( verts.at( i ).nrm, t );
		nif->set<float>( row, "Bitangent X", b[0] );
		nif->set<float>( row, "Bitangent Y", b[1] );
		nif->set<float>( row, "Bitangent Z", b[2] );
	}
	QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
	nif->updateArraySize( iTriangles );
	nif->setArray<Triangle>( iTriangles, tris );

	// bounds by hand rather than through gl/gltools.h's BoundSphere: this has to
	// build without a GL context so the CLI can write the same document. A cube
	// centred on the origin needs no fitting - the sphere through its corners is
	// the exact bound.
	QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
	if ( iBound.isValid() ) {
		nif->set<Vector3>( iBound, "Center", Vector3() );
		nif->set<float>( iBound, "Radius", size * 0.5f * std::sqrt( 3.0f ) );
	}
	nif->restoreState();

	// FO4 keeps the material in the shader property's Name (a .bgsm path); an
	// empty one renders with the shader's own defaults, which is what a starter
	// shape should do until a material is chosen.
	QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
	QModelIndex iTextures = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
	nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTextures ) );

	/* A neutral mid-grey base map, Blender's default-material look.
	 *
	 * "#AARRGGBB" is the renderer's own inline-colour texture syntax - the same
	 * one its built-in fallbacks use (`white` is "#FFFFFFFF" in renderer.cpp) -
	 * and a texture-set slot resolves through the same bind() those go through.
	 * So this needs no file on disk and cannot go missing.
	 *
	 * Slot 0 is diffuse and slot 1 is the normal map; "#FFFF8080" is the flat
	 * normal the renderer itself defaults to. Ten slots to match vanilla.
	 *
	 * 0x80 here is LINEAR 0.5, which displays around 0xB8 and lands close to
	 * Blender's default material. Append the renderer's sRGB suffix -
	 * "#FF808080s", as `gray` in renderer.cpp does - to mean a perceptual 50%
	 * grey instead, which reads noticeably darker.
	 */
	nif->set<uint>( iTextures, "Num Textures", 10 );
	nif->updateArraySize( iTextures, "Textures" );
	QModelIndex iTexArray = nif->getIndex( iTextures, "Textures" );
	nif->set<QString>( nif->getIndex( iTexArray, 0 ), QStringLiteral( "#FF808080" ) );
	nif->set<QString>( nif->getIndex( iTexArray, 1 ), QStringLiteral( "#FFFF8080" ) );
	nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );

	addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iShape ) );

	nif->holdUpdates( false );
	nif->updateModel();

	if ( error )
		error->clear();
	return true;
}

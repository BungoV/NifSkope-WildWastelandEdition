/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodinative.h"

#include "lodifile.h"
#include "lodofile.h"

#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{

/* Guard rails for ONE document, not the format's. The same species of limit
 * btdterrain.cpp keeps for the terrain route, and for the same reason: a
 * region the caller asked for by mistake must be refused in words before it
 * spends four minutes building a scene nobody can move. */
constexpr qint64 MAX_TOTAL_VERTS = 9500000;
constexpr qint64 MAX_SHAPES = 8192;
//! BSTriShape counts vertices in a u16, and its triangles index them the same way.
constexpr int MAX_SHAPE_VERTS = 65000;

//! Full precision, 28 bytes a vertex: the descriptor the terrain route and the
//! starter cube already write, known to load and render. The `.BTO`'s own
//! 20-byte half-precision layout is not usable here -- these vertices are in
//! REGION space, up to tens of thousands of units from the shape's origin, and
//! a half float cannot hold that.
constexpr std::uint64_t LODI_VERTEX_DESC = 0x0041B00000650407ULL;

struct OutVert
{
	Vector3 pos, nrm, tan, bit;
	Vector2 uv;
	float uv2y = 0.0f;
	/*! The CHANNEL's colour for this vertex, 0..1 a component -- what the vertex
	 *  colour is written from. For WW_LODL_AO (and WW_LODL_CHANNEL=ao) all three
	 *  are the AO grey, which is what this field held when it was named `ao`. */
	float chan[3] = { 1.0f, 1.0f, 1.0f };
	//! `.lodo` library values kept from the decode: self-AO and the sway weight,
	//! both 0..1. The placement loop needs them per drawn vertex.
	float selfAo = 1.0f;
	float sway = 0.0f;
	//! index into LodoLibrary::vertices (the .lodi v6 vertex-AO stream is in this order)
	quint32 libIndex = 0;
};

//! One (base, material) bucket: every placement of that base welded in.
struct Bucket
{
	quint16 baseId = 0;
	quint16 materialId = 0;
	QString name;
	QString matString;
	bool hasAlpha = false;
	quint8 alphaThreshold = 0;
	bool twoSided = false;
	bool emits = false;
	float emissiveScale = 1.0f;
	int layer = -1;
	//! WW_LODL_AO / WW_LODL_CHANNEL: write the vertices' `chan` as a vertex colour
	bool withColour = false;
	std::vector<OutVert> verts;
	std::vector<Triangle> tris;
};

//! `a op b` on 3x3 row-major times a column vector.
inline Vector3 rotate( const float m[9], const Vector3 & v )
{
	return Vector3( m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
		m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
		m[6] * v[0] + m[7] * v[1] + m[8] * v[2] );
}

//! The `.lodo` material string is either a BGSM/BGEM material or a diffuse
//! texture path. Both are what the `.BTO` shape for the same source carries:
//! src/nativeemit.cpp writes `sh.matName.isEmpty() ? sh.tex0 : sh.matName`.
bool isMaterialFile( const QString & s )
{
	return s.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive )
		|| s.endsWith( QStringLiteral( ".bgem" ), Qt::CaseInsensitive );
}

/*! The material name EXACTLY as the `.lodo` stored it, which is the source
 *  NIF's own shader name.
 *
 *  MEASURED, not assumed: those names are absolute build-machine paths --
 *  `C:\Projects\Fallout4\Build\PC\Data\Materials\LOD\RockSlab01LOD.BGSM` and
 *  135 more like it in this library. `GameManager::get_full_path` already
 *  handles that shape: it looks for `materials/` ANYWHERE on a separator
 *  boundary and throws away everything before it, so the absolute path lands
 *  on `materials/lod/rockslab01lod.bgsm` and resolves. Prepending `materials\`
 *  first is what BREAKS it -- the prefix then sits at offset 0, the search
 *  stops there, and nothing is erased. So this function only normalises the
 *  separators, and the chunk builder's own route (src/lodgen.cpp ~2183, which
 *  prepends only when the prefix is absent) is the one being copied. */
QString materialNameFor( const QString & s )
{
	QString p = s;
	p.replace( QChar( '/' ), QChar( '\\' ) );
	return p;
}

/*! The `_n` beside a `_d`. The generator writes its LOD textures as a
 *  `_d`/`_n`/`_s` triple under `textures\LOD\...` (see any `--tex-dir` output),
 *  and the chunk builder pairs them the same way; the `.lodo` keeps only ONE
 *  string a material, so the pairing has to be redone here. Returns an empty
 *  string when the diffuse is not named `_d`, rather than guessing. */
QString siblingSuffix( const QString & diffuse, const char * suffix )
{
	const int dot = diffuse.lastIndexOf( QChar( '.' ) );
	if ( dot < 2 )
		return QString();
	const QString stem = diffuse.left( dot );
	if ( !stem.endsWith( QStringLiteral( "_d" ), Qt::CaseInsensitive ) )
		return QString();
	return stem.left( stem.size() - 2 ) + QLatin1String( suffix ) + diffuse.mid( dot );
}

//! A box drawn as twelve thin quads, so it reads as a WIRE box in a renderer
//! that has no line pass. `t` is the bar half-thickness in world units.
void appendWireBox( std::vector<OutVert> & verts, std::vector<Triangle> & tris,
	const Vector3 & centre, const Vector3 & half, const float m[9], float t )
{
	auto local = [&]( float x, float y, float z ) {
		return centre + rotate( m, Vector3( x, y, z ) );
	};
	// the twelve edges as (from, to) corner pairs of the unit box
	static const int corner[8][3] = {
		{ -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
		{ -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }
	};
	static const int edge[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	};
	for ( int e = 0; e < 12; e++ ) {
		const int * a = corner[edge[e][0]];
		const int * b = corner[edge[e][1]];
		const Vector3 pa = local( a[0] * half[0], a[1] * half[1], a[2] * half[2] );
		const Vector3 pb = local( b[0] * half[0], b[1] * half[1], b[2] * half[2] );
		Vector3 dir = pb - pa;
		if ( dir.length() < 1.0e-4f )
			continue;
		dir.normalize();
		Vector3 up = Vector3( 0, 0, 1 );
		if ( std::fabs( Vector3::dotproduct( dir, up ) ) > 0.9f )
			up = Vector3( 1, 0, 0 );
		Vector3 side = Vector3::crossproduct( dir, up );
		side.normalize();
		Vector3 nrm = Vector3::crossproduct( dir, side );
		nrm.normalize();
		const quint16 base = quint16( verts.size() );
		const Vector3 quad[4] = { pa - side * t, pa + side * t, pb + side * t, pb - side * t };
		for ( int i = 0; i < 4; i++ ) {
			OutVert v;
			v.pos = quad[i];
			v.nrm = nrm;
			v.tan = side;
			v.bit = Vector3::crossproduct( nrm, side );
			v.uv = Vector2( float( i & 1 ), float( ( i >> 1 ) & 1 ) );
			verts.push_back( v );
		}
		tris.push_back( Triangle( base, quint16( base + 1 ), quint16( base + 2 ) ) );
		tris.push_back( Triangle( base, quint16( base + 2 ), quint16( base + 3 ) ) );
	}
}

/*! Write one bucket's geometry as one or more BSTriShapes under `iRoot`.
 *  Splits at MAX_SHAPE_VERTS, because a BSTriShape counts vertices in a u16. */
bool emitBucket( NifModel * nif, const QModelIndex & iRoot, const Bucket & b,
	const Vector3 & origin, qint64 & shapesOut, qint64 & vertsOut, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};
	if ( b.verts.empty() || b.tris.empty() )
		return true;

	BSVertexDesc desc( LODI_VERTEX_DESC );
	if ( b.layer >= 0 )
		desc.SetFlag( VertexFlags::VF_UV_2 );
	if ( b.withColour )
		desc.SetFlag( VertexFlags::VF_COLORS );
	if ( b.layer >= 0 || b.withColour )
		desc.ResetAttributeOffsets( 130 );
	const std::uint64_t vertexDesc = desc.Value();
	const int stride = int( desc.GetVertexSize() );

	/* The split walks TRIANGLES, not vertices: a triangle whose three
	 * vertices are not all in the current part starts a new part and takes
	 * copies of them. Splitting the vertex array instead would leave
	 * triangles pointing across the cut. */
	size_t first = 0;
	int part = 0;
	while ( first < b.tris.size() ) {
		QHash<int, quint16> remap;
		std::vector<OutVert> pv;
		std::vector<Triangle> pt;
		size_t i = first;
		for ( ; i < b.tris.size(); i++ ) {
			if ( pv.size() + 3 > size_t( MAX_SHAPE_VERTS ) )
				break;
			const Triangle & t = b.tris[i];
			quint16 idx[3];
			for ( int k = 0; k < 3; k++ ) {
				const int src = int( t[k] );
				auto it = remap.constFind( src );
				if ( it != remap.constEnd() ) {
					idx[k] = it.value();
				} else {
					idx[k] = quint16( pv.size() );
					remap.insert( src, idx[k] );
					pv.push_back( b.verts[size_t( src )] );
				}
			}
			pt.push_back( Triangle( idx[0], idx[1], idx[2] ) );
		}
		first = i;
		part++;
		if ( pv.empty() || pt.empty() )
			break;

		shapesOut++;
		vertsOut += qint64( pv.size() );
		if ( shapesOut > MAX_SHAPES )
			return fail( QString( "this region needs more than %1 shapes; ask for a "
				"smaller WW_LODI_REGION or a coarser WW_LODI_LEVEL" ).arg( MAX_SHAPES ) );
		if ( vertsOut > MAX_TOTAL_VERTS )
			return fail( QString( "this region needs more than %L1 vertices; ask for a "
				"smaller WW_LODI_REGION or a coarser WW_LODI_LEVEL" ).arg( MAX_TOTAL_VERTS ) );

		QModelIndex iShape = nif->insertNiBlock( QStringLiteral( "BSTriShape" ) );
		nif->set<QString>( iShape, "Name", part > 1
			? QString( "%1 #%2" ).arg( b.name ).arg( part ) : b.name );
		nif->set<quint32>( iShape, "Flags", 14 );
		nif->set<float>( iShape, "Scale", 1.0f );
		nif->set<Vector3>( iShape, "Translation", origin );
		nif->set<BSVertexDesc>( iShape, "Vertex Desc", vertexDesc );
		nif->set<quint32>( iShape, "Num Vertices", quint32( pv.size() ) );
		nif->set<quint32>( iShape, "Num Triangles", quint32( pt.size() ) );
		nif->set<quint32>( iShape, "Data Size",
			quint32( qint64( pv.size() ) * stride + qint64( pt.size() ) * 6 ) );

		nif->setState( BaseModel::Processing );
		QModelIndex iVertexData = nif->getIndex( iShape, "Vertex Data" );
		nif->updateArraySize( iVertexData );
		Vector3 lo( 3.4e38f, 3.4e38f, 3.4e38f ), hi( -3.4e38f, -3.4e38f, -3.4e38f );
		for ( size_t v = 0; v < pv.size(); v++ ) {
			QModelIndex row = nif->index( int( v ), 0, iVertexData );
			const OutVert & o = pv[v];
			nif->set<Vector3>( row, "Vertex", o.pos );
			nif->set<HalfVector2>( row, "UV", HalfVector2( o.uv ) );
			nif->set<ByteVector3>( row, "Normal", ByteVector3( o.nrm ) );
			nif->set<ByteVector3>( row, "Tangent", ByteVector3( o.tan ) );
			// a zero bitangent is a NaN in the shader's basis and renders BLACK
			nif->set<float>( row, "Bitangent X", o.bit[0] );
			nif->set<float>( row, "Bitangent Y", o.bit[1] );
			nif->set<float>( row, "Bitangent Z", o.bit[2] );
			if ( b.layer >= 0 ) {
				QModelIndex iUv2 = nif->getIndex( row, "UV 2" );
				if ( iUv2.isValid() )
					nif->set<HalfVector2>( row, "UV 2", HalfVector2( Vector2( 0.0f, o.uv2y ) ) );
			}
			if ( b.withColour )
				nif->set<ByteColor4>( row, "Vertex Colors",
					ByteColor4( FloatVector4( o.chan[0], o.chan[1], o.chan[2], 1.0f ) ) );
			for ( int k = 0; k < 3; k++ ) {
				lo[k] = qMin( lo[k], o.pos[k] );
				hi[k] = qMax( hi[k], o.pos[k] );
			}
		}
		QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
		nif->updateArraySize( iTriangles );
		{
			QVector<Triangle> qt;
			qt.reserve( int( pt.size() ) );
			for ( const Triangle & t : pt )
				qt.append( t );
			nif->setArray<Triangle>( iTriangles, qt );
		}
		QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
		if ( iBound.isValid() ) {
			const Vector3 c = ( lo + hi ) / 2.0f;
			const Vector3 h = ( hi - lo ) / 2.0f;
			nif->set<Vector3>( iBound, "Center", c );
			nif->set<float>( iBound, "Radius", h.length() );
		}
		nif->restoreState();

		/* The material plumbing is the chunk builder's (src/lodgen.cpp ~4189),
		 * not a second one: the same block pair, the same ten slots, the same
		 * flag words, and the BGSM in the shader's Name so the renderer
		 * resolves it exactly as it does for a `.BTO` shape whose source named
		 * a material (src/gl/glproperty.cpp, BSShaderLightingProperty::setMaterial). */
		QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
		nif->set<quint32>( iShader, "Shader Type", 0 );
		nif->set<quint32>( iShader, "Shader Flags 1",
			b.emits ? 2151677953U : ( 2151677953U & ~0x400000U ) );
		// 0x20 = vertex colours, the same bit the terrain route sets for its plane views
		nif->set<quint32>( iShader, "Shader Flags 2", b.withColour ? 0x25U : 5U );
		QModelIndex iTexSet = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
		nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTexSet ) );
		nif->set<uint>( iTexSet, "Num Textures", 10 );
		nif->updateArraySize( iTexSet, "Textures" );
		QModelIndex iArr = nif->getIndex( iTexSet, "Textures" );
		if ( isMaterialFile( b.matString ) ) {
			nif->set<QString>( iShader, "Name", materialNameFor( b.matString ) );
		} else if ( !b.matString.isEmpty() ) {
			nif->set<QString>( nif->getIndex( iArr, 0 ), b.matString );
			const QString n = siblingSuffix( b.matString, "_n" );
			if ( !n.isEmpty() )
				nif->set<QString>( nif->getIndex( iArr, 1 ), n );
			const QString s = siblingSuffix( b.matString, "_s" );
			if ( !s.isEmpty() )
				nif->set<QString>( nif->getIndex( iArr, 7 ), s );
		}
		if ( b.emits )
			nif->set<float>( iShader, "Emissive Multiple", b.emissiveScale );
		nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );
		if ( b.hasAlpha ) {
			QModelIndex iAlpha = nif->insertNiBlock( QStringLiteral( "NiAlphaProperty" ) );
			nif->set<int>( iAlpha, "Flags", 4844 );
			nif->set<int>( iAlpha, "Threshold", int( b.alphaThreshold ) );
			nif->setLink( iShape, "Alpha Property", nif->getBlockNumber( iAlpha ) );
		}
		addLink( nif, iRoot, QStringLiteral( "Children" ), nif->getBlockNumber( iShape ) );
	}
	return true;
}

} // namespace


bool lodiSpecFromEnv( LodiSceneSpec & spec )
{
	bool got = false;
	const QByteArray env = qgetenv( "WW_LODI_REGION" );
	if ( !env.isEmpty() ) {
		const QStringList parts = QString::fromLatin1( env ).split( QLatin1Char( ',' ) );
		if ( parts.size() == 4 ) {
			spec.x0 = parts[0].toInt();
			spec.y0 = parts[1].toInt();
			spec.x1 = parts[2].toInt();
			spec.y1 = parts[3].toInt();
			if ( spec.x1 < spec.x0 )
				std::swap( spec.x0, spec.x1 );
			if ( spec.y1 < spec.y0 )
				std::swap( spec.y0, spec.y1 );
			spec.haveRegion = true;
			spec.valid = true;
			got = true;
		} else {
			qCritical().noquote() << QString( "REFUSED: WW_LODI_REGION=\"%1\" is not "
				"\"x0,y0,x1,y1\" in cells; this run is ignoring it." )
				.arg( QString::fromLatin1( env ) );
		}
	}
	if ( !qgetenv( "WW_LODI_LEVEL" ).isEmpty() ) {
		spec.level = qMax( 0, qEnvironmentVariableIntValue( "WW_LODI_LEVEL" ) );
		spec.valid = true;
		got = true;
	}
	if ( qEnvironmentVariableIntValue( "WW_LODI_BOXES" ) == 1 ) {
		spec.boxes = true;
		spec.valid = true;
		got = true;
	}
	return got;
}


namespace {

//! The one table: the name a user types, and the channel it means. Both
//! `lodlChannelFromEnv` and `lodlChannelName` read this and nothing else, so a
//! name can never mean one thing going in and another coming out.
struct ChannelName
{
	const char * name;
	LodlChannel channel;
};

const ChannelName CHANNEL_NAMES[] = {
	{ "identity", LodlChannel::Identity },
	{ "placement", LodlChannel::Placement },
	{ "identityraw", LodlChannel::IdentityRaw },
	{ "sky", LodlChannel::Sky },
	{ "ground", LodlChannel::Ground },
	{ "seed", LodlChannel::Seed },
	{ "sway", LodlChannel::Sway },
	{ "selfao", LodlChannel::SelfAo },
	{ "ao", LodlChannel::Ao },
	{ "mask-r", LodlChannel::MaskR },
	{ "mask-g", LodlChannel::MaskG },
	{ "mask-b", LodlChannel::MaskB },
	{ "mask-a", LodlChannel::MaskA },
	{ "emissive", LodlChannel::Emissive },
	{ "normal", LodlChannel::Normal },
	{ "scrappable", LodlChannel::Scrappable },
};

} // namespace


LodlChannel lodlChannelFromEnv( QString * given, int * bin )
{
	const QString env = qEnvironmentVariable( "WW_LODL_CHANNEL" ).trimmed().toLower();
	if ( given )
		*given = env;
	if ( bin )
		*bin = 0;
	if ( env.isEmpty() )
		return LodlChannel::None;
	/* NO CHANNEL TAKES AN ARGUMENT any more -- `horizonbin=<n>` was the only
	 * one and it went with the baked-horizon route (lane HORIZONOUT). The split
	 * stays because the REFUSAL is the point: a name with an argument on it is
	 * refused whole, so a stale `WW_LODL_CHANNEL=horizonbin=3` in a script draws
	 * nothing and is named in the note line, rather than quietly drawing the
	 * default picture. */
	const int eq = env.indexOf( QLatin1Char( '=' ) );
	const QString head = ( eq >= 0 ) ? env.left( eq ) : env;
	for ( const ChannelName & c : CHANNEL_NAMES ) {
		if ( head != QLatin1String( c.name ) )
			continue;
		if ( eq >= 0 )
			return LodlChannel::None;       // an argument on a name that takes none
		return c.channel;
	}
	return LodlChannel::None;
}

QString lodlChannelName( LodlChannel c )
{
	for ( const ChannelName & n : CHANNEL_NAMES )
		if ( n.channel == c )
			return QLatin1String( n.name );
	return QString();
}

QString lodlChannelNames()
{
	QStringList all;
	for ( const ChannelName & n : CHANNEL_NAMES )
		all << QString( QLatin1String( n.name ) );
	return all.join( QStringLiteral( ", " ) );
}


bool nifAppendLodiObjects( NifModel * nif, const QModelIndex & iRoot,
	const QString & lodiPath, const LodiSceneSpec & specIn,
	QString * error, QString * notes )
{
	auto fail = [error]( const QString & message ) {
		if ( error )
			*error = message;
		return false;
	};
	if ( !nif || !iRoot.isValid() )
		return fail( QStringLiteral( "no model" ) );

	QElapsedTimer timer;
	timer.start();

	// the `.lodo` beside it, by worldspace stem
	const QFileInfo fi( lodiPath );
	const QString lodoPath = fi.absolutePath() + QChar( '/' )
		+ fi.completeBaseName() + QStringLiteral( ".lodo" );
	if ( !QFileInfo::exists( lodoPath ) )
		return fail( QString( "no %1 beside %2 -- a .lodi is a placement table and "
			"carries no geometry; its .lodo library has to sit next to it" )
			.arg( QFileInfo( lodoPath ).fileName(), fi.fileName() ) );

	LodoHeader oh;
	LodoLibrary lib;
	QString err;
	if ( !lodoRead( lodoPath, &oh, &lib, false, &err ) )
		return fail( QString( "%1: %2" ).arg( QFileInfo( lodoPath ).fileName(), err ) );

	LodiHeader ih;
	LodiTable table;
	if ( !lodiRead( lodiPath, &ih, &table, false, &err ) )
		return fail( QString( "%1: %2" ).arg( fi.fileName(), err ) );

	LodiSceneSpec spec = specIn;
	const int cellW = int( ih.chunkCells );
	const int fileX0 = int( ih.chunkWest ) * cellW;
	const int fileY0 = int( ih.chunkSouth ) * cellW;
	const int fileX1 = ( int( ih.chunkEast ) + 1 ) * cellW - 1;
	const int fileY1 = ( int( ih.chunkNorth ) + 1 ) * cellW - 1;
	if ( !spec.haveRegion ) {
		spec.x0 = fileX0;
		spec.y0 = fileY0;
		spec.x1 = fileX1;
		spec.y1 = fileY1;
	}

	QStringList note;
	note << QString( "lodi %1 v%2: chunks [%3,%4]..[%5,%6] (%7 cells a chunk), "
			"%8 instances in %9 of %10 chunks; library %11: %12 bases, %13 meshes, "
			"%14 clusters, %15 materials, levelMax %16" )
		.arg( fi.fileName() ).arg( ih.version )
		.arg( ih.chunkWest ).arg( ih.chunkSouth ).arg( ih.chunkEast ).arg( ih.chunkNorth )
		.arg( cellW ).arg( ih.instanceCount ).arg( ih.presentChunks ).arg( ih.chunkCount )
		.arg( QFileInfo( lodoPath ).fileName() )
		.arg( oh.baseCount ).arg( oh.meshCount ).arg( oh.clusterCount )
		.arg( oh.materialCount ).arg( int( oh.levelMax ) );
	note << QString( "region asked: cells [%1,%2]..[%3,%4]%5; cluster level %6" )
		.arg( spec.x0 ).arg( spec.y0 ).arg( spec.x1 ).arg( spec.y1 )
		.arg( spec.haveRegion ? QString() : QStringLiteral( " (the file's whole extent)" ) )
		.arg( spec.level );

	const Vector3 origin( float( spec.x0 ) * 4096.0f, float( spec.y0 ) * 4096.0f, 0.0f );

	// the census the gates read; written by the code that PLACES the instances
	const QString dumpPath = qEnvironmentVariable( "WW_LODI_DUMP" );
	QFile dumpFile( dumpPath );
	QTextStream dump;
	if ( !dumpPath.isEmpty() ) {
		if ( dumpFile.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			dump.setDevice( &dumpFile );
			dump << "# ww lodi census 1 file " << fi.fileName()
				<< " region " << spec.x0 << ' ' << spec.y0 << ' ' << spec.x1 << ' ' << spec.y1
				<< " level " << spec.level
				<< " columns ref part base x y z scale mesh material level\n";
		} else {
			note << QString( "WW_LODI_DUMP=%1 could not be opened for writing" ).arg( dumpPath );
		}
	}

	/* One mesh is rebuilt ONCE per (mesh, level) and then stamped into every
	 * placement, so a base that stands 700 times does not cost 700 decodes.
	 * `parts` is that decode: the mesh's clusters at the chosen level, grouped
	 * by material, in MESH-LOCAL space. */
	struct MeshPart
	{
		quint16 materialId = 0;
		std::vector<OutVert> verts;
		std::vector<Triangle> tris;
	};
	QHash<quint32, std::vector<MeshPart>> meshCache;
	const int wantSlot = qEnvironmentVariableIsSet( "WW_LODI_SLOT" )
		? qBound( 0, qEnvironmentVariableIntValue( "WW_LODI_SLOT" ), 3 ) : -1;
	QHash<quint16, int> levelUsed;
	/* WW_LODL_AO=1 (bungo 2026-09-18, "AO overlaid on the terrain and objects /
	 * trees"): every vertex carries the model's self-AO (`.lodo` selfAO, 255 =
	 * not baked = open) times its placement's own AO (`.lodi` v5 blob, 0xFF =
	 * not measured = open) as a grey vertex colour, which the lit path
	 * multiplies into the albedo. Unset, the vertex is the 28 bytes it was. */
	/* WW_LODL_CHANNEL=<name> (lane CHANVIEW1, bungo 2026-09-18): the same seam,
	 * one baked channel at a time. `ao` IS `WW_LODL_AO=1` -- the same branch, the
	 * same bytes -- so `wantAo` is simply true for that name too, and a run with
	 * WW_LODL_CHANNEL unset cannot behave differently from the day before. */
	QString channelGiven;
	int channelBin = 0;
	const LodlChannel channel = lodlChannelFromEnv( &channelGiven, &channelBin );
	const bool wantAo = qEnvironmentVariableIntValue( "WW_LODL_AO" ) != 0
		|| channel == LodlChannel::Ao;
	//! true for the names this builder draws on the PLACEMENTS (the terrain names
	//! are btdterrain's, and leave the objects at their default look)
	/* v7: `sky` is a PER-VERTEX channel when the file carries the stream and a
	 * per-placement one when it does not. The decision is taken from what was
	 * READ, never from the version word alone, and the note line says which of
	 * the two it drew (root MISTAKES: name a channel from the WRITER of the file
	 * in front of you). */
	const bool skyPerVertex = ( channel == LodlChannel::Sky ) && !table.vertexSkyFirst.empty()
		&& !table.vertexSky.empty();
	const bool objectChannel = channel == LodlChannel::Identity
		|| channel == LodlChannel::Placement
		|| channel == LodlChannel::IdentityRaw || ( channel == LodlChannel::Sky && !skyPerVertex )
		|| channel == LodlChannel::Ground || channel == LodlChannel::Seed
		|| channel == LodlChannel::Sway || channel == LodlChannel::SelfAo
		|| channel == LodlChannel::Scrappable;
	//! v7 read-back: how many placements `identity` had to fall back on, and how many drew a group
	qint64 groupDrawn = 0, groupFellBack = 0, skyVertSlices = 0, skyVertBytes = 0, skyVertMismatch = 0;
	/* READ BACK, not intent: every one of these is accumulated from the value
	 * that goes INTO the vertex buffer, at the push, one entry a drawn vertex or
	 * a drawn placement -- never from the file byte the loop started with. */
	qint64 chanCount = 0;
	int chanLo = 255, chanHi = 0;
	double chanSum = 0.0;
	auto chanSeen = [&]( int v ) {
		chanLo = qMin( chanLo, v );
		chanHi = qMax( chanHi, v );
		chanSum += v;
		chanCount++;
	};
	/*! The stock channel-1 palette, `res/shaders/fo4_default.frag` ~283: a value
	 *  noise hash of the index into a bright, well separated colour. Mirrored
	 *  here rather than reinvented so "the identity bake" looks the way the
	 *  `.BTO` channel view has always looked. */
	auto hashColour = []( quint32 idx, float out[3] ) {
		const float seeds[3] = { 12.9898f, 78.233f, 45.164f };
		for ( int k = 0; k < 3; k++ ) {
			const float s = std::sin( ( float( idx ) + 1.0f ) * seeds[k] ) * 43758.5453f;
			out[k] = ( s - std::floor( s ) ) * 0.8f + 0.2f;
		}
	};
	//! v6: placements whose per-vertex scene-AO stream was used / present but not matching the drawn mesh
	qint64 vaoUsed = 0, vaoMismatch = 0;
	double vaoSum = 0.0;
	qint64 vaoBytes = 0;
	QHash<quint16, QPair<quint32, quint32>> meshRange;  // meshId -> (first library vertex, count)
	qint64 aoMeasured = 0, aoUnmeasured = 0;
	double aoSum = 0.0;

	auto decodeMesh = [&]( quint16 meshId, int wantLevel ) -> const std::vector<MeshPart> & {
		const quint32 key = ( quint32( meshId ) << 8 ) | quint32( wantLevel & 0xFF );
		auto it = meshCache.find( key );
		if ( it != meshCache.end() )
			return it.value();

		std::vector<MeshPart> parts;
		const LodoMesh & mesh = lib.meshes[meshId];
		const int levels = qMax( 1, int( mesh.levelCount ) );
		const int level = qBound( 0, wantLevel, levels - 1 );
		levelUsed.insert( meshId, level );

		QHash<quint16, int> partOf;
		for ( quint32 c = mesh.clusterFirst; c < mesh.clusterFirst + mesh.clusterCount; c++ ) {
			if ( c >= lib.clusters.size() )
				break;
			/* One level is NOT `level == n` alone. A piece that stopped simplifying
			 * below n is a root (parentCount 0): nothing coarser replaces it, so it
			 * is still the drawn piece at n. Leaving roots out is what made walls
			 * and roofs vanish at the coarse levels (bungo 2026-09-17; 1,842 of
			 * 5,562 urban meshes carry such pieces). */
			if ( c < lib.clusterLods.size() ) {
				const LodoClusterLod & cll = lib.clusterLods[c];
				const bool rootBelow = int( cll.level ) < level && cll.parentCount == 0;
				if ( int( cll.level ) != level && !rootBelow )
					continue;
			}
			const LodoCluster & cl = lib.clusters[c];
			int pi;
			auto pit = partOf.constFind( cl.materialId );
			if ( pit != partOf.constEnd() ) {
				pi = pit.value();
			} else {
				pi = int( parts.size() );
				partOf.insert( cl.materialId, pi );
				parts.push_back( MeshPart() );
				parts.back().materialId = cl.materialId;
			}
			MeshPart & p = parts[size_t( pi )];
			const quint16 base = quint16( p.verts.size() );
			for ( int v = 0; v < int( cl.vertexCount ); v++ ) {
				const size_t vi = size_t( cl.vertexBase ) + size_t( v );
				if ( vi >= lib.vertices.size() )
					break;
				const LodoVertex & lv = lib.vertices[vi];
				OutVert o;
				for ( int k = 0; k < 3; k++ )
					o.pos[k] = lodoDequantU16( lv.pos[k], mesh.aabbMin[k], mesh.aabbExtent[k] );
				o.uv = Vector2( lodoDequantU16( lv.uv[0], mesh.uvMin[0], mesh.uvExtent[0] ),
					lodoDequantU16( lv.uv[1], mesh.uvMin[1], mesh.uvExtent[1] ) );
				float n[3];
				lodoUnpackOct12( quint32( lv.nrm[0] ) | ( quint32( lv.nrm[1] ) << 8 )
					| ( quint32( lv.nrm[2] ) << 16 ), n );
				o.nrm = Vector3( n[0], n[1], n[2] );
				float t[3];
				bool flip = false;
				lodoUnpackTangent( n, lv.tangent, t, &flip );
				o.tan = Vector3( t[0], t[1], t[2] );
				o.bit = Vector3::crossproduct( o.nrm, o.tan );
				if ( flip )
					o.bit = -o.bit;
				o.selfAo = float( lv.selfAO ) / 255.0f;
				o.sway = float( lv.sway ) / 255.0f;
				o.libIndex = quint32( vi );
				p.verts.push_back( o );
			}
			const size_t li = size_t( c ) * LODO_LOCAL_INDEX_BYTES;
			for ( int t = 0; t < int( cl.triangleCount ); t++ ) {
				if ( li + size_t( t ) * 3 + 2 >= lib.localIndices.size() )
					break;
				const quint8 a = lib.localIndices[li + size_t( t ) * 3];
				const quint8 b = lib.localIndices[li + size_t( t ) * 3 + 1];
				const quint8 cc = lib.localIndices[li + size_t( t ) * 3 + 2];
				if ( a == LODO_LOCAL_INDEX_NONE || b == LODO_LOCAL_INDEX_NONE
					|| cc == LODO_LOCAL_INDEX_NONE )
					continue;
				if ( a >= cl.vertexCount || b >= cl.vertexCount || cc >= cl.vertexCount )
					continue;
				p.tris.push_back( Triangle( quint16( base + a ), quint16( base + b ),
					quint16( base + cc ) ) );
			}
		}
		return meshCache.insert( key, std::move( parts ) ).value();
	};

	QHash<quint32, Bucket> buckets;
	QHash<quint32, QVector<Bucket>> spilled;	// full buckets set aside, in fill order
	qint64 read = 0, placed = 0, outsideRegion = 0, noMesh = 0, noGeometry = 0;
	QSet<quint16> basesSeen;

	for ( quint32 ci = 0; ci < quint32( table.chunks.size() ); ci++ ) {
		const LodiChunk & chunk = table.chunks[ci];
		if ( !chunk.instanceCount )
			continue;
		int chunkX = 0, chunkY = 0;
		lodiChunkAt( ih, ci, &chunkX, &chunkY );
		// a whole chunk outside the region cannot hold a placement inside it
		const int cx0 = chunkX * cellW, cy0 = chunkY * cellW;
		if ( cx0 + cellW - 1 < spec.x0 || cx0 > spec.x1
			|| cy0 + cellW - 1 < spec.y0 || cy0 > spec.y1 )
			continue;

		for ( quint32 k = 0; k < chunk.instanceCount; k++ ) {
			const size_t ii = size_t( chunk.instanceFirst ) + size_t( k );
			if ( ii >= table.instances.size() )
				break;
			read++;
			const LodiInstance & inst = table.instances[ii];
			float xyz[3];
			lodiDecodePosition( ih, ci, chunk, inst, xyz );
			const int cellX = int( std::floor( xyz[0] / 4096.0f ) );
			const int cellY = int( std::floor( xyz[1] / 4096.0f ) );
			if ( cellX < spec.x0 || cellX > spec.x1 || cellY < spec.y0 || cellY > spec.y1 ) {
				outsideRegion++;
				continue;
			}
			if ( inst.baseId >= lib.bases.size() ) {
				noMesh++;
				continue;
			}
			const LodoBase & base = lib.bases[inst.baseId];
			quint16 meshId = LODO_NO_MESH;
			/* WW_LODI_SLOT=n (0..3) draws the base's n-th AUTHORED slot and nothing
			 * else: an empty slot means the object is not drawn at that distance,
			 * which is what the stock engine does with an empty MNAM slot. Unset, the
			 * first slot that holds a mesh is drawn, as before (bungo 2026-09-17,
			 * authored LODs only: the four levels are the four slots). */
			if ( wantSlot >= 0 ) {
				if ( base.rep[wantSlot] != LODO_NO_MESH && base.rep[wantSlot] < lib.meshes.size() )
					meshId = base.rep[wantSlot];
			} else {
				for ( int r = 0; r < 4 && meshId == LODO_NO_MESH; r++ )
					if ( base.rep[r] != LODO_NO_MESH && base.rep[r] < lib.meshes.size() )
						meshId = base.rep[r];
			}
			if ( meshId == LODO_NO_MESH ) {
				noMesh++;
				continue;
			}

			const std::vector<MeshPart> & parts = decodeMesh( meshId, spec.level );
			if ( parts.empty() ) {
				noGeometry++;
				continue;
			}

			float quat[4], m[9];
			lodiUnpackRotation( inst.rot, quat, m );
			const float scale = float( inst.scale ) / LODI_SCALE_DIVISOR;
			/* REGION SPACE. `lodiDecodePosition` gives the WORLD position, and
			 * every shape this builder emits carries the region `origin` as its
			 * Translation (emitBucket, "Translation"), so a vertex has to be the
			 * world position MINUS that origin or the object is placed twice.
			 * The occluder-box arm below has always subtracted it by hand; this
			 * is the same subtraction on the arm that draws the placements.
			 * Restored by lane HORIZONOUT 2026-09-19: without it the counts, the
			 * census and every note line stayed right and the picture was empty
			 * -- the objects were drawn a chunk-origin away from the camera. */
			const Vector3 pos = Vector3( xyz[0], xyz[1], xyz[2] ) - origin;
			float placementAo = 1.0f;
			if ( wantAo ) {
				const quint8 pa = ( ii < table.placementAo.size() )
					? table.placementAo[ii] : LODI_PLACEMENT_AO_UNMEASURED;
				if ( pa != LODI_PLACEMENT_AO_UNMEASURED ) {
					placementAo = float( pa ) / 255.0f;
					aoMeasured++;
					aoSum += pa;
				} else {
					aoUnmeasured++;
				}
			}
			/* The PER-PLACEMENT channels. One value a placement, so the note
			 * line's N is the placement count and its mean is comparable to the
			 * `.lodi` reader's mean over the same population. */
			float placeChan[3] = { 1.0f, 1.0f, 1.0f };
			if ( objectChannel && channel != LodlChannel::Sway
				&& channel != LodlChannel::SelfAo ) {
				const LodiCold cold = ( ii < table.cold.size() ) ? table.cold[ii] : LodiCold();
				switch ( channel ) {
				case LodlChannel::Identity:
					/* v7: the GROUP. A v6 file has no group table, so this
					 * FALLS BACK to the placement identity and the note line
					 * names the arm that served it. */
					if ( ii < table.group.size() ) {
						hashColour( quint32( table.group[ii] ) + 1u, placeChan );
						chanSeen( int( table.group[ii] ) );
						groupDrawn++;
					} else {
						hashColour( quint32( cold.identity ), placeChan );
						chanSeen( int( cold.identity ) );
						groupFellBack++;
					}
					break;
				case LodlChannel::Placement:
					hashColour( quint32( cold.identity ), placeChan );
					chanSeen( int( cold.identity ) );
					break;
				case LodlChannel::IdentityRaw: {
					// the raw value's LOW byte: the u16 itself over 0..65535 would
					// draw this chunk's 0..2448 as black, which is not a picture
					const int v = int( cold.identity & 0xFF );
					placeChan[0] = placeChan[1] = placeChan[2] = float( v ) / 255.0f;
					chanSeen( v );
					break;
				}
				case LodlChannel::Sky:
					placeChan[0] = placeChan[1] = placeChan[2] = float( inst.sky ) / 255.0f;
					chanSeen( int( inst.sky ) );
					break;
				case LodlChannel::Ground:
					placeChan[0] = placeChan[1] = placeChan[2] = float( inst.ground ) / 255.0f;
					chanSeen( int( inst.ground ) );
					break;
				/* v9 (lane HORIZON3, 2026-09-19). TWO COLOURS AND NOTHING BETWEEN
				 * THEM, because the bit is one bit: a gradient here would invite
				 * the eye to read a confidence that the file does not carry.
				 * Magenta is not in the identity palette and is not a colour any
				 * other channel draws, so a scrappable placement cannot be
				 * mistaken for a hash collision. */
				case LodlChannel::Scrappable: {
					const bool scrap = ( inst.flags & LODI_INST_SCRAPPABLE ) != 0;
					placeChan[0] = scrap ? 1.0f : 0.25f;
					placeChan[1] = scrap ? 0.0f : 0.25f;
					placeChan[2] = scrap ? 1.0f : 0.25f;
					chanSeen( scrap ? 1 : 0 );
					break;
				}
				case LodlChannel::Seed:
					// 0 is NOT a tree (docs s4.3), and black says so; a tree's seed
					// takes the same hash the identity does, so two trees that share
					// a seed share a colour and the repetition can be seen
					if ( inst.seed )
						hashColour( quint32( inst.seed ), placeChan );
					else
						placeChan[0] = placeChan[1] = placeChan[2] = 0.0f;
					chanSeen( int( inst.seed ) );
					break;
				default:
					break;
				}
			}
			/* .lodi v6: the bake cast every placement's vertices against the SCENE (its
			 * own triangles, the other placements, the terrain, the neighbouring chunks'
			 * placements) and streamed one byte a library vertex, in the mesh's vertex
			 * order (bungo 2026-09-18: "the AO on the objects was from the objects
			 * themselves, from objects amongst each other, and with the objects and
			 * terrain and with objects on nearby chunks too"). That byte already holds
			 * the self-AO and what the flat placement byte approximated, so it is used
			 * ALONE; the self x placement product is the fallback for a v5 file, an
			 * empty slice, or a slice cast for another slot's mesh. */
			const quint8 * vaoSlice = nullptr;
			quint32 vaoFirstVertex = 0;
			if ( wantAo && ii + 1 < table.vertexAoFirst.size() ) {
				const quint32 f = table.vertexAoFirst[ii], l = table.vertexAoFirst[ii + 1];
				if ( l > f && l <= table.vertexAo.size() ) {
					auto rit = meshRange.find( meshId );
					if ( rit == meshRange.end() ) {
						quint32 lo = 0xFFFFFFFFu, hi = 0;
						const LodoMesh & mr = lib.meshes[meshId];
						for ( quint32 c = mr.clusterFirst; c < mr.clusterFirst + mr.clusterCount && c < lib.clusters.size(); c++ ) {
							lo = qMin( lo, lib.clusters[c].vertexBase );
							hi = qMax( hi, lib.clusters[c].vertexBase + quint32( lib.clusters[c].vertexCount ) );
						}
						rit = meshRange.insert( meshId, qMakePair( lo, hi > lo ? hi - lo : 0u ) );
					}
					if ( rit.value().second == l - f ) {
						vaoSlice = table.vertexAo.data() + f;
						vaoFirstVertex = rit.value().first;
						vaoUsed++;
						vaoBytes += qint64( l - f );
					} else {
						vaoMismatch++;
					}
				}
			}
			/* v7: the per-vertex SKY slice, found exactly as the AO slice above
			 * is -- same offsets shape, same "does its length match the drawn
			 * mesh" gate, so a slice cast for another slot's mesh is refused
			 * here rather than drawn as somebody else's numbers. */
			const quint8 * vskSlice = nullptr;
			quint32 vskFirstVertex = 0;
			if ( skyPerVertex && ii + 1 < table.vertexSkyFirst.size() ) {
				const quint32 f = table.vertexSkyFirst[ii], l = table.vertexSkyFirst[ii + 1];
				if ( l > f && l <= table.vertexSky.size() ) {
					auto rit = meshRange.find( meshId );
					if ( rit == meshRange.end() ) {
						quint32 lo = 0xFFFFFFFFu, hi = 0;
						const LodoMesh & mr = lib.meshes[meshId];
						for ( quint32 c = mr.clusterFirst; c < mr.clusterFirst + mr.clusterCount && c < lib.clusters.size(); c++ ) {
							lo = qMin( lo, lib.clusters[c].vertexBase );
							hi = qMax( hi, lib.clusters[c].vertexBase + quint32( lib.clusters[c].vertexCount ) );
						}
						rit = meshRange.insert( meshId, qMakePair( lo, hi > lo ? hi - lo : 0u ) );
					}
					if ( rit.value().second == l - f ) {
						vskSlice = table.vertexSky.data() + f;
						vskFirstVertex = rit.value().first;
						skyVertSlices++;
						skyVertBytes += qint64( l - f );
					} else {
						skyVertMismatch++;
					}
				}
			}

			/* The repetition breaker's UV mirror, applied the way the chunk
			 * builder applies it (src/lodgen.cpp ~3905): about the U MIDPOINT,
			 * never 1-u, because a tree LOD texture is often an atlas cell and
			 * a global flip would sample the neighbour's cell. The midpoint
			 * here is the MESH's stored UV rect rather than one source shape's
			 * own range -- that rect is what the `.lodo` carries, and it is the
			 * tightest bound the file has. */
			const bool mirrorU = ( inst.flags & LODI_INST_MIRRORED ) != 0;
			const LodoMesh & meshRow = lib.meshes[meshId];
			const float uMidMesh = 2.0f * meshRow.uvMin[0] + meshRow.uvExtent[0];

			for ( const MeshPart & p : parts ) {
				/* The mirror is about THIS part's own U range, as the chunk builder
				 * has it per shape (src/lodgen.cpp, `uMid = uMin + uMax`). The
				 * mesh-wide range put the maple's branch cards (u 0..0.5 of an atlas
				 * whose right half is bark) onto the bark (bungo 2026-09-17). */
				float uMid = uMidMesh;
				if ( mirrorU && !p.verts.empty() ) {
					float uLo = p.verts[0].uv[0], uHi = uLo;
					for ( const OutVert & sv : p.verts ) {
						uLo = qMin( uLo, sv.uv[0] );
						uHi = qMax( uHi, sv.uv[0] );
					}
					uMid = uLo + uHi;
				}
				const quint32 bkey = ( quint32( inst.baseId ) << 16 ) | quint32( p.materialId );
				auto bit = buckets.find( bkey );
				if ( bit == buckets.end() ) {
					Bucket nb;
					nb.baseId = inst.baseId;
					nb.materialId = p.materialId;
					const QString model = lib.stringAt( base.modelStringOffset );
					nb.name = QString( "%1 %2 m%3" )
						.arg( QString::number( base.formId, 16 ).rightJustified( 8, QChar( '0' ) ) )
						.arg( model.isEmpty() ? QStringLiteral( "?" )
							: QFileInfo( QString( model ).replace( QChar( '\\' ), QChar( '/' ) ) )
								.completeBaseName() )
						.arg( p.materialId );
					if ( p.materialId < lib.materials.size() ) {
						const LodoMaterial & mat = lib.materials[p.materialId];
						nb.matString = lib.stringAt( mat.lodmStringOffset );
						nb.hasAlpha = mat.alphaThreshold != 0;
						nb.alphaThreshold = mat.alphaThreshold;
						/* PICTURE-ONLY dial for the tree-card cutoff question (bungo
						 * 2026-09-18, "what is going with those trees? They seem broken"):
						 * WW_LODL_TREE_ALPHA=1..255 draws every TREE material that tests
						 * alpha at that threshold instead of the file's (the chunk builder
						 * writes 128 on every LOD material, lodgen.cpp ~2179; the tree LOD
						 * BGSMs themselves say 80/82). The file is not touched. NOTE the
						 * renderer takes the BGSM's own ref over this NiAlphaProperty when
						 * the BGSM loads (src/gl/renderer.cpp ~1321, same dial there); this
						 * one covers the no-BGSM fallback only. */
						if ( nb.hasAlpha && ( mat.flags & LODO_MAT_TREE ) ) {
							const int dial = qEnvironmentVariableIntValue( "WW_LODL_TREE_ALPHA" );
							if ( dial >= 1 && dial <= 255 )
								nb.alphaThreshold = quint8( dial );
						}
						nb.twoSided = ( mat.flags & LODO_MAT_TWO_SIDED ) != 0;
						nb.emits = ( mat.flags & LODO_MAT_EMITS ) != 0;
						nb.emissiveScale = mat.emissiveScale;
						nb.layer = ( mat.layer == LODO_NO_LAYER ) ? -1 : int( mat.layer );
					}
					nb.withColour = wantAo || objectChannel || skyPerVertex;
					bit = buckets.insert( bkey, nb );
				}
				Bucket & bk = bit.value();
				/* A Triangle is three u16. One (base, material) bucket takes every
				 * placement in the region, so a common tree walks past 65,536
				 * vertices, and `vstart + t[k]` then wrapped: the triangle kept one
				 * tree's corner and took two from trees placed long before, drawn as
				 * the long thin streaks between trees (bungo 2026-09-17). emitBucket's
				 * split runs after the wrap and cannot undo it, so a bucket that the
				 * next part would overflow is set aside whole and a fresh one begun. */
				if ( bk.verts.size() + p.verts.size() > size_t( 65535 ) && !bk.verts.empty() ) {
					Bucket full = bk;
					bk.verts.clear();
					bk.tris.clear();
					spilled[bkey].append( full );
				}
				const size_t vstart = bk.verts.size();
				bk.verts.reserve( vstart + p.verts.size() );
				for ( const OutVert & sv : p.verts ) {
					OutVert o;
					o.pos = pos + rotate( m, sv.pos * scale );
					o.nrm = rotate( m, sv.nrm );
					o.tan = rotate( m, sv.tan );
					o.bit = rotate( m, sv.bit );
					o.uv = mirrorU ? Vector2( uMid - sv.uv[0], sv.uv[1] ) : sv.uv;
					o.uv2y = bk.layer >= 0 ? float( bk.layer ) : 0.0f;
					if ( channel == LodlChannel::Sway ) {
						o.chan[0] = o.chan[1] = o.chan[2] = sv.sway;
						chanSeen( int( sv.sway * 255.0f + 0.5f ) );
					} else if ( channel == LodlChannel::SelfAo ) {
						o.chan[0] = o.chan[1] = o.chan[2] = sv.selfAo;
						chanSeen( int( sv.selfAo * 255.0f + 0.5f ) );
					} else if ( objectChannel ) {
						for ( int k = 0; k < 3; k++ )
							o.chan[k] = placeChan[k];
					} else if ( skyPerVertex ) {
						/* v7: one byte a library vertex. A placement with no
						 * slice draws its flat 0x11 byte rather than white, so
						 * an absent slice reads as "no stream here", not as
						 * "fully open sky". */
						const quint8 sk = vskSlice ? vskSlice[sv.libIndex - vskFirstVertex] : inst.sky;
						o.chan[0] = o.chan[1] = o.chan[2] = float( sk ) / 255.0f;
						chanSeen( int( sk ) );
					} else if ( vaoSlice ) {
						const quint8 a = vaoSlice[sv.libIndex - vaoFirstVertex];
						o.chan[0] = o.chan[1] = o.chan[2] = float( a ) / 255.0f;
						vaoSum += a;
					} else {
						const float g = sv.selfAo * placementAo;
						o.chan[0] = o.chan[1] = o.chan[2] = g;
					}
					bk.verts.push_back( o );
				}
				bk.tris.reserve( bk.tris.size() + p.tris.size() );
				for ( const Triangle & t : p.tris )
					bk.tris.push_back( Triangle( quint16( vstart + t[0] ),
						quint16( vstart + t[1] ), quint16( vstart + t[2] ) ) );
			}

			basesSeen.insert( inst.baseId );
			placed++;
			if ( dump.device() ) {
				const LodiCold cold = ( ii < table.cold.size() ) ? table.cold[ii] : LodiCold();
				dump << QString::number( cold.refFormId, 16 ).rightJustified( 8, QChar( '0' ) )
					<< ' ' << cold.scolPart
					<< ' ' << QString::number( base.formId, 16 ).rightJustified( 8, QChar( '0' ) )
					<< ' ' << QString::number( double( xyz[0] ), 'f', 3 )
					<< ' ' << QString::number( double( xyz[1] ), 'f', 3 )
					<< ' ' << QString::number( double( xyz[2] ), 'f', 3 )
					<< ' ' << QString::number( double( scale ), 'f', 5 )
					<< ' ' << meshId
					<< ' ' << ( parts.empty() ? -1 : int( parts.front().materialId ) )
					<< ' ' << levelUsed.value( meshId, spec.level ) << '\n';
			}
		}
	}

	const qint64 msRead = timer.elapsed();

	qint64 shapes = 0, verts = 0;
	QList<quint32> keys = buckets.keys();
	std::sort( keys.begin(), keys.end() );
	for ( quint32 key : keys ) {
		bool ok = true;
		for ( const Bucket & full : spilled.value( key ) )
			ok = ok && emitBucket( nif, iRoot, full, origin, shapes, verts, error );
		if ( !ok || !emitBucket( nif, iRoot, buckets[key], origin, shapes, verts, error ) ) {
			if ( dump.device() ) {
				dump.flush();
				dumpFile.close();
			}
			return false;
		}
	}

	// the occluder boxes, drawn ONLY on request and then as wire boxes
	qint64 boxes = 0;
	if ( spec.boxes ) {
		Bucket wire;
		wire.name = QStringLiteral( "LODI occluder boxes" );
		wire.matString = QStringLiteral( "#FF00FF00" );
		for ( const LodiOccluder & o : table.occluders ) {
			const int cellX = int( std::floor( o.centre[0] / 4096.0f ) );
			const int cellY = int( std::floor( o.centre[1] / 4096.0f ) );
			if ( cellX < spec.x0 || cellX > spec.x1 || cellY < spec.y0 || cellY > spec.y1 )
				continue;
			float quat[4], m[9];
			lodiUnpackRotation( o.rot, quat, m );
			const float t = qMax( 2.0f,
				qMin( qMin( o.halfExtent[0], o.halfExtent[1] ), o.halfExtent[2] ) * 0.04f );
			appendWireBox( wire.verts, wire.tris,
				Vector3( o.centre[0] - origin[0], o.centre[1] - origin[1],
					o.centre[2] - origin[2] ),
				Vector3( o.halfExtent[0], o.halfExtent[1], o.halfExtent[2] ), m, t );
			boxes++;
		}
		if ( !emitBucket( nif, iRoot, wire, origin, shapes, verts, error ) )
			return false;
		note << QString( "WW_LODI_BOXES=1: %1 of the file's %2 occluder boxes are in this "
				"region, drawn as wire boxes" ).arg( boxes ).arg( table.occluders.size() );
	}

	if ( dump.device() ) {
		dump.flush();
		dumpFile.close();
		note << QString( "instance census written to %1 (%2 rows)" ).arg( dumpPath ).arg( placed );
	}

	/* THE NOTE LINE, the way WW_LODL_AO writes one: the channel's name, the file
	 * it came out of, how many values went into the buffer, and their min, max
	 * and mean READ BACK from the buffer -- so a picture is never the only
	 * evidence and a channel that is constant says so in words as well as in
	 * grey (root MISTAKES 05:1x: a dial whose two settings render identically is
	 * not wired). An unknown name refuses BY NAME and changes nothing. */
	if ( !channelGiven.isEmpty() && channel == LodlChannel::None )
		note << QString( "WW_LODL_CHANNEL: REFUSED \"%1\" -- no such channel; the scene is "
				"the default one. Known names: %2" )
			.arg( channelGiven ).arg( lodlChannelNames() );
	if ( skyPerVertex )
		note << QString( "WW_LODL_CHANNEL=sky: the PER-VERTEX SKY STREAM (.lodi v7 0x110) from %1, "
				"%L2 bytes over %L3 slices, %L4 values read; %5%6" )
			.arg( QFileInfo( lodiPath ).fileName() )
			.arg( skyVertBytes ).arg( skyVertSlices ).arg( chanCount )
			.arg( chanCount == 0 ? QStringLiteral( "nothing was drawn" )
				: chanLo == chanHi
					? QString( "constant %1" ).arg( chanLo )
					: QString( "min %1, max %2, mean %3" ).arg( chanLo ).arg( chanHi )
						.arg( chanSum / double( chanCount ), 0, 'f', 3 ) )
			.arg( skyVertMismatch ? QString( "; %1 slice(s) did not match the drawn mesh and drew the 0x11 byte" )
				.arg( skyVertMismatch ) : QString() );
	if ( channel == LodlChannel::Sky && !skyPerVertex )
		note << QString( "WW_LODL_CHANNEL=sky: no per-vertex stream in %1 (a version-%2 file), so the "
				"PLACEMENT BYTE (.lodi 0x11) served it" )
			.arg( QFileInfo( lodiPath ).fileName() ).arg( ih.version );
	if ( channel == LodlChannel::Scrappable && ih.version < LODI_VERSION_SCRAPPABLE )
		note << QString( "WW_LODL_CHANNEL=scrappable: %1 is a version-%2 file and version %3 is the one "
				"that carries the bit -- every placement is drawn grey because the FILE says nothing, "
				"not because nothing is scrappable; re-bake with --scrappable" )
			.arg( QFileInfo( lodiPath ).fileName() ).arg( ih.version ).arg( LODI_VERSION_SCRAPPABLE );
	if ( channel == LodlChannel::Identity )
		note << ( groupDrawn
			? QString( "WW_LODL_CHANNEL=identity: the GROUP (.lodi v7 0x100) on %L1 placements, "
					"%L2 groups in the file" ).arg( groupDrawn ).arg( ih.groupCount )
			: QString( "WW_LODL_CHANNEL=identity: no group table in %1 (a version-%2 file), so the "
					"PLACEMENT IDENTITY served it on %L3 placements" )
				.arg( QFileInfo( lodiPath ).fileName() ).arg( ih.version ).arg( groupFellBack ) );
	if ( objectChannel )
		note << QString( "WW_LODL_CHANNEL=%1: %2 from %3, %L4 %5 read; %6" )
			.arg( lodlChannelName( channel ) )
			.arg( channel == LodlChannel::Sway
					? QStringLiteral( "the per-vertex wind-sway weight (.lodo 0x0E)" )
				: channel == LodlChannel::SelfAo
					? QStringLiteral( "the per-vertex self-AO (.lodo 0x0F)" )
				: channel == LodlChannel::Identity
					? QStringLiteral( "the group, hashed to colour (the stock channel 1 palette)" )
				: channel == LodlChannel::Placement
					? QStringLiteral( "the placement identity, hashed to colour (the stock channel 1 palette)" )
				: channel == LodlChannel::IdentityRaw
					? QStringLiteral( "the placement identity's low byte as grey" )
				: channel == LodlChannel::Sky
					? QStringLiteral( "the per-placement sky visibility (.lodi 0x11)" )
				: channel == LodlChannel::Ground
					? QStringLiteral( "the per-placement ground-contact blend (.lodi 0x12)" )
				: channel == LodlChannel::Scrappable
					? QStringLiteral( "the workshop-scrappable bit, magenta = scrappable, grey = stays "
						"(.lodi v9 0x14 bit 6); the read-back below is 1s and 0s, so its MEAN is the share" )
					: QStringLiteral( "the per-placement tree seed, hashed to colour; 0 = not a tree = black (.lodi 0x13)" ) )
			.arg( ( channel == LodlChannel::Sway || channel == LodlChannel::SelfAo )
				? QFileInfo( lodoPath ).fileName() : QFileInfo( lodiPath ).fileName() )
			.arg( chanCount )
			.arg( ( channel == LodlChannel::Sway || channel == LodlChannel::SelfAo )
				? QStringLiteral( "vertices" ) : QStringLiteral( "placements" ) )
			.arg( chanCount == 0 ? QStringLiteral( "nothing was drawn" )
				: chanLo == chanHi
					? QString( "constant %1" ).arg( chanLo )
					: QString( "min %1, max %2, mean %3" ).arg( chanLo ).arg( chanHi )
						.arg( chanSum / double( chanCount ), 0, 'f', 3 ) );
	if ( wantAo && !table.vertexAoFirst.empty() )
		note << QString( "WW_LODL_AO: .lodi v6 scene vertex AO used on %1 placements (%L2 bytes, mean %3), "
				"%4 slices did not match the drawn mesh" )
			.arg( vaoUsed ).arg( vaoBytes ).arg( vaoBytes ? vaoSum / double( vaoBytes ) : 255.0, 0, 'f', 1 )
			.arg( vaoMismatch );
	if ( wantAo )
		note << QString( "WW_LODL_AO: self-AO x placement AO as vertex colour; %1 placements measured "
				"(mean %2), %3 unmeasured (drawn open)" )
			.arg( aoMeasured ).arg( aoMeasured ? aoSum / double( aoMeasured ) : 255.0, 0, 'f', 1 )
			.arg( aoUnmeasured );
	note << QString( "%1 placements read, %2 drawn (%3 outside the region, %4 with no mesh, "
			"%5 with no geometry at this level); %6 bases, %L7 buckets" )
		.arg( read ).arg( placed ).arg( outsideRegion ).arg( noMesh ).arg( noGeometry )
		.arg( basesSeen.size() ).arg( buckets.size() );
	note << QString( "%1 shapes, %L2 vertices; read and decoded at %3 ms, built at %4 ms" )
		.arg( shapes ).arg( verts ).arg( msRead ).arg( timer.elapsed() );

	if ( notes )
		*notes = ( notes->isEmpty() ? QString() : *notes + QStringLiteral( "\n" ) )
			+ note.join( QStringLiteral( "\n" ) );
	if ( error )
		error->clear();
	return true;
}


bool nifCreateLodiObjectScene( NifModel * nif, const QString & lodiPath,
	const LodiSceneSpec & spec, QString * error, QString * notes )
{
	if ( !nif ) {
		if ( error )
			*error = QStringLiteral( "no model" );
		return false;
	}
	if ( !nif->createNew( 0x14020007, 12, 130 ) ) {
		if ( error )
			*error = QStringLiteral( "could not create a Fallout 4 document" );
		return false;
	}

	nif->holdUpdates( true );
	QModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
	nif->set<QString>( iRoot, "Name",
		QString( "%1 objects" ).arg( QFileInfo( lodiPath ).completeBaseName() ) );
	nif->set<quint32>( iRoot, "Flags", 14 );
	nif->set<float>( iRoot, "Scale", 1.0f );

	const bool ok = nifAppendLodiObjects( nif, iRoot, lodiPath, spec, error, notes );

	nif->holdUpdates( false );
	nif->updateModel();
	return ok;
}

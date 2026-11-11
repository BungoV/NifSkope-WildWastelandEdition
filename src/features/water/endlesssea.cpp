#include "features/water/endlesssea.h"
#include "wwswitch.h"

#include "features/far-lod/lodtfile.h"
#include "features/water/cellwater.h"
#include "features/water/esmwater.h"
#include "gl/celllooks.h"
#include "gl/glnode.h"
#include "gl/glscene.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{

constexpr int kSegments = 128;		// one segment count for every ring: each ring shares its edges with the next
constexpr double kFirstRing = 256.0;	// the first ring round the centre, world units
constexpr float kSnap = 2048.0f;	// the sheet moves in whole steps of this (world units)
constexpr double kEarthU = 6371000.0 * 70.0;	// the earth's radius in world units (70 u a metre)

float envFloat( const QByteArray & value, float def )
{
	bool ok = false;
	const float v = value.trimmed().toFloat( &ok );
	return ok ? v : def;
}

// Node::local is protected; the sheet's follow writes it as the controllers do
struct Peek : Node
{
	static Transform Node::* loc() { return &Peek::local; }
};

// the sea: the largest sea body that is not tiny; its height is the sea level (lane SKYSEA1: also its WATR form)
quint32 seaBody( const LodtFile & f, float & seaZ, quint32 * form = nullptr )
{
	quint32 seaId = 0, seaArea = 0;
	for ( int id = 1; id <= f.bodyCount(); id++ ) {
		LodtWaterBody B;
		if ( f.waterBody( id, B ) && B.cls == 0 && !( B.flags & 16 ) && B.area > seaArea ) {
			seaArea = B.area;
			seaId = quint32( id );
			seaZ = B.waterHeight;
			if ( form )
				*form = B.watrForm;
		}
	}
	return seaId;
}

}	// namespace

bool wwEndlessSeaOn()
{
	WW_SWITCH_STATIC( QByteArray, red, wwEnv( "WW_ENDLESS_SEA_RED" ).trimmed() );
	return red != "off" && wwLookOn( WwLook::EndlessSea );
}

quint32 wwEndlessSeaBodyForm( const QString & lodlPath )
{
	WW_SWITCH_STATIC( QByteArray, red, wwEnv( "WW_ENDLESS_SEA_RED" ).trimmed() );
	LodtFile f;
	if ( red == "worldrec" || !f.open( lodlPath, nullptr ) )
		return 0;
	float z = 0.0f;
	quint32 form = 0;
	return seaBody( f, z, &form ) ? form : 0;
}

float wwEndlessSeaOverlap()
{
	WW_SWITCH_STATIC( QByteArray, red, wwEnv( "WW_ENDLESS_SEA_RED" ).trimmed() );
	return red == "seamcut" ? 0.0f : 4096.0f;
}

float wwEndlessSeaSeam()
{
	WW_SWITCH_STATIC( QByteArray, seam, wwEnv( "WW_ENDLESS_SEA_SEAM" ).trimmed() );
	return std::max( envFloat( seam, 98304.0f ), 0.0f );
}

QString wwEndlessSeaAppend( NifModel * nif, const QModelIndex & parent, const WwWaterRecord & rec, float defaultZ,
	const QString & lodlPath, const int * drawnCells )
{
	QElapsedTimer clock;
	clock.start();
	if ( !nif || !parent.isValid() )
		return QStringLiteral( "endless sea: NONE (no far node)" );
	LodtFile f;
	QString err;
	if ( !f.open( lodlPath, &err ) )
		return QStringLiteral( "endless sea: NONE -- %1" ).arg( err );
	f.setBlockCacheSize( 256 );
	const int minX = f.cellMinX(), minY = f.cellMinY(), maxX = f.cellMaxX(), maxY = f.cellMaxY();
	const float box[4] = { float( minX ) * 4096.0f, float( minY ) * 4096.0f, float( maxX + 1 ) * 4096.0f,
		float( maxY + 1 ) * 4096.0f };
	float seaZ = defaultZ;
	const quint32 seaId = seaBody( f, seaZ );
	// the coverage census: cells of the box with baked water (any of 4 x 4 body samples wet) and without
	int wet = 0, dry = 0;
	const int bs = f.bodyIdSamples();
	if ( bs > 0 ) {
		for ( int cy = 0; cy <= maxY - minY; cy++ ) {
			for ( int cx = 0; cx <= maxX - minX; cx++ ) {
				bool w = false;
				for ( int s = 0; s < 16 && !w; s++ )
					w = f.bodyIdAt( cx * bs + ( s & 3 ) * bs / 4 + bs / 8, cy * bs + ( s >> 2 ) * bs / 4 + bs / 8 ) != 0;
				( w ? wet : dry )++;
			}
		}
	}

	// the rings: radii grow by 1 + 2 pi / segments (cells about square), the last one clamped to the rim
	const float farU = std::clamp( envFloat( wwEnv( "WW_ENDLESS_SEA_FAR" ), 4194304.0f ), 65536.0f, 33554432.0f );
	// DEBUGVIEW1 item 5: the sheet bends with the earth by default, so its rim meets the curved horizon (red WW_ENDLESS_SEA_CURVE=0: flat)
	const bool curve = wwEnv( "WW_ENDLESS_SEA_CURVE" ).trimmed() != "0";
	const double grow = 1.0 + 2.0 * M_PI / kSegments;
	std::vector<double> radii;
	for ( double r = kFirstRing; ; r *= grow ) {
		if ( r >= double( farU ) ) {
			radii.push_back( double( farU ) );
			break;
		}
		radii.push_back( r );
	}
	const int R = int( radii.size() );
	const int nv = 1 + R * kSegments;
	if ( nv > 65535 )
		return QStringLiteral( "endless sea: NONE (%1 vertices: over one shape's 65535)" ).arg( nv );
	auto zAt = [&]( double r ) { return float( double( seaZ ) - ( curve ? r * r / ( 2.0 * kEarthU ) : 0.0 ) ); };
	QVector<Triangle> tris;
	tris.reserve( kSegments * ( 2 * R - 1 ) );
	auto vid = [&]( int ring, int seg ) { return quint16( 1 + ring * kSegments + ( seg % kSegments ) ); };
	for ( int j = 0; j < kSegments; j++ )
		tris.append( Triangle( 0, vid( 0, j ), vid( 0, j + 1 ) ) );	// CCW seen from above, as the far water
	for ( int i = 0; i + 1 < R; i++ ) {
		for ( int j = 0; j < kSegments; j++ ) {
			tris.append( Triangle( vid( i, j ), vid( i + 1, j ), vid( i + 1, j + 1 ) ) );
			tris.append( Triangle( vid( i, j ), vid( i + 1, j + 1 ), vid( i, j + 1 ) ) );
		}
	}

	// the shape, written as the far water's (btdterrain.cpp addWaterShapes): position, UV, normal frame, colour
	BSVertexDesc desc( 0x0041B00000650407ULL );
	desc.SetFlag( VertexFlags::VF_COLORS );
	desc.ResetAttributeOffsets( 130 );
	const int stride = int( desc.GetVertexSize() );
	QModelIndex iShape = nif->insertNiBlock( QStringLiteral( "BSTriShape" ) );
	const int block = nif->getBlockNumber( iShape );
	nif->set<QString>( iShape, "Name", QStringLiteral( "FarLOD sea" ) );	// "FarLOD": out of the scene bounds (glscene.cpp)
	nif->set<quint32>( iShape, "Flags", 14 );
	nif->set<float>( iShape, "Scale", 1.0f );
	nif->set<Vector3>( iShape, "Translation", Vector3( 0.0f, 0.0f, 0.0f ) );
	nif->set<BSVertexDesc>( iShape, "Vertex Desc", BSVertexDesc( desc.Value() ) );
	nif->set<quint32>( iShape, "Num Vertices", quint32( nv ) );
	nif->set<quint32>( iShape, "Num Triangles", quint32( tris.size() ) );
	nif->set<quint32>( iShape, "Data Size", quint32( nv * stride + tris.size() * 6 ) );
	nif->setState( BaseModel::Processing );
	QModelIndex iVertexData = nif->getIndex( iShape, "Vertex Data" );
	nif->updateArraySize( iVertexData );
	const Vector3 nrm( 0.0f, 0.0f, 1.0f ), tan( 1.0f, 0.0f, 0.0f );
	const Vector3 bit = Vector3::crossproduct( nrm, tan );
	const ByteColor4 col( FloatVector4( float( 0x2E ) / 255.0f, float( 0x52 ) / 255.0f, float( 0x6B ) / 255.0f, 1.0f ) );	// the far water's colour
	for ( int v = 0; v < nv; v++ ) {
		Vector3 p( 0.0f, 0.0f, zAt( 0.0 ) );
		if ( v > 0 ) {
			const int i = ( v - 1 ) / kSegments, j = ( v - 1 ) % kSegments;
			const double a = 2.0 * M_PI * double( j ) / kSegments;
			p = Vector3( float( radii[size_t( i )] * std::cos( a ) ), float( radii[size_t( i )] * std::sin( a ) ),
				zAt( radii[size_t( i )] ) );
		}
		QModelIndex row = nif->index( v, 0, iVertexData );
		nif->set<Vector3>( row, "Vertex", p );
		nif->set<HalfVector2>( row, "UV", HalfVector2( Vector2( 0.0f, 0.0f ) ) );
		nif->set<ByteVector3>( row, "Normal", ByteVector3( nrm ) );
		nif->set<ByteVector3>( row, "Tangent", tan );
		nif->set<float>( row, "Bitangent X", bit[0] );
		nif->set<float>( row, "Bitangent Y", bit[1] );
		nif->set<float>( row, "Bitangent Z", bit[2] );
		nif->set<ByteColor4>( row, "Vertex Colors", col );
	}
	QModelIndex iTriangles = nif->getIndex( iShape, "Triangles" );
	nif->updateArraySize( iTriangles );
	nif->setArray<Triangle>( iTriangles, tris );
	QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
	if ( iBound.isValid() ) {
		nif->set<Vector3>( iBound, "Center", Vector3( 0.0f, 0.0f, seaZ ) );
		nif->set<float>( iBound, "Radius", farU );
	}
	nif->restoreState();
	QModelIndex iShader = nif->insertNiBlock( QStringLiteral( "BSLightingShaderProperty" ) );
	QModelIndex iTextures = nif->insertNiBlock( QStringLiteral( "BSShaderTextureSet" ) );
	nif->setLink( iShader, "Texture Set", nif->getBlockNumber( iTextures ) );
	nif->set<uint>( iTextures, "Num Textures", 10 );
	nif->updateArraySize( iTextures, "Textures" );
	QModelIndex iTexArray = nif->getIndex( iTextures, "Textures" );
	nif->set<QString>( nif->getIndex( iTexArray, 0 ), QStringLiteral( "#FFFFFFFF" ) );
	nif->set<QString>( nif->getIndex( iTexArray, 1 ), QStringLiteral( "#FFFF8080" ) );
	const quint32 sf2 = nif->get<quint32>( iShader, "Shader Flags 2" );
	nif->set<quint32>( iShader, "Shader Flags 2", sf2 | 0x20u );
	nif->setLink( iShape, "Shader Property", nif->getBlockNumber( iShader ) );
	addLink( nif, parent, QStringLiteral( "Children" ), block );

	wwCellWaterShape( nif, block, rec, false );
	// lane STREAMGN1: the sheet's inner edge = the bake box cut to the far water rings' footprint (header)
	float seam[4] = { box[0], box[1], box[2], box[3] };
	const bool cut = drawnCells && wwEnv( "WW_ENDLESS_SEA_RED" ).trimmed() != "bakebox"
		&& drawnCells[0] <= drawnCells[2] && drawnCells[1] <= drawnCells[3];
	if ( cut ) {
		seam[0] = std::max( box[0], float( drawnCells[0] ) * 4096.0f );
		seam[1] = std::max( box[1], float( drawnCells[1] ) * 4096.0f );
		seam[2] = std::min( box[2], float( drawnCells[2] + 1 ) * 4096.0f );
		seam[3] = std::min( box[3], float( drawnCells[3] + 1 ) * 4096.0f );
	}
	std::fprintf( stderr, "endless sea: inner edge %s: cells %d,%d..%d,%d (bake box %d,%d..%d,%d)\n",
		cut ? "the far water rings' footprint" : drawnCells ? "the bake box (RED bakebox)" : "the bake box (no rings given)",
		int( std::floor( seam[0] / 4096.0f ) ), int( std::floor( seam[1] / 4096.0f ) ),
		int( std::floor( seam[2] / 4096.0f ) ) - 1, int( std::floor( seam[3] / 4096.0f ) ) - 1, minX, minY, maxX, maxY );
	std::fflush( stderr );
	wwCellWaterEndless( nif, block, seam, seaZ, farU, seaId );
	const double toKm = 1.0 / 70000.0;
	return QStringLiteral( "endless sea: rings %1 (%2 u .. %3 u, x%4 a ring, %5 segments, %6 vertices, %7 triangles), "
		"far sheet %3 u (%8 km)%9, seam band %10 u (%11 km), coverage mask: bake box cells %12,%13..%14,%15 = %16 cells, "
		"%17 with baked water, %18 without (the sheet draws only outside the box), sea body %19 at z %20 (WRLD default %21), "
		"snap %22 u, built in %23 ms" )
		.arg( R ).arg( kFirstRing, 0, 'f', 0 ).arg( double( farU ), 0, 'f', 0 ).arg( grow, 0, 'f', 4 ).arg( kSegments )
		.arg( nv ).arg( tris.size() ).arg( double( farU ) * toKm, 0, 'f', 1 )
		.arg( curve ? QStringLiteral( ", curved (rim %1 u down; red WW_ENDLESS_SEA_CURVE=0: flat)" ).arg( double( farU ) * farU / ( 2.0 * kEarthU ), 0, 'f', 0 )
			: QStringLiteral( ", flat (red WW_ENDLESS_SEA_CURVE=0)" ) )
		.arg( double( wwEndlessSeaSeam() ), 0, 'f', 0 ).arg( double( wwEndlessSeaSeam() ) * toKm, 0, 'f', 2 )
		.arg( minX ).arg( minY ).arg( maxX ).arg( maxY ).arg( ( maxX - minX + 1 ) * ( maxY - minY + 1 ) )
		.arg( wet ).arg( dry ).arg( seaId ).arg( double( seaZ ), 0, 'f', 1 ).arg( double( defaultZ ), 0, 'f', 1 )
		.arg( double( kSnap ), 0, 'f', 0 ).arg( clock.elapsed() );
}

namespace
{

/* MODULES1 10b-6: the sheet's own draw counter. One sample query a frame round the first draw of the sheet; its
 * result is read without waiting on a later frame. Red: WW_ENDLESS_SEA_RED=nocount (no query, no line). */
struct SeaDraw
{
	const void * nif = nullptr;
	int block = -1;		// the sheet's block this frame (-1 none)
	unsigned int query = 0;
	bool open = false, pending = false, busy = false;
	int drawsFrame = 0;	// draws of the sheet since the last frame step
	long long trisFrame = 0, trisPending = 0;
	int drawsPending = 0;
	long long saidTris = -2, saidPix = -2;
	int saidState = -1;	// 0 not sent, 1 sent and hidden, 2 sent and drawn, 3 sent, counter busy
};

SeaDraw & seaDraw()
{
	static SeaDraw d;
	return d;
}

constexpr unsigned int kSamplesPassed = 0x8914, kCurrentQuery = 0x8865, kQueryResult = 0x8866, kQueryReady = 0x8867;

void seaDrawSay( SeaDraw & d, int state, long long tris, long long pix, int draws )
{
	const bool pixMoved = pix >= 0 && ( d.saidPix < 0 || std::llabs( pix - d.saidPix ) * 10 > std::max( d.saidPix, 1LL ) );
	if ( state == d.saidState && tris == d.saidTris && !pixMoved )
		return;
	d.saidState = state;
	d.saidTris = tris;
	d.saidPix = pix;
	static const char * const names[4] = { "not sent (the sheet did not reach its draw call this frame)",
		"sent, hidden (no pixel passed the depth test)", "sent, drawn", "sent, pixels not counted (the pass census holds the sample counter)" };
	qInfo().noquote() << QStringLiteral( "endless sea draw: %1; triangles sent %2 in %3 draws, pixels passed %4 (first draw of the frame)" )
		.arg( QLatin1String( names[state] ) ).arg( tris ).arg( draws )
		.arg( pix >= 0 ? QString::number( pix ) : QStringLiteral( "n/a" ) );
}

//! Once a frame: reads the last query when it is ready and says what changed.
void seaDrawStep( SeaDraw & d, const void * nif, int block )
{
	QOpenGLContext * ctx = QOpenGLContext::currentContext();
	QOpenGLExtraFunctions * f = ctx ? ctx->extraFunctions() : nullptr;
	if ( d.pending && f ) {
		unsigned int ready = 0;
		f->glGetQueryObjectuiv( d.query, kQueryReady, &ready );
		if ( ready ) {
			unsigned int pix = 0;
			f->glGetQueryObjectuiv( d.query, kQueryResult, &pix );
			d.pending = false;
			seaDrawSay( d, pix > 0 ? 2 : 1, d.trisPending, qint64( pix ), d.drawsPending );
		}
	}
	if ( d.block >= 0 && d.nif == nif ) {
		if ( d.drawsFrame == 0 )
			seaDrawSay( d, 0, 0, -1, 0 );
		else if ( d.busy )
			seaDrawSay( d, 3, d.trisFrame, -1, d.drawsFrame );
		else if ( !d.pending && d.query ) {
			d.pending = true;
			d.trisPending = d.trisFrame;
			d.drawsPending = d.drawsFrame;
		}
	}
	d.nif = nif;
	d.block = block;
	d.drawsFrame = 0;
	d.trisFrame = 0;
	d.busy = false;
}

}

void wwEndlessSeaDrawBegin( Scene * scene, int block, long long triangles )
{
	SeaDraw & d = seaDraw();
	if ( d.block < 0 || block != d.block || !scene || scene->nifModel != d.nif )
		return;
	d.drawsFrame++;
	d.trisFrame += triangles;
	if ( d.drawsFrame > 1 || d.pending || wwEnv( "WW_ENDLESS_SEA_RED" ).trimmed() == "nocount" )
		return;
	QOpenGLContext * ctx = QOpenGLContext::currentContext();
	QOpenGLExtraFunctions * f = ctx ? ctx->extraFunctions() : nullptr;
	if ( !f )
		return;
	int cur = 0;
	f->glGetQueryiv( kSamplesPassed, kCurrentQuery, &cur );
	if ( cur != 0 ) {	// the pass census (cellpasstime.cpp) counts samples now: two cannot be open at once
		d.busy = true;
		return;
	}
	if ( !d.query )
		f->glGenQueries( 1, &d.query );
	f->glBeginQuery( kSamplesPassed, d.query );
	d.open = true;
}

void wwEndlessSeaDrawEnd()
{
	SeaDraw & d = seaDraw();
	if ( !d.open )
		return;
	d.open = false;
	if ( QOpenGLContext * ctx = QOpenGLContext::currentContext() )
		ctx->extraFunctions()->glEndQuery( kSamplesPassed );
}

void wwEndlessSeaFollow( Scene * scene, const Transform & view )
{
	wwCellWaterTierFrame();	// DEBUGVIEW1 item 5: last frame's water draws per tier, on change
	if ( !scene || !scene->nifModel || !wwEndlessSeaOn() ) {
		seaDrawStep( seaDraw(), nullptr, -1 );
		return;
	}
	float box[4], seaZ = 0.0f, reach = 0.0f;
	quint32 seaId = 0;
	const int b = wwCellWaterEndlessBlock( scene->nifModel, box, seaZ, reach, seaId );
	seaDrawStep( seaDraw(), scene->nifModel, b );
	if ( b < 0 )
		return;
	Node * n = scene->findNode( scene->nifModel, scene->nifModel->getBlockIndex( b ) );
	if ( !n )
		return;
	// the eye in world units (celllights.cpp wwCellCamDump)
	const float sc = view.scale != 0.0f ? view.scale : 1.0f;
	float e[3];
	for ( int k = 0; k < 3; k++ ) {
		float w = 0.0f;
		for ( int j = 0; j < 3; j++ )
			w -= view.rotation( j, k ) * view.translation[j];
		e[k] = w / sc;
	}
	const float tx = std::round( e[0] / kSnap ) * kSnap, ty = std::round( e[1] / kSnap ) * kSnap;
	/* MODULES1 R6-2: where the sea ends for this eye. A flat sheet's rim lies atan( h / rim ) under eye level,
	 * the sky starts at eye level: the line says how wide that strip is. Said when the height moves by a tenth. */
	{
		static float saidH = -1.0f;
		const float h = std::max( e[2] - seaZ, 1.0f );
		if ( saidH < 0.0f || std::fabs( h - saidH ) > 0.1f * saidH ) {
			saidH = h;
			const double rim = double( reach );
			qInfo().noquote() << QStringLiteral( "endless sea: far water extent %1 u (%2 km) from the eye, eye %3 u over"
				" the sea; the rim lies %4 deg under eye level; the sea-level horizon of the earth is %5 u out (%6 deg under)" )
				.arg( rim, 0, 'f', 0 ).arg( rim / 70000.0, 0, 'f', 1 ).arg( double( h ), 0, 'f', 0 )
				.arg( std::atan2( double( h ), rim ) * 180.0 / M_PI, 0, 'f', 3 )
				.arg( std::sqrt( 2.0 * kEarthU * double( h ) ), 0, 'f', 0 )
				.arg( std::sqrt( 2.0 * double( h ) / kEarthU ) * 180.0 / M_PI, 0, 'f', 3 );
		}
	}
	Transform & loc = n->*Peek::loc();
	if ( loc.translation[0] != tx || loc.translation[1] != ty || loc.translation[2] != 0.0f ) {
		loc.translation = Vector3( tx, ty, 0.0f );
		scene->transformDirty = true;
	}
}

float wwEndlessSeaReach( Scene * scene )
{
	if ( !scene || !scene->nifModel || !wwEndlessSeaOn() )
		return 0.0f;
	float box[4], seaZ = 0.0f, reach = 0.0f;
	quint32 seaId = 0;
	return wwCellWaterEndlessBlock( scene->nifModel, box, seaZ, reach, seaId ) >= 0 ? reach + kSnap : 0.0f;
}

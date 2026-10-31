#include "cellprecip.h"

#include "wwswitch.h"
#include "recx.h"
#include "gl/celllights.h"
#include "gl/celllooks.h"
#include "gl/cellgrass.h"
#include "gl/cellhdr.h"
#include "gl/cellvolair.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "gl/precipmath.h"
#include "gl/precipspec.h"
#include "gl/renderer.h"
#include "gl/sunshadow.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

using namespace WwPrecip;

namespace
{

struct PcTex
{
	GLuint id = 0;
	int width = 0, height = 0, mips = 0;
	QString file;	// the file it came from; empty: none found (the soft-dot fallback)
};

struct PcLayer
{
	WwPrecipLayerSpec spec;
	Layer field;
	QString weather;	// its weather's EditorID
	QString source;		// "recx:<file>" or "record"
};

struct PcState
{
	QHash<QString, PcTex> tex;	// key: the texture path as linked
	QElapsedTimer clock;
	QElapsedTimer specAge;
	QString specKey;
	QVector<PcLayer> specCur, specOld;	// the current weather's layers, the leaving one's
	QStringList roots;
	QString rootsFor;
	QString said, echo = QStringLiteral( "precip: off" );
	int lastDrawn = 0;
	bool timePinned = false;
	GLuint vao = 0, vbo = 0;
	std::vector<float> buf;
};

PcState & st()
{
	static PcState s;
	return s;
}

constexpr int kFloats = 14;	// pos 3, colour 4, cell uv 2, angle 1, half size 1, velocity 3

int redFromEnv()
{
	WW_SWITCH_STATIC( QByteArray, r, wwEnv( "WW_CELL_PRECIP_RED" ).trimmed().toLower() );
	int bits = 0;
	for ( const QByteArray & t : r.split( ',' ) ) {
		const QByteArray w = t.trimmed();
		bits |= w == "constdensity" ? RedConstDensity : w == "nowrap" ? RedNoWrap : w == "onespeed" ? RedOneSpeed
			: w == "nowind" ? RedNoWind : 0;
	}
	return bits;
}

//! a linked texture (under Textures\) looked up loose in the roots, first root wins
QString findTexture( const QStringList & roots, QString rel )
{
	rel.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	while ( rel.startsWith( QLatin1Char( '/' ) ) )
		rel.remove( 0, 1 );
	if ( rel.startsWith( QLatin1String( "textures/" ), Qt::CaseInsensitive ) )
		rel.remove( 0, 9 );
	for ( const QString & r : roots ) {
		const QString p = r + QStringLiteral( "/Textures/" ) + rel;
		if ( QFileInfo::exists( p ) )
			return p;
	}
	return QString();
}

//! a block-compressed DDS into a GL texture (BC1 / BC3 / BC7, sRGB kept); false: none (the caller draws soft dots)
bool loadDds( NifSkopeOpenGLContext::GLFunctions * fn, const QString & path, PcTex & t )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return false;
	const QByteArray b = f.readAll();
	DdsInfo info;
	if ( !ddsParse( reinterpret_cast<const unsigned char *>( b.constData() ), size_t( b.size() ), info ) )
		return false;
	GLenum fmt = 0;
	switch ( info.dxgi ) {
	case 71: fmt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
	case 72: fmt = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT; break;
	case 77: fmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
	case 78: fmt = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT; break;
	case 98: fmt = GL_COMPRESSED_RGBA_BPTC_UNORM; break;
	case 99: fmt = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM; break;
	default: return false;
	}
	while ( fn->glGetError() != GL_NO_ERROR ) {}	// an earlier pass's error is not this load's
	GLuint id = 0;
	fn->glGenTextures( 1, &id );
	fn->glActiveTexture( GL_TEXTURE0 );
	fn->glBindTexture( GL_TEXTURE_2D, id );
	size_t off = info.dataOffset;
	for ( int m = 0; m < info.mips; m++ ) {
		const size_t n = ddsMipBytes( info.width, info.height, m, info.blockBytes );
		fn->glCompressedTexImage2D( GL_TEXTURE_2D, m, fmt, std::max( 1, info.width >> m ), std::max( 1, info.height >> m ), 0,
			GLsizei( n ), b.constData() + off );
		off += n;
	}
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0 );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, info.mips - 1 );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, info.mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	if ( fn->glGetError() != GL_NO_ERROR ) {
		fn->glDeleteTextures( 1, &id );
		return false;
	}
	t.id = id;
	t.width = info.width;
	t.height = info.height;
	t.mips = info.mips;
	t.file = path;
	return true;
}

//! one weather's layers: its .recx block, else its record's class flags
QVector<PcLayer> layersOf( const QStringList & roots, const QStringList & files, quint32 formID, const QString & edid,
	const QString & owner, unsigned classFlags, int spgdType, double spgdDensity, int spgdRed, QStringList & notes )
{
	QVector<PcLayer> out;
	const quint32 lid = ( formID >> 24 ) == 0xFEu ? ( formID & 0xFFFu ) : ( formID & 0xFFFFFFu );
	const WwRecxHit h = wwRecxFind( roots, files, QStringLiteral( "Weathers" ), QStringLiteral( "weather" ), owner, lid, edid,
		QStringLiteral( "precipitation" ), true );
	QVector<WwPrecipLayerSpec> specs;
	QString source;
	if ( h.ok ) {
		QStringList n;
		specs = wwPrecipParse( h.block, n );
		source = QStringLiteral( "recx:" ) + h.file.section( QLatin1Char( '/' ), -3 );
		for ( const QString & l : n )
			notes << QStringLiteral( "%1 /precipitation/%2" ).arg( h.file.section( QLatin1Char( '/' ), -3 ), l );
	} else {
		// lane RAINFOG2 (PRECIPFLAG1): the precipitation shader's layer (MNAM -> SPGD type, rate x density / 7); red
		// WW_PRECIP_SPGD_RED=flags: the class flags (WEATHERLOOK1 B), =rate: the type's default rate
		specs = wwPrecipFromSpgd( spgdType, spgdDensity, classFlags, spgdRed );
		source = ( spgdRed & WwPrecip::SpgdRedFlags ) ? QStringLiteral( "record(flags 0x%1)" ).arg( classFlags, 2, 16, QLatin1Char( '0' ) )
			: QStringLiteral( "record(spgd %1 density %2)" ).arg( spgdType ).arg( spgdDensity );
	}
	for ( int i = 0; i < specs.size(); i++ ) {
		PcLayer L;
		L.spec = specs.at( i );
		L.weather = edid;
		L.source = source;
		L.field.type = L.spec.type;
		L.field.sizeMm = L.spec.sizeMm;
		L.field.drawScale = L.spec.drawScale;
		L.field.windShare = L.spec.windShare;
		L.field.spin = L.spec.spin;
		L.field.swayU = L.spec.sway;
		L.field.seed = hash32( formID * 2654435761u + quint32( i ) * 0x9E3779B9u + quint32( L.spec.type ) );
		out.append( L );
	}
	return out;
}

}	// namespace

QString wwCellPrecipEcho()
{
	return st().echo;
}

bool wwCellPrecipWantsFrames( Scene * scene )
{
	const PcState & s = st();
	return scene && wwLookOn( WwLook::Precip ) && s.lastDrawn > 0 && !s.timePinned;
}

int wwCellPrecipDraw( Scene * scene )
{
	PcState & s = st();
	s.lastDrawn = 0;
	if ( !wwLookOn( WwLook::Precip ) ) {
		s.echo = QStringLiteral( "precip: off" );
		return 0;
	}
	if ( !scene || !scene->renderer || !wwCellLightsWanted( scene ) || !wwCellHdrActive() )
		return 0;
	const WwCellLighting * CL = wwCellLightsFor( scene->nifModel );
	if ( CL && CL->interior ) {
		s.echo = QStringLiteral( "precip: none (interior)" );
		return 0;
	}
	redBits() = redFromEnv();

	// the weathers: the current one at pct, the one left behind at 1 - pct
	quint32 formID = 0;
	QString edid, owner, dataDir;
	QStringList files;
	if ( !wwLookdevWeatherId( formID, edid, owner, files, dataDir ) ) {
		s.echo = QStringLiteral( "precip: none (no weather)" );
		return 0;
	}
	unsigned flags = 0, thunder = 0;
	wwLookdevWeatherClass( flags, thunder );
	quint32 oldID = 0;
	QString oldEdid, oldOwner;
	unsigned oldFlags = 0;
	float pct = 1.0f;
	const bool leaving = wwLookdevWeatherLeaving( oldID, oldEdid, oldOwner, oldFlags, pct );
	const QString rk = files.join( QLatin1Char( ';' ) ) + QLatin1Char( '|' ) + dataDir;
	if ( rk != s.rootsFor ) {
		s.rootsFor = rk;
		s.roots = wwRecxRoots( files, dataDir );
	}
	// lane RAINFOG2: the precipitation shaders (MNAM -> SPGD) and the switch WW_PRECIP_SPGD / its red
	WW_SWITCH_STATIC( int, spgdRed, WwPrecip::spgdRedBits( wwEnv( "WW_PRECIP_SPGD" ).trimmed().toLower().constData(),
		wwEnv( "WW_PRECIP_SPGD_RED" ).trimmed().toLower().constData() ) );
	int ptype[2] = { -2, -2 };
	float pdens[2] = { -1.0f, -1.0f };
	unsigned pflags[2] = { 0u, 0u };
	bool pblend = false;
	float ppct = 1.0f;
	wwLookdevWeatherPrecip( ptype, pdens, pflags, pblend, ppct );
	const QString key = QStringLiteral( "%1|%2|%3|%4" ).arg( formID ).arg( leaving ? oldID : 0u ).arg( rk ).arg( spgdRed );
	if ( key != s.specKey || !s.specAge.isValid() || s.specAge.elapsed() > 500 ) {	// files re-read on a change (recx.h)
		QStringList notes;
		s.specCur = layersOf( s.roots, files, formID, edid, owner, flags, ptype[0], double( pdens[0] ), spgdRed, notes );
		s.specOld = leaving ? layersOf( s.roots, files, oldID, oldEdid, oldOwner, oldFlags, ptype[1], double( pdens[1] ), spgdRed, notes )
							: QVector<PcLayer>();
		static QSet<QString> warned;
		for ( const QString & n : notes )
			if ( !warned.contains( n ) ) {
				warned.insert( n );
				std::fprintf( stderr, "precip recx: %s\n", qPrintable( n ) );
			}
		s.specKey = key;
		s.specAge.start();
	}
	if ( s.specCur.isEmpty() && s.specOld.isEmpty() ) {
		s.echo = QStringLiteral( "precip: none (%1 has no layer)" ).arg( edid );
		if ( s.echo != s.said ) {
			s.said = s.echo;
			std::fprintf( stderr, "%s\n", qPrintable( s.echo ) );
		}
		return 0;
	}

	// the clock: a harness pin, else the wall clock
	WW_SWITCH_STATIC( QByteArray, timePin, wwEnv( "WW_CELL_PRECIP_TIME" ).trimmed() );
	bool okT = false;
	const double tPin = timePin.toDouble( &okT );
	s.timePinned = okT;
	if ( !s.clock.isValid() )
		s.clock.start();
	const double t = okT ? tPin : double( s.clock.nsecsElapsed() ) * 1e-9;

	// the wind: the weather's (blended on pct during a change), at the game's bearing + the range offset
	float w[5] = { 0, 0, 0, 0, 0 };
	double wv[2] = { 0.0, 0.0 };
	if ( wwLookdevWind( w ) )
		windVelocity( double( w[0] ), double( wwGameWindAngle( quint8( std::clamp( w[1], 0.0f, 255.0f ) ), w[4] ) ), wv );

	// the camera (world) and world -> view rows, as the far bulbs
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	double cam[3];
	for ( int j = 0; j < 3; j++ ) {
		float acc = 0.0f;
		for ( int k = 0; k < 3; k++ )
			acc -= vt.rotation( k, j ) * vt.translation[k];
		cam[j] = double( acc / sc );
	}

	// build every tier's particles into one buffer; a draw per (layer, tier)
	struct Run
	{
		int layer;	// index into all
		bool farT;
		int first, count;
	};
	QVector<const PcLayer *> all;
	QVector<double> weights;
	for ( const PcLayer & L : s.specCur ) {
		all.append( &L );
		weights.append( blendWeight( false, leaving ? double( pct ) : 1.0 ) );
	}
	for ( const PcLayer & L : s.specOld ) {
		all.append( &L );
		weights.append( blendWeight( true, double( pct ) ) );
	}
	QVector<Run> runs;
	s.buf.clear();
	QStringList census;
	for ( int li = 0; li < all.size(); li++ ) {
		const PcLayer & L = *all.at( li );
		const TypeInfo & ti = typeInfo( L.spec.type );
		const double dens = L.spec.densityM3();
		const Box nb = nearBox( dens ), fb = farBox( dens, nb );
		const int cells = std::max( 1, L.spec.cols * L.spec.rows );
		const float cu = 1.0f / float( std::max( 1, L.spec.cols ) ), cv = 1.0f / float( std::max( 1, L.spec.rows ) );
		long drawnL[2] = { 0, 0 };
		Layer field = L.field;
		prepare( field );	// the per-variant numbers once (precipmath.h; the same values as the slow path)
		for ( int tier = 0; tier < 2; tier++ ) {
			const Box & b = tier ? fb : nb;
			const long n = drawCount( b.count, weights.at( li ) );
			if ( n <= 0 )
				continue;
			const int first = int( s.buf.size() / kFloats );
			// one particle stands for `thin` (the geometry shader scales by the area, the fragment clamps at 1)
			const float alpha = float( b.thin );
			for ( long i = 0; i < n; i++ ) {
				const Particle p = particleAt( field, b, i, tier != 0, t, cam, wv );
				if ( p.inside )
					continue;
				const int cell = p.variant % cells;
				const float v[kFloats] = { float( p.pos[0] ), float( p.pos[1] ), float( p.pos[2] ), 1.0f, 1.0f, 1.0f, alpha,
					float( cell % std::max( 1, L.spec.cols ) ) * cu, float( cell / std::max( 1, L.spec.cols ) ) * cv,
					float( p.angle ), float( p.half ), float( p.vel[0] ), float( p.vel[1] ), float( p.vel[2] ) };
				s.buf.insert( s.buf.end(), v, v + kFloats );
			}
			const int count = int( s.buf.size() / kFloats ) - first;
			drawnL[tier] = b.count;	// the census says the allocated count (the drawn share moves every frame of a change)
			if ( count > 0 )
				runs.append( Run{ li, tier != 0, first, count } );
		}
		census << QStringLiteral( "%1(%2 from %3, w %4: %5/m3 size %6 mm fall %7 m/s, near %8 u x %9, far %10 u x %11 thin %12, tex %13)" )
			.arg( QLatin1String( ti.name ), L.weather, L.source ).arg( weights.at( li ), 0, 'f', 1 ).arg( dens, 0, 'g', 4 )
			.arg( L.spec.sizeMm, 0, 'g', 3 ).arg( fallSpeedMps( L.spec.type, L.spec.sizeMm ), 0, 'f', 2 )
			.arg( nb.side, 0, 'f', 0 ).arg( drawnL[0] ).arg( fb.side, 0, 'f', 0 ).arg( drawnL[1] ).arg( fb.thin, 0, 'f', 1 )
			.arg( L.spec.texture );
	}
	const QString echo = QStringLiteral( "precip: on, wind %1 u/s, red %2: %3" ).arg( std::hypot( wv[0], wv[1] ), 0, 'f', 1 )
		.arg( redBits() ).arg( census.join( QStringLiteral( "; " ) ) );
	s.echo = echo;
	if ( echo != s.said ) {
		s.said = echo;
		std::fprintf( stderr, "%s\n", qPrintable( echo ) );
	}
	if ( runs.isEmpty() )
		return 0;

	NifSkopeOpenGLContext::Program * prog = r->useProgram( "cell_precip.prog" );
	if ( !prog ) {
		static bool saidNo = false;
		if ( !saidNo ) {
			saidNo = true;
			std::fprintf( stderr, "precip: cell_precip.prog not loaded: nothing drawn\n" );
		}
		return 0;
	}
	// the cell's uniforms (fog first, as the particles), the sun's cascades, the air's medium for vpLight
	wwLookdevFogUniforms( scene );
	wwCellLightsUniforms( scene );
	wwSunShadowUniforms( scene );
	wwVolPartMediumUniforms( scene );
	prog->uni1f( "lightingInfluence", 1.0f );
	prog->uni1i( "fxLitMode", 1 );
	for ( int j = 0; j < 3; j++ )
		prog->uni4f_l( prog->uniLocation( "pcRow[%d]", j ), FloatVector4( vt.rotation( j, 0 ) * sc, vt.rotation( j, 1 ) * sc,
			vt.rotation( j, 2 ) * sc, vt.translation[j] ) );
	const auto & pm = r->globalUniforms->projectionMatrix;
	fn->glUniformMatrix4fv( prog->uniLocation( "pcProj" ), 1, GL_FALSE, &pm[0][0] );
	GLint vp[4] = { 0, 0, 1, 1 };
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	prog->uni1f( "pcFocal", pm[1][1] * 0.5f * float( vp[3] ) );
	prog->uni1f( "pcShutter", float( kShutter ) );
	prog->uni1f( "pcMinPx", 0.75f );
	prog->uni2f( "pcNear", 8.0f * sc, 40.0f * sc );	// game units: a flake fades out over the last 40 before the eye
	prog->uni1i( "pcTex", 0 );

	// the buffer: one upload a frame
	if ( !s.vao ) {
		fn->glGenVertexArrays( 1, &s.vao );
		fn->glGenBuffers( 1, &s.vbo );
		fn->glBindVertexArray( s.vao );
		fn->glBindBuffer( GL_ARRAY_BUFFER, s.vbo );
		const int comps[6] = { 3, 4, 2, 1, 1, 3 };
		size_t off = 0;
		for ( int a = 0; a < 6; a++ ) {
			fn->glEnableVertexAttribArray( GLuint( a ) );
			fn->glVertexAttribPointer( GLuint( a ), comps[a], GL_FLOAT, GL_FALSE, GLsizei( kFloats * sizeof( float ) ),
				reinterpret_cast<const void *>( off * sizeof( float ) ) );
			off += size_t( comps[a] );
		}
	}
	fn->glBindVertexArray( s.vao );
	fn->glBindBuffer( GL_ARRAY_BUFFER, s.vbo );
	fn->glBufferData( GL_ARRAY_BUFFER, GLsizeiptr( s.buf.size() * sizeof( float ) ), s.buf.data(), GL_STREAM_DRAW );

	GLboolean depthMask = GL_TRUE;
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	const bool wasBlend = fn->glIsEnabled( GL_BLEND ), wasCull = fn->glIsEnabled( GL_CULL_FACE );
	fn->glEnable( GL_DEPTH_TEST );
	fn->glDepthFunc( GL_LEQUAL );
	fn->glDepthMask( GL_FALSE );
	fn->glDisable( GL_CULL_FACE );
	fn->glEnable( GL_BLEND );
	fn->glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	// the HDR frame sorts pixels by the stencil (gl/cellhdr.h): 1 = linear light, tone-mapped once
	fn->glEnable( GL_STENCIL_TEST );
	fn->glStencilMask( 0x03 );
	fn->glStencilFunc( GL_ALWAYS, 1, 0xFF );
	fn->glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );

	int drawn = 0;
	for ( const Run & run : runs ) {
		const PcLayer & L = *all.at( run.layer );
		const TypeInfo & ti = typeInfo( L.spec.type );
		PcTex & tx = s.tex[L.spec.texture];
		if ( !tx.id && tx.file.isEmpty() ) {
			const QString path = findTexture( s.roots, L.spec.texture );
			if ( path.isEmpty() || !loadDds( fn, path, tx ) ) {
				tx.file = QStringLiteral( "-" );	// tried: the soft-dot fallback from now on
				std::fprintf( stderr, "precip: texture %s %s: soft dots instead\n", qPrintable( L.spec.texture ),
					path.isEmpty() ? "not found loose in the .recx roots" : "not a BC1/BC3/BC7 DDS" );
			} else {
				std::fprintf( stderr, "precip: texture %s <- %s (%dx%d, %d mips)\n", qPrintable( L.spec.texture ), qPrintable( path ),
					tx.width, tx.height, tx.mips );
			}
		}
		const bool hasTex = tx.id != 0;
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_2D, tx.id );
		const int cols = std::max( 1, L.spec.cols ), rows = std::max( 1, L.spec.rows );
		const float cellPx = hasTex ? float( tx.width ) / float( cols ) : 1.0f;
		// a texture the type did not ship: 16 px a cell is the deepest mip read (the snow_fine rule); the type's own
		// atlas: its measured cap (precipmath.h maxMip)
		const bool ownAtlas = L.spec.texture.compare( QLatin1String( ti.texture ), Qt::CaseInsensitive ) == 0;
		const float maxLod = hasTex ? std::min( float( tx.mips - 1 ), ownAtlas ? float( ti.maxMip )
			: std::max( 0.0f, std::log2( cellPx / 16.0f ) ) ) : 0.0f;
		const float oneLod = hasTex ? std::min( float( tx.mips - 1 ), std::max( 0.0f, std::log2( cellPx ) ) ) : 0.0f;
		prog->uni4f( "pcAtlas", FloatVector4( 1.0f / float( cols ), 1.0f / float( rows ), maxLod, oneLod ) );
		prog->uni2f( "pcInset", hasTex ? 0.5f * std::exp2( maxLod ) / float( tx.width ) : 0.0f,
			hasTex ? 0.5f * std::exp2( maxLod ) / float( tx.height ) : 0.0f );
		prog->uni1b( "pcHasTex", hasTex );
		prog->uni1i( "pcFar", run.farT ? 1 : 0 );
		prog->uni1f( "pcTrans", float( L.spec.translucency ) );
		fn->glDrawArrays( GL_POINTS, GLint( run.first ), GLsizei( run.count ) );
		drawn += run.count;
	}
	fn->glBindVertexArray( 0 );
	fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	r->stopProgram();
	fn->glDepthMask( depthMask );
	if ( !wasBlend )
		fn->glDisable( GL_BLEND );
	if ( wasCull )
		fn->glEnable( GL_CULL_FACE );
	s.lastDrawn = drawn;
	return drawn;
}

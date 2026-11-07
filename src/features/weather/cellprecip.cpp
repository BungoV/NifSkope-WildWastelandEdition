#include "cellprecip.h"

#include "wwswitch.h"
#include "recx.h"
#include "gl/celllights.h"
#include "gl/celllooks.h"
#include "gl/cellgrass.h"
#include "features/exposure/cellhdr.h"
#include "features/shared/cellpasstime.h"
#include "features/air/cellvolair.h"
#include "gl/glscene.h"
#include "gl/lookdevstage.h"
#include "features/weather/precipmath.h"
#include "features/weather/preciproof.h"	// lane INTSKY1: rain only under open sky in a sunlit interior
#include "gl/cellunits.h"	// lane BUGS1102B: the roof map's unit
#include "features/weather/rainfogmath.h"	// lane FO76WEATHER1: the rain's share of a weather change
#include "features/weather/rainwetlaw.h"
#include "features/weather/weatherrecx.h"	// lane WEATHERRECX1: precipitation.wetting / drying
#include "features/shadows/shadowset.h"	// lane FO76WEATHER2: the cover map leaves the terrain out
#include "gl/glshape.h"
#include "features/weather/precipspec.h"
#include "gl/renderer.h"
#include "features/shadows/sunshadow.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
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
	GLuint vaoGpu = 0;	// lane PERFFIX3 D: the GPU field reads no attribute (core profile still needs a bound VAO)
	GLuint roofTex = 0;	// lane BUGS1102B: the roof map for the GPU field (R32F), uploaded once per map (roofTexGen)
	long roofTexGen = -1;
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

/* lane PERFFIX3 D: WW_CELL_PRECIP_RED=cpufield (a word beside the field reds) = the old CPU field: every particle built
 * on the CPU each frame, uploaded, lit per fragment (cell_precip.prog). The field red nowrap has no GPU form and takes it too */
bool cpuFieldFromEnv()
{
	WW_SWITCH_STATIC( QByteArray, r, wwEnv( "WW_CELL_PRECIP_RED" ).trimmed().toLower() );
	for ( const QByteArray & t : r.split( ',' ) )
		if ( t.trimmed() == "cpufield" )
			return true;
	return false;
}

/* lane PRECIPLOOK1: WW_CELL_PRECIP_LOOK=rain,snow,clump,hail (1/on/all = every word; default off) = the new look on the
 * GPU field (cell_precip_look.prog); WW_CELL_PRECIP_LOOK_RED=1 = the old path whatever the look says */
enum Look : int { LookRain = 1, LookSnow = 2, LookClump = 4, LookHail = 8, LookAll = 15 };
int lookFromEnv()
{
	WW_SWITCH_STATIC( QByteArray, red, wwEnv( "WW_CELL_PRECIP_LOOK_RED" ).trimmed().toLower() );
	if ( !red.isEmpty() && red != "0" && red != "off" )
		return 0;
	WW_SWITCH_STATIC( QByteArray, r, wwEnv( "WW_CELL_PRECIP_LOOK" ).trimmed().toLower() );
	int bits = 0;
	for ( const QByteArray & t : r.split( ',' ) ) {
		const QByteArray w = t.trimmed();
		bits |= ( w == "1" || w == "on" || w == "all" ) ? LookAll : w == "rain" ? LookRain : w == "snow" ? LookSnow
			: w == "clump" ? LookClump : w == "hail" ? LookHail : 0;
	}
	return bits;
}

//! the texture path's key with the old and new shipped folders made one (FO4CS/Precip <-> WW/Precip)
QString texKey( QString rel )
{
	rel.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	while ( rel.startsWith( QLatin1Char( '/' ) ) )
		rel.remove( 0, 1 );
	if ( rel.startsWith( QLatin1String( "textures/" ), Qt::CaseInsensitive ) )
		rel.remove( 0, 9 );
	rel = rel.toLower();
	if ( rel.startsWith( QLatin1String( "ww/precip/" ) ) )
		rel = QStringLiteral( "fo4cs/precip/" ) + rel.mid( 10 );
	return rel;
}

//! lane BUGS1102: WW_CELL_PRECIP_RED=gpuroof, the old rule (the GPU field drew rain under a sunlit interior's roof)
bool gpuRoofRedFromEnv()
{
	WW_SWITCH_STATIC( QByteArray, r, wwEnv( "WW_CELL_PRECIP_RED" ).trimmed().toLower() );
	for ( const QByteArray & t : r.split( ',' ) )
		if ( t.trimmed() == "gpuroof" )
			return true;
	return false;
}

//! lane BUGS1102B: WW_CELL_PRECIP_RED=cpuroof, BUGS1102's rule (a roofed interior takes the CPU field and its upload)
bool cpuRoofRedFromEnv()
{
	WW_SWITCH_STATIC( QByteArray, r, wwEnv( "WW_CELL_PRECIP_RED" ).trimmed().toLower() );
	for ( const QByteArray & t : r.split( ',' ) )
		if ( t.trimmed() == "cpuroof" )
			return true;
	return false;
}

//! a linked texture (under Textures\) looked up loose in the roots, first root wins. PRECIPLOOK1: a path under
//! FO4CS/Precip is also looked for under the old WW/Precip (and the other way round), in each root after its own name
QString findTexture( const QStringList & roots, QString rel )
{
	rel.replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	while ( rel.startsWith( QLatin1Char( '/' ) ) )
		rel.remove( 0, 1 );
	if ( rel.startsWith( QLatin1String( "textures/" ), Qt::CaseInsensitive ) )
		rel.remove( 0, 9 );
	QString alt;
	if ( rel.startsWith( QLatin1String( "fo4cs/precip/" ), Qt::CaseInsensitive ) )
		alt = QStringLiteral( "WW/Precip/" ) + rel.mid( 13 );
	else if ( rel.startsWith( QLatin1String( "ww/precip/" ), Qt::CaseInsensitive ) )
		alt = QStringLiteral( "FO4CS/Precip/" ) + rel.mid( 10 );
	for ( const QString & r : roots ) {
		const QString p = r + QStringLiteral( "/Textures/" ) + rel;
		if ( QFileInfo::exists( p ) )
			return p;
		if ( !alt.isEmpty() ) {
			const QString q = r + QStringLiteral( "/Textures/" ) + alt;
			if ( QFileInfo::exists( q ) )
				return q;
		}
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
		/* MODULES1 part 24: a key our file added to a dry record (radstorm rain, weathertranslate.py radstorm_rain) is
		 * listed in `_added`; red ^WW_RADSTORM_RAIN_RED=1 drops them (dry, as vanilla) */
		WW_SWITCH_STATIC( bool, rainRed, wwEnv( "WW_RADSTORM_RAIN_RED" ).trimmed() == QLatin1String( "1" ) );
		QJsonObject b = h.block;
		const bool dropAdded = rainRed && b.contains( QStringLiteral( "_added" ) );
		if ( dropAdded )
			for ( const QJsonValue & k : b.value( QStringLiteral( "_added" ) ).toArray() )
				b.remove( k.toString() );
		specs = wwPrecipParse( b, n );
		source = QStringLiteral( "recx:" ) + h.file.section( QLatin1Char( '/' ), -3 ) + ( dropAdded
			? QStringLiteral( " (rain red: added keys dropped)" ) : QString() );
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
		L.field.seed = hash32( formID * 2654435761u + quint32( i ) * 0x9E3779B9u + quint32( typeInfo( L.spec.type ).seedTag ) );
		out.append( L );
	}
	return out;
}

}	// namespace

QString wwCellPrecipEcho()
{
	return st().echo;
}

// ---- lane FO76WEATHER1: rain wetness (cellprecip.h)
namespace
{
struct RwState
{
	bool init = false, moving = false;
	bool cover = false;	// lane FO76WEATHER2: the cover map is built and tested
	QString coverWhy;
	float wet = 0.0f, target = 0.0f;
	const void * doc = nullptr;
	QElapsedTimer clock;
	qint64 lastMs = 0, memoMs = -1000;
	QString echo = QStringLiteral( "rain wet: off" );
};

RwState & rw()
{
	static RwState s;
	return s;
}

void rwSay( RwState & s, const QString & e )
{
	if ( e != s.echo ) {
		s.echo = e;
		std::fprintf( stderr, "%s\n", qPrintable( e ) );
	}
}

//! the rain's share of the weather (a change: both, by pct); snow does not wet. rainNow / rainPrev: the current weather
//! rains, the leaving one rains while a change runs (lane WETSNAP1, the game's snap)
float rwTarget( Scene * scene, bool & interior, bool & rainNow, bool & rainPrev )
{
	rainNow = rainPrev = false;
	const WwCellLighting * CL = wwCellLightsFor( scene->nifModel );
	interior = CL && CL->interior;
	if ( interior )
		return 0.0f;
	int t[2] = { -2, -2 };
	float d[2] = { -1.0f, -1.0f };
	unsigned f[2] = { 0u, 0u };
	bool blending = false;
	float pct = 1.0f;
	if ( !wwLookdevWeatherPrecip( t, d, f, blending, pct ) )
		return 0.0f;
	rainNow = WwPrecip::spgdKind( t[0] ) == WwPrecip::SpgdRain;
	rainPrev = blending && WwPrecip::spgdKind( t[1] ) == WwPrecip::SpgdRain;
	const double one[2] = { 1.0, 1.0 };
	double rain = 0.0, snow = 0.0;
	WwRainFog::shares( t, one, blending, double( pct ), true, rain, snow );
	return WwRainWet::sat( float( rain ) );
}

/* lane FO76WEATHER2: the cover map of the exterior around the eye (rainwetlaw.h exposed; INTSKY1's WwRoofMap and its
 * triangle rule), Fallout 76's top-down precipitation depth over its occlusion range. Solid shapes without the terrain,
 * kRcHalf u around the eye at kRcCell u texels; rebuilt when the eye moves kRcMove u from the build centre or the
 * cell's shapes change. */
constexpr float kRcHalf = 8192.0f, kRcCell = 16.0f, kRcMove = 2048.0f;

struct RcState
{
	const void * nif = nullptr;
	int shapes = -1;
	float cx = 0.0f, cy = 0.0f;
	WwRoofMap map;
	long gen = 0;
	GLuint tex = 0;
	long texGen = 0;
};

RcState & rc()
{
	static RcState s;
	return s;
}

void rcEye( Scene * scene, float out[3] )
{
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	for ( int j = 0; j < 3; j++ ) {
		float acc = 0.0f;
		for ( int k = 0; k < 3; k++ )
			acc -= vt.rotation( k, j ) * vt.translation[k];
		out[j] = acc / sc;
	}
}

bool rcCovers( const Shape * sh )
{
	return sh && sh->isVisible() && sh->wwBlocksRain() && !sh->wwFarInstData() && !wwCellIsTerrainShape( sh );
}

//! the map, built or kept; "" when kept, else the census line
QString rcBuild( Scene * scene )
{
	RcState & R = rc();
	int shapes = 0;
	for ( Node * node : scene->nodes.list() )
		if ( rcCovers( dynamic_cast<const Shape *>( node ) ) )
			shapes++;
	float eye[3];
	rcEye( scene, eye );
	if ( R.nif == scene->nifModel && R.shapes == shapes && R.map.valid()
		&& std::fabs( eye[0] - R.cx ) < kRcMove && std::fabs( eye[1] - R.cy ) < kRcMove )
		return QString();
	static long builds = 0;
	QElapsedTimer t;
	t.start();
	R.nif = scene->nifModel;
	R.shapes = shapes;
	R.cx = eye[0];
	R.cy = eye[1];
	R.gen = ++builds;
	R.map.reset( eye[0] - kRcHalf, eye[1] - kRcHalf, eye[0] + kRcHalf, eye[1] + kRcHalf, kRcCell, 1024 );
	const float bx0 = R.map.x0, by0 = R.map.y0, bx1 = R.map.x0 + R.map.cell * R.map.nx, by1 = R.map.y0 + R.map.cell * R.map.ny;
	long tris = 0;
	int used = 0;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !rcCovers( sh ) )
			continue;
		const Transform & T = sh->worldTrans();
		bool any = false;
		for ( const Triangle & f : sh->triangles ) {
			if ( f[0] >= sh->verts.size() || f[1] >= sh->verts.size() || f[2] >= sh->verts.size() )
				continue;
			float w[3][3];
			for ( int k = 0; k < 3; k++ ) {
				const Vector3 v = T * sh->verts.at( f[k] );
				w[k][0] = v[0];
				w[k][1] = v[1];
				w[k][2] = v[2];
			}
			if ( std::max( { w[0][0], w[1][0], w[2][0] } ) < bx0 || std::min( { w[0][0], w[1][0], w[2][0] } ) > bx1
				|| std::max( { w[0][1], w[1][1], w[2][1] } ) < by0 || std::min( { w[0][1], w[1][1], w[2][1] } ) > by1 )
				continue;
			R.map.addTri( w[0], w[1], w[2] );
			tris++;
			any = true;
		}
		used += any ? 1 : 0;
	}
	return QStringLiteral( "cover map %1 x %2 of %3 u around (%4, %5): %6 of %7 shapes, %8 triangles, %9 ms" )
		.arg( R.map.nx ).arg( R.map.ny ).arg( double( R.map.cell ), 0, 'f', 1 ).arg( double( eye[0] ), 0, 'f', 0 )
		.arg( double( eye[1] ), 0, 'f', 0 ).arg( used ).arg( shapes ).arg( tris ).arg( t.elapsed() );
}
}	// namespace

float wwCellRainWetAmount( Scene * scene )
{
	RwState & s = rw();
	s.moving = false;
	if ( !scene || !wwLookOn( WwLook::RainWet ) ) {
		s.init = false;
		rwSay( s, QStringLiteral( "rain wet: off" ) );
		return 0.0f;
	}
	WW_SWITCH_STATIC( QByteArray, red, wwEnv( "WW_CELL_RAINWET_RED" ).trimmed() );
	if ( red == "shore" ) {
		s.init = false;
		rwSay( s, QStringLiteral( "rain wet: RED WW_CELL_RAINWET_RED=shore (shore wetness only)" ) );
		return 0.0f;
	}
	if ( !s.clock.isValid() )
		s.clock.start();
	const qint64 now = s.clock.elapsed();
	if ( s.init && s.doc == scene->nifModel && now - s.memoMs < 8 ) {	// one step per frame, not per draw
		s.moving = s.wet != s.target;
		return s.wet;
	}
	s.memoMs = now;
	bool interior = false;
	bool rainNow = false, rainPrev = false;
	const float target = rwTarget( scene, interior, rainNow, rainPrev );
	WW_SWITCH_STATIC( QByteArray, pin, wwEnv( "WW_CELL_RAINWET_AMOUNT" ).trimmed() );
	bool pinOk = false;
	const float pinned = pin.isEmpty() ? 0.0f : pin.toFloat( &pinOk );
	s.target = pinOk ? ( interior ? 0.0f : WwRainWet::sat( pinned ) ) : target;
	if ( !s.init || s.doc != scene->nifModel || pinOk ) {	// a new document or the row turned on: settled
		s.init = true;
		s.doc = scene->nifModel;
		s.wet = s.target;
	} else {	// lane WETSNAP1: the game's snap to dry (red nosnap = the FO76WEATHER1 slow dry in every case)
		// lane WEATHERRECX1: the .recx's wetting / drying minutes (red WW_WEATHER_RECORD_RED=precip: the game's speeds)
		const WwWeatherRecx & wx = wwWeatherRecx();
		const bool fromFile = !wwWeatherRecordRed( WwRecPrecip );
		s.wet = WwRainWet::update( s.wet, s.target, float( now - s.lastMs ) * 0.001f, rainNow, rainPrev, !interior,
			red != "nosnap", fromFile ? wx.wetting : -1.0f, fromFile ? wx.drying : -1.0f );
	}
	s.lastMs = now;
	s.moving = s.wet != s.target;
	// lane FO76WEATHER2: the cover map, once a frame while anything is wet (red nocover = FO76WEATHER1, no cover test)
	s.cover = false;
	if ( red == "nocover" ) {
		s.coverWhy = QStringLiteral( " (RED WW_CELL_RAINWET_RED=nocover)" );
	} else if ( !interior && s.wet > 0.0f ) {
		const QString note = rcBuild( scene );
		if ( !note.isEmpty() )
			std::fprintf( stderr, "rain wet: %s\n", qPrintable( note ) );
		s.cover = rc().map.valid();
		s.coverWhy = s.cover ? QStringLiteral( " cover %1" ).arg( rc().gen ) : QStringLiteral( " (no cover map)" );
	} else {
		s.coverWhy = QString();
	}
	rwSay( s, QStringLiteral( "rain wet: %1 target %2%3%4%5%6" ).arg( double( s.wet ), 0, 'f', 2 ).arg( double( s.target ), 0, 'f', 2 )
		.arg( interior ? QStringLiteral( " (interior)" ) : QString() )
		.arg( pinOk ? QStringLiteral( " (WW_CELL_RAINWET_AMOUNT)" ) : QString() ).arg( s.coverWhy )
		.arg( red == "nosnap" ? QStringLiteral( " (RED WW_CELL_RAINWET_RED=nosnap)" )
			: !pinOk && !interior && WwRainWet::snaps( rainNow, rainPrev, true ) ? QStringLiteral( " (snap: no rain in the change)" ) : QString() ) );
	return s.wet;
}

void wwCellRainWetUniforms( Scene * scene, float customPorosity, bool useCustom )
{
	if ( !scene || !scene->renderer )
		return;
	NifSkopeOpenGLContext::Program * prog = scene->renderer->getCurrentProgram();
	if ( !prog || prog->uniLocation( "rainWet" ) < 0 )
		return;
	const float a = wwCellRainWetAmount( scene );
	// lane FO76WEATHER2: the cover map at its own unit; the sampler always names it (a program's samplers each need a unit)
	bool cover = false;
	if ( prog->uniLocation( "rainCover" ) >= 0 ) {
		prog->uni1i( "rainCover", WwCellUnit::RainCover );
		RcState & R = rc();
		if ( rw().cover && R.map.valid() && a > 0.0f ) {
			auto fn = scene->renderer->fn;
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::RainCover ) );
			if ( !R.tex )
				fn->glGenTextures( 1, &R.tex );
			fn->glBindTexture( GL_TEXTURE_2D, R.tex );
			if ( R.texGen != R.gen ) {
				std::vector<float> z( R.map.z.size() );
				std::transform( R.map.z.begin(), R.map.z.end(), z.begin(), wwRoofTexel );
				fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
				fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, R.map.nx, R.map.ny, 0, GL_RED, GL_FLOAT, z.data() );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
				R.texGen = R.gen;
			}
			fn->glActiveTexture( GL_TEXTURE0 );
			prog->uni4f( "rainCoverGrid", FloatVector4( R.map.x0, R.map.y0, 1.0f / R.map.cell, R.map.cell ) );
			cover = true;
		}
	}
	prog->uni4f( "rainWet", FloatVector4( a, useCustom ? customPorosity : 0.0f, useCustom ? 1.0f : 0.0f, cover ? 1.0f : 0.0f ) );
}

QString wwCellRainWetEcho()
{
	return rw().echo;
}

bool wwCellPrecipWantsFrames( Scene * scene )
{
	const PcState & s = st();
	if ( scene && rw().moving )	// lane FO76WEATHER1: the ground wets or dries
		return true;
	return scene && wwLookOn( WwLook::Precip ) && s.lastDrawn > 0 && !s.timePinned;
}

namespace
{
/* lane INTSKY1: the roof map of the open interior, built once per document (its shapes do not move); the shape count
 * keys it too (a live rebuild of the cell) */
struct PcRoof
{
	const void * nif = nullptr;
	int shapes = -1;
	WwRoofMap map;
	QString note;
	long gen = 0;	// lane BUGS1102B: a global count of map builds (the GPU texture's key; never reset with the map)
};

PcRoof & pcRoof()
{
	static PcRoof r;
	return r;
}

const WwRoofMap & pcRoofFor( Scene * scene )
{
	PcRoof & R = pcRoof();
	int shapes = 0;
	for ( Node * node : scene->nodes.list() )
		if ( const Shape * sh = dynamic_cast<const Shape *>( node ); sh && sh->isVisible() && sh->wwBlocksRain() && !sh->wwFarInstData() )
			shapes++;
	if ( R.nif == scene->nifModel && R.shapes == shapes )
		return R.map;
	static long builds = 0;
	R = PcRoof();
	R.gen = ++builds;
	R.nif = scene->nifModel;
	R.shapes = shapes;
	QElapsedTimer t;
	t.start();
	float lo[2] = { INFINITY, INFINITY }, hi[2] = { -INFINITY, -INFINITY };
	std::vector<float> tri;
	for ( Node * node : scene->nodes.list() ) {
		const Shape * sh = dynamic_cast<const Shape *>( node );
		if ( !sh || !sh->isVisible() || !sh->wwBlocksRain() || sh->wwFarInstData() )
			continue;
		const Transform & T = sh->worldTrans();
		for ( const Triangle & f : sh->triangles ) {
			if ( f[0] >= sh->verts.size() || f[1] >= sh->verts.size() || f[2] >= sh->verts.size() )
				continue;
			for ( int k = 0; k < 3; k++ ) {
				const Vector3 w = T * sh->verts.at( f[k] );
				tri.insert( tri.end(), { w[0], w[1], w[2] } );
				lo[0] = std::min( lo[0], w[0] );
				lo[1] = std::min( lo[1], w[1] );
				hi[0] = std::max( hi[0], w[0] );
				hi[1] = std::max( hi[1], w[1] );
			}
		}
	}
	if ( tri.empty() ) {
		R.note = QStringLiteral( "no shapes" );
		return R.map;
	}
	R.map.reset( lo[0], lo[1], hi[0], hi[1] );
	for ( size_t i = 0; i + 8 < tri.size(); i += 9 )
		R.map.addTri( &tri[i], &tri[i + 3], &tri[i + 6] );
	R.note = QStringLiteral( "roof map %1 x %2 texels of %3 u from %4 shapes, %5 triangles, %6 ms" ).arg( R.map.nx ).arg( R.map.ny )
		.arg( double( R.map.cell ), 0, 'f', 1 ).arg( shapes ).arg( tri.size() / 9 ).arg( t.elapsed() );
	std::fprintf( stderr, "precip: %s\n", qPrintable( R.note ) );
	return R.map;
}
}	// namespace

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
	// lane INTSKY1: an interior with the live weather (wwCellSunlit().weather) gets its rain, only under open sky
	if ( CL && CL->interior && !wwCellSkyLitNow( CL ) ) {
		s.echo = QStringLiteral( "precip: none (interior)" );
		return 0;
	}
	const bool roofTest = CL && CL->interior && !( wwIntSkyRed() & 16 );
	const WwRoofMap * roof = roofTest ? &pcRoofFor( scene ) : nullptr;
	long roofDropped = 0;
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
	// lane PERFFIX3 D: from here the pass has work: its own Frame Timers zone ("precip", CPU + GPU ms)
	PassTime::Scope ptPrecip( PassTime::Precip );
	// lane BUGS1102B: a roofed interior keeps the GPU field, which reads the roof map as a texture (preciproof.h)
	const bool gpuField = wwPrecipGpuField( cpuFieldFromEnv(), ( redBits() & RedNoWrap ) != 0, roof != nullptr, cpuRoofRedFromEnv() );
	const bool gpuRoof = wwPrecipGpuRoofTest( gpuField, roof != nullptr, gpuRoofRedFromEnv() ) && roof->valid();

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
		GpuRun g;	// lane PERFFIX3 D: the GPU field's uniforms (first = 0, count = g.count)
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
			drawnL[tier] = b.count;	// the census says the allocated count (the drawn share moves every frame of a change)
			if ( gpuField ) {
				// lane PERFFIX3 D: the field is the vertex shader's (cell_precip_field.glsl); the inside-the-near-box
				// far particles and the off-screen ones are culled there
				Run run{ li, tier != 0, 0, int( n ), gpuRun( field, b, tier != 0, t, cam, wv, n ) };
				runs.append( run );
				continue;
			}
			const int first = int( s.buf.size() / kFloats );
			// one particle stands for `thin` (the geometry shader scales by the area, the fragment clamps at 1)
			const float alpha = float( b.thin );
			for ( long i = 0; i < n; i++ ) {
				const Particle p = particleAt( field, b, i, tier != 0, t, cam, wv );
				if ( p.inside )
					continue;
				if ( !wwPrecipDrawn( roof, false, p.pos ) ) {	// lane INTSKY1: under a roof
					roofDropped++;
					continue;
				}
				const int cell = p.variant % cells;
				const float v[kFloats] = { float( p.pos[0] ), float( p.pos[1] ), float( p.pos[2] ), 1.0f, 1.0f, 1.0f, alpha,
					float( cell % std::max( 1, L.spec.cols ) ) * cu, float( cell / std::max( 1, L.spec.cols ) ) * cv,
					float( p.angle ), float( p.half ), float( p.vel[0] ), float( p.vel[1] ), float( p.vel[2] ) };
				s.buf.insert( s.buf.end(), v, v + kFloats );
			}
			const int count = int( s.buf.size() / kFloats ) - first;
			if ( count > 0 )
				runs.append( Run{ li, tier != 0, first, count, GpuRun() } );
		}
		census << QStringLiteral( "%1(%2 from %3, w %4: %5/m3 size %6 mm fall %7 m/s, near %8 u x %9, far %10 u x %11 thin %12, tex %13)" )
			.arg( QLatin1String( ti.name ), L.weather, L.source ).arg( weights.at( li ), 0, 'f', 1 ).arg( dens, 0, 'g', 4 )
			.arg( L.spec.sizeMm, 0, 'g', 3 ).arg( fallSpeedMps( L.spec.type, L.spec.sizeMm ), 0, 'f', 2 )
			.arg( nb.side, 0, 'f', 0 ).arg( drawnL[0] ).arg( fb.side, 0, 'f', 0 ).arg( drawnL[1] ).arg( fb.thin, 0, 'f', 1 )
			.arg( L.spec.texture );
	}
	// lane PRECIPLOOK1: the look rides the GPU field only (the CPU field red keeps the old draw)
	const int look = gpuField ? lookFromEnv() : 0;
	QStringList lookWords;
	if ( look & LookRain )
		lookWords << QStringLiteral( "rain" );
	if ( look & LookSnow )
		lookWords << QStringLiteral( "snow" );
	if ( look & LookClump )
		lookWords << QStringLiteral( "clump" );
	if ( look & LookHail )
		lookWords << QStringLiteral( "hail" );
	const QString echo = QStringLiteral( "precip: on, field %4, wind %1 u/s, red %2: %3" ).arg( std::hypot( wv[0], wv[1] ), 0, 'f', 1 )
		.arg( redBits() ).arg( census.join( QStringLiteral( "; " ) ) )
		.arg( gpuField ? QStringLiteral( "gpu" ) : QStringLiteral( "cpu" ) )
		+ ( look ? QStringLiteral( "; look %1 (shutter 1/%2 s)" ).arg( lookWords.join( QLatin1Char( ',' ) ) ).arg( 1.0 / kShutter, 0, 'f', 0 )
			: QString() )
		+ ( roof && gpuField && gpuRoofRedFromEnv() ? QStringLiteral( "; interior, RED WW_CELL_PRECIP_RED=gpuroof (GPU field, no roof test)" )
			: roof && gpuField ? QStringLiteral( "; interior under open sky only, roof map on the GPU (%1)" ).arg( pcRoof().note )
			: roof && cpuRoofRedFromEnv() ? QStringLiteral( "; interior under open sky only, RED WW_CELL_PRECIP_RED=cpuroof (CPU field) (%1)" ).arg( pcRoof().note )
			: roof ? QStringLiteral( "; interior under open sky only (%1)" ).arg( pcRoof().note )
			: CL && CL->interior ? QStringLiteral( "; interior, RED WW_INTSKY_RED=rain (no roof test)" ) : QString() );
	s.echo = echo;
	if ( echo != s.said ) {
		s.said = echo;
		std::fprintf( stderr, "%s\n", qPrintable( echo ) );
	}
	if ( runs.isEmpty() )
		return 0;

	const char * progName = look ? "cell_precip_look.prog" : gpuField ? "cell_precip_gpu.prog" : "cell_precip.prog";
	NifSkopeOpenGLContext::Program * prog = r->useProgram( progName );
	if ( !prog ) {
		static bool saidNo[3] = { false, false, false };
		const int which = look ? 2 : gpuField ? 1 : 0;
		if ( !saidNo[which] ) {
			saidNo[which] = true;
			std::fprintf( stderr, "precip: %s not loaded: nothing drawn\n", progName );
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
	if ( look )
		prog->uni1i( "pcTexN", 1 );
	if ( gpuField ) {
		prog->uni4f( "pcViewport", FloatVector4( float( vp[0] ), float( vp[1] ), float( vp[2] ), float( vp[3] ) ) );
		if ( !s.vaoGpu )
			fn->glGenVertexArrays( 1, &s.vaoGpu );
		fn->glBindVertexArray( s.vaoGpu );
		// lane BUGS1102B (PRECIPROOF-GPU2): the roof map as an R32F texture, uploaded once per map build
		prog->uni1b( "pcRoofOn", gpuRoof );
		prog->uni1i( "pcRoof", WwCellUnit::PrecipRoof );
		if ( gpuRoof ) {
			const WwRoofMap & m = *roof;
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::PrecipRoof ) );
			if ( !s.roofTex )
				fn->glGenTextures( 1, &s.roofTex );
			fn->glBindTexture( GL_TEXTURE_2D, s.roofTex );
			if ( s.roofTexGen != pcRoof().gen ) {
				std::vector<float> z( m.z.size() );
				std::transform( m.z.begin(), m.z.end(), z.begin(), wwRoofTexel );
				fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
				fn->glTexImage2D( GL_TEXTURE_2D, 0, GL_R32F, m.nx, m.ny, 0, GL_RED, GL_FLOAT, z.data() );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
				fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
				s.roofTexGen = pcRoof().gen;
			}
			fn->glActiveTexture( GL_TEXTURE0 );
			// the eye in the map's texels, in double here (the shader adds the particle's camera-relative offset)
			float rc[3];
			wwRoofCam( m, cam, rc );
			prog->uni3f( "pcRoofCam", rc[0], rc[1], rc[2] );
			prog->uni1f( "pcRoofInv", 1.0f / m.cell );
		}
	} else {
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
	}

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
		// a texture the type did not ship: 16 px a cell is the deepest mip read (the snow_fine rule); the type's own
		// atlas: its measured cap (precipmath.h maxMip)
		const bool ownAtlas = texKey( L.spec.texture ) == texKey( QLatin1String( ti.texture ) );
		/* lane PRECIPLOOK1: the look swaps only the type's own atlas (a .recx's own texture is kept): the clumps read the
		 * merged density atlas at their threshold, hail the 3D build; a snow/hail atlas's normal map (<name>_n.dds) on unit 1 */
		const bool isClump = L.spec.type == SnowWetClump || L.spec.type == SnowBigClump;
		const bool lookRain = ( look & LookRain ) && L.spec.type == Rain && ownAtlas;
		const bool lookClump = ( look & LookClump ) && isClump && ownAtlas;
		const bool lookHail = ( look & LookHail ) && L.spec.type == Hail && ownAtlas;
		const bool wantN = ( ( look & LookSnow ) && L.spec.type >= SnowFine ) || lookHail;
		const QString texName = lookClump ? QLatin1String( kClumpAtlas ) : lookHail ? QLatin1String( kHailAtlas ) : L.spec.texture;
		auto loadTex = [&]( const QString & name, bool quiet ) -> PcTex & {
			PcTex & x = s.tex[name];
			if ( !x.id && x.file.isEmpty() ) {
				const QString path = findTexture( s.roots, name );
				if ( path.isEmpty() || !loadDds( fn, path, x ) ) {
					x.file = QStringLiteral( "-" );	// tried: the soft-dot fallback from now on
					if ( !quiet )
						std::fprintf( stderr, "precip: texture %s %s: soft dots instead\n", qPrintable( name ),
							path.isEmpty() ? "not found loose in the .recx roots" : "not a BC1/BC3/BC7 DDS" );
				} else {
					x.file = path;
					std::fprintf( stderr, "precip: texture %s <- %s (%dx%d, %d mips)\n", qPrintable( name ), qPrintable( path ),
						x.width, x.height, x.mips );
				}
			}
			return x;
		};
		PcTex & tx = loadTex( texName, false );
		const bool hasTex = tx.id != 0;
		GLuint nId = 0;
		if ( wantN && hasTex && texName.endsWith( QLatin1String( "_d.dds" ), Qt::CaseInsensitive ) ) {
			const PcTex & nx = loadTex( texName.left( texName.size() - 6 ) + QStringLiteral( "_n.dds" ), true );
			if ( nx.id && nx.width == tx.width && nx.height == tx.height )
				nId = nx.id;
		}
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_2D, tx.id );
		const int cols = std::max( 1, L.spec.cols ), rows = std::max( 1, L.spec.rows );
		const float cellPx = hasTex ? float( tx.width ) / float( cols ) : 1.0f;
		const int ownMip = lookClump ? kClumpMaxMip : lookHail ? kHailMaxMip : ti.maxMip;
		const float maxLod = hasTex ? std::min( float( tx.mips - 1 ), ownAtlas ? float( ownMip )
			: std::max( 0.0f, std::log2( cellPx / 16.0f ) ) ) : 0.0f;
		if ( look ) {
			fn->glActiveTexture( GL_TEXTURE1 );
			fn->glBindTexture( GL_TEXTURE_2D, nId );
			fn->glActiveTexture( GL_TEXTURE0 );
			const ClumpLook cl = L.spec.type == SnowWetClump ? kClumpWet : kClumpDry;
			prog->uni1i( "pcLook", lookRain ? 1 : nId ? 2 : 0 );
			prog->uni1b( "pcHasN", nId != 0 );
			prog->uni2f( "pcClump", lookClump ? float( cl.t ) : 0.0f, lookClump ? float( cl.k ) : 1.0f );
			prog->uni1f( "pcAlbedo", lookClump ? float( cl.albedo ) : 1.0f );
			prog->uni1f( "pcMmPerView", float( kMmPerUnit / ( double( sc ) * std::max( L.field.drawScale, 1e-3 ) ) ) );
		}
		const float oneLod = hasTex ? std::min( float( tx.mips - 1 ), std::max( 0.0f, std::log2( cellPx ) ) ) : 0.0f;
		prog->uni4f( "pcAtlas", FloatVector4( 1.0f / float( cols ), 1.0f / float( rows ), maxLod, oneLod ) );
		prog->uni2f( "pcInset", hasTex ? 0.5f * std::exp2( maxLod ) / float( tx.width ) : 0.0f,
			hasTex ? 0.5f * std::exp2( maxLod ) / float( tx.height ) : 0.0f );
		prog->uni1b( "pcHasTex", hasTex );
		prog->uni1i( "pcFar", run.farT ? 1 : 0 );
		prog->uni1f( "pcTrans", lookRain ? 1.0f : float( L.spec.translucency ) );	// a drop: all thin (its phase table)
		if ( gpuField ) {
			// lane PERFFIX3 D: the run's field (precipmath.h gpuRun; the shader's names in cell_precip_field.glsl)
			const GpuRun & g = run.g;
			float offZ[16], fallU[16], halfU[16];
			for ( int k = 0; k < 16; k++ ) {
				offZ[k] = float( g.offZ[k] );
				fallU[k] = float( g.fallU[k] );
				halfU[k] = float( g.halfU[k] * lookHalfScale( L.spec.type, k, lookRain, lookClump ) );	// PRECIPLOOK1
			}
			fn->glUniform1ui( prog->uniLocation( "pcSeedS" ), GLuint( g.seedS ) );
			prog->uni1f( "pcL", float( g.L ) );
			prog->uni1f( "pcInner", float( g.inner ) );
			prog->uni1f( "pcThin", float( g.thin ) );
			prog->uni2f( "pcOffXY", float( g.offXY[0] ), float( g.offXY[1] ) );
			prog->uni1fv_l( prog->uniLocation( "pcOffZ" ), offZ, 16 );
			prog->uni1i( "pcNCand", g.nCand );
			prog->uni1i( "pcBest", g.best );
			prog->uni1iv_l( prog->uniLocation( "pcCand" ), g.cand, 16 );
			prog->uni1fv_l( prog->uniLocation( "pcFallU" ), fallU, 16 );
			prog->uni1fv_l( prog->uniLocation( "pcHalfU" ), halfU, 16 );
			prog->uni2f( "pcWind", float( g.wind[0] ), float( g.wind[1] ) );
			prog->uni1f( "pcSway", float( g.sway ) );
			prog->uni1f( "pcSpin", float( g.spin ) );
			prog->uni1f( "pcT", float( t ) );
			prog->uni1i( "pcCols", cols );
			prog->uni1i( "pcCells", std::max( 1, L.spec.cols * L.spec.rows ) );
			prog->uni2f( "pcCellUv", 1.0f / float( cols ), 1.0f / float( rows ) );
		}
		fn->glDrawArrays( GL_POINTS, GLint( run.first ), GLsizei( run.count ) );
		drawn += run.count;
	}
	fn->glBindVertexArray( 0 );
	if ( !gpuField )
		fn->glBindBuffer( GL_ARRAY_BUFFER, 0 );
	if ( look ) {
		fn->glActiveTexture( GL_TEXTURE1 );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glActiveTexture( GL_TEXTURE0 );
	}
	fn->glBindTexture( GL_TEXTURE_2D, 0 );
	if ( gpuRoof ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + WwCellUnit::PrecipRoof ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glActiveTexture( GL_TEXTURE0 );
	}
	r->stopProgram();
	fn->glDepthMask( depthMask );
	if ( !wasBlend )
		fn->glDisable( GL_BLEND );
	if ( wasCull )
		fn->glEnable( GL_CULL_FACE );
	s.lastDrawn = drawn;
	return drawn;
}

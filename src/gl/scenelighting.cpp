/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "scenelighting.h"

#include "gltex.h"
#include "model/nifmodel.h"

#include "libfo76utils/src/filebuf.hpp"

#include <QHash>
#include <QOpenGLContext>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{

const char * const kExposureKey = "Settings/Render/Scene/Exposure EV";
const char * const kViewKey = "Settings/Render/Scene/View Transform";

struct WwSceneState
{
	bool init = false;
	int mode = WwSceneLegacy;
	float ev = 0.0f;
	int view = WwViewPbrNeutral;	// s5.2: PBR Neutral is the default for judging material colour
	float probe = -1.0f;
	bool pinMode = false, pinEv = false, pinView = false;
	QString cube;
	QString red;
	float sun = 1.0f;	// lane PBRR3: WW_STUDIO_SUN, the Studio sun's scale (0 = the white furnace)
	int term = 0;		// lane PBRR3: WW_R3_TERM, 0 all, 1 diffuse only, 2 specular only
	QString red3;		// lane PBRR3: WW_R3_RED
	QString red4;		// lane PBRR4: WW_R4_RED
};

int wwParseView( const QString & s )
{
	const QString v = s.trimmed().toLower();
	if ( v == QLatin1StringView( "standard" ) || v == QLatin1StringView( "none" ) || v == QLatin1StringView( "0" ) )
		return WwViewStandard;
	if ( v == QLatin1StringView( "agx" ) || v == QLatin1StringView( "1" ) )
		return WwViewAgX;
	if ( v == QLatin1StringView( "neutral" ) || v == QLatin1StringView( "pbrneutral" ) || v == QLatin1StringView( "2" ) )
		return WwViewPbrNeutral;
	return -1;
}

WwSceneState & st()
{
	static WwSceneState s;
	if ( !s.init ) {
		s.init = true;
		const QString m = qEnvironmentVariable( "WW_LIGHTING_MODE" ).trimmed().toLower();
		if ( m == QLatin1StringView( "studio" ) ) {
			s.mode = WwSceneStudio;
			s.pinMode = true;
		} else if ( m == QLatin1StringView( "legacy" ) ) {
			s.pinMode = true;
		} else if ( m == QLatin1StringView( "lookdev" ) ) {
			s.mode = WwSceneLookdev;
			s.pinMode = true;
		}
		// WW_LOOKDEV=1 (docs s6.2): the Lookdev mode unless the mode is pinned otherwise
		if ( qEnvironmentVariable( "WW_LOOKDEV" ).trimmed() == QLatin1StringView( "1" ) && m.isEmpty() ) {
			s.mode = WwSceneLookdev;
			s.pinMode = true;
		}
		bool ok = false;
		const double ev = qEnvironmentVariable( "WW_EXPOSURE_EV" ).toDouble( &ok );
		if ( ok ) {
			s.ev = float( ev );
			s.pinEv = true;
		}
		const int vt = wwParseView( qEnvironmentVariable( "WW_VIEW_TRANSFORM" ) );
		if ( vt >= 0 ) {
			s.view = vt;
			s.pinView = true;
		}
		const double pr = qEnvironmentVariable( "WW_STUDIO_PROBE" ).toDouble( &ok );
		if ( ok && pr >= 0.0 )
			s.probe = float( pr );
		s.cube = qEnvironmentVariable( "WW_STUDIO_CUBE" ).trimmed();
		if ( s.cube.isEmpty() )
			s.cube = QStringLiteral( "textures/shared/cubemaps/mipblur_defaultoutside1.dds" );
		s.red = qEnvironmentVariable( "WW_R2A_RED" ).trimmed().toLower();
		const double sun = qEnvironmentVariable( "WW_STUDIO_SUN" ).toDouble( &ok );
		if ( ok && sun >= 0.0 )
			s.sun = float( sun );
		const QString t = qEnvironmentVariable( "WW_R3_TERM" ).trimmed().toLower();
		s.term = ( t == QLatin1StringView( "diffuse" ) ? 1 : t == QLatin1StringView( "specular" ) ? 2
			: t == QLatin1StringView( "fresnel" ) ? 3 : 0 );
		s.red3 = qEnvironmentVariable( "WW_R3_RED" ).trimmed().toLower();
		s.red4 = qEnvironmentVariable( "WW_R4_RED" ).trimmed().toLower();
	}
	return s;
}

} // namespace

int wwSceneMode() { return st().mode; }
void wwSetSceneMode( int mode ) { st().mode = ( mode == WwSceneStudio || mode == WwSceneLookdev ? mode : WwSceneLegacy ); }
float wwSceneExposureEV() { return st().ev; }
void wwSetSceneExposureEV( float ev ) { st().ev = std::clamp( ev, -10.0f, 10.0f ); }
int wwSceneViewTransform() { return st().view; }
void wwSetSceneViewTransform( int vt ) { if ( vt >= WwViewStandard && vt <= WwViewPbrNeutral ) st().view = vt; }
float wwStudioProbe() { return st().probe; }
QString wwStudioCubePath() { return st().cube; }

bool wwR2aRed( const char * name )
{
	return !st().red.isEmpty() && st().red == QLatin1StringView( name );
}

float wwStudioSunScale() { return st().sun; }
int wwR3Term() { return st().term; }

int wwR3RedBits()
{
	const QString & r = st().red3;
	if ( r == QLatin1StringView( "noms" ) ) return 1;
	if ( r == QLatin1StringView( "nosplit" ) ) return 2;
	if ( r == QLatin1StringView( "fo4csweight" ) ) return 4;
	if ( r == QLatin1StringView( "notint" ) ) return 8;
	if ( r == QLatin1StringView( "f90scaled" ) ) return 16;
	if ( r == QLatin1StringView( "lambert" ) ) return 32;
	return 0;
}

int wwR4RedBits()
{
	const QString & r = st().red4;
	if ( r == QLatin1StringView( "nodiv" ) ) return 1;
	if ( r == QLatin1StringView( "emitmul" ) ) return 2;
	if ( r == QLatin1StringView( "notintmask" ) ) return 4;
	if ( r == QLatin1StringView( "emitraw" ) ) return 8;
	if ( r == QLatin1StringView( "nocomp" ) ) return 16;
	return 0;
}

float wwSceneExposureScale()
{
	const float ev = st().ev;
	// red "ev": half the stops -> EV+1 gives sqrt(2), which the doubling gate must refuse
	return std::exp2( wwR2aRed( "ev" ) ? ev * 0.5f : ev );
}

void wwSceneLoadSettings()
{
	WwSceneState & s = st();
	QSettings settings;
	if ( !s.pinEv && settings.contains( QLatin1StringView( kExposureKey ) ) )
		wwSetSceneExposureEV( settings.value( QLatin1StringView( kExposureKey ) ).toFloat() );
	if ( !s.pinView && settings.contains( QLatin1StringView( kViewKey ) ) ) {
		const int vt = wwParseView( settings.value( QLatin1StringView( kViewKey ) ).toString() );
		if ( vt >= 0 )
			s.view = vt;
	}
	// the mode is never restored: every launch starts Legacy (ruling Q7)
}

void wwSceneSaveSettings()
{
	if ( wwR2aRed( "nosave" ) )
		return;
	static const char * const names[] = { "standard", "agx", "neutral" };
	QSettings settings;
	settings.setValue( QLatin1StringView( kExposureKey ), double( st().ev ) );
	settings.setValue( QLatin1StringView( kViewKey ), QString::fromLatin1( names[st().view] ) );
}

QString wwSceneEcho()
{
	static const char * const names[] = { "standard", "agx", "neutral" };
	const WwSceneState & s = st();
	QString e = QStringLiteral( "lighting=%1(asked=%2) ev=%3(asked=%4) view=%5(asked=%6)" )
		.arg( s.mode == WwSceneStudio ? QStringLiteral( "studio" ) : s.mode == WwSceneLookdev ? QStringLiteral( "lookdev" ) : QStringLiteral( "legacy" ),
			qEnvironmentVariableIsSet( "WW_LIGHTING_MODE" ) ? qEnvironmentVariable( "WW_LIGHTING_MODE" ) : QStringLiteral( "unset" ) )
		.arg( double( s.ev ), 0, 'f', 2 )
		.arg( qEnvironmentVariableIsSet( "WW_EXPOSURE_EV" ) ? qEnvironmentVariable( "WW_EXPOSURE_EV" ) : QStringLiteral( "unset" ) )
		.arg( QLatin1StringView( names[s.view] ),
			qEnvironmentVariableIsSet( "WW_VIEW_TRANSFORM" ) ? qEnvironmentVariable( "WW_VIEW_TRANSFORM" ) : QStringLiteral( "unset" ) );
	if ( s.probe >= 0.0f )
		e += QStringLiteral( " probe=%1" ).arg( double( s.probe ), 0, 'f', 4 );
	if ( !s.red.isEmpty() )
		e += QStringLiteral( " red=%1" ).arg( s.red );
	if ( s.sun != 1.0f )
		e += QStringLiteral( " sun=%1" ).arg( double( s.sun ), 0, 'f', 3 );
	if ( s.term )
		e += QStringLiteral( " term=%1" ).arg( s.term == 1 ? QStringLiteral( "diffuse" )
			: s.term == 2 ? QStringLiteral( "specular" ) : QStringLiteral( "fresnel" ) );
	if ( !s.red3.isEmpty() )
		e += QStringLiteral( " r3red=%1" ).arg( s.red3 );
	if ( !s.red4.isEmpty() )
		e += QStringLiteral( " r4red=%1" ).arg( s.red4 );
	return e;
}


/* ---- Studio cube ---- */

bool wwStudioCubeNormalise( QByteArray & data, QString * why )
{
	auto fail = [why]( const QString & s ) {
		if ( why )
			*why = s;
		return false;
	};
	if ( data.size() < 128 || FileBuffer::readUInt32Fast( data.constData() ) != 0x20534444 )	// "DDS "
		return fail( QStringLiteral( "not a DDS file" ) );
	const unsigned char * p = reinterpret_cast< const unsigned char * >( data.constData() );
	const std::uint32_t height = FileBuffer::readUInt32Fast( p + 12 );
	const std::uint32_t width = FileBuffer::readUInt32Fast( p + 16 );
	std::uint32_t mips = FileBuffer::readUInt32Fast( p + 28 );
	const std::uint32_t pfFlags = FileBuffer::readUInt32Fast( p + 80 );
	const std::uint32_t fourCC = FileBuffer::readUInt32Fast( p + 84 );
	const std::uint32_t caps = FileBuffer::readUInt32Fast( p + 108 );
	const std::uint32_t caps2 = FileBuffer::readUInt32Fast( p + 112 );
	const bool srgb = !wwR2aRed( "cubedecode" );
	if ( mips < 1 )
		mips = 1;

	if ( fourCC == 0x30315844 ) {	// "DX10": the format is explicit
		if ( data.size() < 148 )
			return fail( QStringLiteral( "truncated DX10 header" ) );
		const std::uint32_t misc = FileBuffer::readUInt32Fast( p + 136 );
		if ( !( caps2 & 0x200 ) && !( misc & 4 ) && !( caps & 0x200 ) )
			return fail( QStringLiteral( "not a cube map (no cube flag in caps2, caps or the DX10 misc flag)" ) );
		unsigned char fmt = p[128];
		if ( srgb ) {
			if ( fmt == 0x1C ) fmt = 0x1D;			// R8G8B8A8_UNORM -> _SRGB
			else if ( fmt == 0x57 ) fmt = 0x5B;		// B8G8R8A8_UNORM -> _SRGB
			else if ( fmt == 0x47 ) fmt = 0x48;		// BC1
			else if ( fmt == 0x4A ) fmt = 0x4B;		// BC2
			else if ( fmt == 0x4D ) fmt = 0x4E;		// BC3
			else if ( fmt == 0x62 ) fmt = 0x63;		// BC7
		}
		QByteArray out( 148, '\0' );
		if ( !FileBuffer::writeDDSHeader( reinterpret_cast< unsigned char * >( out.data() ), fmt,
				int( width ), int( height ), int( mips ), true ) )
			return fail( QStringLiteral( "unsupported DXGI format 0x%1" ).arg( fmt, 2, 16, QLatin1Char( '0' ) ) );
		out += data.mid( 148 );
		data = out;
		return true;
	}

	// legacy header: FO4 puts the cube bits in dwCaps with dwCaps2 = 0
	if ( !( caps2 & 0x200 ) && !( caps & 0x200 ) )
		return fail( QStringLiteral( "not a cube map (no cube flag in caps2 or caps)" ) );
	unsigned char fmt = 0;
	if ( pfFlags & 0x4 ) {	// DDPF_FOURCC
		if ( fourCC == 0x31545844 ) fmt = srgb ? 0x48 : 0x47;			// DXT1
		else if ( fourCC == 0x33545844 ) fmt = srgb ? 0x4B : 0x4A;		// DXT3
		else if ( fourCC == 0x35545844 ) fmt = srgb ? 0x4E : 0x4D;		// DXT5
	} else if ( ( pfFlags & 0x40 ) && FileBuffer::readUInt32Fast( p + 88 ) == 32 ) {	// DDPF_RGB, 32 bpp
		const std::uint32_t rMask = FileBuffer::readUInt32Fast( p + 92 );
		const std::uint32_t gMask = FileBuffer::readUInt32Fast( p + 96 );
		const std::uint32_t bMask = FileBuffer::readUInt32Fast( p + 100 );
		if ( rMask == 0x00FF0000 && gMask == 0x0000FF00 && bMask == 0x000000FF )
			fmt = srgb ? 0x5B : 0x57;	// B8G8R8A8
		else if ( rMask == 0x000000FF && gMask == 0x0000FF00 && bMask == 0x00FF0000 )
			fmt = srgb ? 0x1D : 0x1C;	// R8G8B8A8
	}
	if ( !fmt )
		return fail( QStringLiteral( "unsupported legacy pixel format (flags 0x%1, fourCC 0x%2)" )
			.arg( pfFlags, 0, 16 ).arg( fourCC, 8, 16, QLatin1Char( '0' ) ) );
	QByteArray out( 148, '\0' );
	if ( !FileBuffer::writeDDSHeader( reinterpret_cast< unsigned char * >( out.data() ), fmt,
			int( width ), int( height ), int( mips ), true ) )
		return fail( QStringLiteral( "could not write the DX10 header" ) );
	out += data.mid( 128 );
	data = out;
	return true;
}

bool wwStudioCubeLoad( TexCache * tc, const NifModel * nif, const QString & name, QByteArray data,
	unsigned int * id, QString * why )
{
	if ( !tc )
		return false;
	if ( !wwStudioCubeNormalise( data, why ) )
		return false;
	const GLuint mips = tc->loadStudioCube( nif, name, data, id );
	if ( !mips || !id[0] || !id[1] ) {
		if ( why )
			*why = QStringLiteral( "the prefilter refused the cube" );
		return false;
	}
	return true;
}

namespace
{
struct WwStudioCube
{
	GLuint id[2] = { 0, 0 };
	bool tried = false;
	bool ok = false;
	QString why;
};

QHash<QString, WwStudioCube> & studioCubes()
{
	static QHash<QString, WwStudioCube> h;
	return h;
}
} // namespace

bool wwBindStudioCube( TexCache * tc, const NifModel * nif, const QString & fname, bool irradiance )
{
	if ( !tc || fname.isEmpty() )
		return false;
	// keyed by context too: texture names belong to one GL context
	const QString key = QStringLiteral( "%1|%2|%3" )
		.arg( quintptr( QOpenGLContext::currentContext() ), 0, 16 ).arg( quintptr( tc ), 0, 16 ).arg( fname.toLower() );
	WwStudioCube & c = studioCubes()[key];
	if ( !c.tried ) {
		c.tried = true;
		QByteArray data;
		if ( nif && nif->getResourceFile( data, fname, "textures", "" ) ) {
			GLuint id[2] = { 0, 0 };
			c.ok = wwStudioCubeLoad( tc, nif, fname, data, id, &c.why );
			c.id[0] = id[0];
			c.id[1] = id[1];
		} else {
			c.why = QStringLiteral( "file not found" );
		}
	}
	if ( !c.ok )
		return false;
	glBindTexture( GL_TEXTURE_CUBE_MAP, c.id[irradiance ? 1 : 0] );
	glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	return true;
}

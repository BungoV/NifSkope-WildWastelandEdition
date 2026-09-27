/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_LODLIT_H
#define WW_LODLIT_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <cmath>

/* -------------------------------------------------------------------------
 * WW_LODL_LIT=1 -- the far-LOD view under a real sun and sky (lane LIT1).
 *
 * Unset, the LOD viewer draws what it drew before: the default headlight
 * (`frontalLight`, a light that points down the view axis and moves with the
 * camera) through fo4_default.frag. Set, the SAME built document is drawn
 * through `lod_lit.prog`, and that program reuses the PBR BRDF of
 * pbrm_default.frag (`Surface`, `directLight`, `dfgLazarov`, `multiScatter`,
 * `specAlbedo`) instead of adding a second one. The inputs are:
 *
 *   terrain : colour sheet, the `_msn` sheet (model space), and the MASK sheet
 *             (role 5: R roughness, G metallic, B sky AO). The builder binds the
 *             mask in slot 7 only while this mode is on.
 *   objects : the BGSM's or the siblings' diffuse, `_n` and `_s` (R specular
 *             mask, G gloss), and the baked ambient visibility. That visibility is
 *             the `.lodi` v6 per-vertex AO times the v7 per-vertex sky visibility,
 *             carried in the vertex ALPHA while this mode is on. A bucket whose
 *             alpha is real opacity (`.lodo` VERTEX_ALPHA) keeps its alpha and
 *             draws with visibility 1, and the builder's note counts those.
 *
 * One sun, one sky. They are FIXED CONSTANTS, the numbers below. Both builders
 * print them in their notes, so every picture's log carries the light it was
 * taken under. Nothing here is a user dial.
 *
 *   WW_LODL_LIT_TERM = all | sun | sky | spec     the term-isolation panels
 *   WW_LODL_LIT_RED  = flipnorth,flipup,nospec,sunmirror   refuters only
 *
 * The channel views (WW_LODL_AO, WW_LODL_CHANNEL) are DATA views and win: with
 * either one set, this mode is off and the note says so by name.
 * ------------------------------------------------------------------------- */

namespace WwLodLit
{
//! The sun: elevation above the horizon and azimuth clockwise from north, in degrees.
//! 225 = the sun stands in the south-west, so south- and west-facing slopes are lit.
constexpr float kSunElevationDeg = 35.0f;
constexpr float kSunAzimuthDeg = 225.0f;
//! The sun's intensity: a white Lambert surface facing it reads this (linear), times its colour.
constexpr float kSunIntensity = 3.0f;
constexpr float kSunColour[3] = { 1.00f, 0.95f, 0.88f };
//! The sky: radiance at the zenith and the ground bounce at the nadir, linear.
//! A normal between them takes the mix by its world up component.
constexpr float kSkyZenith[3] = { 0.34f, 0.42f, 0.55f };
constexpr float kSkyNadir[3] = { 0.12f, 0.11f, 0.10f };

enum Red
{
	RedFlipNorth = 1,	//!< the terrain `_msn` north axis (its B channel) negated
	RedFlipUp = 2,		//!< the terrain `_msn` up axis (its G channel) negated
	RedNoSpec = 4,		//!< specular weight 0 everywhere
	RedSunMirror = 8	//!< the sun's north component negated (y -> -y)
};

enum Term
{
	TermAll = 0,
	TermSun = 1,
	TermSky = 2,
	TermSpec = 3
};

//! The world direction TO the sun, unit, Z up, +Y north, +X east.
inline void sunDirection( float out[3] )
{
	const float el = kSunElevationDeg * float( 3.14159265358979 / 180.0 );
	const float az = kSunAzimuthDeg * float( 3.14159265358979 / 180.0 );
	out[0] = std::cos( el ) * std::sin( az );
	out[1] = std::cos( el ) * std::cos( az );
	out[2] = std::sin( el );
}

//! WW_LODL_LIT=1 and no data view asked for. `*refused` names the view that won.
inline bool wanted( QString * refused = nullptr )
{
	if ( qEnvironmentVariableIntValue( "WW_LODL_LIT" ) == 0 )
		return false;
	if ( qEnvironmentVariableIntValue( "WW_LODL_AO" ) != 0 ) {
		if ( refused )
			*refused = QStringLiteral( "WW_LODL_AO" );
		return false;
	}
	if ( !qEnvironmentVariable( "WW_LODL_CHANNEL" ).trimmed().isEmpty() ) {
		if ( refused )
			*refused = QStringLiteral( "WW_LODL_CHANNEL" );
		return false;
	}
	return true;
}

//! Read once a process: the renderer asks per draw.
inline bool on()
{
	static const int v = wanted() ? 1 : 0;
	return v != 0;
}

inline int term()
{
	static const int t = [] {
		const QString s = qEnvironmentVariable( "WW_LODL_LIT_TERM" ).trimmed().toLower();
		if ( s == QLatin1String( "sun" ) )
			return int( TermSun );
		if ( s == QLatin1String( "sky" ) )
			return int( TermSky );
		if ( s == QLatin1String( "spec" ) )
			return int( TermSpec );
		return int( TermAll );
	}();
	return t;
}

inline int red()
{
	static const int r = [] {
		int m = 0;
		const QStringList parts = qEnvironmentVariable( "WW_LODL_LIT_RED" ).toLower()
			.split( QChar( ',' ), Qt::SkipEmptyParts );
		for ( const QString & p0 : parts ) {
			const QString p = p0.trimmed();
			if ( p == QLatin1String( "flipnorth" ) )
				m |= RedFlipNorth;
			else if ( p == QLatin1String( "flipup" ) )
				m |= RedFlipUp;
			else if ( p == QLatin1String( "nospec" ) )
				m |= RedNoSpec;
			else if ( p == QLatin1String( "sunmirror" ) )
				m |= RedSunMirror;
		}
		return m;
	}();
	return r;
}

inline QString termName( int t )
{
	switch ( t ) {
	case TermSun: return QStringLiteral( "sun" );
	case TermSky: return QStringLiteral( "sky" );
	case TermSpec: return QStringLiteral( "spec" );
	default: return QStringLiteral( "all" );
	}
}

//! The note line both builders print: the light every lit picture was taken under.
inline QString noteLine()
{
	float d[3];
	sunDirection( d );
	QStringList reds;
	const int r = red();
	if ( r & RedFlipNorth ) reds << QStringLiteral( "flipnorth" );
	if ( r & RedFlipUp ) reds << QStringLiteral( "flipup" );
	if ( r & RedNoSpec ) reds << QStringLiteral( "nospec" );
	if ( r & RedSunMirror ) reds << QStringLiteral( "sunmirror" );
	return QString( "WW_LODL_LIT: sun elevation %1 deg azimuth %2 deg, to-sun (%3, %4, %5), "
			"intensity %6 x colour (%7, %8, %9); sky zenith (%10, %11, %12) nadir (%13, %14, %15); "
			"term %16; red %17; program lod_lit.prog (the PBR BRDF of pbrm_default.frag)" )
		.arg( double( kSunElevationDeg ) ).arg( double( kSunAzimuthDeg ) )
		.arg( double( d[0] ), 0, 'f', 4 ).arg( double( d[1] ), 0, 'f', 4 ).arg( double( d[2] ), 0, 'f', 4 )
		.arg( double( kSunIntensity ) )
		.arg( double( kSunColour[0] ) ).arg( double( kSunColour[1] ) ).arg( double( kSunColour[2] ) )
		.arg( double( kSkyZenith[0] ) ).arg( double( kSkyZenith[1] ) ).arg( double( kSkyZenith[2] ) )
		.arg( double( kSkyNadir[0] ) ).arg( double( kSkyNadir[1] ) ).arg( double( kSkyNadir[2] ) )
		.arg( termName( term() ) )
		.arg( reds.isEmpty() ? QStringLiteral( "none" ) : reds.join( QChar( ',' ) ) );
}
} // namespace WwLodLit

#endif // WW_LODLIT_H

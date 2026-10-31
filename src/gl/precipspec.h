#ifndef WW_PRECIPSPEC_H
#define WW_PRECIPSPEC_H

/* lane WEATHERLOOK1 phase B: a weather's precipitation layers, from its .recx block "precipitation" (docs/RECX.md)
 * or, with no block, from its own record (WTHR DATA class flags: Rainy -> one rain layer, Snow -> one snow_fine
 * layer, the type defaults of gl/precipmath.h). Header only (Qt Core), so tests/precip reads the same parser.
 *
 *   "precipitation": { "layers": [ { "type": "snow_fine", "rate": 1.5, "size": 5.5, "drawScale": 3,
 *                                    "texture": "WW/Precip/snow_fine_d.dds", "atlas": [4, 4] }, ... ] }
 *
 * Keys of a layer (all but type optional; the type's default where absent): type (rain, sleet, hail, snow_fine,
 * snow_far_dot, snow_wet_clump, snow_big_clump), rate (mm/h of water), density (particles per m^3; wins over rate),
 * size (mm), drawScale, wind (the share of the wind it rides, 0..2), translucency (0..1), spin (rad/s), sway (game
 * units), texture (under Textures\, as the game links textures), atlas ([columns, rows]). Unknown keys and bad values
 * are reported in `notes` and dropped. */

#include "gl/precipmath.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cmath>

struct WwPrecipLayerSpec
{
	int type = WwPrecip::Rain;
	double sizeMm = 0.0, rateMmH = 0.0, density = -1.0;	// density < 0: from the rate
	double drawScale = 1.0, windShare = 1.0, translucency = 0.5, spin = 0.0, sway = 0.0;
	QString texture;	// under Textures\ (the type's default when the file names none)
	int cols = 1, rows = 1;
	QString from;	// "recx" or "record"

	//! the full density (weight 1), per m^3
	double densityM3() const
	{
		if ( WwPrecip::redBits() & WwPrecip::RedConstDensity )
			return WwPrecip::kRedDensity;
		return density >= 0.0 ? density : WwPrecip::densityFromRate( type, sizeMm, rateMmH );
	}
};

//! the type's defaults
inline WwPrecipLayerSpec wwPrecipDefault( int type, const QString & from )
{
	const WwPrecip::TypeInfo & ti = WwPrecip::typeInfo( type );
	WwPrecipLayerSpec s;
	s.type = type;
	s.sizeMm = ti.sizeMm;
	s.rateMmH = ti.rateMmH;
	s.drawScale = ti.drawScale;
	s.translucency = ti.translucency;
	s.spin = ti.spin;
	s.sway = ti.swayU;
	s.texture = QLatin1String( ti.texture );
	s.cols = ti.cols;
	s.rows = ti.rows;
	s.from = from;
	return s;
}

//! the record's own layers (WTHR DATA class flags: 4 Rainy, 8 Snow)
inline QVector<WwPrecipLayerSpec> wwPrecipFromRecord( unsigned classFlags )
{
	QVector<WwPrecipLayerSpec> out;
	if ( classFlags & 4u )
		out.append( wwPrecipDefault( WwPrecip::Rain, QStringLiteral( "record" ) ) );
	if ( classFlags & 8u )
		out.append( wwPrecipDefault( WwPrecip::SnowFine, QStringLiteral( "record" ) ) );
	return out;
}

/* lane RAINFOG2 (PRECIPFLAG1 + the per-weather rate): the record's own layers from its precipitation shader (WTHR MNAM
 * -> SPGD, gl/precipmath.h spgdKind): none without one (the game draws no particles there: the 4 Rainy-flag and 10
 * Snow-flag vanilla weathers with no MNAM), else one layer of the SPGD's type at the type's default rate x the SPGD's
 * density / 7 (vanilla's). redBits (spgdRedBits): flags = wwPrecipFromRecord (the class flags) again, rate = the
 * default rate again. spgdType: -2 no MNAM, -1 MNAM not read, 0 rain, 1 snow. */
inline QVector<WwPrecipLayerSpec> wwPrecipFromSpgd( int spgdType, double density, unsigned classFlags, int redBits )
{
	QVector<WwPrecipLayerSpec> out = ( redBits & WwPrecip::SpgdRedFlags ) ? wwPrecipFromRecord( classFlags ) : QVector<WwPrecipLayerSpec>();
	if ( !( redBits & WwPrecip::SpgdRedFlags ) ) {
		const int k = WwPrecip::spgdKind( spgdType );
		if ( k == WwPrecip::SpgdRain )
			out.append( wwPrecipDefault( WwPrecip::Rain, QStringLiteral( "record" ) ) );
		else if ( k == WwPrecip::SpgdSnow )
			out.append( wwPrecipDefault( WwPrecip::SnowFine, QStringLiteral( "record" ) ) );
	}
	if ( !( redBits & WwPrecip::SpgdRedRate ) && spgdType >= -1 )
		for ( WwPrecipLayerSpec & s : out )
			s.rateMmH *= WwPrecip::spgdRateScale( density );
	return out;
}

//! the block's layers; `notes` gets one line per dropped key or layer
inline QVector<WwPrecipLayerSpec> wwPrecipParse( const QJsonObject & block, QStringList & notes )
{
	QVector<WwPrecipLayerSpec> out;
	auto get = []( const QJsonObject & o, const char * name ) -> QJsonValue {
		const QString n = QLatin1String( name );
		for ( auto it = o.constBegin(); it != o.constEnd(); ++it )
			if ( it.key().compare( n, Qt::CaseInsensitive ) == 0 )
				return QJsonValue( it.value() );
		return QJsonValue( QJsonValue::Undefined );
	};
	const QJsonValue lv = get( block, "layers" );
	if ( !lv.isArray() ) {
		notes << QStringLiteral( "layers dropped (not-array)" );
		return out;
	}
	static const char * const kKnown[] = { "type", "rate", "density", "size", "drawScale", "wind", "translucency", "spin",
		"sway", "texture", "atlas", "comment" };
	const QJsonArray arr = lv.toArray();
	for ( int i = 0; i < arr.size(); i++ ) {
		const QString where = QStringLiteral( "layers/%1" ).arg( i );
		if ( !arr.at( i ).isObject() ) {
			notes << where + QStringLiteral( " dropped (not-object)" );
			continue;
		}
		const QJsonObject o = arr.at( i ).toObject();
		const int type = WwPrecip::typeOf( get( o, "type" ).toString().trimmed().toLower().toLatin1().constData() );
		if ( type < 0 ) {
			notes << where + QStringLiteral( " dropped (not-type \"%1\")" ).arg( get( o, "type" ).toString() );
			continue;
		}
		for ( auto it = o.constBegin(); it != o.constEnd(); ++it ) {
			bool ok = it.key().startsWith( QLatin1Char( '_' ) );
			for ( const char * k : kKnown )
				ok = ok || it.key().compare( QLatin1String( k ), Qt::CaseInsensitive ) == 0;
			if ( !ok )
				notes << where + QStringLiteral( ": unknown key \"%1\"" ).arg( it.key() );
		}
		WwPrecipLayerSpec s = wwPrecipDefault( type, QStringLiteral( "recx" ) );
		auto num = [&]( const char * name, double lo, double hi, double & dst ) {
			const QJsonValue v = get( o, name );
			if ( v.isUndefined() )
				return;
			if ( !v.isDouble() || !std::isfinite( v.toDouble() ) ) {
				notes << where + QStringLiteral( ": %1 dropped (not-number)" ).arg( QLatin1String( name ) );
				return;
			}
			double x = v.toDouble();
			if ( x < lo || x > hi ) {
				notes << where + QStringLiteral( ": %1 %2 clamped to %3..%4" ).arg( QLatin1String( name ) ).arg( x ).arg( lo ).arg( hi );
				x = std::min( std::max( x, lo ), hi );
			}
			dst = x;
		};
		num( "rate", 0.0, 500.0, s.rateMmH );
		num( "density", 0.0, 100000.0, s.density );
		num( "size", 0.1, 100.0, s.sizeMm );
		num( "drawScale", 0.1, 20.0, s.drawScale );
		num( "wind", 0.0, 2.0, s.windShare );
		num( "translucency", 0.0, 1.0, s.translucency );
		num( "spin", 0.0, 20.0, s.spin );
		num( "sway", 0.0, 200.0, s.sway );
		const QJsonValue tv = get( o, "texture" );
		if ( tv.isString() && !tv.toString().trimmed().isEmpty() )
			s.texture = tv.toString().trimmed();
		const QJsonValue av = get( o, "atlas" );
		if ( !av.isUndefined() ) {
			const QJsonArray a = av.toArray();
			if ( a.size() == 2 && a.at( 0 ).toInt() >= 1 && a.at( 1 ).toInt() >= 1 && a.at( 0 ).toInt() <= 16 && a.at( 1 ).toInt() <= 16 ) {
				s.cols = a.at( 0 ).toInt();
				s.rows = a.at( 1 ).toInt();
			} else {
				notes << where + QStringLiteral( ": atlas dropped (not [columns, rows] 1..16)" );
			}
		}
		out.append( s );
	}
	return out;
}

#endif	// WW_PRECIPSPEC_H

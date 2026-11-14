#ifndef WW_PRECIPTYPES_H
#define WW_PRECIPTYPES_H

/* lane WEATHERRECX2 part 4: precipitation TYPES as files, <root>/Plugins/<Provider>/Weathers/Precip/<name>.recx:
 *   { "version": "1.8", "entries": [ { "precipitationType": "rain", "type": { ... } } ] }
 * Keys of "type" (all optional; "base" names the type it starts from, else the shipped type of that name, else rain):
 *   base, size (mm), variants [[sizeMm, cellMm], ...] (the atlas cells, at most 16), material (water | ice | drySnow |
 *   wetSnow) or mass [coef, exp] (mg = coef D^exp, D mm), shape (drop | pellet | flake | clump) and/or drag {areaCoef,
 *   areaExp, c0, vmax m/s} (precipmath.h Law), melts (bool), wets / settles (0..1, stored for later), rate (mm/h),
 *   texture, atlas [columns, rows], maxMip, drawScale, translucency (0..1), spin (rad/s), sway (game units).
 * The first root's file of one name wins (as a mod manager shows it); every file read, every base and every dropped key
 * goes to `notes`; unknown and mod-namespaced ("Mod:key") keys are kept in the file and logged. The entry's
 * "precipitationType" key claims it: a Weathers scan never claims these (no "weather", no "editorId").
 * Header only (Qt Core): tests/features/weather/precip and notes/weatherrecx2/unit read the same code. */

#include "features/weather/precipmath.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

struct WwPrecipTypeFiles
{
	QStringList roots;	// where the last load looked
	QStringList loaded;	// "<name> <- <path>"
};
inline WwPrecipTypeFiles & wwPrecipTypeFiles()
{
	static WwPrecipTypeFiles f;
	return f;
}

//! where a type file of `name` is looked for (the log line of a missing type)
inline QString wwPrecipTypeWhere( const QString & name )
{
	QStringList w;
	for ( const QString & r : wwPrecipTypeFiles().roots )
		w << r + QStringLiteral( "/Plugins/*/Weathers/Precip/" ) + name + QStringLiteral( ".recx" );
	return w.isEmpty() ? QStringLiteral( "no type roots loaded" ) : w.join( QStringLiteral( "; " ) );
}

//! string storage for the registry (TypeInfo holds plain pointers); never freed (a handful per load)
inline const char * wwPrecipKeep( const QString & s )
{
	static QList<QByteArray> kept;
	kept.append( s.toUtf8() );
	return kept.last().constData();
}

//! the gate's red (notes/weatherrecx2/unit/lawgate.cpp): 1 = "base" ignored
inline int & wwPrecipTypesRed()
{
	static int bits = 0;
	return bits;
}

inline QJsonValue wwPrecipGet( const QJsonObject & o, const char * k )
{
	for ( auto it = o.constBegin(); it != o.constEnd(); ++it )
		if ( it.key().compare( QLatin1String( k ), Qt::CaseInsensitive ) == 0 )
			return QJsonValue( it.value() );
	return QJsonValue( QJsonValue::Undefined );
}

//! one type block into the registry
inline bool wwPrecipApplyType( const QString & name, const QJsonObject & t, const QString & where, QStringList & notes )
{
	static const char * const kKnown[] = { "base", "size", "variants", "material", "mass", "shape", "drag", "melts", "wets",
		"settles", "rate", "texture", "atlas", "maxMip", "drawScale", "translucency", "spin", "sway", "comment" };
	for ( auto it = t.constBegin(); it != t.constEnd(); ++it ) {
		const bool ns = it.key().contains( QLatin1Char( ':' ) );
		bool ok = it.key().startsWith( QLatin1Char( '_' ) );
		for ( const char * k : kKnown )
			ok = ok || it.key().compare( QLatin1String( k ), Qt::CaseInsensitive ) == 0;
		if ( !ok )
			notes << where + QStringLiteral( ": %1 key \"%2\" kept, not read" ).arg( ns ? QStringLiteral( "mod" ) : QStringLiteral( "unknown" ), it.key() );
	}
	const QString baseName = ( wwPrecipTypesRed() & 1 ) ? QString() : wwPrecipGet( t, "base" ).toString().trimmed();
	const int own = WwPrecip::typeOf( name.toUtf8().constData() );
	const int fromBase = baseName.isEmpty() ? -1 : WwPrecip::typeOf( baseName.toUtf8().constData() );
	if ( !baseName.isEmpty() && fromBase < 0 )
		notes << where + QStringLiteral( ": base \"%1\" not found (looked for %2): rain's values" ).arg( baseName, wwPrecipTypeWhere( baseName ) );
	else if ( !baseName.isEmpty() )
		notes << where + QStringLiteral( ": base \"%1\"" ).arg( baseName );
	else if ( own < 0 )
		notes << where + QStringLiteral( ": new type \"%1\" with no base: rain's values where it is silent" ).arg( name );
	const int from = fromBase >= 0 ? fromBase : ( baseName.isEmpty() && own >= 0 ? own : int( WwPrecip::Rain ) );
	WwPrecip::TypeInfo ti = WwPrecip::typeInfo( from );
	ti.name = own >= 0 ? WwPrecip::typeInfo( own ).name : wwPrecipKeep( name );
	auto num = [&]( const char * k, double lo, double hi, double & dst ) {
		const QJsonValue v = wwPrecipGet( t, k );
		if ( v.isUndefined() )
			return;
		if ( !v.isDouble() ) {
			notes << where + QStringLiteral( ": %1 dropped (not-number)" ).arg( QLatin1String( k ) );
			return;
		}
		double x = v.toDouble();
		if ( x < lo || x > hi ) {
			notes << where + QStringLiteral( ": %1 %2 clamped to %3..%4" ).arg( QLatin1String( k ) ).arg( x ).arg( lo ).arg( hi );
			x = std::min( std::max( x, lo ), hi );
		}
		dst = x;
	};
	num( "size", 0.1, 100.0, ti.sizeMm );
	num( "rate", 0.0, 500.0, ti.rateMmH );
	num( "drawScale", 0.1, 20.0, ti.drawScale );
	num( "translucency", 0.0, 1.0, ti.translucency );
	num( "spin", 0.0, 20.0, ti.spin );
	num( "sway", 0.0, 200.0, ti.swayU );
	num( "wets", 0.0, 1.0, ti.wets );
	num( "settles", 0.0, 1.0, ti.settles );
	double mip = ti.maxMip;
	num( "maxMip", 0.0, 12.0, mip );
	ti.maxMip = int( mip );
	const QJsonValue melts = wwPrecipGet( t, "melts" );
	if ( melts.isBool() )
		ti.melts = melts.toBool();
	else if ( !melts.isUndefined() )
		notes << where + QStringLiteral( ": melts dropped (not-bool)" );
	const QJsonValue vv = wwPrecipGet( t, "variants" );
	if ( vv.isArray() ) {
		int n = 0;
		for ( const QJsonValue & e : vv.toArray() ) {
			const QJsonArray p = e.toArray();
			if ( n < 16 && p.size() == 2 && p.at( 0 ).toDouble() > 0.0 && p.at( 1 ).toDouble() > 0.0 )
				ti.v[n++] = { p.at( 0 ).toDouble(), p.at( 1 ).toDouble() };
			else
				notes << where + QStringLiteral( ": a variant dropped (not [sizeMm, cellMm] > 0, at most 16)" );
		}
		if ( n > 0 )
			ti.nVariants = n;
	}
	const QString mat = wwPrecipGet( t, "material" ).toString().trimmed();
	if ( !mat.isEmpty() ) {
		static const struct { const char * n; double a, b; } kMat[] = { { "water", 0.5236, 3.0 }, { "ice", 0.47124, 3.0 },
			{ "drySnow", 0.073, 1.4 }, { "wetSnow", 0.037, 1.9 } };
		bool hit = false;
		for ( const auto & m : kMat )
			if ( mat.compare( QLatin1String( m.n ), Qt::CaseInsensitive ) == 0 ) {
				ti.law.massCoef = m.a;
				ti.law.massExp = m.b;
				hit = true;
			}
		if ( !hit )
			notes << where + QStringLiteral( ": material \"%1\" dropped (not water/ice/drySnow/wetSnow)" ).arg( mat );
	}
	const QJsonValue mv = wwPrecipGet( t, "mass" );
	const QJsonArray mass = mv.toArray();
	if ( mass.size() == 2 && mass.at( 0 ).toDouble() > 0.0 ) {
		ti.law.massCoef = mass.at( 0 ).toDouble();
		ti.law.massExp = mass.at( 1 ).toDouble();
	} else if ( !mv.isUndefined() ) {
		notes << where + QStringLiteral( ": mass dropped (not [coef > 0, exp])" );
	}
	const QString shape = wwPrecipGet( t, "shape" ).toString().trimmed();
	if ( !shape.isEmpty() && !WwPrecip::lawShape( shape.toLatin1().constData(), ti.law ) )
		notes << where + QStringLiteral( ": shape \"%1\" dropped (not drop/pellet/flake/clump)" ).arg( shape );
	const QJsonValue drag = wwPrecipGet( t, "drag" );
	if ( drag.isObject() ) {
		const QJsonObject d = drag.toObject();
		auto dn = [&]( const char * k, double lo, double hi, double & dst ) {
			const QJsonValue v = wwPrecipGet( d, k );
			if ( v.isDouble() && v.toDouble() >= lo && v.toDouble() <= hi )
				dst = v.toDouble();
			else if ( !v.isUndefined() )
				notes << where + QStringLiteral( ": drag.%1 dropped (not %2..%3)" ).arg( QLatin1String( k ) ).arg( lo ).arg( hi );
		};
		dn( "areaCoef", 0.01, 100.0, ti.law.areaCoef );
		dn( "areaExp", -2.0, 2.0, ti.law.areaExp );
		dn( "c0", 0.01, 10.0, ti.law.c0 );
		dn( "vmax", 0.0, 100.0, ti.law.vmax );
	}
	const QString tex = wwPrecipGet( t, "texture" ).toString().trimmed();
	if ( !tex.isEmpty() )
		ti.texture = wwPrecipKeep( tex );
	const QJsonValue av = wwPrecipGet( t, "atlas" );
	const QJsonArray at = av.toArray();
	if ( at.size() == 2 && at.at( 0 ).toInt() >= 1 && at.at( 1 ).toInt() >= 1 && at.at( 0 ).toInt() <= 16 && at.at( 1 ).toInt() <= 16 ) {
		ti.cols = at.at( 0 ).toInt();
		ti.rows = at.at( 1 ).toInt();
	} else if ( !av.isUndefined() ) {
		notes << where + QStringLiteral( ": atlas dropped (not [columns, rows] 1..16)" );
	}
	if ( WwPrecip::setType( ti ) < 0 ) {
		notes << where + QStringLiteral( ": dropped (the type table is full, %1)" ).arg( WwPrecip::kMaxTypes );
		return false;
	}
	return true;
}

/*! read every type file under `roots` (first root wins per name) into the registry; a file's base loads first (a cycle
 *  or a missing base is logged). Returns how many types were set. */
inline int wwPrecipLoadTypes( const QStringList & roots, QStringList & notes )
{
	wwPrecipTypeFiles().roots = roots;
	wwPrecipTypeFiles().loaded.clear();
	struct F { QString name, base, path; QJsonObject t; };
	QList<F> files;
	QSet<QString> seen;
	for ( const QString & r : roots ) {
		const QDir plugins( r + QStringLiteral( "/Plugins" ) );
		for ( const QString & p : plugins.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase ) ) {
			const QDir d( plugins.filePath( p ) + QStringLiteral( "/Weathers/Precip" ) );
			for ( const QFileInfo & fi : d.entryInfoList( { QStringLiteral( "*.recx" ) }, QDir::Files, QDir::Name | QDir::IgnoreCase ) ) {
				QFile f( fi.absoluteFilePath() );
				if ( !f.open( QIODevice::ReadOnly ) ) {
					notes << fi.absoluteFilePath() + QStringLiteral( ": unreadable" );
					continue;
				}
				QJsonParseError err;
				const QJsonDocument doc = QJsonDocument::fromJson( f.readAll(), &err );
				if ( err.error != QJsonParseError::NoError ) {
					notes << fi.absoluteFilePath() + QStringLiteral( ": not JSON (%1)" ).arg( err.errorString() );
					continue;
				}
				for ( const QJsonValue & e : doc.object().value( QStringLiteral( "entries" ) ).toArray() ) {
					const QJsonObject o = e.toObject();
					const QString word = wwPrecipGet( o, "precipitationType" ).toString().trimmed();
					if ( word.isEmpty() || !wwPrecipGet( o, "type" ).isObject() ) {
						notes << fi.absoluteFilePath() + QStringLiteral( ": an entry dropped (no \"precipitationType\" name or no \"type\" block)" );
						continue;
					}
					const QByteArray w8 = word.toUtf8();
					const char * alias = WwPrecip::typeAlias( w8.constData() );
					const QString name = alias ? QString::fromLatin1( alias ) : word;
					if ( alias )
						notes << fi.absoluteFilePath() + QStringLiteral( ": type \"%1\" is the old name of \"%2\"" ).arg( word, name );
					if ( seen.contains( name.toLower() ) ) {
						notes << fi.absoluteFilePath() + QStringLiteral( ": %1 hidden by an earlier root's file" ).arg( name );
						continue;
					}
					seen.insert( name.toLower() );
					const QJsonObject t = wwPrecipGet( o, "type" ).toObject();
					files.append( { name, wwPrecipGet( t, "base" ).toString().trimmed(), fi.absoluteFilePath(), t } );
				}
			}
		}
	}
	int set = 0;
	QSet<QString> done, busy;
	std::function<void( int )> load = [&]( int i ) {
		const F & f = files.at( i );
		const QString k = f.name.toLower();
		if ( done.contains( k ) )
			return;
		if ( busy.contains( k ) ) {
			notes << f.path + QStringLiteral( ": base cycle at \"%1\"" ).arg( f.name );
			return;
		}
		busy.insert( k );
		for ( int j = 0; j < files.size(); j++ )
			if ( !f.base.isEmpty() && files.at( j ).name.compare( f.base, Qt::CaseInsensitive ) == 0 )
				load( j );
		if ( !done.contains( k ) && wwPrecipApplyType( f.name, f.t, f.path, notes ) ) {
			set++;
			wwPrecipTypeFiles().loaded << f.name + QStringLiteral( " <- " ) + f.path;
		}
		done.insert( k );
	};
	for ( int i = 0; i < files.size(); i++ )
		load( i );
	return set;
}

#endif	// WW_PRECIPTYPES_H

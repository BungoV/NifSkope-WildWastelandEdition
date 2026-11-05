#include "features/sky/skyplacerun.h"

#include "esmfile.hpp"
#include "esmweather.h"
#include "recx.h"
#include "wwswitch.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QTextStream>

#include <algorithm>
#include <cstdio>
#include <functional>

/* lane INTSKY1 (bungo 11-01, LATE ITEM 27g): the runtime half of gl/skyplace.h. */

namespace
{

constexpr unsigned int kGrup = 0x50555247U;

std::string edidOf( ESMFile & esm, const ESMFile::ESMRecord & r )
{
	ESMFile::ESMField f( esm, r );
	while ( f.next() )
		if ( f == "EDID" ) {
			const char * p = reinterpret_cast<const char *>( f.data() );
			size_t n = f.size();
			while ( n > 0 && p[n - 1] == '\0' )
				n--;
			return std::string( p, n );
		}
	return std::string();
}

QString pluginOf( ESMFile & esm, const QStringList & plugins, quint32 form )
{
	const quint32 idx = esm.getFormIDFile( form );
	return idx < quint32( plugins.size() ) ? QFileInfo( plugins.at( int( idx ) ) ).fileName() : QString();
}

struct Run
{
	bool haveCell = false;
	std::vector<wwplace::World> worlds;
	int world = -1;				// the opened (or the interior's) worldspace, an index into worlds
	QString why;				// how an interior found its worldspace
	QStringList plugins, roots;
	quint64 recxGen = ~0ULL, liveGen = 0;
	int red = -1;
	wwplace::Place place;
	QString summary = QStringLiteral( "sky place: no cell (Boston, the default)" );
	QString said;
};

Run & run()
{
	static Run r;
	return r;
}

bool recxPlace( const QStringList & roots, const QStringList & plugins, const wwplace::World & co, wwplace::Place & out,
	std::string & file, QString * warn )
{
	if ( roots.isEmpty() || co.climatePlugin.empty() )
		return false;
	const WwRecxHit h = wwRecxFind( roots, plugins, QStringLiteral( "Climates" ), QStringLiteral( "climate" ),
		QString::fromStdString( co.climatePlugin ), co.climateLocal, QString::fromStdString( co.climateEdid ),
		QStringLiteral( "location" ), true );
	if ( !h.ok )
		return false;
	auto num = [&h]( const char * k, bool & have, bool * mean = nullptr ) -> double {
		const QJsonValue v = wwRecxBlockValue( h.block, QLatin1String( k ), true );
		have = !v.isUndefined() && !v.isNull();
		if ( mean )
			*mean = v.isString() && v.toString().trimmed().compare( QLatin1String( "mean" ), Qt::CaseInsensitive ) == 0;
		return v.isDouble() ? v.toDouble() : NAN;
	};
	bool hLat = false, hLon = false, hTz = false, tzMean = false, hDate = false;
	const double lat = num( "latitude", hLat ), lon = num( "longitude", hLon ), tz = num( "timeZone", hTz, &tzMean );
	const QJsonValue dv = wwRecxBlockValue( h.block, QStringLiteral( "startDate" ), true );
	hDate = dv.isString();
	std::string why, w;
	if ( !wwplace::placeFromKeys( hLat, lat, hLon, lon, hTz, tzMean, tz, hDate ? dv.toString().toStdString() : std::string(),
			out, why, w ) ) {
		if ( warn )
			*warn += QStringLiteral( "%1: location refused (%2); " ).arg( h.file, QString::fromStdString( why ) );
		return false;
	}
	if ( warn && !w.empty() )
		*warn += QStringLiteral( "%1: %2" ).arg( h.file, QString::fromStdString( w ) );
	file = QDir::toNativeSeparators( h.file ).toStdString() + " (" + h.rung.toStdString() + ")";
	return true;
}

QString clockWords( const wwplace::Place & p )
{
	return p.localMeanTime() ? QStringLiteral( "local mean time" )
		: QStringLiteral( "UTC%1%2 h" ).arg( p.tzHours < 0 ? "" : "+" ).arg( p.tzHours, 0, 'g', 4 );
}

QString lineOf( const std::vector<wwplace::World> & ws, int i, const wwplace::Resolved & r, const QString & why )
{
	const wwplace::Place & p = r.place;
	return QStringLiteral( "%1%2 -> lat %3 lon %4 %5 start %6-%7-%8 by %9 (%10) climate %11" )
		.arg( i >= 0 ? QString::fromStdString( ws[size_t( i )].edid ) : QStringLiteral( "none" ) )
		.arg( why.isEmpty() ? QString() : QStringLiteral( " (%1)" ).arg( why ) )
		.arg( p.latDeg, 0, 'f', 4 ).arg( p.lonDeg, 0, 'f', 4 ).arg( clockWords( p ) ).arg( p.startYear )
		.arg( p.startMonth, 2, 10, QChar( '0' ) ).arg( p.startDay, 2, 10, QChar( '0' ) )
		.arg( QLatin1String( wwplace::ruleName( r.rule ) ) ).arg( QString::fromStdString( r.name ) )
		.arg( r.climateEdid.empty() ? QStringLiteral( "none" ) : QString::fromStdString( r.climateEdid ) );
}

void resolveNow( Run & s )
{
	QString warn;
	const QStringList roots = s.roots, plugins = s.plugins;
	const wwplace::RecxFn fn = [&]( const wwplace::World & co, wwplace::Place & out, std::string & file ) {
		return recxPlace( roots, plugins, co, out, file, &warn );
	};
	const wwplace::Resolved r = wwplace::resolve( s.worlds, s.world, fn, s.red );
	s.place = r.place;
	s.summary = QStringLiteral( "sky place: " ) + lineOf( s.worlds, s.world, r, s.why )
		+ ( s.red ? QStringLiteral( " RED WW_LOCATION_RED=%1" ).arg( wwEnvStr( "WW_LOCATION_RED" ).trimmed() ) : QString() )
		+ ( warn.isEmpty() ? QString() : QStringLiteral( " warnings: %1" ).arg( warn ) );
	if ( s.summary != s.said ) {
		std::fprintf( stderr, "%s\n", qPrintable( s.summary ) );
		s.said = s.summary;
	}
}

} // namespace

int wwSkyPlaceRed()
{
	WW_SWITCH_STATIC( int, red, [] {
		int bits = 0;
		for ( const QString & v : wwEnvStr( "WW_LOCATION_RED" ).trimmed().toLower().split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
			bits |= v == QLatin1String( "table" ) ? int( wwplace::RedTable ) : v == QLatin1String( "noparent" ) ? int( wwplace::RedParent )
				: v == QLatin1String( "norecx" ) ? int( wwplace::RedRecx ) : 0;
		return bits;
	}() );
	return red;
}

std::vector<wwplace::World> wwSkyPlaceWorlds( ESMFile & esm, const QStringList & plugins )
{
	std::vector<wwplace::World> out;
	std::vector<quint32> forms, parents;
	const quint32 label = quint32( 'W' ) | ( quint32( 'R' ) << 8 ) | ( quint32( 'L' ) << 16 ) | ( quint32( 'D' ) << 24 );
	const ESMFile::ESMRecord * r0 = esm.findRecord( 0U );
	for ( unsigned int id = r0 ? r0->next : 0; id; ) {
		const ESMFile::ESMRecord * g = esm.findRecord( id );
		if ( !g )
			break;
		if ( g->type == kGrup && g->formID == 0 && g->flags == label ) {
			for ( unsigned int c = g->children; c; ) {
				const ESMFile::ESMRecord * r = esm.findRecord( c );
				if ( !r )
					break;
				if ( r->type != kGrup && *r == "WRLD" ) {
					wwplace::World w;
					w.edid = edidOf( esm, *r );
					w.plugin = pluginOf( esm, plugins, r->formID ).toStdString();
					w.local = ESMFile::getFormIDLocal( r->formID );
					quint32 wnam = 0, cnam = 0, xlcn = 0;
					ESMFile::ESMField f( esm, *r );
					while ( f.next() ) {
						if ( f == "WNAM" && f.size() >= 4 )
							wnam = esm.mapFormID( *r, f.readUInt32() );
						else if ( f == "PNAM" && f.size() >= 2 )
							w.useFlags = f.readUInt16();
						else if ( f == "CNAM" && f.size() >= 4 )
							cnam = esm.mapFormID( *r, f.readUInt32() );
						else if ( f == "XLCN" && f.size() >= 4 )
							xlcn = esm.mapFormID( *r, f.readUInt32() );
					}
					w.location = xlcn;
					if ( const ESMFile::ESMRecord * cr = cnam ? esm.findRecord( cnam ) : nullptr ) {
						if ( *cr == "CLMT" ) {
							w.climateEdid = edidOf( esm, *cr );
							w.climatePlugin = pluginOf( esm, plugins, cnam ).toStdString();
							w.climateLocal = ESMFile::getFormIDLocal( cnam );
						}
					}
					forms.push_back( r->formID );
					parents.push_back( wnam );
					out.push_back( w );
				}
				c = r->next;
			}
		}
		id = g->next;
	}
	for ( size_t i = 0; i < out.size(); i++ )
		for ( size_t j = 0; j < forms.size() && parents[i]; j++ )
			if ( forms[j] == parents[i] && j != i )
				out[i].parent = int( j );
	return out;
}

std::vector<std::uint32_t> wwSkyPlaceLocationChain( ESMFile & esm, quint32 cellForm )
{
	std::vector<std::uint32_t> chain;
	const ESMFile::ESMRecord * r = cellForm ? esm.findRecord( cellForm ) : nullptr;
	const char * want = "XLCN";
	for ( int guard = 0; r && r->type != kGrup && guard < 32; guard++ ) {
		quint32 next = 0;
		ESMFile::ESMField f( esm, *r );
		while ( f.next() )
			if ( f == want && f.size() >= 4 ) {
				next = esm.mapFormID( *r, f.readUInt32() );
				break;
			}
		if ( !next || std::find( chain.begin(), chain.end(), next ) != chain.end() )
			break;
		chain.push_back( next );
		r = esm.findRecord( next );
		if ( r && !( *r == "LCTN" ) )
			break;
		want = "PNAM";	// an LCTN's parent location
	}
	return chain;
}

void wwSkyPlaceSetCell( ESMFile * esm, const QStringList & plugins, const QString & dataDir, quint32 worldForm,
	quint32 interiorCellForm, const QString & interiorPlugin )
{
	Run & s = run();
	s = Run();
	if ( !esm )
		return;
	s.haveCell = true;
	s.plugins = plugins;
	s.roots = wwRecxRoots( plugins, dataDir );
	s.worlds = wwSkyPlaceWorlds( *esm, plugins );
	if ( interiorCellForm ) {
		std::string why;
		s.world = wwplace::interiorWorld( s.worlds, wwSkyPlaceLocationChain( *esm, interiorCellForm ), interiorPlugin.toStdString(), why );
		s.why = QStringLiteral( "interior by %1" ).arg( QString::fromStdString( why ) );
	} else {
		const QString p = pluginOf( *esm, plugins, worldForm );
		const quint32 local = ESMFile::getFormIDLocal( worldForm );
		for ( size_t i = 0; i < s.worlds.size(); i++ )
			if ( s.worlds[i].local == local && QString::fromStdString( s.worlds[i].plugin ).compare( p, Qt::CaseInsensitive ) == 0 )
				s.world = int( i );
	}
	s.red = wwSkyPlaceRed();
	s.liveGen = wwSwitchGen();
	s.recxGen = wwRecxScan( s.roots, QStringLiteral( "Climates" ) );
	resolveNow( s );
}

const wwplace::Place & wwSkyPlaceNow()
{
	Run & s = run();
	if ( !s.haveCell ) {
		static const wwplace::Place boston;
		return boston;
	}
	const int red = wwSkyPlaceRed();
	const quint64 g = wwRecxScan( s.roots, QStringLiteral( "Climates" ) );	// rate-limited inside (half a second)
	if ( red != s.red || g != s.recxGen ) {
		s.red = red;
		s.recxGen = g;
		resolveNow( s );
	}
	return s.place;
}

QString wwSkyPlaceSummary()
{
	return run().summary;
}

int wwSkyPlaceCli( const QStringList & args )
{
	QTextStream out( stdout ), err( stderr );
	QStringList plugins, interiors;
	QString data = EsmWeather::gameDataDir();
	for ( int i = 0; i < args.size(); i++ ) {
		if ( args.at( i ) == QLatin1String( "--plugins" ) && i + 1 < args.size() ) {
			for ( const QString & p : args.at( ++i ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
				plugins << p.trimmed();
		} else if ( args.at( i ) == QLatin1String( "--data" ) && i + 1 < args.size() ) {
			data = args.at( ++i );
		} else if ( args.at( i ) == QLatin1String( "--interior" ) && i + 1 < args.size() ) {
			interiors << args.at( ++i );
		}
	}
	if ( plugins.isEmpty() ) {
		err << "skyplace: needs --plugins <a.esm,b.esm,...> (load order)" << Qt::endl;
		return 2;
	}
	try {
		ESMFile esm( plugins.join( QLatin1Char( ',' ) ).toLocal8Bit().constData() );
		const std::vector<wwplace::World> ws = wwSkyPlaceWorlds( esm, plugins );
		const QStringList roots = wwRecxRoots( plugins, data );
		const int red = wwSkyPlaceRed();
		QString warn;
		const wwplace::RecxFn fn = [&]( const wwplace::World & co, wwplace::Place & o, std::string & file ) {
			return recxPlace( roots, plugins, co, o, file, &warn );
		};
		int unresolved = 0;
		int byRule[8] = {};
		for ( size_t i = 0; i < ws.size(); i++ ) {
			const wwplace::Resolved r = wwplace::resolve( ws, int( i ), fn, red );
			byRule[r.rule]++;
			if ( !std::isfinite( r.place.latDeg ) || !std::isfinite( r.place.lonDeg ) )
				unresolved++;
			out << "skyplace: " << QString::fromStdString( ws[i].plugin ) << " " << lineOf( ws, int( i ), r, QString() ) << Qt::endl;
		}
		// interiors by EditorID: walk the CELL top group once
		int intDone = 0;
		if ( !interiors.isEmpty() ) {
			const quint32 label = quint32( 'C' ) | ( quint32( 'E' ) << 8 ) | ( quint32( 'L' ) << 16 ) | ( quint32( 'L' ) << 24 );
			std::function<void( unsigned int, int )> walk = [&]( unsigned int id, int depth ) {
				for ( ; id; ) {
					const ESMFile::ESMRecord * r = esm.findRecord( id );
					if ( !r )
						return;
					if ( r->type == kGrup ) {
						const bool enter = depth == 0 ? ( r->formID == 0 && r->flags == label ) : ( r->formID == 2 || r->formID == 3 );
						if ( enter && r->children )
							walk( r->children, depth + 1 );
					} else if ( depth > 0 && *r == "CELL" ) {
						const QString e = QString::fromStdString( edidOf( esm, *r ) );
						if ( interiors.contains( e, Qt::CaseInsensitive ) || interiors.contains( QStringLiteral( "*" ) ) ) {
							std::string why;
							const int wi = wwplace::interiorWorld( ws, wwSkyPlaceLocationChain( esm, r->formID ),
								pluginOf( esm, plugins, r->formID ).toStdString(), why );
							const wwplace::Resolved rr = wwplace::resolve( ws, wi, fn, red );
							out << "skyplace interior: " << e << " -> " << lineOf( ws, wi, rr, QStringLiteral( "by %1" )
								.arg( QString::fromStdString( why ) ) ) << Qt::endl;
							intDone++;
						}
					}
					id = r->next;
				}
			};
			const ESMFile::ESMRecord * r0 = esm.findRecord( 0U );
			if ( r0 )
				walk( r0->next, 0 );
		}
		out << "skyplace: " << ws.size() << " worldspaces, unresolved " << unresolved << ", by rule recx " << byRule[wwplace::RuleRecx]
			<< " table(worldspace) " << byRule[wwplace::RuleTableWorld] << " parent " << byRule[wwplace::RuleParent]
			<< " table(climate) " << byRule[wwplace::RuleTableClimate] << " default " << byRule[wwplace::RuleDefault]
			<< "; interiors " << intDone << ( red ? QStringLiteral( "; RED %1" ).arg( wwEnvStr( "WW_LOCATION_RED" ) ) : QString() )
			<< ( warn.isEmpty() ? QString() : QStringLiteral( "; warnings: %1" ).arg( warn ) ) << Qt::endl;
		return unresolved ? 1 : 0;
	} catch ( std::exception & e ) {
		err << "skyplace: " << e.what() << Qt::endl;
		return 1;
	}
}

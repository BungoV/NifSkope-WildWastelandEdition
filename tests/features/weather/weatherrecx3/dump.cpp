/* lane WEATHERRECX3: dump every resolved weather parameter of the REAL .recx reader (weatherrecx.cpp + recx.cpp +
 * wwswitch.cpp), no GL and no window. Built twice by gate.py: against origin/modules1-20261105's sources (-DWX3_OLD,
 * the reader the 1.4 files were written for) and against this tree's (the 1.8 reader). The mod root is WW_RECX_ROOT.
 * Usage: dump <plan.tsv> <hours, comma list>  -> per weather and hour one line "edid|hour" then key=value (tab).
 * Plan columns (tab): edid owner lidHex flags windSpeed windDir windTurb transDelta precipIn precipOut thunderIn
 *   thunderOut precipType precipDensity thunder
 * Besides the struct, the consumers' helpers (haze, haze optics, air filters, wind bytes, ozone, textures) are dumped:
 * a key the file leaves unset resolves there to the value the picture uses. Branch only: "stored." keys (spec "stored
 * for later"; not compared by the gate) and "source". */
#include "features/weather/weathergen.h"
#include "features/weather/weatherrecx.h"
#include "features/weather/wxoverride.h"
#include "gl/celllooks.h"

#include <QCoreApplication>
#include <QFile>
#include <QStringList>

#include <cstdio>

static double g_hour = 14.0;
static QString g_key, g_edid, g_owner, g_root;
static quint32 g_lid = 0;
static WwWeatherGen::In g_gen;

bool wwLookOn( WwLook l )
{
	return l == WwLook::WeatherRecx;
}
double wwLookdevHour()
{
	return g_hour;
}
QString wwLookdevWeatherKey()
{
	return g_key;
}
bool wwLookdevWeatherId( quint32 & formID, QString & edid, QString & owner, QStringList & files, QString & dataDir )
{
	formID = g_lid;
	edid = g_edid;
	owner = g_owner;
	files = { g_root + QLatin1Char( '/' ) + g_owner };
	dataDir = g_root;
	return true;
}
int wwLookdevWeatherGen( WwWeatherGen::In in[2], quint32 formID[2], float & pct )
{
	in[0] = g_gen;
	formID[0] = g_lid;
	pct = 1.0f;
	return 1;
}
unsigned long long wwWxOvStamp()
{
	return 0;
}
float wwWxOv( WwWx, float weather )
{
	return weather;
}

namespace
{
QStringList g_out;
void put( const char * k, double v )
{
	g_out << QStringLiteral( "%1=%2" ).arg( QLatin1String( k ) ).arg( v, 0, 'g', 9 );
}
void put( const QString & k, double v )
{
	g_out << QStringLiteral( "%1=%2" ).arg( k ).arg( v, 0, 'g', 9 );
}
void puts( const char * k, const QString & v )
{
	g_out << QStringLiteral( "%1=%2" ).arg( QLatin1String( k ), v.isEmpty() ? QStringLiteral( "-" ) : v );
}
void put3( const char * k, const float v[3] )
{
	for ( int i = 0; i < 3; i++ )
		put( QStringLiteral( "%1.%2" ).arg( QLatin1String( k ) ).arg( i ), v[i] );
}

void dump( const WwWeatherRecx & r )
{
	put( "on", r.on );
	put( "recordFirst", r.recordFirst );
	put( "visibilityM", r.visibilityM );
	put( "hazeHeightM", r.hazeHeightM );
	put( "ozone", r.ozone );
	put( "coverage", r.coverage );
	put( "density", r.density );
	put( "type", r.type );
	put( "layers", r.layers );
	for ( int i = 0; i < 32; i++ ) {
		if ( r.layerBase[i] < 0.0f && r.layerTop[i] < 0.0f && r.layerLwc[i] < 0.0f && r.layerRadius[i] < 0.0f
			&& r.layerIce[i] < 0.0f && r.layerAnvil[i] < 0.0f && r.layerType[i] < 0.0f )
			continue;
		put( QStringLiteral( "layer%1.base" ).arg( i ), r.layerBase[i] );
		put( QStringLiteral( "layer%1.top" ).arg( i ), r.layerTop[i] );
		put( QStringLiteral( "layer%1.lwc" ).arg( i ), r.layerLwc[i] );
		put( QStringLiteral( "layer%1.radius" ).arg( i ), r.layerRadius[i] );
		put( QStringLiteral( "layer%1.ice" ).arg( i ), r.layerIce[i] );
		put( QStringLiteral( "layer%1.anvil" ).arg( i ), r.layerAnvil[i] );
		put( QStringLiteral( "layer%1.type" ).arg( i ), r.layerType[i] );
	}
	put( "stars", r.stars );
	put( "moonlight", r.moonlight );
	puts( "hazeType", r.hazeType );
	put( "humidity", r.humidity );
	put( "groundAlbedo", r.groundAlbedo );
	put( "angstrom", r.angstrom );
	put( "hazeAnisotropy", r.hazeAnisotropy );
	put( "airglow", r.airglow );
	put3( "hazeAlbedo", r.hazeAlbedo );
	put3( "hazeGlow", r.hazeGlow );
	put3( "cloudAlbedo", r.cloudAlbedo );
	put3( "inscatterColor", r.inscatterColor );
	put3( "dirInscatter", r.dirInscatter );
	put( "dirInscatterExp", r.dirInscatterExp );
	put( "dirInscatterStart", r.dirInscatterStart );
	put( "windSpeed", r.windSpeed );
	put( "windDirection", r.windDirection );
	put( "gusts", r.gusts );
	put( "turbulence", r.turbulence );
	put( "airTemp", r.airTemp );
	put( "lightningFreq", r.lightningFreq );
	put( "cloudToGround", r.cloudToGround );
	put( "lightningDistance", r.lightningDistance );
	put( "transHours", r.transHours );
	put( "precipStart", r.precipStart );
	put( "precipEnd", r.precipEnd );
	put( "lightningStart", r.lightningStart );
	put( "lightningEnd", r.lightningEnd );
	put( "wetting", r.wetting );
	put( "drying", r.drying );
	put( "rainRate", r.rainRate );
	put( "snowRate", r.snowRate );
	puts( "precipLayer", r.precipLayer );
	put( "liquidWater", r.liquidWater );
	put( "dropletRadius", r.dropletRadius );
	put( "iceFraction", r.iceFraction );
	put( "evolution", r.evolution );
	put( "precipitationBase", r.precipitationBase );
	put( "shadowOpacity", r.shadowOpacity );
	put( "shadowStrength", r.shadowStrength );
	put( "shadowLayerDensity", r.shadowLayerDensity );
	put( "shadowDeck", r.shadowDeck );
	put( "densityLegacy", r.densityLegacy );
	put( "densityGen", r.densityGen );
	put( "densityRatio", r.densityRatio );
	for ( int i = 0; i < 9; i++ )
		g_out << QStringLiteral( "texture%1=%2" ).arg( i ).arg( r.textures[i].isEmpty() ? QStringLiteral( "-" ) : r.textures[i] );
	puts( "genClass", r.genClass );
	puts( "genRegion", r.genRegion );
	put( "recordRed", r.recordRed );
	put( "phys", r.phys );
	put( "coverAlone", r.coverAlone );
	put( "densityAlone", r.densityAlone );
	put( "heightsAlone", r.heightsAlone );
	put( "stormAlone", r.stormAlone );
	put( "lightningFreqOwn", r.lightningFreqOwn );
	// the consumers' resolution (an unset key -> the value the picture uses)
	for ( int fog = 0; fog < 2; fog++ ) {
		float haze[3] = { 0.05f, 1.2f, 0.0f };
		wwWeatherRecxHaze( haze, fog != 0, 0.012f );
		for ( int i = 0; i < 3; i++ )
			put( QStringLiteral( "use.haze%1.%2" ).arg( fog ).arg( i ), haze[i] );
	}
	put( "use.ozone", wwWeatherRecxOzone() );
	float alb[3] = { -9, -9, -9 }, ang = -9, g = -9, glowA = -9;
	put( "use.optics", wwWeatherRecxHazeOptics( alb, ang, g, glowA ) );
	put3( "use.opticsAlbedo", alb );
	put( "use.opticsAngstrom", ang );
	put( "use.opticsG", g );
	put( "use.opticsAirglow", glowA );
	float glow[3] = { -9, -9, -9 }, ca[3] = { -9, -9, -9 }, ins[3] = { -9, -9, -9 }, dir[3] = { -9, -9, -9 }, de = -9, ds = -9;
	put( "use.filters", wwWeatherRecxAirFilters( glow, ca, ins, dir, de, ds ) );
	put3( "use.glow", glow );
	put3( "use.cloudAlbedo", ca );
	put3( "use.inscatter", ins );
	put3( "use.dirInscatter", dir );
	put( "use.dirExp", de );
	put( "use.dirStart", ds );
	float ws = -9, wd = -9, wt = -9;
	put( "use.wind", wwWeatherRecxWindBytes( ws, wd, wt ) );
	put( "use.windSpeedByte", ws );
	put( "use.windDirByte", wd );
	put( "use.windTurbByte", wt );
#ifndef WX3_OLD
	for ( const QString & s : r.stored )
		g_out << QStringLiteral( "stored.%1" ).arg( s );
	g_out << QStringLiteral( "source=%1" ).arg( r.source.isEmpty() ? QStringLiteral( "-" ) : r.source );
#endif
}
}	// namespace

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	qputenv( "WW_ENV_CACHE_RED", "1" );
	QCoreApplication app( argc, argv );
	if ( argc < 3 )
		return 2;
	g_root = QString::fromLocal8Bit( qgetenv( "WW_RECX_ROOT" ) );
	QList<double> hours;
	for ( const QString & h : QString::fromLatin1( argv[2] ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) )
		hours << h.toDouble();
	QFile f( QString::fromLocal8Bit( argv[1] ) );
	if ( !f.open( QIODevice::ReadOnly ) )
		return 2;
	int n = 0;
	while ( !f.atEnd() ) {
		const QString line = QString::fromUtf8( f.readLine() ).remove( QLatin1Char( '\r' ) ).remove( QLatin1Char( '\n' ) );
		if ( line.isEmpty() || line.startsWith( QLatin1Char( '#' ) ) )
			continue;
		const QStringList c = line.split( QLatin1Char( '\t' ) );
		if ( c.size() < 15 ) {
			std::printf( "BADLINE %d cols\n", int( c.size() ) );
			return 2;
		}
		g_edid = c[0];
		g_owner = c[1];
		g_lid = c[2].toUInt( nullptr, 16 );
		g_gen = WwWeatherGen::In();
		g_gen.edid = c[0].toStdString();
		g_gen.owner = c[1].toStdString();
		g_gen.flags = c[3].toInt();
		g_gen.windSpeed = c[4].toInt();
		g_gen.windDir = c[5].toInt();
		g_gen.windTurb = c[6].toInt();
		g_gen.transDelta = c[7].toInt();
		g_gen.precipIn = c[8].toInt();
		g_gen.precipOut = c[9].toInt();
		g_gen.thunderIn = c[10].toInt();
		g_gen.thunderOut = c[11].toInt();
		g_gen.precipType = c[12].toInt();
		g_gen.precipDensity = c[13].toDouble();
		g_gen.thunder = c[14].toInt() != 0;
		for ( double h : hours ) {
			g_hour = h;
			g_key = QStringLiteral( "case%1" ).arg( n++ );
			std::fprintf( stderr, "== %s %g\n", qPrintable( g_edid ), h );
			g_out.clear();
			dump( wwWeatherRecx() );
			std::printf( "%s|%g\t%s\n", qPrintable( g_edid + QLatin1Char( '@' ) + g_owner ), h,
				qPrintable( g_out.join( QLatin1Char( '\t' ) ) ) );
		}
	}
	return 0;
}

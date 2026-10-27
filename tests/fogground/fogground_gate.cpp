// lane FOGHEIGHT1: the air's ground layer, offline. Links the REAL vaFit / vaNearShare / vaGround / vaDerived (cut by text
// from src/gl/cellvolair.cpp into cut.inc by notes/fogheight1/gate/mk.py) with the real recx.cpp + wwswitch.cpp, stubs the
// Lookdev weather, and prints the uniforms the pass would upload for each weather/hour of a K list.
// env: GATE_ROW (the Show row, 0/1), GATE_BURN (burn-off row), WW_RECX_ROOT (the .recx root), WW_CELL_AIR_GROUND_DERIVED_RED.
#include "recx.h"
#include "wwswitch.h"
#include "esmweather.h"
#include "gl/celllooks.h"

#include <QJsonObject>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

constexpr float kUnitsPerKm = 69930.07f;	// cellvolair.cpp

static QString gEdid;
static quint32 gId = 0;
static double gHour = 0.0;

bool wwLookOn( WwLook l )
{
	if ( l == WwLook::GroundBurnOff )
		return qEnvironmentVariableIntValue( "GATE_BURN" ) != 0;
	return qEnvironmentVariableIntValue( "GATE_ROW" ) != 0;
}
bool wwLookdevWeatherId( quint32 & formID, QString & edid, QString & owner, QStringList & files, QString & dataDir )
{
	formID = gId;
	edid = gEdid;
	owner = QStringLiteral( "Fallout4.esm" );
	files = QStringList{ QStringLiteral( "Fallout4.esm" ) };
	dataDir = QString();
	return true;
}
double wwLookdevHour() { return gHour; }
void wwLookdevClimateTnam( unsigned char out[4] )
{
	const unsigned char t[4] = { 30, 54, 102, 126 };	// the vanilla climate's TNAM default (esmweather.h)
	std::copy( t, t + 4, out );
}
QVector<WwWeatherEntry> wwLookdevWeathers() { return {}; }

namespace
{
#include "cut.inc"
}

int main( int argc, char ** argv )
{
	if ( argc < 2 )
		return 2;
	std::ifstream in( argv[1] );
	std::string line;
	int n = 0;
	while ( std::getline( in, line ) ) {
		std::istringstream ss( line );
		std::string edid, id;
		float K[6][4];
		ss >> edid >> id >> gHour;
		for ( auto & r : K )
			for ( float & v : r )
				ss >> v;
		if ( !ss )
			continue;
		gEdid = QString::fromStdString( edid );
		gId = quint32( std::stoul( id, nullptr, 16 ) );
		VaMedium M = vaFit( K, false, false );
		VaLayer G;
		vaNearShare( K, M, G );
		vaGround( K, M, false, false, G );
#ifndef GATE_BASE
		vaKeepStreet( M, G );	// as the pass does (vaPassInner)
#endif
		std::printf( "%s %.2f | med %a %a %a | layer %a %a %a %a | ground %a %a | recx %s\n", edid.c_str(), gHour,
			double( M.sigma0 ), double( M.zb ), double( M.invH ), double( G.w ), double( G.zn ), double( G.invHn ),
			double( G.s ), double( G.zg ), double( G.invHg ), qPrintable( G.recx ) );
		n++;
	}
	std::fprintf( stderr, "cases %d\n", n );
	return n > 0 ? 0 : 1;
}

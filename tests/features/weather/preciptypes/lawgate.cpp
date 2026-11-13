// WEATHERRECX2 part 4 gate: the one fall-speed law vs the old per-type switch, and the precipitation type files.
// G1 law speed within 7 % of the switch at every atlas size + a dense grid of each type's range; mass equal (1e-9)
// G2 the six shipped type files load and hold today's values (every TypeInfo field equal to the compiled table)
// G3 base inheritance (sleetLike <- hail), aliases (snow, snow_fine), hidden old-name file, mod key kept + logged
// G4 a missing type logs the path it looked for
// Reds (LAWGATE_RED): 1 = the drop preset's C0 doubled (G1 must fail), 2 = no roots loaded (G2 must fail),
// 3 = base ignored (G3 must fail).
#include "features/weather/precipspec.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace WwPrecip;
static int g_fail[5] = {}, g_checks = 0;

static void check( int g, bool ok, const char * what, double got, double want )
{
	g_checks++;
	if ( !ok )
		g_fail[g]++;
	std::printf( "%s G%d %s got %.6g want %.6g\n", ok ? "ok  " : "FAIL", g, what, got, want );
}

static bool sameInfo( const TypeInfo & a, const TypeInfo & b )
{
	bool ok = std::strcmp( a.name, b.name ) == 0 && std::strcmp( a.texture, b.texture ) == 0 && a.cols == b.cols && a.rows == b.rows
		&& a.nVariants == b.nVariants && a.sizeMm == b.sizeMm && a.rateMmH == b.rateMmH && a.drawScale == b.drawScale
		&& a.translucency == b.translucency && a.spin == b.spin && a.swayU == b.swayU && a.maxMip == b.maxMip
		&& a.seedTag == b.seedTag && a.melts == b.melts && a.wets == b.wets && a.settles == b.settles
		&& a.law.massCoef == b.law.massCoef && a.law.massExp == b.law.massExp && a.law.areaCoef == b.law.areaCoef
		&& a.law.areaExp == b.law.areaExp && a.law.c0 == b.law.c0 && a.law.vmax == b.law.vmax;
	for ( int k = 0; ok && k < a.nVariants; k++ )
		ok = a.v[k].sizeMm == b.v[k].sizeMm && a.v[k].cellMm == b.v[k].cellMm;
	return ok;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const int red = std::getenv( "LAWGATE_RED" ) ? std::atoi( std::getenv( "LAWGATE_RED" ) ) : 0;
	const QString root = argc > 1 ? QString::fromLocal8Bit( argv[1] ) : QString();
	std::printf( "lawgate: red %d root %s\n", red, qPrintable( root ) );
	TypeInfo compiled[TypeCount];
	for ( int t = 0; t < TypeCount; t++ )
		compiled[t] = typeInfo( t );
	if ( red == 1 ) {
		TypeInfo r = typeInfo( Rain );
		r.law.c0 *= 2.0;
		setType( r );
	}

	// G1
	char what[200];
	const double lo[TypeCount] = { 0.5, 5, 3, 3, 5, 3 }, hi[TypeCount] = { 6, 50, 50, 50, 25, 50 };
	for ( int t = 0; t < TypeCount; t++ ) {
		double worstV = 0.0, worstM = 0.0, atV = 0.0;
		auto one = [&]( double D ) {
			const double ev = std::fabs( fallSpeedMps( t, D ) / switchFallSpeedMps( t, D ) - 1.0 );
			const double em = std::fabs( massMg( t, D ) / switchMassMg( t, D ) - 1.0 );
			if ( ev > worstV ) {
				worstV = ev;
				atV = D;
			}
			worstM = std::max( worstM, em );
		};
		for ( int k = 0; k < typeInfo( t ).nVariants; k++ )
			one( typeInfo( t ).v[k].sizeMm );
		for ( int i = 0; i <= 40; i++ )
			one( lo[t] * std::pow( hi[t] / lo[t], i / 40.0 ) );
		std::snprintf( what, sizeof what, "%s worst fall-speed error (at %.2f mm), tolerance 0.07", typeInfo( t ).name, atV );
		check( 1, worstV <= 0.07, what, worstV, 0.07 );
		std::snprintf( what, sizeof what, "%s worst mass error", typeInfo( t ).name );
		check( 1, worstM <= 1e-9, what, worstM, 0.0 );
	}

	// G2
	QStringList notes;
	if ( red == 3 )
		wwPrecipTypesRed() = 1;	// "base" ignored: sleetLike takes rain's values where it is silent
	const int n = wwPrecipLoadTypes( red == 2 ? QStringList() : QStringList{ root }, notes );
	for ( const QString & l : notes )
		std::printf( "note: %s\n", qPrintable( l ) );
	check( 2, n == 7, "type files set (6 shipped + sleetLike)", double( n ), 7.0 );
	for ( int t = 0; t < TypeCount; t++ ) {
		const bool same = sameInfo( typeInfo( t ), compiled[t] );	// red 1: the file's "shape": "drop" restores the preset
		std::snprintf( what, sizeof what, "%s from its file = the compiled table (1 = equal)", compiled[t].name );
		check( 2, same, what, same ? 1.0 : 0.0, 1.0 );
	}

	// G3
	const int sl = typeOf( "sleetLike" );
	const bool slOk = sl >= TypeCount && typeInfo( sl ).sizeMm == 4.0
		&& typeInfo( sl ).law.c0 == typeInfo( Hail ).law.c0 && typeInfo( sl ).law.massCoef == typeInfo( Hail ).law.massCoef
		&& std::strcmp( typeInfo( sl ).texture, typeInfo( Hail ).texture ) == 0 && typeInfo( sl ).seedTag > 6;
	check( 3, slOk, "sleetLike = hail + size 4 (1 = yes)", slOk ? 1.0 : 0.0, 1.0 );
	check( 3, typeOf( "snow" ) == SnowFine && typeOf( "snow_fine" ) == SnowFine && typeOf( "SNOWWETCLUMP" ) == SnowWetClump,
		"aliases snow / snow_fine / case-free", double( typeOf( "snow" ) ), double( SnowFine ) );
	bool hidden = false, modKept = false, baseLog = false;
	for ( const QString & l : notes ) {
		hidden = hidden || ( l.contains( QLatin1String( "zzOld.recx" ) ) && l.contains( QLatin1String( "hidden" ) ) );
		modKept = modKept || l.contains( QLatin1String( "mod key \"MyMod:tint\" kept" ) );
		baseLog = baseLog || l.contains( QLatin1String( "base \"hail\"" ) );
	}
	check( 3, hidden && modKept && baseLog, "logs: old-name file hidden, mod key kept, base named (1 = all)",
		hidden && modKept && baseLog ? 1.0 : 0.0, 1.0 );

	// G4
	QStringList pn;
	const QVector<WwPrecipLayerSpec> L = wwPrecipParse(
		QJsonDocument::fromJson( R"({ "layers": [ { "type": "graupel" }, { "type": "snow_big_clump" } ] })" ).object(), pn );
	bool where = false, alias = false;
	for ( const QString & l : pn ) {
		std::printf( "parse note: %s\n", qPrintable( l ) );
		where = where || l.contains( QLatin1String( "Weathers/Precip/graupel.recx" ) );
		alias = alias || l.contains( QLatin1String( "old name of \"snowBigClump\"" ) );
	}
	check( 4, L.size() == 1 && L[0].type == SnowBigClump && where && alias, "missing type logs its path; alias logged (1 = yes)",
		where && alias ? 1.0 : 0.0, 1.0 );

	const int fails = g_fail[1] + g_fail[2] + g_fail[3] + g_fail[4];
	std::printf( "lawgate: %d of %d failed (G1 %d, G2 %d, G3 %d, G4 %d)\nLAWGATE %s\n", fails, g_checks, g_fail[1], g_fail[2],
		g_fail[3], g_fail[4], fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}

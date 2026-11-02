// lane MATLOOK3: src/gl/wwhaireye.h (the twin of res/shaders/ww_hair_eye.glsl) against an independent reading of the
// laws, the design gates (the shifted highlights, a sharp coat), and pins of the shader and C++ text.
// usage: haireye <tree root>; rc 0 = all checks pass. Built with -DWWHAIREYE_RED it must fail (tests/haireye/haireye.sh).
#include "gl/wwhaireye.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace wwhe;

static int fails = 0;
static void check( bool ok, const char * what, double v = 0.0 )
{
	std::printf( "%s %s %.6g\n", ok ? "ok  " : "FAIL", what, v );
	if ( !ok )
		fails++;
}

static V3 cross( V3 a, V3 b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

// independent: Kajiya-Kay as sin(T, H)^n, the shifted tangent as T turned toward N by atan(shift)
static double indHairD( V3 T, V3 H, double n )
{
	const V3 c = cross( T, H );
	const double s = std::sqrt( dot( c, c ) );
	const double th = dot( T, H );
	return std::min( 1.0, std::max( 0.0, th + 1.0 ) ) * ( n + 2.0 ) / ( 2.0 * 3.14159265358979 ) * std::pow( s, n );
}
static double indHair( V3 T, V3 N, V3 L, V3 V, double gloss, double alb, double s1, double s2 )
{
	const double n = std::pow( 2.0, gloss * 10.0 + 1.0 );
	const V3 H = norm( V + L );
	const double b1 = std::atan( s1 ), b2 = std::atan( s2 );
	const V3 T1 = T * std::cos( b1 ) + N * std::sin( b1 ), T2 = T * std::cos( b2 ) + N * std::sin( b2 );
	const double ior = 1.55, f0 = ( ( 1.0 - ior ) / ( 1.0 + ior ) ) * ( ( 1.0 - ior ) / ( 1.0 + ior ) );
	const double F = f0 + ( 1.0 - f0 ) * std::pow( 1.0 - std::max( 0.0, dot( H, L ) ), 5.0 );
	const double v = 0.25 * F * ( indHairD( T1, H, n ) + indHairD( T2, H, n * 0.5 ) * std::sqrt( alb ) );
	return std::min( v, 15.0 ) * 3.14159265358979;
}
// independent: GGX with Heitz's height-correlated Smith G2 = 1 / (1 + Lambda_L + Lambda_V), BRDF = D G F / (4 NL NV)
static double indCoat( V3 Ng, V3 L, V3 V, double rough )
{
	const double NL = dot( Ng, L ), NV = dot( Ng, V );
	if ( NL <= 0.0 || NV <= 0.0 )
		return 0.0;
	const double a = rough * rough, a2 = a * a;
	const V3 H = norm( V + L );
	const double c = dot( Ng, H );
	const double D = a2 / ( 3.14159265358979 * std::pow( c * c * ( a2 - 1.0 ) + 1.0, 2.0 ) );
	auto lam = [a2]( double cs ) { const double t2 = ( 1.0 - cs * cs ) / ( cs * cs ); return ( -1.0 + std::sqrt( 1.0 + a2 * t2 ) ) * 0.5; };
	const double G = 1.0 / ( 1.0 + lam( NL ) + lam( NV ) );
	const double ior = 1.376, f0 = ( ( ior - 1.0 ) / ( ior + 1.0 ) ) * ( ( ior - 1.0 ) / ( ior + 1.0 ) );
	const double F = f0 + ( 1.0 - f0 ) * std::pow( 1.0 - std::max( 0.0, dot( V, H ) ), 5.0 );
	return std::min( D * G * F / ( 4.0 * NL * NV ), 15.0 ) * 3.14159265358979;
}

static std::string readAll( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string root = argc > 1 ? argv[1] : ".";
	const double kPi = 3.14159265358979;

	// 1. the twin equals the independent reading (with the DESIGN constants, not the twin's) over a grid
	{
		double worstH = 0.0, worstE = 0.0;
		const V3 N = { 0, 0, 1 };
		for ( int it = 0; it < 7; it++ )
			for ( int il = 0; il < 9; il++ )
				for ( int iv = 0; iv < 9; iv++ ) {
					const double ta = it * 0.45, la = -1.2 + il * 0.3, lz = 0.2 + 0.1 * il, va = 0.4 - iv * 0.25, vz = 0.3 + 0.08 * iv;
					const V3 T = hairStrand( { std::cos( ta ), std::sin( ta ), 0.3 }, N );
					const V3 L = norm( { std::cos( la ), std::sin( la ), lz } ), V = norm( { std::sin( va ), std::cos( va ), vz } );
					for ( double g : { 0.3, 0.6, 0.9 } ) {
						const double a = hairSpec( T, N, L, V, g, 0.2 ), b = indHair( T, N, L, V, g, 0.2, 0.105, -0.158 );
						worstH = std::max( worstH, std::fabs( a - b ) / std::max( 1e-3, std::fabs( b ) ) );
					}
					const double c = eyeCoat( N, L, V ), d = indCoat( N, L, V, 0.05 );
					worstE = std::max( worstE, std::fabs( c - d ) / std::max( 1e-3, std::fabs( d ) ) );
				}
		check( worstH < 1e-4, "hair twin == independent Kajiya-Kay (design shifts), worst rel err", worstH );
		check( worstE < 1e-4, "cornea twin == independent GGX + Heitz Smith (roughness 0.05), worst rel err", worstE );
	}

	// 2. the shifted highlights: L = V = H in the T-N plane; primary peak at -atan(0.105) = -6.0 deg, secondary at +9.0
	{
		const V3 N = { 0, 0, 1 }, T = { 1, 0, 0 };
		double best = -1.0, bestPhi = 0.0;
		double prev2 = 0, prev1 = 0;
		double localSec = 1e9;
		for ( int k = -300; k <= 300; k++ ) {
			const double phi = k * 0.1 * kPi / 180.0;
			const V3 H = { std::sin( phi ), 0, std::cos( phi ) };
			const double v = hairSpec( T, N, H, H, 0.8, 1.0 );
			if ( v > best ) {
				best = v;
				bestPhi = k * 0.1;
			}
			if ( k > -299 && prev1 > prev2 && prev1 > v && ( k - 1 ) * 0.1 > 2.0 )
				localSec = ( k - 1 ) * 0.1;
			prev2 = prev1;
			prev1 = v;
		}
		check( std::fabs( bestPhi - ( -6.0 ) ) < 0.3, "primary highlight angle (deg, want -6.0)", bestPhi );
		check( std::fabs( localSec - 9.0 ) < 0.5, "secondary highlight angle (deg, want +9.0)", localSec );
		double mx = 0.0;
		for ( int k = -900; k <= 900; k++ ) {
			const double phi = k * 0.1 * kPi / 180.0;
			const V3 H = { std::sin( phi ), 0, std::cos( phi ) };
			mx = std::max( mx, hairSpec( T, N, H, H, 1.0, 1.0 ) );
		}
		check( mx <= 15.0 * 3.141593 + 1e-9, "hair lobe never over the game's clamp 15 pi", mx );
	}

	// 3. the coat: a sharp mirror highlight (clamped at 15 pi) that is gone 5 deg off the mirror; F0 = 0.025
	{
		const V3 N = { 0, 0, 1 };
		const double peak = eyeCoat( N, N, N );
		const double off = 5.0 * kPi / 180.0;
		const V3 L = { std::sin( off ), 0, std::cos( off ) };
		const double side = eyeCoat( N, L, N );
		check( peak > 40.0, "coat peak at the mirror (want the 15 pi clamp)", peak );
		check( side < 0.01 * peak, "coat 5 deg off the mirror / peak (want < 1 %)", side / peak );
		check( std::fabs( eyeFv( N, N ) - std::pow( 0.376 / 2.376, 2.0 ) ) < 1e-5, "cornea F0 (IOR 1.376)", eyeFv( N, N ) );
		check( std::fabs( kHairF0 - std::pow( 0.55 / 2.55, 2.0 ) ) < 1e-5, "hair F0 (IOR 1.55)", kHairF0 );
	}

	// 4. pins: the shader text carries the twin's constants and the call sites
	{
		const std::string glsl = readAll( root + "/res/shaders/ww_hair_eye.glsl" );
		const std::string cl = readAll( root + "/res/shaders/cell_lights.glsl" );
		const std::string fd = readAll( root + "/res/shaders/fo4_default.frag" );
		const std::string gc = readAll( root + "/src/gl/glcontext.cpp" );
		const std::string rd = readAll( root + "/src/gl/renderer.cpp" );
		const std::string lk = readAll( root + "/src/gl/celllooks.cpp" );
		check( glsl.find( "const float wwHairS1 = 0.105, wwHairS2 = -0.158;" ) != std::string::npos, "pin: shader shifts", 0 );
		check( glsl.find( "const float wwHairF0 = 0.046521;" ) != std::string::npos, "pin: shader hair F0", 0 );
		check( glsl.find( "const float wwEyeF0 = 0.025043;" ) != std::string::npos, "pin: shader eye F0", 0 );
		check( glsl.find( "const float wwEyeA = 0.0025;" ) != std::string::npos, "pin: shader coat alpha", 0 );
		check( glsl.find( "return min( s, vec3( 15.0 ) ) * 3.141593;" ) != std::string::npos, "pin: shader hair clamp", 0 );
		check( cl.find( "sL = wwHairSpec( wwHairT, N, L, Vw, gloss, wwHairAlb );" ) != std::string::npos, "pin: cell lights hair", 0 );
		check( cl.find( "wwEyeAdd += t1.rgb * g * a * max( dot( wwEyeN, L ), 0.0 ) * wwEyeCoat( wwEyeN, L, Vw );" ) != std::string::npos,
			"pin: cell lights coat", 0 );
		check( fd.find( "#include \"ww_hair_eye.glsl\"" ) != std::string::npos, "pin: fo4_default includes the lobes", 0 );
		check( fd.find( "wwEyeLive = cellOn && wwEyeOn && ( alphaFlags & 8 ) == 0;" ) != std::string::npos, "pin: opaque eyes only", 0 );
		check( gc.find( "#define WW_CELL_HAIRKK 1\\n" ) != std::string::npos && gc.find( "#define WW_CELL_CORNEA 1\\n" ) != std::string::npos,
			"pin: loader defines", 0 );
		check( rd.find( "bgsm->bHair ) || lsp->isST( ShaderFlags::ST_HairTint )" ) != std::string::npos, "pin: hair gate", 0 );
		check( rd.find( "bgsm->bEnvironmentMappingEye ) || lsp->isST( ShaderFlags::ST_EyeEnvmap )" ) != std::string::npos, "pin: eye gate", 0 );
		check( lk.find( "{ \"Hair specular\", \"WW/CellHairSpecular\" }," ) != std::string::npos
			&& lk.find( "{ \"Eye cornea\", \"WW/CellEyeCornea\" }," ) != std::string::npos, "pin: rows ship off", 0 );
	}
	std::printf( "%s fails %d\n", fails ? "RED" : "GREEN", fails );
	return fails ? 1 : 0;
}

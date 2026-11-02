// lane SKINSSS2 (EYEIRIS1): src/gl/wwiris.h (the twin of wwIrisShift, res/shaders/ww_hair_eye.glsl) against a trace
// of the eye itself in double: a cornea sphere (7.8 mm) over a flat iris plane (limbus 5.85 mm), n 1.376, the atlas
// painted from the front. For views up to 45 degrees off the axis and points inside 0.9 of the limbus, where the
// refracted ray lands on the iris, the twin's uv step must match the trace's: bars (written before the first run) max
// error <= 0.03 and mean <= 0.01 iris radii, while the red (no step) is off by >= 0.05 on average. Plus the atlas rule
// and pins of the shader, loader, row and renderer text. usage: eyeiris <tree root>; rc 0 = all pass.
// Built with -DWWIRIS_RED it must fail (tests/eyeiris/eyeiris.sh).
#include "gl/wwiris.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static int fails = 0;
static void check( bool ok, const char * what, double v = 0.0 )
{
	std::printf( "%s %s %.6g\n", ok ? "ok  " : "FAIL", what, v );
	if ( !ok )
		fails++;
}

static std::string readAll( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

static size_t countOf( const std::string & s, const std::string & k )
{
	size_t n = 0;
	for ( size_t at = s.find( k ); at != std::string::npos; at = s.find( k, at + k.size() ) )
		n++;
	return n;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string root = argc > 1 ? argv[1] : ".";
	const double	RC = 7.8, RL = 5.85, n = 1.376, ZC = -std::sqrt( RC * RC - RL * RL );
	const float	iris[3] = { 0.502f, 0.752f, 0.162f };
	const double	R = iris[2];
	// screen derivatives of uv: an arbitrary skewed pixel footprint (the twin must not care)
	const float	dUx[2] = { 0.0021f, 0.0004f }, dUy[2] = { -0.0006f, 0.0018f };

	double	emax = 0.0, esum = 0.0, rsum = 0.0, outside = 0.0;
	int	count = 0;
	for ( int ia = 0; ia <= 45; ia += 5 ) {
		const double	th = ia * 3.14159265358979 / 180.0;
		for ( int iz = 0; iz < 360; iz += 30 ) {
			const double	ph = iz * 3.14159265358979 / 180.0;
			const double	D[3] = { std::sin( th ) * std::cos( ph ), std::sin( th ) * std::sin( ph ), std::cos( th ) };
			for ( int ix = -12; ix <= 12; ix++ ) {
				for ( int iy = -12; iy <= 12; iy++ ) {
					const double	x = ix * 0.1 * RL, y = iy * 0.1 * RL;
					const double	r2 = ( x * x + y * y ) / ( RL * RL );
					if ( r2 > 1.44 )
						continue;
					// the visible surface over (x, y): the cornea inside the limbus, a plane (the sclera here) outside
					const bool	in = r2 < 1.0;
					const double	z = in ? ZC + std::sqrt( RC * RC - x * x - y * y ) : 0.0;
					double	Nn[3] = { in ? x / RC : 0.0, in ? y / RC : 0.0, in ? ( z - ZC ) / RC : 1.0 };
					const double	dzdx = in ? -x / ( z - ZC ) : 0.0, dzdy = in ? -y / ( z - ZC ) : 0.0;
					// dP per texture uv: u = cu + x / RL * R, v = cv - y / RL * R
					const double	Tu[3] = { RL / R, 0.0, dzdx * RL / R }, Tv[3] = { 0.0, -RL / R, -dzdy * RL / R };
					WwIris::V3	dPx = { float( Tu[0] * dUx[0] + Tv[0] * dUx[1] ), float( Tu[1] * dUx[0] + Tv[1] * dUx[1] ),
						float( Tu[2] * dUx[0] + Tv[2] * dUx[1] ) };
					WwIris::V3	dPy = { float( Tu[0] * dUy[0] + Tv[0] * dUy[1] ), float( Tu[1] * dUy[0] + Tv[1] * dUy[1] ),
						float( Tu[2] * dUy[0] + Tv[2] * dUy[1] ) };
					const float	uv[2] = { float( iris[0] + x / RL * R ), float( iris[1] - y / RL * R ) };
					float	got[2];
					WwIris::shift( uv, iris, { float( x ), float( y ), float( z ) }, { float( D[0] ), float( D[1] ), float( D[2] ) },
						{ float( Nn[0] ), float( Nn[1] ), float( Nn[2] ) }, dPx, dPy, dUx, dUy, got );
					if ( !in ) {
						outside = std::fmax( outside, std::fabs( got[0] ) + std::fabs( got[1] ) );
						continue;
					}
					if ( r2 > 0.81 )
						continue;
					// the trace: refract at the cornea (Snell, vector form), down to the iris plane z = 0
					const double	cosi = Nn[0] * D[0] + Nn[1] * D[1] + Nn[2] * D[2];
					if ( cosi < 0.2 )
						continue;
					const double	eta = 1.0 / n;
					const double	k = 1.0 - eta * eta * ( 1.0 - cosi * cosi );
					if ( k < 0.0 )
						continue;
					double	t[3];
					for ( int c = 0; c < 3; c++ )
						t[c] = -eta * D[c] + ( eta * cosi - std::sqrt( k ) ) * Nn[c];
					if ( t[2] >= 0.0 )
						continue;
					const double	s = -z / t[2];
					const double	qx = x + t[0] * s, qy = y + t[1] * s;
					if ( qx * qx + qy * qy > RL * RL )
						continue;
					const double	tu = ( qx - x ) / RL * R, tv = -( qy - y ) / RL * R;	// the true uv step
					const double	e = std::hypot( got[0] - tu, got[1] - tv ) / R;
					emax = std::fmax( emax, e );
					esum += e;
					rsum += std::hypot( tu, tv ) / R;
					count++;
				}
			}
		}
	}
	check( count > 20000, "trace: samples on the iris", count );
	check( emax <= 0.03, "trace: max error <= 0.03 iris radii (views <= 45 deg)", emax );
	check( esum / count <= 0.01, "trace: mean error <= 0.01 iris radii", esum / count );
	check( rsum / count >= 0.05, "trace: the step it adds is >= 0.05 iris radii on average", rsum / count );
	check( outside <= 1.0e-5, "at and outside the limbus: no step (<= 1e-5 uv, float rounding at the edge)", outside );

	float	l[3];
	WwIris::layout( "Actors\\Character\\Eyes\\EyeBrown.dds", l );
	check( l[2] > 0.16f && l[2] < 0.165f && std::fabs( l[0] - 0.502f ) < 1e-6f && std::fabs( l[1] - 0.752f ) < 1e-6f,
		"atlas: human eye", l[2] );
	WwIris::layout( "textures/actors/character/eyes/ghouleyesjaundice_d.dds", l );
	check( l[2] > 0.0f, "atlas: ghoul eye", l[2] );
	WwIris::layout( "actors/supermutant/supermutanteyes_d.dds", l );
	check( l[2] == 0.0f, "atlas: supermutant (own layout) = no step", l[2] );
	WwIris::layout( nullptr, l );
	check( l[2] == 0.0f, "atlas: no texture", l[2] );

	// pins
	const std::string	glsl = readAll( root + "/res/shaders/ww_hair_eye.glsl" );
	const std::string	frag = readAll( root + "/res/shaders/fo4_default.frag" );
	const std::string	ctx = readAll( root + "/src/gl/glcontext.cpp" );
	const std::string	looks = readAll( root + "/src/gl/celllooks.cpp" );
	const std::string	rd = readAll( root + "/src/gl/renderer.cpp" );
	const std::string	ws = readAll( root + "/src/cellworkspace.cpp" );
	const std::string	pr = readAll( root + "/src/ui/cellpresettable.h" );
	check( glsl.find( "vec2 wwIrisShift( vec2 uv, vec3 V, vec3 N )" ) != std::string::npos, "pin: shader step" );
	check( glsl.find( "const float wwIrisA = 1.333333;" ) != std::string::npos
		&& glsl.find( "const float wwIrisH1 = 0.881917;" ) != std::string::npos
		&& glsl.find( "1.0 / 1.376" ) != std::string::npos, "pin: shader constants = twin" );
	check( countOf( frag, "offset += wwIrisShift( offset, normalize( ViewDir ), normalize( btnMatrix[2] ) );" ) == 1,
		"pin: fo4_default shifts the uv once, before the maps" );
	check( frag.find( "defined( WW_CELL_IRIS )" ) != std::string::npos, "pin: fo4_default includes the lobe file" );
	check( ctx.find( "\"#define WW_CELL_IRIS 1\\n\"" ) != std::string::npos
		&& ctx.find( "WW_CELL_IRIS_RED" ) != std::string::npos, "pin: loader define + red" );
	check( looks.find( "{ \"Eye iris refraction\", \"WW/CellEyeIris\" }" ) != std::string::npos, "pin: row" );
	check( rd.find( "prog->uni1b( \"wwIrisOn\"" ) != std::string::npos
		&& rd.find( "WwIris::layout( " ) != std::string::npos, "pin: renderer gate + atlas" );
	check( ws.find( "CellWorkspaceLook_WW_CELL_IRIS" ) != std::string::npos, "pin: workspace row" );
	check( pr.find( "{ \"CellWorkspaceLook_WW_CELL_IRIS\", '0'," ) != std::string::npos, "pin: preset table: off" );

	std::printf( "%s fails=%d\n", fails ? "EYEIRIS FAIL" : "EYEIRIS PASS", fails );
	return fails ? 1 : 0;
}

// lane MATLOOK1 (TRANSLUC1): src/gl/wwtransl.h (the twin of res/shaders/cell_transl.glsl) against an independent
// reading of FO4CS's BGSM22 translucency law (its packed record: rgb = -(tint x amount), a = -(tag + turbulence)),
// over a grid of normals, light and view directions, materials and lighting-map values; and the shader's text
// pinned to the same law. Build: g++ -std=c++17 -I<tree>/src tests/translucency/translucency.cpp; run from the tree.
// Red: -DWWTRANSL_RED (the upstream FO76 term) must fail.

#include "gl/wwtransl.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace WwTransl;

namespace {

struct Ref4 { float r, g, b, a; };

// FO4CS's payload: 0 when masked or the amount is at or under the gate; a = -(1 or 3 + turbulence)
Ref4 refPayload( V3 colour, bool mixAlbedo, V3 albedo, float lB, float scale, float turbIn, bool masked, bool thick )
{
	float amount = lB * scale;
	if ( masked || !( amount > 0.001f ) )
		return { 0, 0, 0, 0 };
	float tr = mixAlbedo ? colour.x * albedo.x : colour.x;
	float tg = mixAlbedo ? colour.y * albedo.y : colour.y;
	float tb = mixAlbedo ? colour.z * albedo.z : colour.z;
	float t = std::floor( std::min( std::max( turbIn, 0.0f ), 1.0f ) * 127.0f + 0.5f ) / 127.0f;
	return { -std::max( tr * amount, 0.0f ), -std::max( tg * amount, 0.0f ), -std::max( tb * amount, 0.0f ),
			 -( ( thick ? 3.0f : 1.0f ) + t ) };
}

V3 refExcess( Ref4 aux, V3 N, V3 L, V3 V, V3 lc, V3 diff )
{
	bool present = aux.a < -0.5f, thickRec = aux.a < -2.5f;
	if ( !present || thickRec )
		return V3{};
	float turb = std::min( std::max( -aux.a - 1.0f, 0.0f ), 1.0f );
	float vt = std::max( V.x * -L.x + V.y * -L.y + V.z * -L.z, 0.0f );
	float x = ( L.x * -N.x + L.y * -N.y + L.z * -N.z ) * vt * vt;
	float lobeR = std::max( x * ( 1.0f - turb ) + 0.5f * turb, 0.0f );
	V3 ca = v3( std::max( -aux.r, 0.0f ), std::max( -aux.g, 0.0f ), std::max( -aux.b, 0.0f ) );
	return v3( std::max( ca.x * lc.x * lobeR - diff.x, 0.0f ), std::max( ca.y * lc.y * lobeR - diff.y, 0.0f ),
			   std::max( ca.z * lc.z * lobeR - diff.z, 0.0f ) );
}

V3 unit( float th, float ph )
{
	return v3( std::sin( th ) * std::cos( ph ), std::sin( th ) * std::sin( ph ), std::cos( th ) );
}

}	// namespace

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	int fails = 0;
	// 1. the law, on a grid
	std::vector<V3> dirs;
	for ( int i = 0; i < 9; i++ )
		for ( int j = 0; j < 8; j++ )
			dirs.push_back( unit( 3.14159265f * ( i + 0.5f ) / 9.0f, 6.2831853f * j / 8.0f ) );
	const V3 N = v3( 0, 0, 1 );
	const float turbs[] = { 0.0f, 0.004f, 0.3f, 0.5f, 1.0f, 1.7f, -0.2f };
	const float lBs[] = { 0.0f, 0.0005f, 0.25f, 1.0f };
	const float scales[] = { 0.0f, 0.5f, 2.0f };
	const V3 colour = v3( 0.9f, 0.4f, 0.2f ), albedo = v3( 0.5f, 0.6f, 0.1f ), lc = v3( 3.0f, 2.5f, 2.0f );
	long cases = 0, lit = 0;
	double worst = 0.0;
	for ( float turb : turbs )
		for ( float lB : lBs )
			for ( float sc : scales )
				for ( int flags = 0; flags < 8; flags++ ) {
					bool mix = flags & 1, masked = flags & 2, thick = flags & 4;
					Rec r = record( colour, mix, albedo, lB, sc, turb, masked, thick );
					Ref4 aux = refPayload( colour, mix, albedo, lB, sc, turb, masked, thick );
					for ( const V3 & L : dirs )
						for ( const V3 & V : dirs ) {
							float nl = std::max( dot( N, L ), 0.0f );
							for ( int d = 0; d < 2; d++ ) {
								V3 diff = d ? albedo * lc * nl : V3{};
								V3 a = excess( r, N, L, V, lc, diff ), b = refExcess( aux, N, L, V, lc, diff );
								double e = std::max( { std::fabs( a.x - b.x ), std::fabs( a.y - b.y ), std::fabs( a.z - b.z ) } );
								worst = std::max( worst, e );
								cases++;
								if ( b.x + b.y + b.z > 1e-4f )
									lit++;
							}
						}
				}
	std::printf( "law: %ld cases, %ld with an excess, worst |twin - FO4CS| %.3g (bar 1e-5, lit >= 10000)\n", cases, lit, worst );
	if ( !( worst <= 1e-5 ) || lit < 10000 ) {
		std::printf( "FAIL law\n" );
		fails++;
	}
	// 2. the record's gates
	Rec g1 = record( colour, false, albedo, 0.001f, 1.0f, 0.0f, false, false );	// amount = gate: none
	Rec g2 = record( colour, false, albedo, 1.0f, 1.0f, 0.0f, true, false );	// glow mask: none
	Rec g3 = record( colour, false, albedo, 1.0f, 1.0f, 0.0f, false, true );	// thick: no local light
	V3 e3 = excess( g3, N, v3( 0, 0, -1 ), v3( 0, 0, 1 ), lc, V3{} );
	std::printf( "gates: gate kind %d, masked kind %d, thick kind %d excess %.3g\n", g1.kind, g2.kind, g3.kind, e3.x );
	if ( g1.kind != 0 || g2.kind != 0 || g3.kind != 2 || e3.x != 0.0f ) {
		std::printf( "FAIL gates\n" );
		fails++;
	}
	// 3. the shader says the same (text pins on res/shaders/cell_transl.glsl and its call sites)
	auto slurp = []( const char * p ) {
		std::ifstream f( p, std::ios::binary );
		std::stringstream s;
		s << f.rdbuf();
		return s.str();
	};
	const std::string sh = slurp( "res/shaders/cell_transl.glsl" ), cl = slurp( "res/shaders/cell_lights.glsl" );
	const char * pins[] = {
		"if ( emissiveMasked || !( amount > 0.001 ) )",
		"vec3 tint = mixAlbedo ? colour * albedoLin : colour;",
		"floor( clamp( turbulence, 0.0, 1.0 ) * 127.0 + 0.5 ) / 127.0",
		"float v = max( dot( V, -L ), 0.0 );",
		"return max( mix( dot( L, -N ) * v * v, 0.5, turbulence ), 0.0 );",
		"if ( wwTrKind < 0.5 || wwTrKind > 1.5 )",
		"return max( wwTrRec.rgb * lightColour * wwTrLobe( N, L, V, wwTrRec.a ) - diffuseLit, vec3( 0.0 ) );",
	};
	const char * sites[] = {
		"wwTrAdd += wwTrExcess( N, L, Vw, t1.rgb * g * a, wwTrAlb * eOn );",
		"wwTrAdd += wwTrExcess( N, L, Vw, t1.rgb * g * a, vec3( 0.0 ) );",
		"wwTrLive = wwTrKind > 0.5 && wwTrKind < 1.5;",
		"linx += wwTrAdd + cellDirVis * wwTrExcess( N, Ld, Vw, cellDirColor, nl > 0.0",
		"lin += wwTrAdd;",
		"add += wwTrAdd;",
	};
	int missing = 0;
	for ( const char * p : pins )
		if ( sh.find( p ) == std::string::npos ) {
			std::printf( "missing in cell_transl.glsl: %s\n", p );
			missing++;
		}
	for ( const char * p : sites )
		if ( cl.find( p ) == std::string::npos ) {
			std::printf( "missing in cell_lights.glsl: %s\n", p );
			missing++;
		}
	// lane MATLOOK2: the bindless twins (slot 7) and the programs without the cell's lights (the NIF view) take it too
	const std::string fr = slurp( "res/shaders/fo4_default.frag" ), bl = slurp( "src/gl/cellbindless.cpp" );
	const std::pair< const std::string *, const char * > more[] = {
		{ &cl, "#if defined( WW_CELL_TRANSL ) && !( defined( WW_BINDLESS ) && defined( WW_CELL_TRANSL_RED_NOBL ) )" },
		{ &sh, "#define WwTrLightMap sampler2D( wwDraws[wwDrawIdx].tex[7] )" },
		{ &bl, "\"EnvironmentMap\", \"WwTrLightMap\" };" },
		{ &bl, "for ( int i = 0; i < 8; i++ )" },
		{ &fr, "#if defined( WW_CELL_TRANSL ) && !defined( WW_CELLLIGHTS ) && !defined( WW_CELL_TRANSL_RED_NOVIEW )" },
		{ &fr, "color.rgb = sqrt( dLin + wwTrExcess( normal, L, V, D.rgb * D.rgb, dLin ) );" },
		{ &fr, "wwTrRecord( wwTrCol.rgb, wwTrFlags.y > 0.5, albedo * albedo, lm.b, wwTrCol.a, wwTrFlags.w, lm.a * glowMult > 0.0," },
	};
	int moreMissing = 0;
	for ( const auto & m : more )
		if ( m.first->find( m.second ) == std::string::npos ) {
			std::printf( "missing (bindless / view): %s\n", m.second );
			moreMissing++;
		}
	// the record must stand twice in fo4_default.frag: the cell path's and the view's
	size_t recs = 0;
	for ( size_t at = fr.find( "wwTrRecord( wwTrCol.rgb" ); at != std::string::npos; at = fr.find( "wwTrRecord( wwTrCol.rgb", at + 1 ) )
		recs++;
	if ( recs != 2 ) {
		std::printf( "fo4_default.frag records: %zu, want 2\n", recs );
		moreMissing++;
	}
	missing += moreMissing;
	std::printf( "shader pins: %d of %d present\n", int( std::size( pins ) + std::size( sites ) + std::size( more ) + 1 ) - missing,
				 int( std::size( pins ) + std::size( sites ) + std::size( more ) + 1 ) );
	if ( missing || sh.empty() ) {
		std::printf( "FAIL shader text\n" );
		fails++;
	}
	std::printf( fails ? "translucency: FAIL (%d)\n" : "translucency: PASS\n", fails );
	return fails ? 1 : 0;
}

/* The oracle `tests/spells/impostor_draw.sh` builds out of src/impostoroct.cpp
 * and nothing else: no Qt, no OpenGL, no project headers. It prints the same
 * table `impostor_oct_ref.py table N` prints, so the two implementations of
 * `docs/LODGEN_IMPOSTOR_SPEC.md`'s drawing mapping can be diffed line for
 * line. One implementation is a hope; two that agree are a mapping.
 *
 * Kept out of NifSkope.pro deliberately -- it has a main(). */

#include "../../src/impostoroct.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

struct Probe { const char * name; float d[3]; };

const char * kAzim[8] = { "E", "NE", "N", "NW", "W", "SW", "S", "SE" };
const float kAzimDeg[8] = { 0, 45, 90, 135, 180, 225, 270, 315 };

std::vector<Probe> probes()
{
	static std::vector<std::string> names;
	std::vector<Probe> out;
	names.clear();
	names.reserve( 20 );
	const float pi = 3.14159265358979323846f;
	const char * en[2] = { "low", "high" };
	const float ed[2] = { 10.0f, 55.0f };
	for ( int a = 0; a < 8; a++ ) {
		for ( int e = 0; e < 2; e++ ) {
			names.push_back( std::string( kAzim[a] ) + "-" + en[e] );
			const float ra = kAzimDeg[a] * pi / 180.0f, re = ed[e] * pi / 180.0f;
			Probe p;
			p.name = nullptr;
			p.d[0] = std::cos( ra ) * std::cos( re );
			p.d[1] = std::sin( ra ) * std::cos( re );
			p.d[2] = std::sin( re );
			out.push_back( p );
		}
	}
	const float extra[4][3] = {
		{ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.6f, 0.2f, -0.8f }
	};
	const char * extraNames[4] = { "horizon-E", "horizon-N", "top", "below" };
	for ( int k = 0; k < 4; k++ ) {
		names.push_back( extraNames[k] );
		Probe p;
		p.name = nullptr;
		std::memcpy( p.d, extra[k], sizeof( p.d ) );
		out.push_back( p );
	}
	for ( size_t k = 0; k < out.size(); k++ )
		out[k].name = names[k].c_str();
	return out;
}

} // namespace

int main( int argc, char ** argv )
{
	int N = 8;
	if ( argc >= 3 && std::strcmp( argv[1], "table" ) == 0 )
		N = std::atoi( argv[2] );
	else if ( argc >= 2 )
		N = std::atoi( argv[1] );
	if ( N < ImpostorOct::kMinGrid || N > ImpostorOct::kMaxGrid ) {
		std::fprintf( stderr, "oracle: N %d outside %d..%d\n", N,
			ImpostorOct::kMinGrid, ImpostorOct::kMaxGrid );
		return 2;
	}

	const std::vector<Probe> ps = probes();
	for ( size_t k = 0; k < ps.size(); k++ ) {
		int idx[3], gi[3], gj[3];
		float w[3];
		if ( !ImpostorOct::pickFrames( ps[k].d, N, idx, gi, gj, w ) ) {
			std::printf( "%s REFUSED\n", ps[k].name );
			continue;
		}
		std::printf( "%s", ps[k].name );
		for ( int t = 0; t < 3; t++ )
			std::printf( " %d %d %d %.6f", idx[t], gi[t], gj[t], w[t] );
		std::printf( "\n" );
	}
	return 0;
}

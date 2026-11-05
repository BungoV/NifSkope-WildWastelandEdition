// Lane PERFFIX3 (the air pass's uniform readbacks): the CPU mirror the air pass reads instead of glGetUniform*
// (src/gl/celluniform.h WwCellUni::Mirror). What a GL program holds per location is what its last write put there; the
// mirror must answer exactly that, or "not known" (the pass then asks GL, as before). Rows:
//   1 a scalar / vec2 / vec3 / vec4 write reads back bit for bit (as floats or as an int, the way it was written)
//   2 an int written location never answers a float read, nor a float one an int read (the pass asks GL then)
//   3 a later write replaces the value; a vec3 read as 4 floats has 0 in w (glGetUniformfv leaves it, the pass zeroes it)
//   4 an array / matrix write forgets every location it covers (those setters are never mirrored)
//   5 unknown locations (-1, past the end, never written) answer "not known"; reset() forgets all (a relink)
//   6 the bits survive: -0.0, a NaN payload, denormals (the pass compared them with memcmp)
// Red (-DAIRMIRROR_RED): a mirror whose forget() does nothing (an array write would leave a stale value): rows 4 FAIL.
#include "gl/celluniform.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

bool wwSetupMemoChecking() { return false; }

static int fails = 0, rows = 0;
static void check( bool ok, const char * what )
{
	rows++;
	if ( !ok )
		fails++;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what );
}

#ifdef AIRMIRROR_RED
struct RedMirror : WwCellUni::Mirror
{
	void forget( int, int ) {}
};
using M = RedMirror;
#else
using M = WwCellUni::Mirror;
#endif

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	using WwCellUni::K1i;
	using WwCellUni::K1f;
	using WwCellUni::K2f;
	using WwCellUni::K3f;
	using WwCellUni::K4f;
	M m;
	m.on = true;
	const float v4[4] = { 1.5f, -2.25f, 3.0e-5f, 12345.678f };
	m.put( 7, K4f, v4, 4 );
	float o[4] = { 9, 9, 9, 9 };
	check( m.get( 7, K4f, o, 4 ) && std::memcmp( o, v4, 16 ) == 0, "1 vec4 back bit for bit" );
	const float v1 = 0.125f;
	m.put( 2, K1f, &v1, 1 );
	float o1 = 0;
	check( m.get( 2, K4f, &o1, 1 ) && o1 == v1, "1 float back" );
	const int i1 = 37;
	m.put( 3, K1i, &i1, 1 );
	int oi = 0;
	check( m.get( 3, K1i, &oi, 1 ) && oi == 37, "1 int back" );
	const float v2[2] = { 4, 5 };
	m.put( 11, K2f, v2, 2 );
	float o2[2] = {};
	check( m.get( 11, K4f, o2, 2 ) && o2[0] == 4 && o2[1] == 5, "1 vec2 back" );
	check( !m.get( 3, K4f, o, 1 ), "2 an int location refuses a float read" );
	check( !m.get( 7, K1i, &oi, 1 ), "2 a float location refuses an int read" );
	const float v3[3] = { 0.5f, 0.25f, 0.125f };
	m.put( 7, K3f, v3, 3 );
	float o3[4] = { 9, 9, 9, 9 };
	check( m.get( 7, K4f, o3, 4 ) && o3[0] == 0.5f && o3[1] == 0.25f && o3[2] == 0.125f && o3[3] == 0.0f, "3 a vec3 over a vec4: the new value, w 0" );
	m.put( 20, K4f, v4, 4 );
	m.put( 21, K4f, v4, 4 );
	m.put( 22, K4f, v4, 4 );
	m.forget( 20, 2 );	// an array write over 20..21 (uni4fv_l, uni4m_l, ...)
	check( !m.get( 20, K4f, o, 4 ) && !m.get( 21, K4f, o, 4 ), "4 an array write forgets the locations it covers" );
	check( m.get( 22, K4f, o, 4 ), "4 and only those" );
	check( !m.get( -1, K4f, o, 4 ) && !m.get( 999, K4f, o, 4 ) && !m.get( 5, K4f, o, 4 ), "5 -1, past the end, never written: not known" );
	m.reset();
	check( !m.get( 2, K4f, o, 1 ) && !m.get( 3, K1i, &oi, 1 ), "5 reset (a relink) forgets every location" );
	float odd[4] = { -0.0f, std::numeric_limits<float>::quiet_NaN(), 1.0e-40f, std::numeric_limits<float>::infinity() };
	m.put( 4, K4f, odd, 4 );
	float oo[4] = {};
	check( m.get( 4, K4f, oo, 4 ) && std::memcmp( oo, odd, 16 ) == 0, "6 -0, NaN, a denormal, inf: the same bits" );
	std::printf( "AIRMIRROR %s (%d of %d rows)%s\n", fails ? "FAIL" : "PASS", rows - fails, rows,
#ifdef AIRMIRROR_RED
		" [red: forget does nothing]"
#else
		""
#endif
	);
	return fails ? 1 : 0;
}

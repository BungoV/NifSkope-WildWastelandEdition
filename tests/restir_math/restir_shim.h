// Lane RESTIRDI1 (2026-10-30): just enough GLSL for C++ to compile src/rtddgi/restir_common.glsl as written (the
// rtddgi_math glsl_shim.h pattern). Include inside namespace glsl, after <cmath>; it includes nothing. Scalar functions
// take double and return float, so a 0.5 literal never makes a call ambiguous.

typedef unsigned int uint;

struct vec3
{
	float x = 0, y = 0, z = 0;
	vec3() = default;
	explicit vec3( double s ) : x( float( s ) ), y( float( s ) ), z( float( s ) ) {}
	vec3( double a, double b, double c ) : x( float( a ) ), y( float( b ) ), z( float( c ) ) {}
};
struct vec4
{
	float x = 0, y = 0, z = 0, w = 0;
	vec4() = default;
	explicit vec4( double s ) : x( float( s ) ), y( float( s ) ), z( float( s ) ), w( float( s ) ) {}
	vec4( double a, double b, double c, double d ) : x( float( a ) ), y( float( b ) ), z( float( c ) ), w( float( d ) ) {}
};

inline float abs( double v ) { return float( std::fabs( v ) ); }
inline float clamp( double v, double a, double b ) { return float( v < a ? a : v > b ? b : v ); }
inline float min( double a, double b ) { return float( a < b ? a : b ); }
inline float max( double a, double b ) { return float( a > b ? a : b ); }
inline float pow( double a, double b ) { return std::pow( float( a ), float( b ) ); }
inline float sqrt( double a ) { return std::sqrt( float( a ) ); }
inline float cos( double a ) { return std::cos( float( a ) ); }
inline float sin( double a ) { return std::sin( float( a ) ); }
inline float length( vec3 a ) { return std::sqrt( a.x * a.x + a.y * a.y + a.z * a.z ); }

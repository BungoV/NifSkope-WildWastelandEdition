// Lane AIRZONE1 (2026-10-29): just enough GLSL for C++ to compile res/shaders/cell_airzone.glsl as written, so the unit
// test runs the shader's own text against src/airzonemath.h (the form of tests/rtddgi_math/glsl_shim.h). Include inside
// namespace glsl, after <cmath> (this file includes nothing). Scalar functions take double and return float (GLSL's
// float maths, rounded once more), so a literal like 0.5 never makes a call ambiguous.

#define uniform

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

inline vec3 operator+( vec3 a, vec3 b ) { return vec3( a.x + b.x, a.y + b.y, a.z + b.z ); }
inline vec3 operator-( vec3 a, vec3 b ) { return vec3( a.x - b.x, a.y - b.y, a.z - b.z ); }
inline vec3 operator*( vec3 a, vec3 b ) { return vec3( a.x * b.x, a.y * b.y, a.z * b.z ); }
inline vec3 operator*( vec3 a, double s )
{
	const float f = float( s );
	return vec3( a.x * f, a.y * f, a.z * f );
}

inline float abs( double v ) { return float( std::fabs( v ) ); }
inline float floor( double v ) { return float( std::floor( v ) ); }
inline float sqrt( double v ) { return std::sqrt( float( v ) ); }
inline float exp( double v ) { return std::exp( float( v ) ); }
inline float clamp( double v, double a, double b ) { return float( v < a ? a : v > b ? b : v ); }
inline float min( double a, double b ) { return float( a < b ? a : b ); }
inline float max( double a, double b ) { return float( a > b ? a : b ); }
inline float pow( double a, double b ) { return std::pow( float( a ), float( b ) ); }
inline float smoothstep( double e0, double e1, double x )
{
	const float t = clamp( ( float( x ) - float( e0 ) ) / ( float( e1 ) - float( e0 ) ), 0.0, 1.0 );
	return t * t * ( 3.0f - 2.0f * t );
}
inline float dot( vec3 a, vec3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length( vec3 a ) { return std::sqrt( dot( a, a ) ); }

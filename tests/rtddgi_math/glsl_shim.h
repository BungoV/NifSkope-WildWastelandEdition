// Lane RTXGI1 M2 (2026-10-29): just enough GLSL for C++ to compile res/shaders/cell_ddgi.glsl as written, so the unit
// test runs the shader's own text against rtddgimath.h. Include inside namespace glsl, after <cmath> (this file
// includes nothing: a standard header opened inside a namespace breaks). The scalar functions take double and return
// float (GLSL's float maths, rounded once more), so a literal like 0.5 never makes a call ambiguous.

#define uniform

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
struct ivec4
{
	int x = 0, y = 0, z = 0, w = 0;
};
struct samplerBuffer
{
	const float * data = nullptr;   // RGBA32F texels
	int texels = 0;
	mutable int outside = 0;        // reads past the end (a layout bug: the check fails on any)
};

inline vec3 operator+( vec3 a, vec3 b ) { return vec3( a.x + b.x, a.y + b.y, a.z + b.z ); }
inline vec3 operator-( vec3 a, vec3 b ) { return vec3( a.x - b.x, a.y - b.y, a.z - b.z ); }
inline vec3 operator*( vec3 a, vec3 b ) { return vec3( a.x * b.x, a.y * b.y, a.z * b.z ); }
inline vec3 operator*( vec3 a, double s ) { return vec3( a.x * float( s ), a.y * float( s ), a.z * float( s ) ); }
inline vec4 operator+( vec4 a, vec4 b ) { return vec4( a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w ); }
inline vec4 operator*( vec4 a, double s )
{
	const float f = float( s );
	return vec4( a.x * f, a.y * f, a.z * f, a.w * f );
}

inline float abs( double v ) { return float( std::fabs( v ) ); }
inline float floor( double v ) { return float( std::floor( v ) ); }
inline float clamp( double v, double a, double b ) { return float( v < a ? a : v > b ? b : v ); }
inline float min( double a, double b ) { return float( a < b ? a : b ); }
inline float max( double a, double b ) { return float( a > b ? a : b ); }
inline float pow( double a, double b ) { return std::pow( float( a ), float( b ) ); }
inline float dot( vec3 a, vec3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length( vec3 a ) { return std::sqrt( dot( a, a ) ); }
inline vec3 normalize( vec3 a ) { return a * ( 1.0 / double( length( a ) ) ); }
inline vec4 texelFetch( const samplerBuffer & s, int i )
{
	if ( i < 0 || i >= s.texels ) {
		s.outside++;
		return vec4( 0.0 );
	}
	const float * t = s.data + size_t( i ) * 4;
	return vec4( t[0], t[1], t[2], t[3] );
}

int cellGiLoop8 = 8;   // cell_lights.glsl's uniform loop bound

// Lane HORIZONBAND1: just enough GLSL for C++ to compile res/shaders/atmo_skyband.glsl as written (scalars and vec3).
// Include inside namespace glsl, after <cmath>. Scalar functions take double and return float (GLSL's float maths).

#define uniform

struct vec3
{
	float x = 0, y = 0, z = 0;
	vec3() = default;
	explicit vec3( double s ) : x( float( s ) ), y( float( s ) ), z( float( s ) ) {}
	vec3( double a, double b, double c ) : x( float( a ) ), y( float( b ) ), z( float( c ) ) {}
};

inline vec3 operator+( vec3 a, vec3 b ) { return vec3( a.x + b.x, a.y + b.y, a.z + b.z ); }
inline vec3 operator*( vec3 a, double s ) { return vec3( a.x * float( s ), a.y * float( s ), a.z * float( s ) ); }

inline float clamp( double v, double a, double b ) { return float( v < a ? a : v > b ? b : v ); }
inline float min( double a, double b ) { return float( a < b ? a : b ); }
inline float max( double a, double b ) { return float( a > b ? a : b ); }

// Lane RTEMIS1 part 2: the vec3 operators restirgi_common.glsl needs on top of tests/restir_math/restir_shim.h (include
// after it, inside namespace glsl; it includes nothing).

inline vec3 operator+( vec3 a, vec3 b ) { return vec3( a.x + b.x, a.y + b.y, a.z + b.z ); }
inline vec3 operator-( vec3 a, vec3 b ) { return vec3( a.x - b.x, a.y - b.y, a.z - b.z ); }
inline vec3 operator*( vec3 a, double s ) { return vec3( a.x * float( s ), a.y * float( s ), a.z * float( s ) ); }
inline float dot( vec3 a, vec3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 cross( vec3 a, vec3 b ) { return vec3( a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x ); }
inline vec3 normalize( vec3 a ) { return a * ( 1.0 / double( length( a ) ) ); }
struct ivec2 { int x, y; ivec2( int a, int b ) : x( a ), y( b ) {} };   // rgSnapTraced (RTEMIS1 11-08)

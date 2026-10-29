#ifndef AIRZONEMATH_H
#define AIRZONEMATH_H

/* lane AIRZONE1 (bungo 10-29): HAND-PLACED VOLUMES, the math. No Qt, header-only.
 *
 * One kind of volume, placed by hand per cell (src/airzone.h reads and writes them; the spec is RECX_SPEC_volumes):
 * a shape (box, cylinder, wedge, capsule, sphere: the GI volumes' five, lane PERF1, same size and rotation rules), a
 * soft edge, a priority, and up to three blocks: `gi` (a GI room or threshold), `fog` (the air's density, colour, height
 * falloff, noise inside the shape), `particles` (the lit particles' density, light response and tint inside it).
 *
 * The GLSL read is res/shaders/cell_airzone.glsl, written in a C++-compatible subset so tests/airzone compiles the
 * shader's own text against the reference here (as RTXGI1 M2 did for cell_ddgi.glsl). Each reference function names
 * its GLSL twin; the GLSL carries `MIRROR airzonemath.h <fn>`. Change both or neither.
 *
 *   weight   w(P) = smoothstep(0, edge, -sd(P)) (1 deep inside, 0 on the surface and outside; the shape is the OUTER
 *            extent, so a zone never leaks past what was placed); edge 0 = hard. Red hardedge: sd <= 0 ? 1 : 0.
 *   order    the zones that carry the block, by priority (high first), then file order. Each takes its share
 *            s = r w of what is left (r starts at 1, r *= 1 - w), the rest r keeps the outside value: a high-priority
 *            zone inside a low one wins where it is whole and blends at its edge.
 *   fog      tau' = tau k + add len (one read per sample: the froxel sub-step's middle). k = sum s scale + r;
 *            add = sum s (add / 1000) h(z) n(P) (optical depth per game unit; `add` is per 1000 units),
 *            h = exp(-max(z - zBottom, 0) / heightFalloff), n = max(0, 1 + noise (2 vnoise(P / noiseScale + drift t) - 1));
 *            the light the sub-step scatters x tint, tint = sum s mix(1, colour, colourStrength) + r.
 *   particles  the lit puff's light x light x tint, its opacity a' = 1 - (1 - a)^density (density 1: a untouched).
 * Outside every zone (or with none) k = 1, add = 0, tint = 1 exactly: x 1 and + 0 are exact, so those pixels are the
 * pixels of a cell with no zones, bit for bit. */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace az {

enum ShapeType : int { Box = 0, Cylinder = 1, Wedge = 2, Capsule = 3, Sphere = 4 };

//! the GI volumes' shape (src/proberooms.h GiVolumeShape): size = box edges; cylinder x, y diameters, height; wedge edges
//! (full height at -y, none at +y); capsule diameter, diameter, end to end on z; sphere diameters. Degrees, R = Rz Ry Rx.
struct Shape
{
	int type = Box;
	float center[3] = { 0, 0, 0 };
	float size[3] = { 0, 0, 0 };
	float rotation[3] = { 0, 0, 0 };
};

struct FogBlock
{
	bool on = false;
	float scale = 1.0f;             //!< x the air's own density inside
	float add = 0.0f;               //!< + optical depth per 1000 game units (3 ~ a visibility of 1000 units)
	float heightFalloff = 0.0f;     //!< game units: the added density / e per this much above the shape's bottom; 0 none
	float colour[3] = { 1, 1, 1 };  //!< x the light the air scatters inside (linear rgb)
	float colourStrength = 0.0f;    //!< 0 the air's colour, 1 colour
	float noise = 0.0f;             //!< 0..1: the added density's variation
	float noiseScale = 512.0f;      //!< game units a noise cell
	float noiseDrift[3] = { 0, 0, 0 };   //!< game units a second (the world clock)
};

struct ParticleBlock
{
	bool on = false;
	float density = 1.0f;           //!< a lit sprite's opacity as optical depth x this
	float light = 1.0f;             //!< x the light a lit puff takes from the air
	float tint[3] = { 1, 1, 1 };
	float tintStrength = 0.0f;
	// read and written, not drawn yet (row AIRPART-EMIT: there is no ambient emitter today, census)
	std::string type;
	float size = 1.0f;
	float drift[3] = { 0, 0, 0 };
};

struct GiBlock
{
	bool on = false;
	bool threshold = false;         //!< a threshold (an opening between two rooms), else a room's shape
	int room = 0;                   //!< the GI room grid's id (ProbeRooms::a; the PRTP Debug View's gi: labels)
	int rooms[2] = { 0, 0 };        //!< a threshold's two rooms (0 outdoors)
	float blendWidth = 0.0f;        //!< a threshold's blend, game units each side; 0 = 1.5 GI voxels
	int seq = -1;                   //!< its place in the GI's list (the converter writes today's order); -1 after, in file order
};

struct Volume
{
	std::string name;
	Shape shape;
	float edge = 0.0f;              //!< game units inside the surface over which the weight goes 0 -> 1
	int priority = 0;
	FogBlock fog;
	ParticleBlock part;
	GiBlock gi;
};

// ---- the packing the GLSL reads (MIRROR res/shaders/cell_airzone.glsl AZ_MAX / AZ_VEC4 and azZ's layout)
constexpr int kMax = 8;             //!< zones read in one frame (the nearest by priority, then distance)
constexpr int kVec4 = 9;            //!< vec4s a zone
/* v0 center xyz, type | v1 half size xyz (>= 1e-3), edge | v2 world->local row 0, zBottom | v3 row 1, flags (1 fog,
 * 2 particles) | v4 row 2, 1 / heightFalloff (0 none) | v5 fog scale, add per unit, noise, 1 / noiseScale |
 * v6 fog tint rgb (premixed), bounding radius^2 | v7 noise offset xyz (noise cells: drift t / noiseScale), particle light |
 * v8 particle tint rgb (premixed), particle density */
constexpr int kFlagFog = 1, kFlagPart = 2;
constexpr int kRedHardEdge = 1;     //!< azRed bit (WW_CELL_AIRZONE_RED=hardedge)

//! R = Rz Ry Rx (degrees), row-major R[i][j]: world = R local + center
inline void rotation( const float deg[3], double R[3][3] )
{
	const double d2r = 3.14159265358979323846 / 180.0;
	const double cx = std::cos( deg[0] * d2r ), sx = std::sin( deg[0] * d2r );
	const double cy = std::cos( deg[1] * d2r ), sy = std::sin( deg[1] * d2r );
	const double cz = std::cos( deg[2] * d2r ), sz = std::sin( deg[2] * d2r );
	const double M[3][3] = { { cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx },
		{ sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx }, { -sy, cy * sx, cy * cx } };
	for ( int i = 0; i < 3; i++ )
		for ( int j = 0; j < 3; j++ )
			R[i][j] = M[i][j];
}

inline void halfSize( const Shape & s, double h[3] )
{
	for ( int c = 0; c < 3; c++ )
		h[c] = std::max( 0.5 * double( s.size[c] ), 1e-3 );
}

//! MIRROR cell_airzone.glsl azSd: the signed distance in the shape's own frame (exact for box, capsule; a bound inside
//! for the ellipsoid, the elliptic cylinder and the wedge's slope). sd <= 0 exactly where cell_givol.glsl volInside holds.
inline double refSd( const double l[3], int type, const double h[3] )
{
	if ( type == Sphere ) {
		const double q = std::sqrt( ( l[0] / h[0] ) * ( l[0] / h[0] ) + ( l[1] / h[1] ) * ( l[1] / h[1] ) + ( l[2] / h[2] ) * ( l[2] / h[2] ) );
		return ( q - 1.0 ) * std::min( h[0], std::min( h[1], h[2] ) );
	}
	if ( type == Capsule ) {
		const double r = std::min( h[0], h[1] ), hz = std::max( h[2] - r, 0.0 );
		const double dz = l[2] - std::clamp( l[2], -hz, hz );
		return std::sqrt( l[0] * l[0] + l[1] * l[1] + dz * dz ) - r;
	}
	const double bx = std::abs( l[0] ) - h[0], by = std::abs( l[1] ) - h[1], bz = std::abs( l[2] ) - h[2];
	if ( type == Cylinder ) {
		const double r = std::min( h[0], h[1] );
		const double a = ( std::sqrt( ( l[0] / h[0] ) * ( l[0] / h[0] ) + ( l[1] / h[1] ) * ( l[1] / h[1] ) ) - 1.0 ) * r;
		const double oa = std::max( a, 0.0 ), ob = std::max( bz, 0.0 );
		return std::sqrt( oa * oa + ob * ob ) + std::min( std::max( a, bz ), 0.0 );
	}
	const double ox = std::max( bx, 0.0 ), oy = std::max( by, 0.0 ), oz = std::max( bz, 0.0 );
#ifdef AIRZONE_RED_SD
	double sd = std::sqrt( ox * ox + oy * oy + oz * oz );	// red: no inside distance (every inside point reads 0: a hard edge)
#else
	double sd = std::sqrt( ox * ox + oy * oy + oz * oz ) + std::min( std::max( bx, std::max( by, bz ) ), 0.0 );
#endif
	if ( type == Wedge )
		sd = std::max( sd, ( l[1] * h[2] + l[2] * h[1] ) / std::sqrt( h[1] * h[1] + h[2] * h[2] ) );
	return sd;
}

//! the point in the shape's frame: R^T (P - center)
inline void toLocal( const Shape & s, const double P[3], double l[3] )
{
	double R[3][3];
	rotation( s.rotation, R );
	const double d[3] = { P[0] - s.center[0], P[1] - s.center[1], P[2] - s.center[2] };
	for ( int i = 0; i < 3; i++ )
		l[i] = R[0][i] * d[0] + R[1][i] * d[1] + R[2][i] * d[2];
}

inline double smooth01( double e0, double e1, double x )
{
	const double t = std::clamp( ( x - e0 ) / ( e1 - e0 ), 0.0, 1.0 );
	return t * t * ( 3.0 - 2.0 * t );
}

//! MIRROR cell_airzone.glsl azWeight
inline double refWeight( const Volume & v, const double P[3], int red )
{
	double l[3], h[3];
	toLocal( v.shape, P, l );
	halfSize( v.shape, h );
	const double sd = refSd( l, v.shape.type, h );
#ifdef AIRZONE_RED_EDGE
	return smooth01( -double( v.edge ), double( v.edge ), -sd );	// red: the edge centred on the surface (leaks past the shape)
#else
	if ( !( v.edge > 0.0f ) || ( red & kRedHardEdge ) )
		return sd <= 0.0 ? 1.0 : 0.0;
	return smooth01( 0.0, double( v.edge ), -sd );
#endif
}

//! MIRROR cell_airzone.glsl azHash: 24 bits of an integer hash of a lattice point, in [0, 1)
inline float refHash( int x, int y, int z )
{
	uint32_t h = ( uint32_t( x ) * 73856093u ) ^ ( uint32_t( y ) * 19349663u ) ^ ( uint32_t( z ) * 83492791u );
	h = ( h ^ ( h >> 13u ) ) * 1274126177u;
	h = h ^ ( h >> 16u );
	return float( h & 16777215u ) * float( 1.0 / 16777216.0 );
}

//! MIRROR cell_airzone.glsl azNoise: value noise, smooth trilinear over the 8 lattice corners, in [0, 1]
inline double refNoise( const float p[3] )
{
	const float f[3] = { std::floor( p[0] ), std::floor( p[1] ), std::floor( p[2] ) };
	const int i[3] = { int( f[0] ), int( f[1] ), int( f[2] ) };
	double u[3];
	for ( int c = 0; c < 3; c++ ) {
		const double t = double( p[c] ) - double( f[c] );
#ifdef AIRZONE_RED_NOISE
		u[c] = t;	// red: a linear fade (a crease at every lattice face)
#else
		u[c] = t * t * ( 3.0 - 2.0 * t );
#endif
	}
	double n = 0.0;
	for ( int k = 0; k < 8; k++ ) {
		const int dx = k & 1, dy = ( k >> 1 ) & 1, dz = ( k >> 2 ) & 1;
		const double w = ( dx ? u[0] : 1.0 - u[0] ) * ( dy ? u[1] : 1.0 - u[1] ) * ( dz ? u[2] : 1.0 - u[2] );
		n += w * double( refHash( i[0] + dx, i[1] + dy, i[2] + dz ) );
	}
	return n;
}

inline float premix( float c, float strength )
{
	return 1.0f + std::clamp( strength, 0.0f, 1.0f ) * ( c - 1.0f );
}

//! the zones the GLSL reads this frame, in the order it composites them: those with a fog or particles block, the
//! kMax best by (priority high first, nearer bounding sphere first, file order), then by (priority, file order)
inline std::vector<int> pick( const std::vector<Volume> & v, const double eye[3], int * dropped = nullptr )
{
	std::vector<int> c;
	std::vector<double> dist( v.size(), 0.0 );
	for ( size_t i = 0; i < v.size(); i++ ) {
		if ( !v[i].fog.on && !v[i].part.on )
			continue;
		double h[3];
		halfSize( v[i].shape, h );
		const double rad = std::sqrt( h[0] * h[0] + h[1] * h[1] + h[2] * h[2] );
		const double d[3] = { eye[0] - v[i].shape.center[0], eye[1] - v[i].shape.center[1], eye[2] - v[i].shape.center[2] };
		dist[i] = std::max( 0.0, std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] ) - rad );
		c.push_back( int( i ) );
	}
	std::stable_sort( c.begin(), c.end(), [&]( int a, int b ) {
		if ( v[size_t( a )].priority != v[size_t( b )].priority )
			return v[size_t( a )].priority > v[size_t( b )].priority;
		return dist[size_t( a )] < dist[size_t( b )];
	} );
	if ( dropped )
		*dropped = std::max( 0, int( c.size() ) - kMax );
	if ( int( c.size() ) > kMax )
		c.resize( size_t( kMax ) );
	std::stable_sort( c.begin(), c.end(), [&]( int a, int b ) {
		if ( v[size_t( a )].priority != v[size_t( b )].priority )
#ifdef AIRZONE_RED_ORDER
			return v[size_t( a )].priority < v[size_t( b )].priority;	// red: the low priority composites first
#else
			return v[size_t( a )].priority > v[size_t( b )].priority;
#endif
		return a < b;
	} );
	return c;
}

/*! MIRROR cell_airzone.glsl azZ's layout. Packs the picked zones into out (kMax x kVec4 x 4 floats, the rest 0);
 *  seconds = the world clock (the noise drift). Returns the zone count (azCount). */
inline int pack( const std::vector<Volume> & v, const std::vector<int> & picked, double seconds, float * out )
{
	std::fill( out, out + kMax * kVec4 * 4, 0.0f );
	int n = 0;
	for ( int idx : picked ) {
		if ( n >= kMax )
			break;
		const Volume & z = v[size_t( idx )];
		float * o = out + n * kVec4 * 4;
		double R[3][3], h[3];
		rotation( z.shape.rotation, R );
		halfSize( z.shape, h );
		double reach = 0.0, rad2 = 0.0;
		for ( int j = 0; j < 3; j++ ) {
			reach += std::abs( R[2][j] ) * h[j];
			rad2 += h[j] * h[j];
		}
		const float inv = z.fog.noiseScale > 0.0f ? 1.0f / z.fog.noiseScale : 0.0f;
#ifdef AIRZONE_RED_PACK
		for ( int i = 0; i < 3; i++ )	// red: R's rows packed (local -> world) where the read wants R^T (world -> local)
			for ( int j = i + 1; j < 3; j++ )
				std::swap( R[i][j], R[j][i] );
#endif
		const float vals[kVec4 * 4] = {
			z.shape.center[0], z.shape.center[1], z.shape.center[2], float( z.shape.type ),
			float( h[0] ), float( h[1] ), float( h[2] ), std::max( z.edge, 0.0f ),
			float( R[0][0] ), float( R[1][0] ), float( R[2][0] ), float( double( z.shape.center[2] ) - reach ),
			float( R[0][1] ), float( R[1][1] ), float( R[2][1] ), float( ( z.fog.on ? kFlagFog : 0 ) | ( z.part.on ? kFlagPart : 0 ) ),
			float( R[0][2] ), float( R[1][2] ), float( R[2][2] ), z.fog.heightFalloff > 0.0f ? 1.0f / z.fog.heightFalloff : 0.0f,
			std::max( z.fog.scale, 0.0f ), std::max( z.fog.add, 0.0f ) / 1000.0f, std::clamp( z.fog.noise, 0.0f, 1.0f ), inv,
			premix( z.fog.colour[0], z.fog.colourStrength ), premix( z.fog.colour[1], z.fog.colourStrength ),
			premix( z.fog.colour[2], z.fog.colourStrength ), float( rad2 * 1.0001 + 1.0 ),
			float( std::fmod( double( z.fog.noiseDrift[0] ) * seconds * double( inv ), 4096.0 ) ),
			float( std::fmod( double( z.fog.noiseDrift[1] ) * seconds * double( inv ), 4096.0 ) ),
			float( std::fmod( double( z.fog.noiseDrift[2] ) * seconds * double( inv ), 4096.0 ) ), std::max( z.part.light, 0.0f ),
			premix( z.part.tint[0], z.part.tintStrength ), premix( z.part.tint[1], z.part.tintStrength ),
			premix( z.part.tint[2], z.part.tintStrength ), std::max( z.part.density, 0.0f ) };
		std::copy( vals, vals + kVec4 * 4, o );
		n++;
	}
	return n;
}

// ---- the reference of the reads (double, from the Volume itself, not from the packing)
struct FogOut
{
	double k = 1.0, add = 0.0, tint[3] = { 1.0, 1.0, 1.0 };
};
struct PartOut
{
	double density = 1.0, light = 1.0, tint[3] = { 1.0, 1.0, 1.0 };
};

//! MIRROR cell_airzone.glsl azFogAt (the picked zones, in pick order)
inline FogOut refFog( const std::vector<Volume> & v, const std::vector<int> & picked, const double P[3], double seconds, int red )
{
	FogOut o;
	o.k = 0.0;
	o.tint[0] = o.tint[1] = o.tint[2] = 0.0;
	double r = 1.0;
	for ( int idx : picked ) {
		const Volume & z = v[size_t( idx )];
		if ( !z.fog.on )
			continue;
		const double w = refWeight( z, P, red );
		if ( w <= 0.0 )
			continue;
		const double s = r * w;
		double hf = 1.0;
		if ( z.fog.heightFalloff > 0.0f ) {
			double R[3][3], h[3], reach = 0.0;
			rotation( z.shape.rotation, R );
			halfSize( z.shape, h );
			for ( int j = 0; j < 3; j++ )
				reach += std::abs( R[2][j] ) * h[j];
			hf = std::exp( -std::max( P[2] - ( double( z.shape.center[2] ) - reach ), 0.0 ) / double( z.fog.heightFalloff ) );
		}
		double n = 1.0;
		if ( z.fog.noise > 0.0f && z.fog.noiseScale > 0.0f ) {
			// the lattice coordinate in float, as the shader forms it (P x 1 / noiseScale + the packed offset)
			const float inv = 1.0f / z.fog.noiseScale;
			float q[3];
			for ( int c = 0; c < 3; c++ )
				q[c] = float( P[c] ) * inv + float( std::fmod( double( z.fog.noiseDrift[c] ) * seconds * double( inv ), 4096.0 ) );
			n = std::max( 1.0 + double( std::min( z.fog.noise, 1.0f ) ) * ( 2.0 * refNoise( q ) - 1.0 ), 0.0 );
		}
		o.k += s * double( std::max( z.fog.scale, 0.0f ) );
		o.add += s * double( std::max( z.fog.add, 0.0f ) ) / 1000.0 * hf * n;
		for ( int c = 0; c < 3; c++ )
			o.tint[c] += s * double( premix( z.fog.colour[c], z.fog.colourStrength ) );
		r *= 1.0 - w;
		if ( r <= 0.0 )
			break;
	}
#ifdef AIRZONE_RED_IDENTITY
	o.k = 1.0 - r > 0.0 ? o.k / ( 1.0 - r ) : 0.0;	// red: the zones' weighted mean alone (outside: no air at all)
#else
	o.k += r;
#endif
	for ( int c = 0; c < 3; c++ )
		o.tint[c] += r;
	return o;
}

//! MIRROR cell_airzone.glsl azPartAt
inline PartOut refPart( const std::vector<Volume> & v, const std::vector<int> & picked, const double P[3], int red )
{
	PartOut o;
	o.density = o.light = 0.0;
	o.tint[0] = o.tint[1] = o.tint[2] = 0.0;
	double r = 1.0;
	for ( int idx : picked ) {
		const Volume & z = v[size_t( idx )];
		if ( !z.part.on )
			continue;
		const double w = refWeight( z, P, red );
		if ( w <= 0.0 )
			continue;
		const double s = r * w;
		o.density += s * double( std::max( z.part.density, 0.0f ) );
		o.light += s * double( std::max( z.part.light, 0.0f ) );
		for ( int c = 0; c < 3; c++ )
			o.tint[c] += s * double( premix( z.part.tint[c], z.part.tintStrength ) );
		r *= 1.0 - w;
		if ( r <= 0.0 )
			break;
	}
	o.density += r;
	o.light += r;
	for ( int c = 0; c < 3; c++ )
		o.tint[c] += r;
	return o;
}

//! MIRROR cell_airzone.glsl azPartAlpha: a lit sprite's opacity under the density (1: a untouched)
inline double refPartAlpha( double a, double density )
{
#ifdef AIRZONE_RED_ALPHA
	return std::min( a * density, 1.0 );	// red: the opacity scaled linearly (not as optical depth)
#else
	if ( density == 1.0 )
		return a;
	if ( density <= 0.0 )
		return 0.0;
	return 1.0 - std::pow( std::max( 1.0 - a, 0.0 ), density );
#endif
}

} // namespace az

#endif // AIRZONEMATH_H

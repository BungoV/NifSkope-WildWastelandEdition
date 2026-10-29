// lane CLOUD2: the volume clouds' density, ONE function for every reader (the march, CLOUD3's shadows, CLOUD4).
// The CPU twin is src/gl/cloudvolmath.h (bandDensity, gradient, calAt): keep the two in step. p = (world x km,
// world y km, height km above z = 0). The band heights live in the one struct below (src/gl/cloudvol.cpp sets it).

struct CloudBands {
	vec3 baseKm;	// Low, Mid, High base, km above z = 0
	vec3 topKm;
	vec3 density;	// relative optical thickness, 1 = a plain cumulus; 0 = an empty band
	vec3 coverage;	// outside the map square
	float sheet;	// the Low band's sheet share outside the map square
	vec4 mapRect;	// world km: x0, y0, 1 / (2 halfExtent); w = 1 when the map texture is bound
	vec3 windKm;	// the noise offset (wind x time), km
};
uniform CloudBands cvBands;
uniform sampler3D cvShape;	// 128^3 R8 Perlin-Worley (src/gl/cloudnoise.h), period kCvShapeKm
uniform sampler3D cvDetail;	// 32^3 R8 Worley fBm, period kCvDetailKm
uniform sampler2D cvMap;	// CLOUD1's map: R Low, G Mid, B Low sheet, A High coverage
uniform float cvCal[34];	// coverage -> threshold coverage, 17 per band (cloudvolmath.h calibrate)

const float kCvShapeKm = 6.0;
const float kCvDetailKm = 0.6;
const float kCvSigma = 40.0;	// extinction /km at density 1
float cvFootKm = 0.0;	// a reader's pixel footprint at p, km (picks the noise mips; 0 = the finest, mip 0)

float cvCalAt( int b, float c )
{
	float t = clamp( c, 0.0, 1.0 ) * 16.0;
	int i = min( int( t ), 15 );
	return mix( cvCal[b * 17 + i], cvCal[b * 17 + i + 1], t - float( i ) );
}

vec4 cvMapAt( vec2 xy )
{
	vec2 uv = ( xy - cvBands.mapRect.xy ) * cvBands.mapRect.z;
	if ( cvBands.mapRect.w < 0.5 || any( lessThan( uv, vec2( 0.0 ) ) ) || any( greaterThan( uv, vec2( 1.0 ) ) ) )
		return vec4( cvBands.coverage.x, cvBands.coverage.y, cvBands.coverage.x * cvBands.sheet, cvBands.coverage.z );
	return textureLod( cvMap, uv, 0.0 );
}

float cvSmooth( float a, float b, float x )
{
	float t = clamp( ( x - a ) / ( b - a ), 0.0, 1.0 );
	return t * t * ( 3.0 - 2.0 * t );
}

float cvGradient( int b, float hf, float sheet )
{
	if ( b == 0 ) {
		float cu = cvSmooth( 0.0, 0.07, hf ) * ( 1.0 - cvSmooth( 0.55, 1.0, hf ) );
		float st = cvSmooth( 0.0, 0.04, hf ) * ( 1.0 - cvSmooth( 0.12, 0.3, hf ) );
		return mix( cu, st, clamp( sheet, 0.0, 1.0 ) );
	}
	return cvSmooth( 0.0, 0.15, hf ) * ( 1.0 - cvSmooth( 0.35, 0.9, hf ) );
}

#ifndef WW_CLOUD_VARIETY
// relative density (extinction = kCvSigma x this, /km); 0 outside the Low and Mid bands
float cloudDensity( vec3 p )
{
	vec4 m = cvMapAt( p.xy );
	float d = 0.0;
	for ( int b = 0; b < 2; b++ ) {
		float base = cvBands.baseKm[b], top = cvBands.topKm[b];
		float hf = ( p.z - base ) / max( top - base, 1e-3 );
		if ( hf <= 0.0 || hf >= 1.0 || cvBands.density[b] <= 0.0 )
			continue;
		float cov = b == 0 ? m.r : m.g;
		float sheet = b == 0 ? clamp( m.b / max( m.r, 1e-3 ), 0.0, 1.0 ) : 0.0;
		float c = cvCalAt( b, cov );
		if ( c <= 0.0 )
			continue;
		vec3 q = p + cvBands.windKm;
		float s = textureLod( cvShape, q / kCvShapeKm, log2( max( cvFootKm * 128.0 / kCvShapeKm, 1.0 ) ) ).r;
		float t = 1.0 - c;
		float base_ = clamp( ( s * cvGradient( b, hf, sheet ) - t ) / max( 1.0 - t, 1e-4 ), 0.0, 1.0 ) * c;
		if ( base_ <= 0.0 )
			continue;
		float det = textureLod( cvDetail, q / kCvDetailKm, log2( max( cvFootKm * 32.0 / kCvDetailKm, 1.0 ) ) ).r;
		float mod_ = mix( det, 1.0 - det, clamp( hf * 4.0, 0.0, 1.0 ) ) * 0.2;
		d += clamp( ( base_ - mod_ ) / ( 1.0 - mod_ ), 0.0, 1.0 ) * cvBands.density[b];
	}
	return d;
}
#else
/* lane CLOUD5: Cloud Variety (the *_v.prog twins define WW_CLOUD_VARIETY; Off draws the function above unchanged).
 * The Low band takes a cloud TYPE 0..1 (stratus, stratocumulus, cumulus, cumulonimbus), each with its own depth and
 * profile; a weather noise over tens of km swings the coverage (mean kept) and the type; the detail lookup is displaced
 * by a curl field and eroded more, less at the base. Sources and the CPU twin: src/gl/cloudvolmath.h varietyDensity. */
uniform vec4 cvVarA;	// x the weather's Low type, y the type's swing, z the coverage's swing, w the Low band's own depth km
uniform vec4 cvVarB;	// x a cumulonimbus' depth km, y the erosion, z the curl km (0 = red nocurl), w the anvil
uniform float cvCalT[68];	// the Low band's coverage tables at the 4 type anchors, 17 each (cloudvolmath.h varCalibrate)

const float kCvVarWrapKm = 384.0;	// cloudvolmath.h kVarWrapKm (the wind's wrap)
const float kCvCurlPeriodKm = 24.0;

float cvVarHash( int ix, int iy, int oct )
{
	uint h = uint( ix ) * 0x8da6b343u ^ uint( iy ) * 0xd8163841u ^ uint( oct ) * 0xcb1ab31fu;
	h ^= h >> 13u;
	h *= 0x5bd1e995u;
	h ^= h >> 15u;
	return float( h >> 8u ) * ( 1.0 / 16777216.0 );
}

float cvVarValue( vec2 xy, float cellKm, int cells, int oct )
{
	vec2 u = mod( xy, vec2( kCvVarWrapKm ) ) / cellKm;
	ivec2 i = min( ivec2( floor( u ) ), ivec2( cells - 1 ) );
	vec2 f = u - vec2( i );
	ivec2 j = ivec2( i.x + 1 == cells ? 0 : i.x + 1, i.y + 1 == cells ? 0 : i.y + 1 );
	vec2 w = f * f * ( 3.0 - 2.0 * f );
	float a = cvVarHash( i.x, i.y, oct ), b = cvVarHash( j.x, i.y, oct ), c = cvVarHash( i.x, j.y, oct ), e = cvVarHash( j.x, j.y, oct );
	return mix( mix( a, b, w.x ), mix( c, e, w.x ), w.y );
}

float cvWeatherNoise( vec2 xy )
{
	float v = 0.65 * cvVarValue( xy, 16.0, 24, 0 ) + 0.35 * cvVarValue( xy, 6.0, 64, 1 );
	return clamp( ( v - 0.5 ) * 4.0, -1.0, 1.0 );
}

vec3 cvCurlRaw( vec3 q )
{
	vec3 u = mod( q, vec3( kCvCurlPeriodKm ) ) / kCvCurlPeriodKm;
	float c1 = cos( 6.2831853 * fract( 13.0 * u.x + 5.0 * u.y + 0.11 ) );
	float c2 = cos( 6.2831853 * fract( -6.0 * u.x + 11.0 * u.y + 0.33 ) );
	float c3 = cos( 6.2831853 * fract( 10.0 * u.x + 8.0 * u.z + 0.64 ) );
	float c4 = cos( 6.2831853 * fract( -7.0 * u.x + 12.0 * u.z + 0.85 ) );
	float ax = c1 * 0.358979 + c2 * 0.877896, ay = c1 * -0.933346 + c2 * 0.478852;
	float bx = c3 * 0.624695 + c4 * 0.863779, bz = c3 * -0.780869 + c4 * 0.503871;
	return 0.5 * vec3( ax + bx, ay, bz );
}

float cvTypeDepth( float T )
{
	float u = clamp( T, 0.0, 1.0 ) * 3.0, dc = cvVarA.w;
	if ( u < 1.0 )
		return dc * mix( 0.3, 0.6, u );
	if ( u < 2.0 )
		return dc * mix( 0.6, 1.0, u - 1.0 );
	return mix( dc, max( cvVarB.x, dc ), u - 2.0 );
}

float cvTypeProfile( float T, float hf )
{
	const float rise[4] = float[4]( 0.10, 0.08, 0.07, 0.03 );
	const float fall[4] = float[4]( 0.60, 0.45, 0.55, 0.85 );
	float u = clamp( T, 0.0, 1.0 ) * 3.0;
	int k = min( int( u ), 2 );
	float a = cvSmooth( 0.0, rise[k], hf ) * ( 1.0 - cvSmooth( fall[k], 1.0, hf ) );
	float b = cvSmooth( 0.0, rise[k + 1], hf ) * ( 1.0 - cvSmooth( fall[k + 1], 1.0, hf ) );
	return mix( a, b, u - float( k ) );
}

float cvCalAtT( float T, float c )
{
	float u = clamp( T, 0.0, 1.0 ) * 3.0;
	int k = min( int( u ), 2 );
	float t = clamp( c, 0.0, 1.0 ) * 16.0;
	int i = min( int( t ), 15 );
	float f = t - float( i );
	float a = mix( cvCalT[k * 17 + i], cvCalT[k * 17 + i + 1], f );
	float b = mix( cvCalT[k * 17 + 17 + i], cvCalT[k * 17 + 18 + i], f );
	return mix( a, b, u - float( k ) );
}

float cloudDensity( vec3 p )
{
	vec4 m = cvMapAt( p.xy );
	vec3 q = p + cvBands.windKm;
	float n = cvWeatherNoise( q.xy );
	float d = 0.0, s = -1.0;
	vec3 curl = vec3( 0.0 );
	bool curlOk = false;
	for ( int b = 0; b < 2; b++ ) {
		if ( cvBands.density[b] <= 0.0 )
			continue;
		float cov = b == 0 ? m.r : m.g;
		cov += cvVarA.z * n * min( cov, 1.0 - cov );
		float hf, prof, c;
		if ( b == 0 ) {
			float sheet = clamp( m.b / max( m.r, 1e-3 ), 0.0, 1.0 );
			float T = clamp( cvVarA.x + cvVarA.y * n, 0.0, 1.0 ) * ( 1.0 - sheet );
			hf = ( p.z - cvBands.baseKm.x ) / max( cvTypeDepth( T ), 1e-3 );
			if ( hf <= 0.0 || hf >= 1.0 )
				continue;
			c = cvCalAtT( T, cov );
			float e = mix( 1.0, 0.5, cvVarB.w * cvSmooth( 0.8, 1.0, T ) * cvSmooth( 0.65, 0.85, hf ) );
			c = c > 0.0 ? pow( c, e ) : 0.0;
			prof = cvTypeProfile( T, hf );
		} else {
			hf = ( p.z - cvBands.baseKm.y ) / max( cvBands.topKm.y - cvBands.baseKm.y, 1e-3 );
			if ( hf <= 0.0 || hf >= 1.0 )
				continue;
			c = cvCalAt( 1, cov );
			prof = cvGradient( 1, hf, 0.0 );
		}
		if ( c <= 0.0 )
			continue;
		if ( s < 0.0 )
			s = textureLod( cvShape, q / kCvShapeKm, log2( max( cvFootKm * 128.0 / kCvShapeKm, 1.0 ) ) ).r;
		float t = 1.0 - c;
		float base_ = clamp( ( s * prof - t ) / max( 1.0 - t, 1e-4 ), 0.0, 1.0 ) * c;
		if ( base_ <= 0.0 )
			continue;
		if ( !curlOk ) {
			curl = cvCurlRaw( q );
			curlOk = true;
		}
		vec3 qd = q + curl * ( cvVarB.z * ( 1.0 - 0.5 * hf ) );
		float det = textureLod( cvDetail, qd / kCvDetailKm, log2( max( cvFootKm * 32.0 / kCvDetailKm, 1.0 ) ) ).r;
		float er = cvVarB.y * mix( 0.55, 1.0, cvSmooth( 0.0, 0.35, hf ) );
		float mod_ = mix( det, 1.0 - det, clamp( hf * 4.0, 0.0, 1.0 ) ) * er;
		d += clamp( ( base_ - mod_ ) / ( 1.0 - mod_ ), 0.0, 1.0 ) * cvBands.density[b];
	}
	return d;
}
#endif

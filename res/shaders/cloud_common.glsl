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

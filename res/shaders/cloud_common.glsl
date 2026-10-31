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

#if defined( WW_CLOUD_SHAPE ) && !defined( WW_CLOUD_VARIETY )
/* lane CLOUDSHAPE1: Cloud Shapes (the *_s.prog twins define WW_CLOUD_SHAPE; Off draws the function below unchanged).
 * RDR2 (Bauer 2019 sl. 14-18): clouds PLACED by a 2D field and SHAPED over height, the detail carved by a saturating
 * rescale. A body field over the coverage threshold, a dome per body on one flat base (taller in stronger cells), the
 * Perlin-Worley lumps growing with height, HZD's erosion (billows above, wisps at the base), then clamp(f / window):
 * dense inside, crisp edge, not multiplied by the coverage. The CPU twin and every constant: src/gl/cloudvolmath.h
 * shapeCore / shapeDensity (kShp*). */
uniform sampler2D cvCell;	// the bodies' field: kCellN^2 R32F over kShpCellKm, uniform 0..1 (cloudnoise.h cellField)
uniform vec4 cvShpT0;	// the Low band's type rule: dome height share, exponent, sheet share, anvil (cloudvolmath.h shapeType)
uniform vec4 cvShpT1;	// the Mid band's

float cvShpAnvil( float anvil, float hf )	// cloudvolmath.h shapeAnvil
{
	return anvil * cvSmooth( 0.8, 0.9, hf ) * ( 1.0 - cvSmooth( 0.96, 1.0, hf ) );
}

const float kShpSoft = 0.1, kShpErode = 1.0, kShpScale = 0.7, kShpShearKm = 0.3, kShpRise = 0.04, kShpCurlKm = 0.25;
const float kShpBaseErode = 0.3, kShpLump = 0.45, kShpCellKm = 32.0, kShpGrowth = 0.8, kShpTapFoot = 0.2;
const float kShpSoftPerKm = 0.5, kShpMean = 0.884, kShpSd = 0.053, kShpTowerScale = 3.0;
// sheet review 11:52 (v2): the billows' octave, the weather swing, the detail's period share, the sky's share a cloud's own
// body leaves its sides (MEASURED against bungo_refs: notes/cloudshape1/v2)
const float kShpBillow = 4.0, kShpBillowW = 0.5, kShpWxScale = 4.7, kShpWx = 0.6, kShpDetailK = 0.6, kShpSkyOcc = 0.7;
const float kShpTowerGain = 2.5;
const float kShpInner = 0.8, kShpInnerLo = 1.0;	// the interior's density ramp (LATE ITEM 14): 1 = off (0.45 and 0.7 took up50's
// contrast range to 1.85 / 1.97, under the references' 2.00: notes/cloudshape1/v2/gate_v2c.txt)
const float kShpAmbSat = 0.5;	// the sky ambient's saturation kept (Godot demo v2: half toward its grey)
const int kShpSub = 2;	// the march's sub-steps inside cloud (cloud_march.frag), each at most kShpSubKm (4 and 8 cost 1.5-2x
// and drew no better once the taps' jitter was its own: notes/cloudshape1/v2/zs2_m.png)
const float kShpSubKm = 0.06;
const float kShpMs = 0.2, kShpGround = 0.4;	// the slab floor's share (3D clouds lose light out of their sides), the base's ambient
// the v2 knobs' constant (the offline sweep read a uniform here: notes/cloudshape1/v2/tools; the shipped code reads none)
#define cvShpK( i, k ) ( k )
const vec2 kShpDir = vec2( 1.0, 0.0 );	// the wind's direction (cvBands.windKm is along +x): the tops lean downwind
const float kShpBack = 1.0, kShpBackLo = 4.0, kShpBackHi = 20.0;	// the back-scatter clamp's weight, its extinction ramp /km

vec3 cvShpCurl( vec3 q )	// cloudvolmath.h curlRaw (24 km period)
{
	vec3 u = mod( q, vec3( 24.0 ) ) / 24.0;
	float c1 = cos( 6.2831853 * fract( 13.0 * u.x + 5.0 * u.y + 0.11 ) );
	float c2 = cos( 6.2831853 * fract( -6.0 * u.x + 11.0 * u.y + 0.33 ) );
	float c3 = cos( 6.2831853 * fract( 10.0 * u.x + 8.0 * u.z + 0.64 ) );
	float c4 = cos( 6.2831853 * fract( -7.0 * u.x + 12.0 * u.z + 0.85 ) );
	float ax = c1 * 0.358979 + c2 * 0.877896, ay = c1 * -0.933346 + c2 * 0.478852;
	float bx = c3 * 0.624695 + c4 * 0.863779, bz = c3 * -0.780869 + c4 * 0.503871;
	return 0.5 * vec3( ax + bx, ay, bz );
}

float cloudDensity( vec3 p )
{
	vec4 m = cvMapAt( p.xy );
	float d = 0.0;
	for ( int b = 0; b < 2; b++ ) {
		float base = cvBands.baseKm[b], top = cvBands.topKm[b];
		float hf = ( p.z - base ) / max( top - base, 1e-3 );
		if ( hf <= 0.0 || hf >= 1.0 || cvBands.density[b] <= 0.0 )
			continue;
		vec4 st = b == 0 ? cvShpT0 : cvShpT1;
		float cov = b == 0 ? m.r : m.g;
		float sheet = b == 0 ? max( clamp( m.b / max( m.r, 1e-3 ), 0.0, 1.0 ), st.z ) : st.z;
		vec3 q = p + cvBands.windKm;
		/* sheet review item 6 (rows of like clouds to the horizon): the weather's own swing at tens of km (Schneider 2015
		 * p. 31: the weather map's coverage varies in km-sized regions): the body field again at kShpWxScale x its period,
		 * turned 37 deg (no common period with the bodies), moves the coverage by +-kShpWx x min(cov, 1 - cov) (never past 0
		 * or 1: a clamp raised the mean at low coverage, 0.184 for 0.1, gate A) */
		{
			vec2 wq = mat2( 0.7986, 0.6018, -0.6018, 0.7986 ) * q.xy / ( kShpCellKm * cvShpK( 11, kShpWxScale ) );
			float wx = textureLod( cvCell, wq + vec2( 0.61, 0.13 ), 0.0 ).r;	// 0.3 km texels: mip 0, as the twin
			cov = clamp( cov + ( wx - 0.5 ) * 2.0 * cvShpK( 10, kShpWx ) * min( cov, 1.0 - cov ), 0.0, 1.0 );	// no clamp: the mean holds
		}
		float c = cvCalAt( b, cov );
		if ( c <= 0.0 )
			continue;
		q.xy += kShpDir * ( kShpShearKm * ( p.z - base ) );
		float cell = textureLod( cvCell, q.xy / kShpCellKm + vec2( 0.37 * float( b ) ),
			log2( max( cvFootKm * 512.0 / kShpCellKm, 1.0 ) ) ).r;
		float w = ( cell - ( 1.0 - c ) ) / max( c, 1e-3 );
		float reach = mix( pow( hf / max( st.x, 1e-3 ), st.y ), hf * 4.0, sheet );
		float f = w - reach;
		if ( st.w > 0.0 ) {	// towers: the body field at kShpTowerScale x its period, only cells past the middle rise
			float big = textureLod( cvCell, q.xy / ( kShpCellKm * kShpTowerScale ) + vec2( 0.21 ),
				log2( max( cvFootKm * 512.0 / ( kShpCellKm * kShpTowerScale ), 1.0 ) ) ).r;
			float ft = ( big - ( 1.0 - c ) ) / max( c, 1e-3 ) - ( 0.5 + 0.5 * pow( hf, 1.2 ) );
			// sheet review item 5 (lace, no tower): the tower's margin over 0 (<= 0.5) is less than the erosion's bite, so
			// the detail shredded the column; its interior as deep as a cumulus core, the erosion carves its surface only
			ft *= cvShpK( 12, kShpTowerGain );
			f = mix( f, max( f, ft ), st.w * 2.0 );
		}
		f += cvShpAnvil( st.w, hf ) - ( 1.0 - cvSmooth( 0.0, kShpRise, hf ) );
		float grow = mix( 1.0 - kShpGrowth, 1.0, hf );
		if ( f + kShpLump * ( grow + max( cvShpK( 9, kShpBillowW ), 0.0 ) * mix( 0.5, 1.0, hf ) ) * 3.0 <= 0.0 )
			continue;	// no lump (nor billow) reaches this far out ((1 - kShpMean) / kShpSd < 3): the density is 0 either way
		float s = textureLod( cvShape, q / kCvShapeKm, log2( max( cvFootKm * 128.0 / kCvShapeKm, 1.0 ) ) ).r;
		f += ( s - kShpMean ) / kShpSd * kShpLump * grow;
		/* sheet review item 1 (smooth plastic): the cauliflower's middle scale (HZD's base shape is an FBM: Perlin-Worley +
		 * Worley octaves 2-4x finer, Schneider 2015 p. 37): the same Perlin-Worley again at 1 / kShpBillow its period, turned,
		 * as rounded heaps on the heaps; only near the surface (the inside stays dense) */
		float bw = cvShpK( 9, kShpBillowW );
		if ( bw > 0.0 && f < 1.0 ) {
			float bk = kCvShapeKm / cvShpK( 8, kShpBillow );
			vec3 qb = vec3( mat2( 0.6, -0.8, 0.8, 0.6 ) * q.xy, q.z ) + vec3( 0.31, 0.77, 0.0 );
			float s2 = textureLod( cvShape, qb / bk, log2( max( cvFootKm * 128.0 / bk, 1.0 ) ) ).r;
			f += ( s2 - kShpMean ) / kShpSd * bw * kShpLump * mix( 0.5, 1.0, hf );
		}
		if ( f <= 0.0 )
			continue;	// the erosion only lowers f
		vec3 qd = q + cvShpCurl( q ) * ( kShpCurlKm * ( 1.0 - 0.5 * hf ) );
		float dk = kCvDetailKm * cvShpK( 5, kShpDetailK );
		float det = textureLod( cvDetail, qd / dk, log2( max( cvFootKm * 32.0 / dk, 1.0 ) ) ).r;
		f -= mix( det, 1.0 - det, clamp( hf * 4.0, 0.0, 1.0 ) ) * cvShpK( 4, kShpErode ) * mix( kShpBaseErode, 1.0, cvSmooth( 0.0, 0.3, hf ) );
		/* LATE ITEM 14 (uniform fill = toon; clayjohn's Godot demo v2 clouds.glsl:136, MIT, keeps the remapped density as is):
		 * the inside rises from kShpInnerLo at the edge to full kShpInner f units in, so the lumps vary the interior too */
		d += clamp( f / max( cvShpK( 7, kShpSoft ) + cvFootKm * kShpSoftPerKm, 1e-3 ), 0.0, 1.0 )
			* mix( kShpInnerLo, 1.0, clamp( f / kShpInner, 0.0, 1.0 ) ) * kShpScale * cvBands.density[b];
	}
	return d;
}
#elif !defined( WW_CLOUD_VARIETY )
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
uniform float cvCalT[221];	// the Low band's coverage tables at T = 1 - (1 - k / 12)^2, 13 x 17 (cloudvolmath.h kVarCal, varCalT)

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

// antithetic: half a wrap east holds 1 - the value, so the noise's mean over the wrap is exactly 0 (cloudvolmath.h varLattice)
float cvVarLattice( int ix, int iy, int cells, int oct )
{
	int h = cells / 2;
	return ix >= h ? 1.0 - cvVarHash( ix - h, iy, oct ) : cvVarHash( ix, iy, oct );
}

float cvVarValue( vec2 xy, float cellKm, int cells, int oct )
{
	vec2 u = mod( xy, vec2( kCvVarWrapKm ) ) / cellKm;
	ivec2 i = min( ivec2( floor( u ) ), ivec2( cells - 1 ) );
	vec2 f = u - vec2( i );
	ivec2 j = ivec2( i.x + 1 == cells ? 0 : i.x + 1, i.y + 1 == cells ? 0 : i.y + 1 );
	vec2 w = f * f * ( 3.0 - 2.0 * f );
	float a = cvVarLattice( i.x, i.y, cells, oct ), b = cvVarLattice( j.x, i.y, cells, oct );
	float c = cvVarLattice( i.x, j.y, cells, oct ), e = cvVarLattice( j.x, j.y, cells, oct );
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
	// the rise and fall heights blend, not the profiles' values (cloudvolmath.h typeProfile: no shoulder half way up)
	float r = mix( rise[k], rise[k + 1], u - float( k ) ), f = mix( fall[k], fall[k + 1], u - float( k ) );
	return cvSmooth( 0.0, r, hf ) * ( 1.0 - cvSmooth( f, 1.0, hf ) );
}

float cvCalAtT( float T, float c )
{
	float u = ( 1.0 - sqrt( 1.0 - clamp( T, 0.0, 1.0 ) ) ) * 12.0;	// cloudvolmath.h varCalT's inverse
	int k = min( int( u ), 11 );
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

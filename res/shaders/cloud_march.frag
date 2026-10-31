#version 410 core

/* lane CLOUD2: the volume clouds' ray march (Schneider & Vos 2015). One fragment = one history pixel
 * (cvStride x fragment + cvPhase): 1/16 of them a frame at stride 4, all at stride 1 (harness runs).
 * Frostbite's energy-conserving step (Hillaire 2015), Wrenninge 2013 octaves toward the key light, the droplet phase,
 * the sky's ambient, then the air in front (aerial perspective). Out: rgb = light, a = transmittance; depth = the
 * transmittance-weighted mean distance, km (1e4 = no cloud). cvProbe = 1: the vertical column opacity (the gate). */

#include "atmo_common.glsl"
#include "cloud_common.glsl"
#include "ww_droplet_phase.glsl"

uniform sampler2D cvTrans;	// the sky's transmittance LUT
uniform sampler2D cvSunView;	// the sky-view LUTs (the air in front of the clouds)
uniform sampler2D cvMoonView;
uniform vec2 cvViewSize;
uniform float cvCamR;	// km from the planet's centre, as the sky-view LUTs were drawn
uniform vec3 cvSunW;
uniform vec3 cvMoonW;
uniform float cvSunK;
uniform float cvMoonK;
uniform vec3 cvKeyW;	// TO the key light (the sun, or the moon by night)
uniform float cvKeyK;
uniform mat3 cvCam;	// world columns: right, up, back
uniform vec4 cvProj;	// P00, P11, P20, P21
uniform vec3 cvCamKm;	// world x, y km, height km
uniform vec2 cvHistSize;
uniform float cvStride;
uniform vec2 cvPhase;
uniform int cvSteps;
uniform int cvTaps;
uniform int cvOctaves;	// 1 = single scattering (the red nooctaves)
uniform float cvLightKm;	// the light taps' reach
uniform vec4 cvPhasePrm;	// the droplet phase (gHG, gD, alpha, wD)
uniform vec3 cvAmb;	// the sky's mean radiance at the clouds, scene units
uniform float cvAmbOn;
uniform float cvApOn;
uniform float cvFrame;	// the jitter's frame (0 = fixed)
uniform float cvMaxKm;	// the march's reach along the ray
uniform float cvSlab;	// > 0: a uniform slab of this density in the Low band (the dark-core gate)
uniform int cvProbe;
uniform vec4 cvProbeRect;	// world km: x0, y0, dx, dy per fragment
uniform int cvProbeBand;
uniform float cvMsOn;	// lane CLOUD2B: the multiple-scattering floor (0 = the reds noms and nooctaves)
uniform float cvMsG;	// the droplet phase's mean cosine (uncapped), for the delta-Eddington scaling
uniform vec3 cvTint[3];	// CLOUD1's band tints (Low, Mid, High): a linear albedo factor, white by default
uniform sampler2D cvScene;	// lane CLOUD2B: the scene's window depth (the last frame's), r = farthest sample
uniform vec4 cvSceneZ;	// the projection's [2][2], [3][2]; km per view unit; 1 = clip the march at the scene
uniform float cvTopOn;	// lane CLOUD2C: the light the cloud below reflects back up (0 = the red notop)
uniform float cvAmbDark;	// lane CLOUDBAND1 (b): 1 = the sky's ambient through the cloud above (0 = Off: the height ramp)
#ifdef WW_CLOUD_VARIETY
uniform float cvAmbKm;	// lane CLOUD5: the span of the ambient's height ramp (the bands without the towers), km
#endif
// lane CLOUDBAND1: the *_a.prog twins (Cloud Accumulation, WW_CLOUDVOL_ACCUM) take the same blue noise offsets
#if defined( WW_CLOUD_VARIETY ) || defined( WW_CLOUD_ACCUM )
uniform sampler2D cvBlue;	// lane CLOUD5 round 2: EA's FAST blue noise, 64 slices of 128 x 128 stacked in y (R used)
uniform int cvBlueSlice;	// this 16-frame refresh cycle's slice; -1 = the red ign (CLOUD2's IGN, the stripes)
#endif

layout ( location = 0 ) out vec4 cvOut;
layout ( location = 1 ) out vec4 cvDepth;

const float kCvAlbedo = 0.99;

float cvDens( vec3 p )
{
	if ( cvSlab > 0.0 )
		return ( p.z > cvBands.baseKm.x && p.z < cvBands.topKm.x ) ? cvSlab : 0.0;
	return cloudDensity( p );
}

#if defined( WW_CLOUD_VARIETY ) || defined( WW_CLOUD_ACCUM )
/* lane CLOUD5 round 2: the march's per-pixel step offset. IGN is a gradient whose lines of equal value run at a slope of
 * -0.0671 / 0.00584 (near vertical), and nothing averages it (the resolve keeps one march a history pixel), so a zoomed,
 * smooth cloud wore fine vertical stripes (bungo 10-29 "banding"; cloudvar_check gate B, MEASURED). One blue noise slice a
 * 16-frame cycle: all 16 phases of the stride-4 picture come from the one slice, so it holds that slice's even grain. */
float cvJitter( vec2 q )
{
	if ( cvBlueSlice >= 0 )
		return ( floor( texelFetch( cvBlue, ivec2( mod( q, 128.0 ) ) + ivec2( 0, 128 * ( cvBlueSlice & 63 ) ), 0 ).r * 255.0
			+ 0.5 ) + 0.5 ) / 256.0;
	return fract( 52.9829189 * fract( dot( q, vec2( 0.06711056, 0.00583715 ) ) ) + cvFrame * 0.618034 );
}
#endif

// the ray's hits of the sphere of radius R about the planet's centre (origin height h0): near, far; far < 0 = none
vec2 cvSphere( float h0, vec3 d, float R )
{
	float r0 = kRb + h0;
	float b = r0 * d.z;
	float c = r0 * r0 - R * R;
	float disc = b * b - c;
	if ( disc < 0.0 )
		return vec2( -1.0 );
	float s = sqrt( disc );
	return vec2( -b - s, -b + s );
}

float cvHeight( vec3 o, vec3 d, float t, out vec3 up )
{
	vec3 e = vec3( d.xy * t, kRb + o.z + d.z * t );
	float r = length( e );
	up = e / r;
	return r - kRb;
}

vec3 cvSky( vec3 d )
{
	vec2 a = d.xy;
	float la = length( a );
	vec3 c = vec3( 0.0 );
	for ( int i = 0; i < 2; i++ ) {
		vec3 l = i == 0 ? cvSunW : cvMoonW;
		float k = i == 0 ? cvSunK : cvMoonK;
		if ( k <= 0.0 )
			continue;
		float lb = length( l.xy );
		float cosAz = ( la > 1e-5 && lb > 1e-5 ) ? dot( a / la, l.xy / lb ) : 1.0;
		vec2 uv = atSkyDirToUv( cvCamR, d.z, cosAz, cvViewSize );
		c += k * ( i == 0 ? texture( cvSunView, uv ).rgb : texture( cvMoonView, uv ).rgb );
	}
	return c;
}

vec4 cvPrm;	// the droplet phase, its forward peak widened to the march's footprint (main)

float cvOctaveSum( float cosT, float tauL )
{
	float s = 0.0, a = 1.0, b = 1.0, k = 1.0;
	for ( int i = 0; i < cvOctaves; i++ ) {
		s += a * exp( -b * tauL ) * wwDropletPhase( cvPrm, cosT, k );
		a *= 0.5;
		b *= 0.5;
		k *= 0.5;
	}
	return s;
}

/* lane CLOUD2B: the light the octaves miss in a thick cloud. Delta-Eddington scaling (Joseph, Wiscombe & Weinman 1976)
 * and the two-stream diffuse transmission for albedo w < 1 (Meador & Weaver 1980, Eddington coefficients) of a slab as
 * thick as the optical depth above the sample, per unit flux on its top. Written with e^(-k tau) only (no overflow).
 * Twin: cloudvolmath.h msDiffuseT. */
float cvTdif( float tau, float mu0, float g, float w )
{
	float f = g * g;
	float wp = ( 1.0 - f ) * w / ( 1.0 - f * w );
	float tp = ( 1.0 - f * w ) * tau;
	float gp = g / ( 1.0 + g );
	float g1 = ( 7.0 - wp * ( 4.0 + 3.0 * gp ) ) * 0.25;
	float g2 = -( 1.0 - wp * ( 4.0 - 3.0 * gp ) ) * 0.25;
	float g3 = ( 2.0 - 3.0 * gp * mu0 ) * 0.25;
	float g4 = 1.0 - g3;
	float a1 = g1 * g4 + g2 * g3;
	float k = sqrt( max( g1 * g1 - g2 * g2, 1e-8 ) );
	if ( abs( 1.0 - k * mu0 ) < 1e-3 )
		mu0 += 2e-3;	// the closed form's removable singularity at k mu0 = 1
	float em = exp( -k * tp ), e0 = exp( -tp / mu0 );
	float den = ( 1.0 - k * k * mu0 * mu0 ) * ( ( k + g1 ) + ( k - g1 ) * em * em );
	float num = ( 1.0 + k * mu0 ) * ( a1 + k * g4 ) * e0 - ( 1.0 - k * mu0 ) * ( a1 - k * g4 ) * em * em * e0
		- 2.0 * k * ( g4 + a1 * mu0 ) * em;
	return max( -wp * num / den, 0.0 );	// the total transmission minus the direct e0
}

/* lane CLOUD2C: the same two-stream slab's reflection of a beam at mu0 (Meador & Weaver 1980 eq. 14, alpha2 =
 * g1 g3 + g2 g4), delta-Eddington scaled, e^(-k tau) only. Twin: cloudvolmath.h msDiffuseR. */
float cvRdir( float tau, float mu0, float g, float w )
{
	float f = g * g;
	float wp = ( 1.0 - f ) * w / ( 1.0 - f * w );
	float tp = ( 1.0 - f * w ) * tau;
	float gp = g / ( 1.0 + g );
	float g1 = ( 7.0 - wp * ( 4.0 + 3.0 * gp ) ) * 0.25;
	float g2 = -( 1.0 - wp * ( 4.0 - 3.0 * gp ) ) * 0.25;
	float g3 = ( 2.0 - 3.0 * gp * mu0 ) * 0.25;
	float g4 = 1.0 - g3;
	float a2 = g1 * g3 + g2 * g4;
	float k = sqrt( max( g1 * g1 - g2 * g2, 1e-8 ) );
	if ( abs( 1.0 - k * mu0 ) < 1e-3 )
		mu0 += 2e-3;
	float em = exp( -k * tp ), e0 = exp( -tp / mu0 );
	float den = ( 1.0 - k * k * mu0 * mu0 ) * ( ( k + g1 ) + ( k - g1 ) * em * em );
	float num = ( 1.0 - k * mu0 ) * ( a2 + k * g3 ) - ( 1.0 + k * mu0 ) * ( a2 - k * g3 ) * em * em
		- 2.0 * k * ( g3 - a2 * mu0 ) * e0 * em;
	return clamp( wp * num / den, 0.0, 1.0 );
}

#ifdef WW_CLOUDFLY
/* lane CLOUD4: the fly-through grid (src/gl/cloudfly.cpp): a box of cells round the camera over the band, each holding
 * its Chebyshev distance in cells (cloud_flysdf.frag) to a cell where the density may be above 0 (cloud_flybake.frag).
 * Inside the box the march steps cfStepKm (a quarter cell) and jumps the free cells less cfSafe (kSafe + the grid's
 * age in cells), so a jump lands on the same fine steps and never passes a cell that may hold cloud. */
uniform sampler3D cfSdf;
uniform vec3 cfOrgKm;
uniform vec3 cfDim;	// cells x, y, z
uniform float cfCellKm;
uniform float cfStepKm;
uniform int cfMaxSteps;
uniform float cfSafe;	// cells kept clear (the red sdfshrink makes it -1)
uniform float cfOn;	// 0 = the plain march (no grid for this view)
uniform float cfSkip;	// 0 = the red skip: fine steps through every cell (the continuity reference)

//! where the ray leaves the box (flat: the earth's curve over the box is under a cell, kept by kSafe)
float cfExit( vec3 o, vec3 d )
{
	vec3 dd = mix( d, vec3( 1e-9 ), lessThan( abs( d ), vec3( 1e-9 ) ) );
	vec3 ta = ( cfOrgKm - o ) / dd, tb = ( cfOrgKm + cfDim * cfCellKm - o ) / dd;
	vec3 tf = max( ta, tb );
	return min( tf.x, min( tf.y, tf.z ) );
}
#endif

void main()
{
	if ( cvProbe == 1 ) {
		vec2 xy = cvProbeRect.xy + ( gl_FragCoord.xy - 0.5 ) * cvProbeRect.zw;
		float base = cvBands.baseKm[cvProbeBand], top = cvBands.topKm[cvProbeBand];
		float tau = 0.0;
		const int NZ = 128;
		for ( int k = 0; k < NZ; k++ ) {
			float z = base + ( float( k ) + 0.5 ) / float( NZ ) * ( top - base );
			tau += cvDens( vec3( xy, z ) ) * kCvSigma * ( top - base ) / float( NZ );
		}
		cvOut = vec4( 1.0 - exp( -tau ) );
		cvDepth = vec4( 0.0 );
		return;
	}
	vec2 q = floor( gl_FragCoord.xy ) * cvStride + cvPhase;
	// the HG peak is about (1 - g) rad wide: no narrower than 4 history pixels (one march texel at the shipped stride),
	// or it aliases (flickers) by the sun; the same for every stride, so a full march matches the resolved picture
	cvPrm = cvPhasePrm;
	cvPrm.x = min( cvPrm.x, 1.0 - 8.0 / ( cvProj.y * cvHistSize.y ) );
	vec2 ndc = ( q + 0.5 ) / cvHistSize * 2.0 - 1.0;
	vec3 d = normalize( cvCam * vec3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 ) );
	vec3 o = cvCamKm;

	float lo = min( cvBands.baseKm.x, cvBands.baseKm.y ), hi = max( cvBands.topKm.x, cvBands.topKm.y );
	if ( cvBands.density.x <= 0.0 ) lo = cvBands.baseKm.y;
	if ( cvBands.density.y <= 0.0 ) hi = cvBands.topKm.x;
	vec2 gnd = cvSphere( o.z, d, kRb );
	float tGround = ( gnd.y > 0.0 && gnd.x > 0.0 ) ? gnd.x : 1e9;
	vec2 sb = cvSphere( o.z, d, kRb + lo ), st = cvSphere( o.z, d, kRb + hi );
	float t0, t1;
	if ( o.z < lo ) {
		t0 = sb.y;
		t1 = st.y;
	} else if ( o.z < hi ) {
		t0 = 0.0;
		t1 = ( sb.x > 0.0 ) ? sb.x : st.y;
	} else {
		t0 = st.x;
		t1 = ( sb.x > 0.0 ) ? sb.x : st.y;
	}
	t1 = min( min( t1, tGround ), t0 + cvMaxKm );
	// lane CLOUD2B: the scene in front ends the ray (a ridge hides what is behind it; what is in front still draws)
	float tScene = 1e9;
	if ( cvSceneZ.w > 0.5 ) {
		float z = texture( cvScene, ( q + 0.5 ) / cvHistSize ).r;
		if ( z < 1.0 ) {
			vec3 v = vec3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 );
			tScene = length( v ) * cvSceneZ.y / ( 2.0 * z - 1.0 + cvSceneZ.x ) * cvSceneZ.z;
			t1 = min( t1, tScene );
		}
	}

	vec3 L = vec3( 0.0 );
	float T = 1.0, wsum = 0.0, dsum = 0.0;
	float cosT = dot( d, cvKeyW );
	if ( hi > lo && t0 >= 0.0 && t1 > t0 && t0 < tGround ) {
#ifdef WW_CLOUDFLY
		float pixKm = 2.0 / ( cvProj.y * cvHistSize.y );
#if defined( WW_CLOUD_VARIETY ) || defined( WW_CLOUD_ACCUM )
		float j = cvJitter( q );
#else
		float j = fract( 52.9829189 * fract( dot( q, vec2( 0.06711056, 0.00583715 ) ) ) + cvFrame * 0.618034 );
#endif
		// lane CLOUD4: fine steps through the box first, jumping its free cells; the plain march takes the rest of the ray
		if ( cfOn > 0.5 ) {
			float tEnd = min( t1, cfExit( o, d ) ), tc = t0, tn = t0;
			for ( int i = 0; i < cfMaxSteps && tc < tEnd; i++, tc = tn ) {
				vec3 cfUp;
				vec3 c = ( vec3( o.xy + d.xy * tc, cvHeight( o, d, tc, cfUp ) ) - cfOrgKm ) / cfCellKm;
				if ( any( lessThan( c, vec3( 0.0 ) ) ) || any( greaterThanEqual( c, cfDim ) ) )
					break;
				float free = floor( texelFetch( cfSdf, ivec3( c ), 0 ).r * 255.0 + 0.5 ) - cfSafe;
				if ( cfSkip > 0.5 && free >= 1.0 ) {
					tn = min( tc + free * cfCellKm, tEnd );
					continue;
				}
				float dt = min( cfStepKm, tEnd - tc );
				tn = tc + dt;
				float t = tc + j * dt;
				#include "cloud_march_step.glsl"
			}
			t0 = T < 0.005 ? t1 : tc;
		}
		float dt = ( t1 - t0 ) / float( cvSteps );
#ifdef WW_CLOUD_SHAPE
		bool fine = false;	// lane CLOUDSHAPE1: inside cloud (the step refinement below)
		float prevA = -1.0;	// the last interval's probe ahead = this interval's own sample's density (-1 unknown)
#endif
		for ( int i = 0; i < cvSteps && t1 > t0; i++ ) {
#else
		float dt = ( t1 - t0 ) / float( cvSteps );
		float pixKm = 2.0 / ( cvProj.y * cvHistSize.y );	// one history pixel's angle (the light taps share it)
#if defined( WW_CLOUD_VARIETY ) || defined( WW_CLOUD_ACCUM )
		float j = cvJitter( q );
#else
		float j = fract( 52.9829189 * fract( dot( q, vec2( 0.06711056, 0.00583715 ) ) ) + cvFrame * 0.618034 );
#endif
#ifdef WW_CLOUD_SHAPE
		bool fine = false;	// lane CLOUDSHAPE1: inside cloud (the step refinement below)
		float prevA = -1.0;	// the last interval's probe ahead = this interval's own sample's density (-1 unknown)
#endif
		for ( int i = 0; i < cvSteps; i++ ) {
#endif
#ifdef WW_CLOUD_SHAPE
			/* lane CLOUDSHAPE1, step refinement (RDR2 halves its step on hitting cloud, Bauer 2019 sl. 50): a dense, crisp
			 * cloud between coarse samples aliases (Ultra's steps reach ~1 km toward the horizon). Once an interval drew
			 * cloud, the next ones take 2 sub-steps (4 cost the mid view 0.119 ms vs Off's 0.089 at Ultra, offline); an
			 * interval that drew none returns to the coarse step. */
			{
				float dtO = dt, T0 = T;
				int nsubF = clamp( int( ceil( dtO / kShpSubKm ) ), 2, kShpSub );	// sub-steps of at most kShpSubKm, 2..kShpSub
				int nsub = fine ? nsubF : 1;
				/* sheet review item 5: a coarse sample that lands in a crisp cloud is already opaque, so the entry was
				 * quantised to the coarse step (the stripes' main cause was the taps' jitter: cloud_march_step.glsl). Probe
				 * the density one interval AHEAD (no light taps) and, on cloud, march THIS interval finely: the surface lies in it
				 * or the next (HZD / RDR2 step back; probing this interval's own sample missed the part before it) */
				if ( !fine ) {
					float tp = t0 + ( float( i ) + 1.0 + j ) * dtO;
					vec3 upP;
					vec3 pp = o + d * tp;
					pp.z = cvHeight( o, d, tp, upP );
					cvFootKm = tp * pixKm;
					float dA = cvDens( pp );
					bool empty = dA <= 0.0 && prevA == 0.0;	// own sample (probed last time) and the next: no cloud
					bool own = prevA > 0.0;	// own sample in cloud, the next not (a wisp): fine too (one coarse dt banded it)
					prevA = dA > 0.0 ? 1.0 : 0.0;
					if ( dA > 0.0 || own )
						nsub = nsubF;
					else if ( empty )
						continue;	// the step's own read would draw nothing: skip it (empty space costs one read, as Off)
				} else
					prevA = -1.0;
				for ( int s = 0; s < nsub; s++ ) {
					float dt = dtO / float( nsub );
					// each sub-step its own jitter (one shared offset lined the entries up in contour stripes)
					float t = t0 + float( i ) * dtO + ( float( s ) + ( nsub > 1 ? fract( j + 0.618034 * float( s + 1 ) ) : j ) ) * dt;
					#include "cloud_march_step.glsl"
				}
				fine = T < T0;
				if ( T < 0.005 )
					break;
			}
#else
			float t = t0 + ( float( i ) + j ) * dt;
			#include "cloud_march_step.glsl"
#endif
		}
	}
	// the High band: a thin sheet at its base, behind the volume
	vec2 sh = cvSphere( o.z, d, kRb + cvBands.baseKm.z );
	float th = o.z < cvBands.baseKm.z ? sh.y : -1.0;
	if ( cvBands.density.z > 0.0 && th > 0.0 && th < tGround && th < tScene && th < 200.0 && T > 0.005 ) {
		vec3 p = o + d * th;
		vec3 up;
		p.z = cvHeight( o, d, th, up );
		float cov = cvMapAt( p.xy ).a;
		float s = textureLod( cvShape, vec3( ( p.xy + cvBands.windKm.xy ) / ( 2.0 * kCvShapeKm ), 0.5 ), 0.0 ).r;
		float a = clamp( ( s - ( 1.0 - cov ) ) / max( cov, 1e-3 ), 0.0, 1.0 ) * cov;
		float tau = 2.0 * cvBands.density.z * a;
		float al = 1.0 - exp( -tau );
		vec3 key = cvKeyK * atTrans( cvTrans, kRb + p.z, dot( up, cvKeyW ) );
		vec3 S = kCvAlbedo * cvTint[2] * ( key * cvOctaveSum( cosT, 0.5 * tau ) + cvAmbOn * cvAmb );
		L += T * al * S;
		dsum += T * al * th;
		wsum += T * al;
		T *= 1.0 - al;
	}
	float dm = wsum > 1e-5 ? dsum / wsum : 1e4;
	if ( cvApOn > 0.5 && wsum > 1e-5 ) {
		vec3 pe = vec3( d.xy * dm, kRb + o.z + d.z * dm );
		float r1 = length( pe ), mu1 = dot( pe, d ) / r1;
		vec3 ta = atTrans( cvTrans, kRb + o.z, d.z ), tb = atTrans( cvTrans, r1, mu1 );
		vec3 tair = clamp( ta / max( tb, vec3( 1e-6 ) ), 0.0, 1.0 );
		L = L * tair + cvSky( d ) * ( 1.0 - tair ) * ( 1.0 - T );
	}
	cvOut = vec4( L, T );
	cvDepth = vec4( dm );
}

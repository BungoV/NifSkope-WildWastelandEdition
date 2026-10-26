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

layout ( location = 0 ) out vec4 cvOut;
layout ( location = 1 ) out vec4 cvDepth;

const float kCvAlbedo = 0.99;

float cvDens( vec3 p )
{
	if ( cvSlab > 0.0 )
		return ( p.z > cvBands.baseKm.x && p.z < cvBands.topKm.x ) ? cvSlab : 0.0;
	return cloudDensity( p );
}

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

	vec3 L = vec3( 0.0 );
	float T = 1.0, wsum = 0.0, dsum = 0.0;
	float cosT = dot( d, cvKeyW );
	if ( hi > lo && t0 >= 0.0 && t1 > t0 && t0 < tGround ) {
		float dt = ( t1 - t0 ) / float( cvSteps );
		float pixKm = 2.0 / ( cvProj.y * cvHistSize.y );	// one history pixel's angle (the light taps share it)
		float j = fract( 52.9829189 * fract( dot( q, vec2( 0.06711056, 0.00583715 ) ) ) + cvFrame * 0.618034 );
		for ( int i = 0; i < cvSteps; i++ ) {
			float t = t0 + ( float( i ) + j ) * dt;
			vec3 p = o + d * t;
			vec3 up;
			p.z = cvHeight( o, d, t, up );
			cvFootKm = t * pixKm;
			float dens = cvDens( p );
			if ( dens <= 0.0 )
				continue;
			float tauL = 0.0;
			for ( int k = 0; k < cvTaps; k++ ) {
				float a = float( k ) / float( cvTaps ), b = float( k + 1 ) / float( cvTaps );
				float tk = cvLightKm * 0.5 * ( a * a + b * b ), dk = cvLightKm * ( b * b - a * a );
				vec3 pl = p + cvKeyW * tk;
				pl.z = p.z + dot( up, cvKeyW ) * tk;
				tauL += cvDens( pl ) * kCvSigma * dk;
			}
			float hf = clamp( ( p.z - lo ) / max( hi - lo, 1e-3 ), 0.0, 1.0 );
			vec3 key = cvKeyK * atTrans( cvTrans, kRb + p.z, dot( up, cvKeyW ) );
			vec3 S = kCvAlbedo * ( key * cvOctaveSum( cosT, tauL ) + cvAmbOn * cvAmb * mix( 0.5, 1.0, hf ) );
			float tr = exp( -dens * kCvSigma * dt );
			float w = T * ( 1.0 - tr );
			L += S * w;	// Frostbite: S (1 - e^(-sigma dt)) / sigma x sigma_s, sigma_s = albedo sigma
			dsum += w * t;
			wsum += w;
			T *= tr;
			if ( T < 0.005 )
				break;
		}
	}
	// the High band: a thin sheet at its base, behind the volume
	vec2 sh = cvSphere( o.z, d, kRb + cvBands.baseKm.z );
	float th = o.z < cvBands.baseKm.z ? sh.y : -1.0;
	if ( cvBands.density.z > 0.0 && th > 0.0 && th < tGround && th < 200.0 && T > 0.005 ) {
		vec3 p = o + d * th;
		vec3 up;
		p.z = cvHeight( o, d, th, up );
		float cov = cvMapAt( p.xy ).a;
		float s = textureLod( cvShape, vec3( ( p.xy + cvBands.windKm.xy ) / ( 2.0 * kCvShapeKm ), 0.5 ), 0.0 ).r;
		float a = clamp( ( s - ( 1.0 - cov ) ) / max( cov, 1e-3 ), 0.0, 1.0 ) * cov;
		float tau = 2.0 * cvBands.density.z * a;
		float al = 1.0 - exp( -tau );
		vec3 key = cvKeyK * atTrans( cvTrans, kRb + p.z, dot( up, cvKeyW ) );
		vec3 S = kCvAlbedo * ( key * cvOctaveSum( cosT, 0.5 * tau ) + cvAmbOn * cvAmb );
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

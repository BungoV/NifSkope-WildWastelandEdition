#version 410 core

/* lane CLOUD3: the cloud shadow map (src/features/clouds/cloudshadow.cpp), a Beer shadow map (Hillaire 2016 sec. 5.5). One fragment =
 * one line toward the sun. Out: x = the height (km) where the cloud starts seen from the sun, y = the mean extinction per
 * km of height below it, z = the line's whole optical depth, w = 1 where any cloud was met. The receiver
 * (ww_sunshadow.glsl ussCloudT) takes OD = min(z, y x max(x - height, 0)).
 * csMode 1: the volume's density (cloud_common.glsl cloudDensity, bound by wwCloudVolBindDensity); 2: the cloud map's
 * coverage alone (the Low tier, or no volume): T = 1 - c (1 - exp(-tau)) per band. */

#include "cloud_common.glsl"

uniform vec3 csE1;	// light space, world: across the sun
uniform vec3 csE2;
uniform vec3 csS;	// TO the sun (z >= sin 3 deg)
uniform vec4 csFit;	// a0, b0 (km, the map's centre in light space), the side km, the texel km
uniform vec2 csSize;	// texels across
uniform int csMode;
uniform int csSteps;	// per band (volume mode)
uniform vec3 csFs;	// csMode 3 (cloudfusionlaw.h): opacity, strength, half the sheet's height km
/* MODULES1 10c: the sheets of the Fusion Shaders cloud shadow model (cloudsheetmath.h is the host half). One sheet a
 * weather cloud layer, flat at its altitude; csSheetN 0 = 10b's one plane per band (red bandsheets). */
#define CS_SHEETS 8
uniform int csSheetN;
uniform vec4 csSheetP;	// taps, hex (1 / 0), tan of the sun's radius, the receivers' reference height km (the penumbra's)
uniform vec4 csSheetA[CS_SHEETS];	// uv per world unit, row-major 2x2
uniform vec4 csSheetB[CS_SHEETS];	// uv at the zenith + the scroll, the altitude km, the sheet's alpha
uniform vec4 csSheetC[CS_SHEETS];	// the blend target's share, hex cells per tile
uniform sampler2D csSheetTex[CS_SHEETS];
uniform sampler2D csSheetTex2[CS_SHEETS];
uniform float csUnitKm;	// km a world unit

out vec4 fragColor;

// the point of this texel's line at height h km
vec3 csLine( vec2 ab, float h )
{
	float lam = ( h - ab.y * csE2.z ) / csS.z;
	return ab.x * csE1 + ab.y * csE2 + lam * csS;
}

float csSheetAlpha( int i, vec2 st, float mip )
{
	float a = textureLod( csSheetTex[i], st, mip ).a;
	if ( csSheetC[i].x > 0.0 )
		a = mix( a, textureLod( csSheetTex2[i], st, mip ).a, csSheetC[i].x );
	return a;
}

vec2 csSheetHash( vec2 s )
{
	s = fract( s * vec2( 1271.5151, 3337.8237 ) );
	s += dot( s, s.yx + 19.19 );
	return fract( ( s.xx + s.yy ) * s.yx );
}

void main()
{
	vec2 uv = gl_FragCoord.xy / csSize;
	vec2 ab = csFit.xy + ( uv - 0.5 ) * csFit.z;
	// a clear line stores the lowest live base: a texel filtered with a cloudy one then never shadows a point above
	// the cloud's top (the highest top did, near every cloud edge)
	float floorKm = 1e4;
	for ( int b = 0; b < 2; b++ )
		if ( cvBands.density[b] > 0.0 && cvBands.topKm[b] > cvBands.baseKm[b] )
			floorKm = min( floorKm, cvBands.baseKm[b] );
	float od = 0.0, zTop = -1.0, zBot = 0.0;
	if ( csMode == 1 ) {
		cvFootKm = csFit.w;	// the noise mips at the map's texel
		for ( int b = 1; b >= 0; b-- ) {
			float base = cvBands.baseKm[b], top = cvBands.topKm[b];
			if ( cvBands.density[b] <= 0.0 || top <= base )
				continue;
			float dh = ( top - base ) / float( csSteps );
			for ( int i = 0; i < csSteps; i++ ) {
				float h = top - ( float( i ) + 0.5 ) * dh;
				float d = cloudDensity( csLine( ab, h ) );
				if ( d <= 0.0 )
					continue;
				if ( zTop < 0.0 )
					zTop = h + 0.5 * dh;
				zBot = h - 0.5 * dh;
				od += kCvSigma * d * dh / csS.z;
			}
		}
	} else if ( csMode == 3 && csSheetN > 0 ) {
		/* MODULES1 10c: one sheet a layer. This texel's sun line meets sheet i at csLine( ab, altitude ): the
		 * parallax. The sun's disc is a tap pattern on the sheet (radius = the distance along the ray x tan of the
		 * sun's radius, stretched along the azimuth by 1 / sun z); hex tiling breaks the texture's repeat. The
		 * light kept is the product of (1 - cover); a receiver above a sheet is not shadowed by it through the
		 * map's height ramp (x = the highest contributing sheet, the ramp runs down to the lowest). */
		float kept = 1.0, lo = 1e4;
		int taps = clamp( int( csSheetP.x + 0.5 ), 1, 16 );
		bool hex = csSheetP.y > 0.5;
		vec2 az = dot( csS.xy, csS.xy ) > 1e-10 ? normalize( csS.xy ) : vec2( 1.0, 0.0 );
		vec2 ac = vec2( -az.y, az.x );
		float sz = max( csS.z, 1e-3 );
		float texelWorld = csFit.w / sz / csUnitKm;
		floorKm = 1e4;
		for ( int i = 0; i < csSheetN; i++ ) {
			float altKm = csSheetB[i].z;
			floorKm = min( floorKm, altKm - csFs.z );
			vec2 hit = csLine( ab, altKm ).xy / csUnitKm;
			float t = max( altKm - csSheetP.w, 0.0 ) / csUnitKm / sz;
			float rAcross = t * csSheetP.z, rAlong = rAcross / sz;
			mat2 m = mat2( csSheetA[i].x, csSheetA[i].z, csSheetA[i].y, csSheetA[i].w );
			ivec2 ts = textureSize( csSheetTex[i], 0 );
			float maxDim = float( max( ts.x, ts.y ) );
			float uvPerWorld = sqrt( abs( determinant( m ) ) );
			float spacing = 2.0 * rAlong * sqrt( 1.0 / float( taps ) );
			float footUv = max( texelWorld, spacing ) * uvPerWorld;
			float maxMip = log2( max( maxDim, 1.0 ) );
			float mip = clamp( log2( max( footUv, 1e-8 ) * maxDim ), 0.0, maxMip );
			vec2 uvC = m * hit + csSheetB[i].xy;
			vec2 uvAlong = m * ( az * rAlong ), uvAcross = m * ( ac * rAcross );
			vec2 o0 = vec2( 0.0 ), o1 = vec2( 0.0 ), o2 = vec2( 0.0 );
			vec3 w = vec3( 1.0, 0.0, 0.0 );
			float mean = 0.0;
			if ( hex ) {
				vec2 p = uvC * csSheetC[i].y;
				vec2 s = vec2( p.x, -0.57735027 * p.x + 1.15470054 * p.y );
				vec2 c = floor( s ), f = s - c;
				float bz = 1.0 - f.x - f.y;
				vec2 c0, c1, c2;
				if ( bz > 0.0 ) {
					c0 = c;
					c1 = c + vec2( 0.0, 1.0 );
					c2 = c + vec2( 1.0, 0.0 );
					w = vec3( bz, f.y, f.x );
				} else {
					c0 = c + vec2( 1.0 );
					c1 = c + vec2( 1.0, 0.0 );
					c2 = c + vec2( 0.0, 1.0 );
					w = vec3( -bz, 1.0 - f.y, 1.0 - f.x );
				}
				o0 = csSheetHash( c0 );
				o1 = csSheetHash( c1 );
				o2 = csSheetHash( c2 );
				w /= max( length( w ), 1e-6 );
				mean = csSheetAlpha( i, uvC, maxMip );
			}
			float cover = 0.0;
			vec2 dir = vec2( 1.0, 0.0 );
			for ( int k = 0; k < taps; k++ ) {
				float r = taps > 1 ? sqrt( ( float( k ) + 0.5 ) / float( taps ) ) : 0.0;
				vec2 st = uvC + uvAlong * ( r * dir.x ) + uvAcross * ( r * dir.y );
				dir = vec2( dir.x * -0.73736887 - dir.y * 0.67549029, dir.x * 0.67549029 + dir.y * -0.73736887 );
				float a;
				if ( hex )
					a = clamp( mean + dot( w, vec3( csSheetAlpha( i, st + o0, mip ), csSheetAlpha( i, st + o1, mip ),
						csSheetAlpha( i, st + o2, mip ) ) - mean ), 0.0, 1.0 );
				else
					a = csSheetAlpha( i, st, mip );
				cover += clamp( a * csSheetB[i].w, 0.0, 1.0 );
			}
			cover = clamp( cover / float( taps ), 0.0, 1.0 );
			if ( cover <= 0.0 )
				continue;
			kept *= 1.0 - cover;
			zTop = max( zTop, altKm + csFs.z );
			lo = min( lo, altKm - csFs.z );
		}
		zBot = lo;
		float mask = pow( 1.0 - clamp( ( 1.0 - kept ) * csFs.x, 0.0, 1.0 ), csFs.y );
		od = -log( max( mask, 1e-4 ) );
	} else if ( csMode == 3 ) {
		/* MODULES1 10b-7: the sheets law. Each band's cover is a flat sheet at the band's middle; the light kept is
		 * the product of (1 - cover); the mask is 1 - saturate((1 - kept) x opacity), then mask ^ strength. */
		float kept = 1.0;
		for ( int b = 1; b >= 0; b-- ) {
			float base = cvBands.baseKm[b], top = cvBands.topKm[b];
			if ( cvBands.density[b] <= 0.0 || top <= base )
				continue;
			float mid = 0.5 * ( base + top );
			vec4 m = cvMapAt( csLine( ab, mid ).xy );
			float c = clamp( b == 0 ? m.r : m.g, 0.0, 1.0 );
			if ( c <= 0.0 )
				continue;
			kept *= 1.0 - c;
			if ( zTop < 0.0 )
				zTop = mid + csFs.z;
			zBot = mid - csFs.z;
		}
		float mask = pow( 1.0 - clamp( ( 1.0 - kept ) * csFs.x, 0.0, 1.0 ), csFs.y );
		od = -log( max( mask, 1e-4 ) );
	} else {
		for ( int b = 1; b >= 0; b-- ) {
			float base = cvBands.baseKm[b], top = cvBands.topKm[b];
			if ( cvBands.density[b] <= 0.0 || top <= base )
				continue;
			vec4 m = cvMapAt( csLine( ab, 0.5 * ( base + top ) ).xy );
			float c = clamp( b == 0 ? m.r : m.g, 0.0, 1.0 );
			float tau = kCvSigma * cvBands.density[b] * 0.4 * ( top - base ) / csS.z;
			float T = 1.0 - c * ( 1.0 - exp( -tau ) );
			if ( T >= 1.0 )
				continue;
			if ( zTop < 0.0 )
				zTop = top;
			zBot = base;
			od += -log( max( T, 1e-4 ) );
		}
	}
	if ( zTop < 0.0 || od <= 0.0 ) {
		fragColor = vec4( floorKm < 1e4 ? floorKm : 0.0, 0.0, 0.0, 0.0 );
		return;
	}
	fragColor = vec4( zTop, od / max( zTop - zBot, 1e-3 ), od, 1.0 );
}

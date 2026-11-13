// HLSL source of features/clouds/cloud_shadowmap.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane CLOUD3: the cloud shadow map (src/features/clouds/cloudshadow.cpp), a Beer shadow map (Hillaire 2016 sec. 5.5). One fragment =
 * one line toward the sun. Out: x = the height (km) where the cloud starts seen from the sun, y = the mean extinction per
 * km of height below it, z = the line's whole optical depth, w = 1 where any cloud was met. The receiver
 * (ww_sunshadow.glsl ussCloudT) takes OD = min(z, y x max(x - height, 0)).
 * csMode 1: the volume's density (cloud_common.glsl cloudDensity, bound by wwCloudVolBindDensity); 2: the cloud map's
 * coverage alone (the Low tier, or no volume): T = 1 - c (1 - exp(-tau)) per band. */

#define WW_LOOSE_TYPES 1
#include "cloud_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "cloud_common.hlsli"
#undef WW_LOOSE_UNIFORMS
#define CS_SHEETS 8	// as below: the uniforms' sizes
	float3 csE1;	// light space, world: across the sun
	float3 csE2;
	float3 csS;	// TO the sun (z >= sin 3 deg)
	float4 csFit;	// a0, b0 (km, the map's centre in light space), the side km, the texel km
	float2 csSize;	// texels across
	int csMode;
	int csSteps;	// per band (volume mode)
	float3 csFs;	// csMode 3 (cloudfusionlaw.h): opacity, strength, half the sheet's height km
	int csSheetN;
	float4 csSheetP;	// taps, hex (1 / 0), tan of the sun's radius, the receivers' reference height km (the penumbra's)
	float4 csSheetA[CS_SHEETS];	// uv per world unit, row-major 2x2
	float4 csSheetB[CS_SHEETS];	// uv at the zenith + the scroll, the altitude km, the sheet's alpha
	float4 csSheetC[CS_SHEETS];	// the blend target's share, hex cells per tile
	float csUnitKm;	// km a world unit
};
#include "cloud_common.hlsli"


/* MODULES1 10c: the sheets of the Fusion Shaders cloud shadow model (cloudsheetmath.h is the host half). One sheet a
 * weather cloud layer, flat at its altitude; csSheetN 0 = 10b's one plane per band (red bandsheets). */
#define CS_SHEETS 8

Texture2D<float4> csSheetTex[CS_SHEETS] : register(t3, space0); SamplerState _csSheetTex_sampler[CS_SHEETS] : register(s3, space0);
Texture2D<float4> csSheetTex2[CS_SHEETS] : register(t11, space0); SamplerState _csSheetTex2_sampler[CS_SHEETS] : register(s11, space0);

static float4 fragColor;

// the point of this texel's line at height h km
float3 csLine( float2 ab, float h )
{
	float lam = ( h - ab.y * csE2.z ) / csS.z;
	return ab.x * csE1 + ab.y * csE2 + lam * csS;
}

float csSheetAlpha( int i, float2 st, float mip )
{
	float a = csSheetTex[i].SampleLevel( _csSheetTex_sampler[i], st, mip ).a;
	if ( csSheetC[i].x > 0.0 )
		a = lerp( a, csSheetTex2[i].SampleLevel( _csSheetTex2_sampler[i], st, mip ).a, csSheetC[i].x );
	return a;
}

float2 csSheetHash( float2 s )
{
	s = frac( s * float2( 1271.5151, 3337.8237 ) );
	s += dot( s, s.yx + 19.19 );
	return frac( ( s.xx + s.yy ) * s.yx );
}

void frag_main()
{
	float2 uv = gl_FragCoord.xy / csSize;
	float2 ab = csFit.xy + ( uv - 0.5 ) * csFit.z;
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
			if ( or( cvBands.density[b] <= 0.0, top <= base ) )
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
		float2 az = dot( csS.xy, csS.xy ) > 1e-10 ? normalize( csS.xy ) : float2( 1.0, 0.0 );
		float2 ac = float2( -az.y, az.x );
		float sz = max( csS.z, 1e-3 );
		float texelWorld = csFit.w / sz / csUnitKm;
		floorKm = 1e4;
		for ( int i = 0; i < csSheetN; i++ ) {
			float altKm = csSheetB[i].z;
			floorKm = min( floorKm, altKm - csFs.z );
			float2 hit = csLine( ab, altKm ).xy / csUnitKm;
			float t = max( altKm - csSheetP.w, 0.0 ) / csUnitKm / sz;
			float rAcross = t * csSheetP.z, rAlong = rAcross / sz;
			float2x2 m = float2x2( csSheetA[i].x, csSheetA[i].z, csSheetA[i].y, csSheetA[i].w );
			int2 ts = textureSize( csSheetTex[i], 0 );
			float maxDim = float( max( ts.x, ts.y ) );
			float uvPerWorld = sqrt( abs( determinant( m ) ) );
			float spacing = 2.0 * rAlong * sqrt( 1.0 / float( taps ) );
			float footUv = max( texelWorld, spacing ) * uvPerWorld;
			float maxMip = log2( max( maxDim, 1.0 ) );
			float mip = clamp( log2( max( footUv, 1e-8 ) * maxDim ), 0.0, maxMip );
			float2 uvC = mul(hit, m) + csSheetB[i].xy;
			float2 uvAlong = mul(( az * rAlong ), m), uvAcross = mul(( ac * rAcross ), m);
			float2 o0 = (float2)( 0.0 ), o1 = (float2)( 0.0 ), o2 = (float2)( 0.0 );
			float3 w = float3( 1.0, 0.0, 0.0 );
			float mean = 0.0;
			if ( hex ) {
				float2 p = uvC * csSheetC[i].y;
				float2 s = float2( p.x, -0.57735027 * p.x + 1.15470054 * p.y );
				float2 c = floor( s ), f = s - c;
				float bz = 1.0 - f.x - f.y;
				float2 c0, c1, c2;
				if ( bz > 0.0 ) {
					c0 = c;
					c1 = c + float2( 0.0, 1.0 );
					c2 = c + float2( 1.0, 0.0 );
					w = float3( bz, f.y, f.x );
				} else {
					c0 = c + (float2)( 1.0 );
					c1 = c + float2( 1.0, 0.0 );
					c2 = c + float2( 0.0, 1.0 );
					w = float3( -bz, 1.0 - f.y, 1.0 - f.x );
				}
				o0 = csSheetHash( c0 );
				o1 = csSheetHash( c1 );
				o2 = csSheetHash( c2 );
				w /= max( length( w ), 1e-6 );
				mean = csSheetAlpha( i, uvC, maxMip );
			}
			float cover = 0.0;
			float2 dir = float2( 1.0, 0.0 );
			for ( int k = 0; k < taps; k++ ) {
				float r = taps > 1 ? sqrt( ( float( k ) + 0.5 ) / float( taps ) ) : 0.0;
				float2 st = uvC + uvAlong * ( r * dir.x ) + uvAcross * ( r * dir.y );
				dir = float2( dir.x * -0.73736887 - dir.y * 0.67549029, dir.x * 0.67549029 + dir.y * -0.73736887 );
				float a;
				if ( hex )
					a = clamp( mean + dot( w, float3( csSheetAlpha( i, st + o0, mip ), csSheetAlpha( i, st + o1, mip ),
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
			if ( or( cvBands.density[b] <= 0.0, top <= base ) )
				continue;
			float mid = 0.5 * ( base + top );
			float4 m = cvMapAt( csLine( ab, mid ).xy );
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
			if ( or( cvBands.density[b] <= 0.0, top <= base ) )
				continue;
			float4 m = cvMapAt( csLine( ab, 0.5 * ( base + top ) ).xy );
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
	if ( or( zTop < 0.0, od <= 0.0 ) ) {
		fragColor = float4( floorKm < 1e4 ? floorKm : 0.0, 0.0, 0.0, 0.0 );
		return;
	}
	fragColor = float4( zTop, od / max( zTop - zBot, 1e-3 ), od, 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}

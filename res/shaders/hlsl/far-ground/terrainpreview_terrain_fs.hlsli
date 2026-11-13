// HLSL source of features/far-ground/terrainpreview_terrain_fs.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float2 uOrigin;
	float uSpacing;
	int2 uGrid;
	float uTile;
	float4 uFullMap;
	float4 uFarMap;
	int uOption;
	float2 uFade;
	float3 uEye;
	float4 uVanMap;
	float4 uBlendMap;
	int uLaw2;
	float2 uROrigin;
	float uRSpacing;
	int2 uRGrid;
	int uRule;
	float4 uLodaMap;
	int uLodaOn;
#else
static float3 vWorld;
static float4 oAlbedo;
static float4 oNormal;
Texture2D<float4> uHeight : register(t0, space0); SamplerState _uHeight_sampler : register(s0, space0);
Texture2D<uint4> uIdx : register(t1, space0); SamplerState _uIdx_sampler : register(s1, space0);
Texture2D<float4> uW : register(t2, space0); SamplerState _uW_sampler : register(s2, space0);
Texture2D<float4> uVclr : register(t3, space0); SamplerState _uVclr_sampler : register(s3, space0);
Texture2DArray<float4> uLtex : register(t4, space0); SamplerState _uLtex_sampler : register(s4, space0);
Texture2D<float4> uFull : register(t5, space0); SamplerState _uFull_sampler : register(s5, space0);
Texture2D<float4> uFar : register(t6, space0); SamplerState _uFar_sampler : register(s6, space0);
// law 2 (lane TERRLIVE1): vanilla's dim-4 diffuse and our weight against it
Texture2D<float4> uVan : register(t7, space0); SamplerState _uVan_sampler : register(s7, space0);
Texture2D<float4> uBlendW : register(t8, space0); SamplerState _uBlendW_sampler : register(s8, space0);
// the rule paint outside (lane TERRLIVE1, `--outside-paint rule`): the .lodr's ids and weights
Texture2D<uint4> uRIdx : register(t9, space0); SamplerState _uRIdx_sampler : register(s9, space0);
Texture2D<float4> uRW : register(t10, space0); SamplerState _uRW_sampler : register(s10, space0);
Texture2D<float4> uRG : register(t11, space0); SamplerState _uRG_sampler : register(s11, space0);
// the AO map (lane TERRLIVE2, `<ws>.loda`): 32 u sky visibility over our painted ground, 1 elsewhere
Texture2D<float4> uLoda : register(t12, space0); SamplerState _uLoda_sampler : register(s12, space0);

float lodaAo()
{
	if ( uLodaOn == 0 )
		return 1.0;
	return uLoda.SampleLevel( _uLoda_sampler, float2( ( vWorld.x - uLodaMap.x ) * uLodaMap.z, ( uLodaMap.y - vWorld.y ) * uLodaMap.w ), 0.0 ).r;
}

// THE ANTI-REPEAT HOOK (lane TILING6 replaces this body): one LTEX layer at a
// world-tiled uv, with the caller's gradients.
float3 ltexFetch( float layer, float2 uv, float2 dx, float2 dy )
{
	return uLtex.SampleGrad( _uLtex_sampler, float3( uv, layer ), dx, dy ).rgb;
}

float liveWeight()
{
	if ( or( uOption == 2, uOption == 3 ) )
		return 1.0;
	if ( or( uOption == 0, uOption == 4 ) )
		return 0.0;
	float d = distance( vWorld, uEye );
	return 1.0 - smoothstep( uFade.x, uFade.y, d );
}

// one splat over an id/weight grid (the .lodl's, or the rule map's); `cover` = the
// bilinear weight of the samples that hold any texture
float3 splatGrid( Texture2D<uint4> tIdx, SamplerState _tIdx_sampler, Texture2D<float4> tW, SamplerState _tW_sampler, float2 origin, float spacing, int2 grid, float2 wdx, float2 wdy,
	out float2 g, out float cover )
{
	g = ( vWorld.xy - origin ) / spacing;
	float2 g0 = floor( g );
	float2 f = g - g0;
	float2 uv = vWorld.xy / uTile;
	float2 dx = wdx / uTile, dy = wdy / uTile;
	float3 acc = (float3)( 0.0 );
	cover = 0.0;
	for ( int c = 0; c < 4; c++ ) {
		int2 o = int2( c & 1, c >> 1 );
		int2 p = clamp( int2( g0 ) + o, (int2)( 0 ), grid - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		if ( bw <= 0.0 )
			continue;
		uint4 id = tIdx.Load( int3(p, 0) );
		float4 w = tW.Load( int3(p, 0) );
		if ( id.x != 65535u )
			cover += bw;
		for ( int k = 0; k < 4; k++ ) {
			if ( w[k] <= 0.0 )
				continue;
			float3 t = id[k] == 65535u ? (float3)( 0.5 ) : ltexFetch( float( id[k] ), uv, dx, dy );
			acc += bw * w[k] * t;
		}
	}
	return acc;
}

float3 splat( float2 wdx, float2 wdy )
{
	float2 g;
	float cover;
	float3 acc = splatGrid( uIdx, _uIdx_sampler, uW, _uW_sampler, uOrigin, uSpacing, uGrid, wdx, wdy, g, cover );
	float3 vc = uVclr.SampleLevel( _uVclr_sampler, ( g + 0.5 ) / float2( uGrid ), 0.0 ).rgb;
	return acc * vc;
}

// the rule map's brightness gain (TERRLIVE2, .lodr v2 plane G, x128), bilinear over the samples
// that exist -- LodgenRuleMap::mixAt's weighting
float ruleGain( float2 g )
{
	float2 g0 = floor( g ), f = g - g0;
	float s = 0.0, t = 0.0;
	for ( int c = 0; c < 4; c++ ) {
		int2 o = int2( c & 1, c >> 1 );
		int2 p = clamp( int2( g0 ) + o, (int2)( 0 ), uRGrid - 1 );
		float bw = ( o.x == 1 ? f.x : 1.0 - f.x ) * ( o.y == 1 ? f.y : 1.0 - f.y );
		if ( or( bw <= 0.0, uRIdx.Load( int3(p, 0) ).x == 65535u ) )
			continue;
		s += bw * uRG.Load( int3(p, 0) ).r;
		t += bw;
	}
	return t > 0.0 ? s / t * ( 255.0 / 128.0 ) : 1.0;
}

// the rule paint: the map's textures through the same hook, no vertex colour (the bake's rule);
// false where no rule sample reaches
bool ruleSplat( float2 wdx, float2 wdy, out float3 R )
{
	float2 g;
	float cover;
	R = splatGrid( uRIdx, _uRIdx_sampler, uRW, _uRW_sampler, uROrigin, uRSpacing, uRGrid, wdx, wdy, g, cover );
	if ( cover <= 0.0 )
		return false;
	R /= cover;
	R *= ruleGain( g );
	return true;
}

float4 baked4( Texture2D<float4> s, SamplerState _s_sampler, float4 m, float2 wdx, float2 wdy )
{
	float2 uv = float2( ( vWorld.x - m.x ) * m.z, ( m.y - vWorld.y ) * m.w );
	return s.SampleGrad( _s_sampler, uv, float2( wdx.x * m.z, -wdx.y * m.w ), float2( wdy.x * m.z, -wdy.y * m.w ) );
}

float3 baked( Texture2D<float4> s, SamplerState _s_sampler, float4 m, float2 wdx, float2 wdy )
{
	return baked4( s, _s_sampler, m, wdx, wdy ).rgb;
}

// our weight against vanilla (0 = vanilla untouched, 1 = ours); a texel with no vanilla sheet stays ours
float oursWeight( float2 wdx, float2 wdy, out float3 V )
{
	V = (float3)( 0.0 );
	if ( uLaw2 == 0 )
		return 1.0;
	float4 v = baked4( uVan, _uVan_sampler, uVanMap, wdx, wdy );
	V = v.rgb;
	float w = uBlendW.SampleLevel( _uBlendW_sampler, float2( ( vWorld.x - uBlendMap.x ) * uBlendMap.z, ( uBlendMap.y - vWorld.y ) * uBlendMap.w ), 0.0 ).r;
	// the rule paint takes vanilla's place in the same band, where it has a sample
	if ( uRule == 1 && w < 1.0 ) {
		float3 R;
		if ( ruleSplat( wdx, wdy, R ) ) {
			V = R;
			return w;
		}
	}
	return 1.0 - ( 1.0 - w ) * v.a;
}

float3 terrainNormal()
{
	float2 g = ( vWorld.xy - uOrigin ) / uSpacing;
	float2 e = 1.0 / float2( uGrid );
	float2 uvh = ( g + 0.5 ) * e;
	float hl = uHeight.SampleLevel( _uHeight_sampler, uvh - float2( e.x, 0.0 ), 0.0 ).r;
	float hr = uHeight.SampleLevel( _uHeight_sampler, uvh + float2( e.x, 0.0 ), 0.0 ).r;
	float hd = uHeight.SampleLevel( _uHeight_sampler, uvh - float2( 0.0, e.y ), 0.0 ).r;
	float hu = uHeight.SampleLevel( _uHeight_sampler, uvh + float2( 0.0, e.y ), 0.0 ).r;
	return normalize( float3( ( hl - hr ) / ( 2.0 * uSpacing ), ( hd - hu ) / ( 2.0 * uSpacing ), 1.0 ) );
}

void main()
{
	float2 wdx = ddx( vWorld.xy ), wdy = ddy( vWorld.xy );
	float wl = liveWeight();
	float3 col;
	if ( uOption == 0 )
		col = baked( uFull, _uFull_sampler, uFullMap, wdx, wdy );
	else if ( uOption == 4 )
		col = baked( uFar, _uFar_sampler, uFarMap, wdx, wdy );
	else {
		float3 L = (float3)( 0.0 ), B = (float3)( 0.0 );
		if ( wl > 0.0 ) {
			float3 V;
			float wo = oursWeight( wdx, wdy, V );
			L = wo > 0.0 ? splat( wdx, wdy ) * lodaAo() : (float3)( 0.0 );
			L = lerp( V, L, wo );
		}
		if ( wl < 1.0 )
			B = baked( uFar, _uFar_sampler, uFarMap, wdx, wdy );
		col = lerp( B, L, wl );
	}
	oAlbedo = float4( col, 1.0 );
	oNormal = float4( terrainNormal() * 0.5 + 0.5, 1.0 );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/far-ground/terrainpreview_terrain_fs.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Not here (declarations, or no converted entry carried them): lodaAo, ltexFetch, liveWeight, splatGrid, splat, ruleGain, ruleSplat, baked4, baked, oursWeight, terrainNormal, main
#endif	// WW_HLSL_SRC

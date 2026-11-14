// HLSL source of features/reflections/cell_reflprobe.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool cellReflOn;		// this draw reads the probes (the row on, not red, an env-mapped material)
	int cellReflCount;		// probes in the arrays (the camera's first)
	float4 cellReflPos[33];	// xyz the capture point (world), w the atlas tile
	float4 cellReflMin[33];	// the box (world), w the fade band
	float4 cellReflMax[33];	// w: 1 the camera's probe (takes what the boxes leave), 0 a placed one
	float4 cellReflAtlas;		// 1 / atlas width, 1 / atlas height, tile size (px), tiles across
	float cellReflLods;		// the atlas's last mip
#else
#ifndef WW_GLSL_MOD
#define WW_GLSL_MOD	// GLSL mod: x - y floor(x / y) (HLSL fmod truncates)
float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
#endif
// lane REFLPROBE1 (src/features/reflections/cellreflprobe.h): the placed reflection probes a cell-lit surface reads.
// Included after cell_lights.glsl by the two cell programs. The atlas comes in on a sampler the draw leaves idle
// (GreyscaleMap without a palette): fo4_cellcsm sits at the 32 samplers a fragment stage may hold.



static float cellReflW = 0.0, cellReflFall = 0.0;	// probe 66: the boxes' weight, the camera probe's share

float2 cellReflOct( float3 d )
{
	float3 n = d / ( abs( d.x ) + abs( d.y ) + abs( d.z ) );
	float2 f = n.xy;
	if ( n.z < 0.0 )
		f = ( 1.0 - abs( n.yx ) ) * float2( n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0 );
	return f * 0.5 + 0.5;
}

float3 cellReflSample( Texture2D<float4> atlas, SamplerState _atlas_sampler, int i, float3 P, float3 R, float lod )
{
	float3 bmin = cellReflMin[i].xyz, bmax = cellReflMax[i].xyz;
	// Lagarde's parallax: the ray to the box's far wall, seen from the capture point
	float3 Rs = float3( abs( R.x ) < 1e-5 ? 1e-5 : R.x, abs( R.y ) < 1e-5 ? 1e-5 : R.y, abs( R.z ) < 1e-5 ? 1e-5 : R.z );
	float3 tf = max( ( bmax - P ) / Rs, ( bmin - P ) / Rs );
	float t = min( tf.x, min( tf.y, tf.z ) );
	float3 d = t > 0.0 ? P + R * t - cellReflPos[i].xyz : R;
	float T = cellReflAtlas.z;
	float tile = cellReflPos[i].w;
	float2 org = float2( mod( tile, cellReflAtlas.w ), floor( tile / cellReflAtlas.w ) ) * T;
	float inset = 0.5 * exp2( ceil( lod ) ) / T;	// the coarser mip's half texel: no bleed from the next tile
	float2 uv = clamp( cellReflOct( normalize( d ) ), (float2)( inset ), (float2)( 1.0 - inset ) );
	return atlas.SampleLevel( _atlas_sampler, ( org + uv * T ) * cellReflAtlas.xy, lod ).rgb;
}

/* Lagarde's blend: each box's fade x its parallax-corrected sample, normalised past 1; the camera's probe takes
 * what is left. The result is light already, so it is added (cellReflAdd) at lane CUBE1's strength k and NOT x the
 * diffuse light; the return scales the material cube's term (0 where the probes cover it). */
float cellReflProbe( Texture2D<float4> atlas, SamplerState _atlas_sampler, float3 posView, float3 reflView, float gloss, float spec, float envScale )
{
	if ( or( or( !cellReflOn, !cellCubeGameOn() ), ( cellProbe != 0 && cellProbe != 66 ) ) )
		return 1.0;
	float3 P = cellWorldPos( posView );
	float3 R = normalize( cellWorldDir( reflView ) );
	float g = clamp( gloss, 0.0, 1.0 );
	float lod = clamp( ( 1.0 - g ) * 6.0 - 1.0, 0.0, cellReflLods );	// lane CUBE1's mip on a 128 cube; ours are 64
	float3 sum = (float3)( 0.0 );
	float W = 0.0;
	int fall = -1;
	for ( int i = 0; i < cellReflCount; i++ ) {
		if ( cellReflMax[i].w > 0.5 ) {
			fall = i;
			continue;
		}
		float3 dmin = min( P - cellReflMin[i].xyz, cellReflMax[i].xyz - P );
		float w = clamp( min( dmin.x, min( dmin.y, dmin.z ) ) / max( cellReflMin[i].w, 1.0 ), 0.0, 1.0 );
		if ( w <= 0.0 )
			continue;
		sum += w * cellReflSample( atlas, _atlas_sampler, i, P, R, lod );
		W += w;
	}
	if ( W > 1.0 ) {
		sum /= W;
		W = 1.0;
	}
	cellReflW = W;
	if ( fall >= 0 && W < 1.0 ) {
		cellReflFall = 1.0 - W;
		sum += cellReflFall * cellReflSample( atlas, _atlas_sampler, fall, P, R, lod );
		W = 1.0;
	}
	float k = 3.0 * clamp( spec, 0.0, 1.0 ) * min( sqrt( clamp( g - 0.3, 0.0, 1.0 ) ), 1.0 ) * clamp( envScale, 0.0, 50.0 );
	cellReflAdd = sum * k;
	return 1.0 - W;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/reflections/cell_reflprobe.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl

#if defined(WW_FN_cellReflOct) || defined(WW_FN_ALL)
float2 cellReflOct(float3 d)
{
    float3 n = d / ((abs(d.x) + abs(d.y)) + abs(d.z)).xxx;
    float2 f = n.xy;
    if (n.z < 0.0f)
    {
        f = (1.0f.xx - abs(n.yx)) * float2((n.x >= 0.0f) ? 1.0f : (-1.0f), (n.y >= 0.0f) ? 1.0f : (-1.0f));
    }
    return (f * 0.5f) + 0.5f.xx;
}
#endif

#if defined(WW_FN_cellReflSample) || defined(WW_FN_ALL)
float3 cellReflSample(Texture2D<float4> atlas, SamplerState _atlas_sampler, int i, float3 P, float3 R, float lod)
{
    float3 bmin = cellReflMin[i].xyz;
    float3 bmax = cellReflMax[i].xyz;
    float RsPick3;
    if (abs(R.x) < 9.9999997473787516355514526367188e-06f)
    {
        RsPick3 = 9.9999997473787516355514526367188e-06f;
    }
    else
    {
        RsPick3 = R.x;
    }
    float RsPick2;
    if (abs(R.y) < 9.9999997473787516355514526367188e-06f)
    {
        RsPick2 = 9.9999997473787516355514526367188e-06f;
    }
    else
    {
        RsPick2 = R.y;
    }
    float RsPick;
    if (abs(R.z) < 9.9999997473787516355514526367188e-06f)
    {
        RsPick = 9.9999997473787516355514526367188e-06f;
    }
    else
    {
        RsPick = R.z;
    }
    // Lagarde's parallax: the ray to the box's far wall, seen from the capture point
    float3 Rs = float3(RsPick3, RsPick2, RsPick);
    float3 tf = max((bmax - P) / Rs, (bmin - P) / Rs);
    float t = min(tf.x, min(tf.y, tf.z));
    float3 dPick;
    if (t > 0.0f)
    {
        dPick = (P + (R * t)) - cellReflPos[i].xyz;
    }
    else
    {
        dPick = R;
    }
    float3 d = dPick;
    float T = cellReflAtlas.z;
    float tile = cellReflPos[i].w;
    float2 org = float2(mod(tile, cellReflAtlas.w), floor(tile / cellReflAtlas.w)) * T;
    // the coarser mip's half texel: no bleed from the next tile
    float inset = (0.5f * exp2(ceil(lod))) / T;
    float3 uvPart = normalize(d);
    float2 uv = clamp(cellReflOct(uvPart), inset.xx, (1.0f - inset).xx);
    return atlas.SampleLevel(_atlas_sampler, (org + (uv * T)) * cellReflAtlas.xy, lod).xyz;
}
#endif

#if defined(WW_FN_cellReflProbe) || defined(WW_FN_ALL)
float cellReflProbe(Texture2D<float4> atlas, SamplerState _atlas_sampler, float3 posView, float3 reflView, float gloss, float spec, float envScale)
{
    bool RsPick3 = !cellReflOn;
    bool RsPick2;
    if (!RsPick3)
    {
        RsPick2 = !cellCubeGameOn();
    }
    else
    {
        RsPick2 = RsPick3;
    }
    bool RsPick;
    if (!RsPick2)
    {
        RsPick = (cellProbe != 0) && (cellProbe != 66);
    }
    else
    {
        RsPick = RsPick2;
    }
    if (RsPick)
    {
        return 1.0f;
    }
    float3 dPick = posView;
    float3 P = cellWorldPos(dPick);
    float3 uvPart = reflView;
    float3 R = normalize(cellWorldDir(uvPart));
    float g = clamp(gloss, 0.0f, 1.0f);
    // lane CUBE1's mip on a 128 cube; ours are 64
    float lod = clamp(((1.0f - g) * 6.0f) - 1.0f, 0.0f, cellReflLods);
    float3 sum = 0.0f.xxx;
    float W = 0.0f;
    int fall = -1;
    for (int i = 0; i < cellReflCount; i++)
    {
        if (cellReflMax[i].w > 0.5f)
        {
            fall = i;
            continue;
        }
        float3 dmin = min(P - cellReflMin[i].xyz, cellReflMax[i].xyz - P);
        float w = clamp(min(dmin.x, min(dmin.y, dmin.z)) / max(cellReflMin[i].w, 1.0f), 0.0f, 1.0f);
        if (w <= 0.0f)
        {
            continue;
        }
        int sumPart8 = i;
        float3 sumPart7 = P;
        float3 sumPart6 = R;
        float sumPart5 = lod;
        sum += (cellReflSample(atlas, _atlas_sampler, sumPart8, sumPart7, sumPart6, sumPart5) * w);
        W += w;
    }
    if (W > 1.0f)
    {
        sum /= W.xxx;
        W = 1.0f;
    }
    cellReflW = W;
    if ((fall >= 0) && (W < 1.0f))
    {
        cellReflFall = 1.0f - W;
        int sumPart4 = fall;
        float3 sumPart3 = P;
        float3 sumPart2 = R;
        float sumPart = lod;
        sum += (cellReflSample(atlas, _atlas_sampler, sumPart4, sumPart3, sumPart2, sumPart) * cellReflFall);
        W = 1.0f;
    }
    float k = ((3.0f * clamp(spec, 0.0f, 1.0f)) * min(sqrt(clamp(g - 0.300000011920928955078125f, 0.0f, 1.0f)), 1.0f)) * clamp(envScale, 0.0f, 50.0f);
    cellReflAdd = sum * k;
    return 1.0f - W;
}
#endif
#endif	// WW_HLSL_SRC

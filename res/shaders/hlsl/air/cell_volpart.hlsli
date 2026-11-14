// HLSL source of features/air/cell_volpart.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	int volPart;			// 0 off: the law above, as before; 1 on
	int volPartRed;			// gates only: 1 flat, 2 nothick, 4 nosoft, 8 nolights, 16 white, 32 nofog, 64 noshadow,
	// 128 noedge, 256 nophase, 512 nonear, 1024 flatloop
	int volPartDump;		// gates only: the probe row's width in pixels (particles.geom), 0 off
	int vpTiled;			// MODULES1 M3: 1 the lit card's UVs tile or sit in an atlas cell (no one sprite): a slab, a0 as drawn
	bool volCsm;			// the sun's cascades are drawn this frame
	int volTerms;			// the air's mask (WW_CELL_AIR_TERMS): 1 directional, 2 placed, 4 GI, 8 ambient
	/* FARCITYWASH1-PART: the air's veil (cell_volair.frag airVeilK, the same law and guards) on the puff's sky-lit share,
	 * so a daytime puff sits at the air's level around it; lamps keep theirs. Red WW_CELL_AIR_RED=veilphys: off. */
	bool volVeil;
#if ((!( defined( WW_FX_DECALLIT ) && defined( WW_FX_DECAL_NOKEEP ) )))
	float4 volEnvGrid;
#endif
#if defined(PC_LOOK)
	/* lane PRECIPLOOK1: the precipitation look (cell_precip_look.prog only). pcLook 1 = a raindrop: lit by a clear water
	 * sphere's phase instead of the air's. pi x p in 64 bins of the scattering angle (bin centres (i + 0.5) pi / 64, 0 =
	 * forward, the light behind the drop): n 1.333, geometric optics, orders 0-3 (van de Hulst ch. 12) + the higher orders
	 * as an isotropic floor, checked by a 3D Monte Carlo trace (notes/preciplook1/tools/dropphase.py: g 0.7668 vs 0.7684,
	 * max 3.4 % over the 26 bins that carry the power, primary bow 137.9 deg). Diffraction (a lobe inside the drop's own
	 * image of the lamp) is left out: the opacity is the geometric coverage. pcLook 2 = a snow flake with its normal map. */
	int pcLook;
#endif
#else
// lane VOLPART1: the volumetric law shared by particles.frag (a sprite) and fo4_effectshader.frag (a lit effect
// card, as the street steam): included once, after cell_lights.glsl, lookdev_fog.glsl, lightingInfluence and fxLitMode.
// lane MODULES1 M3 (G7): the lit mix of vpLit (guarded: the effect and particle shaders include it too)
#include "../lights/cell_fxlaw.hlsli"
/* lane VOLPART1: VOLUMETRIC PARTICLES (src/features/particles/glparticles.cpp; the row "Volumetric Particles" ships off). A puff of
 * steam or fog is a little piece of the air: lit by the light that is really there, from the very list the air
 * (Volumetric Air, cell_volair.frag) reads -- the directional light x its cascade shadow, every placed light (colour x
 * radial curve x cone x its shadow cube) x the air's transmittance to it, the GI and the ambient as six-axis cubes --
 * evaluated DIRECTLY per pixel at the sprite's own point, never from the integrated volume. Then it is seen through
 * the air between it and the eye (the INTEGRATED froxel volume, as every surface: colour x T + L), so steam and air
 * fade alike.
 *   thin   the light the medium scatters toward the eye, the air's phase: per unit opacity, the air's own J
 *   thick  a wrapped diffuse at the puff's normal (the normal map turned by the sprite's rotation, else a sphere over
 *          the sprite): max(0, N.L + w) / (1 + w) per light, the cubes read at N
 *   alpha as thickness: the pixel's opacity a is 1 - exp(-tau), so its single-scatter share is exp(-tau) = 1 - a;
 *          lit = (1 - a) thin + a thick, wrap w = 1 - a (a thin edge lets the light round it)
 *   soft   the sprite is a ball of half-depth h = r sqrt(1 - |q|^2) round its plane; the share of it in front of the
 *          scene (the opaque pass's depth) is its opacity: no hard line where a card meets the ground or a wall
 *   phase  every light the puff scatters goes through the air's phase (its Rayleigh + forward / back HG lobes, the very
 *          volPhasePi the air reads): the thin share takes pi x p(c), the thick share its wrap x p(c) / p_iso, so a
 *          puff with a light behind it glows and one lit from the eye's side reads duller (p_iso = the weights / 4)
 * A lit system only (the effect's lighting flag, as the game); lightingInfluence mixes from 1 as before. An additive
 * one keeps the effect law (it emits) and takes the air's transmittance alone.
 *
 * A LIT EFFECT CARD (the street's ground steam, fo4_effectshader.frag) is a puff too, a fake volume:
 *   puff   the card is a ball of its own size: the view ray's path through it at the card point q (texCoord over
 *          -1..1) is l = sqrt(1 - |q|^2) of the centre's, so the opacity a = 1 - (1 - a0)^(1.5 l) (the 1.5 keeps the
 *          card's mean optical depth: the mean of l over the disc is 2/3): dense in the middle, thin at the rim
 *   edge   a card turned edge-on fades: x smoothstep(VP_EDGE0, VP_EDGE1, |N.V|) at its face normal (no vertical sheets)
 *   near   a card fades by its depth from the eye: x smoothstep(VP_NEAR0, VP_NEAR1, depth in game units), so walking
 *          into steam never cuts a card across the screen
 *   soft   the slab of the card's soft depth against the opaque pass's depth (vpCardSoft), drawn with no depth test
 * The puff's opacity is its thickness (the light's thin / thick share); edge, near and soft are coverage alone.
 * VOLPART1R (on the landed VOLAIR1): the medium is the air's layered law (its near band and ground layer), and the sky's
 * share (the directional light, the GI, the ambient) takes the records' haze colour as the air's does (airEnvColour,
 * red physcol the physical colour); the placed lights keep their own colours. The far grid's terrain shadow (Far Air,
 * ships off) is not mirrored: a puff takes the cascades alone. */
// the cascade variants (WW_SUNSHADOW) have ww_sunshadow.glsl already (the loader takes no comment after an #include)
#ifndef WW_SUNSHADOW
#include "../shadows/ww_sunshadow.hlsli"
#endif
// SKY3: a puff reads the air's own filled volume as the air pass does, not the grid's shell (red WW_CELL_PARTICLES_AIR_RED=1)
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1
#endif
#include "cell_airmed.hlsli"
// lane AIRZONE1: the hand-placed volumes' particles block (the lit-decal programs sit at the limit: not there)
#if defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )
#include "cell_airzone.hlsli"
#endif

/* MODULES1 10b-3 (bungo: the same sky visibility as the air): the air's env strata (its froxel history: stratum 4 =
 * env rgb + leak fade, stratum 5 = sun gate, open-field share, 1 = valid), fetched nearest at the card's own froxel,
 * through the air's own law airAmbKeep. volEnvGrid = columns, rows, slices, 1 = held (0: no strata, the keep is 1, as
 * the air's with no leak fix). Red WW_CELL_FXLOCAL_RED=noleakfade. MODULES1 10c: the lit-decal programs (the mist
 * cards) take it too (measured room: 8.6k instructions under the limit); offline way back WW_FX_DECAL_NOKEEP. */
#if !( defined( WW_FX_DECALLIT ) && defined( WW_FX_DECAL_NOKEEP ) )
Texture3D<float4> volEnvHist : register(t19, space0); SamplerState _volEnvHist_sampler : register(s19, space0);
float vpAmbKeep( float3 posView )
{
	if ( volEnvGrid.w < 0.5 )
		return 1.0;
	float d = length( posView );
	float4 c = mul(float4( posView, 1.0 ), projectionMatrix);
	if ( or( or( d <= volSlices.x, c.w <= 0.0 ), any( (abs( c.xy ) > (float2)( c.w )) ) ) )
		return 1.0;
	int3 n = int3( volEnvGrid.xyz + 0.5 );
	float f = log( d / volSlices.x ) / volSlices.y * float( n.z );
	if ( f >= float( n.z ) )
		return 1.0;
	int2 xc = clamp( int2( ( c.xy / c.w * 0.5 + 0.5 ) * float2( n.xy ) ), (int2)( 0 ), n.xy - 1 );
	int xk = clamp( int( f ), 0, n.z - 1 );
	float4 h1 = volEnvHist.Load( int4(int3( xc, 5 * n.z + xk ), 0) );
	if ( h1.z <= 0.9999 )
		return 1.0;
	return airAmbKeep( volEnvHist.Load( int4(int3( xc, 4 * n.z + xk ), 0) ).a, h1.y );
}
#endif
// MODULES1 R7-1: the sun a puff takes: the surfaces' (cell_fxlaw.glsl vpSun) under the local law, else the game's base
bool vpLocal()
{
	return fxLocalOn() && !cellInterior;
}
float3 vpSunColour()
{
	return vpLocal() ? vpSun.rgb : cellDirColor;
}
bool vpHasSun()
{
	return vpLocal() ? any( (vpSun.rgb > (float3)( 0.0 )) ) : cellHasDir;
}
float vpVeilK( float3 Jenv, float t, float z )
{
	if ( or( or( or( !volVeil, cellInterior ), !( cellOn && vpHasSun() ) ), ( volTerms & 1 ) == 0 ) )
		return 1.0;
	float hb;
	float3 fc;
	wwFogEval( t, z, hb, fc );
	const float3 lumK = float3( 0.2126, 0.7152, 0.0722 );
	float rec = dot( max( fc, (float3)( 0.0 ) ), lumK );
	float open = 0.25 * dot( vpSunColour(), lumK ) + dot( Jenv, lumK );
	return open > rec && open > 1e-8 ? rec / open : 1.0;
}

static const float VP_EDGE0 = 0.05, VP_EDGE1 = 0.35;	// |N.V| a card is gone at / whole from
static const float VP_NEAR0 = 16.0, VP_NEAR1 = 128.0;	// game units from the eye a card is gone at / whole from

#ifdef PC_LOOK

static const float PC_DROP_PI[64] = { 4.04346, 3.90733, 3.67826, 3.38087, 3.04174, 2.68648, 2.33624, 2.00613,
	1.70524, 1.43772, 1.20417, 1.00292, 0.831049, 0.685161, 0.561819, 0.457799, 0.370243, 0.296684, 0.235034, 0.183567,
	0.140872, 0.105796, 0.0774053, 0.0549332, 0.03773, 0.0252197, 0.0168245, 0.0118989, 0.0096194, 0.00885462,
	0.00845281, 0.00811393, 0.00782906, 0.0075927, 0.00740107, 0.00725139, 0.00714302, 0.00707708, 0.00705803, 0.0070959,
	0.00738626, 0.00864573, 0.0111939, 0.0150758, 0.0211726, 0.0439136, 0.00599946, 0.00597454, 0.00595439, 0.248741,
	0.116676, 0.0919814, 0.0766472, 0.0640965, 0.0527782, 0.0426043, 0.0342018, 0.0285447, 0.0262184, 0.0258249, 0.02565,
	0.0255301, 0.0254555, 0.0254196 };
static const float PC_DROP_G = 0.7668;

float pcDropPi( float c )
{
	float x = acos( clamp( c, -1.0, 1.0 ) ) * ( 64.0 / PI ) - 0.5;
	int i = int( floor( x ) );
	float f = x - float( i );
	return lerp( PC_DROP_PI[clamp( i, 0, 63 )], PC_DROP_PI[clamp( i + 1, 0, 63 )], clamp( f, 0.0, 1.0 ) );
}

// volCubeScatter's form (the six-axis cube's first two moments) with the drop's g and no haze tint
float3 pcDropCube( float3 E[6], float3 dW )
{
	float3 fl = ( 2.0 / 3.0 ) * ( E[0] + E[1] + E[2] + E[3] + E[4] + E[5] );
	float3 l1 = ( E[0] - E[1] ) * dW.x + ( E[2] - E[3] ) * dW.y + ( E[4] - E[5] ) * dW.z;
	return 0.25 * max( fl + 3.0 * PC_DROP_G * l1, (float3)( 0.0 ) );
}

// the snow look's key light: the lights' luminance-weighted direction (world, xyz) and luminance (w), their colour
static float4 vpDom = (float4)( 0.0 );
static float3 vpDomC = (float3)( 0.0 );
#endif

// pi x the air's phase at c; red nophase: isotropic (the weights / 4)
float3 vpPhase( float c )
{
#ifdef PC_LOOK
	if ( pcLook == 1 )
		return (float3)( pcDropPi( c ) );
#endif
	return ( volPartRed & 256 ) != 0 ? volGi.rgb * 0.25 : volPhasePi( c );
}

// the phase over its isotropic value (1 on average over the sphere)
float3 vpPhaseN( float c )
{
	return vpPhase( c ) / max( volGi.rgb * 0.25, (float3)( 1e-6 ) );
}

float vpEdgeFade( float nv )
{
	return ( volPartRed & 128 ) != 0 ? 1.0 : smoothstep( VP_EDGE0, VP_EDGE1, nv );
}

float vpNearFade( float depth )
{
	return ( volPartRed & 512 ) != 0 ? 1.0 : smoothstep( VP_NEAR0, VP_NEAR1, depth );
}

// the card's opacity at q (-1..1 over it) from its own a0; red nothick: a0 as drawn
float vpPuffAlpha( float a0, float2 q )
{
	if ( or( ( volPartRed & 2 ) != 0, vpTiled != 0 ) )
		return a0;
	float l = sqrt( max( 1.0 - dot( q, q ), 0.0 ) );
	return 1.0 - pow( 1.0 - clamp( a0, 0.0, 0.999 ), 1.5 * l );
}

float3 vpTint( float3 e )
{
	return ( volPartRed & 16 ) != 0 ? (float3)( dot( e, float3( 0.2126, 0.7152, 0.0722 ) ) ) : e;	// red white: grey light
}

float vpWrap( float nl, float w )
{
	return max( nl + w, 0.0 ) / ( 1.0 + w );
}

// the six-axis cube at N: the sum of n_a^2 over the axes N faces
float3 vpCubeAt( float3 C[6], float3 N )
{
	float3 n2 = N * N;
	return n2.x * ( N.x >= 0.0 ? C[0] : C[1] ) + n2.y * ( N.y >= 0.0 ? C[2] : C[3] ) + n2.z * ( N.z >= 0.0 ? C[4] : C[5] );
}

/* lane INTERIORPERF1: the light list of the cluster that holds posView (cell_cluster.glsl). A particle lights at its
 * sprite's centre, not at this fragment, so the tile is posView's own pixel through this draw's projection, not
 * gl_FragCoord; the clusters are padded past their tiles and slices. Every light a list leaves out is past its
 * radius or outside its cone/plane at P: cellLightE gives exactly 0 there, so the sum is the flat loop's bits.
 * Off screen, outside the depth range, no viewport, no lists or the red flatloop (1024): every light, as before. */
int vpLightsBegin( float3 P, float3 posView, out int n )
{
	n = cellLightCount;
	if ( or( or( or( !cellClusterOn, ( volPartRed & 1024 ) != 0 ), volRect.z <= 0.0 ), volRect.w <= 0.0 ) )
		return -1;
	float4 c = mul(float4( posView, 1.0 ), projectionMatrix);
	if ( or( c.w <= 0.0, any( (abs( c.xyz ) > (float3)( c.w )) ) ) )
		return -1;
	float2 px = volRect.xy + ( c.xy / c.w * 0.5 + 0.5 ) / volRect.zw;
	int3 d = int3( cellClusterDims.xyz + 0.5 );
	int2 t = clamp( int2( floor( ( px - cellClusterOrigin ) / cellClusterDims.w ) ), (int2)( 0 ), d.xy - 1 );
	float s = floor( ( log( max( dot( cellClusterDepth.xyz, P ) + cellClusterDepth.w, 1e-6 ) ) - cellClusterLog.x ) * cellClusterLog.y );
	int cl = ( clamp( int( s ), 0, d.z - 1 ) * d.y + t.y ) * d.x + t.x;
	int k = cellCluster.Load( 2 * cl + 1 ).r;
	if ( k < 0 )
		return -1;
	n = k;
	return cellClusterBase + cellCluster.Load( 2 * cl ).r;
}

#ifndef WW_FX_DECALLIT
/* FARWATER1 late item 4 (bungo b71: the barrel fire does not light its smoke): an air card's lamps past the thin share.
 * A dense plume scatters many times and its light tends to the isotropic diffuse of the irradiance (wrap 1, no phase):
 * per lamp e x max( wrap( N.L, 1 ) - phase, 0 ), added by vpLit x the card's opacity (vpAirCardA), so each lamp gives
 * max( thin, mix( thin, diffuse, a ) ): a lamp's forward lobe toward the eye is kept. The env and the GI stay thin
 * (round 9). Red WW_CELL_FXLOCAL_RED=thinlamps (fxLocalRed 8). */
static float3 vpLampMs = (float3)( 0.0 );
static float vpAirCardA = 0.0;
#endif

/* the light at the sprite's point (posView, world P, height z in the fog's frame) toward the eye along dW (world):
 * thin = the air's J there (pi x radiance per unit opacity), thick = the wrapped diffuse at world normal N */
void vpLight( float3 posView, float3 P, float3 dW, float z, float3 N, float w, out float3 thin, out float3 thick )
{
	thin = (float3)( 0.0 );
	thick = (float3)( 0.0 );
#ifndef WW_FX_DECALLIT
	vpLampMs = (float3)( 0.0 );
#endif
	float3 skyThin = (float3)( 0.0 ), skyThick = (float3)( 0.0 );	// the sky's share: the records' colour at the end (the air's rule)
	float3 jEnv = (float3)( 0.0 );	// the env share alone, as the air's Jenv (the veil's open level)
	airGroundAt( P );	// FOGTERRAIN2: the ground layer on the pass's terrain grid, as the froxels at this point
#ifdef PC_LOOK
	vpDom = (float4)( 0.0 );
	vpDomC = (float3)( 0.0 );
#endif
	if ( ( volPartRed & 1 ) != 0 )
		N = cellWorldDir( float3( 0.0, 0.0, 1.0 ) );	// red flat: every pixel faces the eye's axis
	if ( ( volTerms & 1 ) != 0 && vpHasSun() ) {
		float3 to = normalize( cellDirTo );
		float vis = ( !cellInterior && volCsm && ( volPartRed & 64 ) == 0 ) ? wwSunShadow( posView ) : 1.0;
		// lane WEATHERBLEND4: the sun's column through the drifting air (lookdev_fog.glsl wwAirDriftSunK)
		float3 e = vpTint( vpSunColour() ) * vis * pow( airTupSun( z, to.z ), wwAirDriftSunK( posView, z, to.z, airMed.w ) );	// FIX1101A: airTupSun
		skyThin += e * vpPhase( dot( to, dW ) );
		skyThick += e * vpWrap( dot( N, to ), w ) * vpPhaseN( dot( to, dW ) );
#ifdef PC_LOOK
		vpDom += float4( to, 1.0 ) * dot( e, float3( 0.2126, 0.7152, 0.0722 ) );
		vpDomC += e;
#endif
	}
	float zOff = z - P.z;	// the fog's height frame minus world z
	if ( ( volTerms & 2 ) != 0 && ( volPartRed & 8 ) == 0 ) {
		float dP = wwAirDriftAt( posView );	// lane WEATHERBLEND4: the drifting air at the puff, on its lamps' paths
		float s2 = dot( cellRow[0].xyz, cellRow[0].xyz );	// WEATHERBLEND5: world to view = rows transposed / scale^2
		int nLs;
		int bLs = vpLightsBegin( P, posView, nLs );	// lane INTERIORPERF1: the cluster's list (was every light)
		for ( int k = 0; k < nLs; k++ ) {
			int i = cellLightAt( bLs, k );
			float4 t0 = cellLights.Load( i * CELL_TPL );
			float d = length( t0.xyz - P );
			if ( or( d >= t0.w, d < 0.001 ) )
				continue;
			float3 L = ( t0.xyz - P ) / d, Lo;
			bool noSpec;
			float3 e = cellLightE( i, P, L, Lo, noSpec );	// N = L, as the air: no N.L, the shadow lifted toward the light
			if ( all( (e == (float3)( 0.0 )) ) )
				continue;
			// lane WEATHERBLEND5: the drift over the lamp's path (puff, middle, lamp: 1/4, 1/2, 1/4); red puffonly: at the puff
			float dL = dP;
			int nL = volAirDrift.x > 0.0 && ( volAirDriftRed & 2 ) == 0 ? 2 : 0;	// a uniform count: one copy of the reads
#ifdef WW_FX_DECALLIT
			nL = 0;	// a lit decal's program sits at the instruction limit (GI march): the drift at the puff only
#endif
			if ( nL > 0 ) {
				float3 dV = ( cellRow[0].xyz * ( t0.x - P.x ) + cellRow[1].xyz * ( t0.y - P.y ) + cellRow[2].xyz * ( t0.z - P.z ) ) / s2;
				dL = 0.25 * dP;
				for ( int j = 1; j <= nL; j++ )
					dL += ( j == 1 ? 0.5 : 0.25 ) * wwAirDriftAt( posView + ( 0.5 * float( j ) ) * dV );
			}
			e = vpTint( e ) * exp( -airOd( z, t0.z + zOff, d ) * dL );
			thin += e * vpPhase( dot( L, dW ) );
			thick += e * vpWrap( dot( N, L ), w ) * vpPhaseN( dot( L, dW ) );
#ifndef WW_FX_DECALLIT
			vpLampMs += e * max( vpWrap( dot( N, L ), 1.0 ) - vpPhase( dot( L, dW ) ), (float3)( 0.0 ) );
#endif
#ifdef PC_LOOK
			vpDom += float4( L, 1.0 ) * dot( e, float3( 0.2126, 0.7152, 0.0722 ) );
			vpDomC += e;
#endif
		}
	}
	bool wantGi = ( volTerms & 4 ) != 0 && cellGiOn;
	bool ambDalc = cellInterior ? cellHasDalc : ( or( cellExtDalc, vpLocal() ) );	// R7-1: the weather's ambient at the card
	bool wantAmb = ( volTerms & 8 ) != 0 && ambDalc && !( cellInterior && cellGiAmb == 2 );
	if ( or( wantGi, wantAmb ) ) {
	float3 E[6];
	float share[6];
	for ( int a = 0; a < 6; a++ ) {
		E[a] = (float3)( 0.0 );
		share[a] = 0.0;
	}
	if ( cellGiOn )
		volCubeAll( P, E, share );
	if ( wantGi ) {
		float3 Eg[6];
		for ( int a = 0; a < 6; a++ )
			Eg[a] = E[a] * ( 1.0 / PI );	// the surfaces' GI unit -> the ambient's, as the air
#ifdef PC_LOOK
		skyThin += pcLook == 1 ? pcDropCube( Eg, dW ) : volCubeScatter( Eg, dW );
#else
		skyThin += volCubeScatter( Eg, dW );
#endif
		skyThick += vpCubeAt( Eg, N );
		jEnv += volCubeScatter( Eg, dW );
	}
	if ( wantAmb ) {
		bool replace = cellGiOn && ( cellInterior ? cellGiAmb == 1 : cellGiSky );
		const float3 axes[6] = { float3( 1, 0, 0 ), float3( -1, 0, 0 ), float3( 0, 1, 0 ), float3( 0, -1, 0 ), float3( 0, 0, 1 ), float3( 0, 0, -1 ) };
		float3 Am[6];
		float keepA = 1.0;
#if !( defined( WW_FX_DECALLIT ) && defined( WW_FX_DECAL_NOKEEP ) )
		keepA = vpAmbKeep( posView );
#endif
		for ( int a = 0; a < 6; a++ )
			Am[a] = cellAmbient( axes[a], P ) * ( ( replace ? 1.0 - share[a] : 1.0 ) * keepA );
#ifdef PC_LOOK
		skyThin += pcLook == 1 ? pcDropCube( Am, dW ) : volCubeScatter( Am, dW );
#else
		skyThin += volCubeScatter( Am, dW );
#endif
		skyThick += vpCubeAt( Am, N );
		jEnv += volCubeScatter( Am, dW );
	}
	}
	float t = length( posView ) * fogDistScale;
	float3 dirView = posView / max( length( posView ), 1e-12 );
	float vk = vpVeilK( jEnv, t, z );
	thin += vk * airEnvColour( skyThin, t, z, dirView );
	thick += vk * airEnvColour( skyThick, t, z, dirView );
}

// the lit factor at this pixel (linear), in place of cellFxLit
float3 vpLit( float3 posView, float3 Nview, float a )
{
	if ( or( ( fxLitMode & 8 ) != 0, ( !cellInterior && !cellHasDir && !vpLocal() ) ) )
		return (float3)( 1.0 );	// self-lit, as cellFxLit
	float m = ( volPartRed & 2 ) != 0 ? 1.0 : 1.0 - clamp( a, 0.0, 1.0 );
	float3 thin, thick;
	vpLight( posView, cellWorldPos( posView ), cellWorldDir( posView ), dot( fogView.xyz, posView ) + fogView.w,
		cellWorldDir( Nview ), m, thin, thick );
#ifndef WW_FX_DECALLIT
	thin += vpAirCardA * vpLampMs;	// FARWATER1 late item 4: an air card's lamps, the multiple scatter x its opacity
#endif
#if defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )
	if ( azCount > 0 )	// lane AIRZONE1: a volume's particles block at the puff, one read (the alpha takes its density after)
		return fxLawMix( azPartLit( cellWorldPos( posView ), m * thin + ( 1.0 - m ) * thick ), lightingInfluence, false );
#endif
	return fxLawMix( m * thin + ( 1.0 - m ) * thick, lightingInfluence, false );	// MODULES1 M3: was mix( 1, ., influence )
}

/* gates only: the lit share (no influence) of ONE white light from direction `to` (world, toward the light) on a puff
 * of opacity a facing N (world): m pi p(c) + (1 - m) wrap(N.to) p(c) / p_iso, c = to.dW -- the phase gate's turntable */
float3 vpOneLight( float3 dW, float3 N, float3 to, float a )
{
	float m = ( volPartRed & 2 ) != 0 ? 1.0 : 1.0 - clamp( a, 0.0, 1.0 );
	float c = dot( to, dW );
	return m * vpPhase( c ) + ( 1.0 - m ) * vpWrap( dot( N, to ), m ) * vpPhaseN( c );
}

/* gates only (volPartDump): probe slot `slot` at view point posView with opacity a: 0 thin (the air's J there), 1 thick at
 * N = +Z view, 2 thick at N = -Z view (w = 0), 3 world P + height z, 4 the eye's ray (world) + its distance (game
 * units), 5 the integrated air (L, T) at its column as the draw applies it (a = -1: none applied), 6 the lit factor at N = +Z with its own alpha (alpha in a),
 * 7 its view point + alpha */
float4 vpProbe( int slot, float3 posView, float a )
{
	float3 P = cellWorldPos( posView );
	float3 dW = cellWorldDir( posView );
	float z = dot( fogView.xyz, posView ) + fogView.w;
	float3 Zw = cellWorldDir( float3( 0.0, 0.0, 1.0 ) );
	float3 thin, thick;
	if ( slot <= 1 ) {
		vpLight( posView, P, dW, z, Zw, 0.0, thin, thick );
		return float4( slot == 0 ? thin : thick, 1.0 );
	}
	if ( slot == 2 ) {
		vpLight( posView, P, dW, z, -Zw, 0.0, thin, thick );
		return float4( thick, 1.0 );
	}
	if ( slot == 3 )
		return float4( P, z );
	if ( slot == 4 )
		return float4( dW, length( posView ) * fogDistScale );
	if ( slot == 5 ) {
		float4 c = mul(float4( posView, 1.0 ), projectionMatrix);
		return ( fogOn && ( volPartRed & 32 ) == 0 && volOn && volSlices.w > 0.5 ) ? wwVolAirAtUv( posView, c.xy / c.w * 0.5 + 0.5 ) : float4( 0.0, 0.0, 0.0, -1.0 );
	}
	if ( slot == 6 )
		return float4( vpLit( posView, float3( 0.0, 0.0, 1.0 ), a ), a );
	return float4( posView, a );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/air/cell_volpart.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_precip_gpu_gs.hlsl, cell_precip_look_gs.hlsl, cell_precip_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, particles_cell_ps.hlsl
// Not here (declarations, or no converted entry carried them): pcDropPi, pcDropCube, vpLight
#ifdef WW_FN_ALL
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1
#endif
static const float PC_DROP_G = 0.7668;
#endif

#if defined(WW_FN_vpAmbKeep) || defined(WW_FN_ALL)
float vpAmbKeep(float3 posView)
{
    if (volEnvGrid.w < 0.5f)
    {
        return 1.0f;
    }
    float d = length(posView);
    float4 c = mul(float4(posView, 1.0f), projectionMatrix);
    bool cond3 = d <= volSlices.x;
    bool cond2;
    if (!cond3)
    {
        cond2 = c.w <= 0.0f;
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        float2 condPart2 = abs(c.xy);
        float2 condPart = c.w.xx;
        cond = any(bool2(condPart2.x > condPart.x, condPart2.y > condPart.y));
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 1.0f;
    }
    int3 n = int3(volEnvGrid.xyz + 0.5f.xxx);
    float f = (log(d / volSlices.x) / volSlices.y) * float(n.z);
    if (f >= float(n.z))
    {
        return 1.0f;
    }
    int2 xc = clamp(int2((((c.xy / c.w.xx) * 0.5f) + 0.5f.xx) * float2(n.xy)), int2(0, 0), n.xy - int2(1, 1));
    int xk = clamp(int(f), 0, n.z - 1);
    float4 h1 = volEnvHist.Load(int4(int3(xc, (5 * n.z) + xk), 0));
    if (h1.z <= 0.99989998340606689453125f)
    {
        return 1.0f;
    }
    float volEnvHistTexel = volEnvHist.Load(int4(int3(xc, (4 * n.z) + xk), 0)).w;
    float vpAmbKeepRet = h1.y;
    return airAmbKeep(volEnvHistTexel, vpAmbKeepRet);
}
#endif

#if defined(WW_FN_vpLocal) || defined(WW_FN_ALL)
bool vpLocal()
{
    return fxLocalOn() && (!cellInterior);
}
#endif

#if defined(WW_FN_vpSunColour) || defined(WW_FN_ALL)
float3 vpSunColour()
{
    float3 cond3;
    if (vpLocal())
    {
        cond3 = vpSun.xyz;
    }
    else
    {
        cond3 = cellDirColor;
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpHasSun) || defined(WW_FN_ALL)
bool vpHasSun()
{
    bool cond3;
    if (vpLocal())
    {
        cond3 = any(bool3(vpSun.xyz.x > 0.0f.xxx.x, vpSun.xyz.y > 0.0f.xxx.y, vpSun.xyz.z > 0.0f.xxx.z));
    }
    else
    {
        cond3 = cellHasDir;
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpVeilK) || defined(WW_FN_ALL)
float vpVeilK(float3 Jenv, float t, float z)
{
    bool cond3 = (!volVeil) || cellInterior;
    bool cond2;
    if (!cond3)
    {
        bool cond;
        if (cellOn)
        {
            cond = vpHasSun();
        }
        else
        {
            cond = cellOn;
        }
        cond2 = !cond;
    }
    else
    {
        cond2 = cond3;
    }
    bool condPart2;
    if (!cond2)
    {
        condPart2 = (volTerms & 1) == 0;
    }
    else
    {
        condPart2 = cond2;
    }
    if (condPart2)
    {
        return 1.0f;
    }
    float condPart = t;
    float volEnvHistTexel = z;
    float vpAmbKeepRet;
    float3 partPick;
    float part = wwFogEval(condPart, volEnvHistTexel, vpAmbKeepRet, partPick);
    float hb = vpAmbKeepRet;
    float3 fc = partPick;
    float rec = dot(max(fc, 0.0f.xxx), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float open = (0.25f * dot(vpSunColour(), float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f))) + dot(Jenv, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    float vpVeilKRet;
    if ((open > rec) && (open > 9.9999999392252902907785028219223e-09f))
    {
        vpVeilKRet = rec / open;
    }
    else
    {
        vpVeilKRet = 1.0f;
    }
    return vpVeilKRet;
}
#endif

#if defined(WW_FN_vpPhase) || defined(WW_FN_ALL)
float3 vpPhase(float c)
{
    float3 cond3;
    if ((volPartRed & 256) != 0)
    {
        cond3 = volGi.xyz * 0.25f;
    }
    else
    {
        float cond2 = c;
        cond3 = volPhasePi(cond2);
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpPhaseN) || defined(WW_FN_ALL)
float3 vpPhaseN(float c)
{
    float cond3 = c;
    return vpPhase(cond3) / max(volGi.xyz * 0.25f, 9.9999999747524270787835121154785e-07f.xxx);
}
#endif

#if defined(WW_FN_vpEdgeFade) || defined(WW_FN_ALL)
float vpEdgeFade(float nv)
{
    float cond3;
    if ((volPartRed & 128) != 0)
    {
        cond3 = 1.0f;
    }
    else
    {
        cond3 = smoothstep(0.0500000007450580596923828125f, 0.3499999940395355224609375f, nv);
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpNearFade) || defined(WW_FN_ALL)
float vpNearFade(float depth)
{
    float cond3;
    if ((volPartRed & 512) != 0)
    {
        cond3 = 1.0f;
    }
    else
    {
        cond3 = smoothstep(16.0f, 128.0f, depth);
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpPuffAlpha) || defined(WW_FN_ALL)
float vpPuffAlpha(float a0, float2 q)
{
    bool wwh0 = ((volPartRed & 2) != 0);
    bool wwh1 = (vpTiled != 0);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        return a0;
    }
    float l = sqrt(max(1.0f - dot(q, q), 0.0f));
    return 1.0f - pow(1.0f - clamp(a0, 0.0f, 0.999000012874603271484375f), 1.5f * l);
}
#endif

#if defined(WW_FN_vpTint) || defined(WW_FN_ALL)
float3 vpTint(float3 e)
{
    float3 cond3;
    if ((volPartRed & 16) != 0)
    {
        cond3 = dot(e, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f)).xxx;
    }
    else
    {
        cond3 = e;
    }
    return cond3;
}
#endif

#if defined(WW_FN_vpWrap) || defined(WW_FN_ALL)
float vpWrap(float nl, float w)
{
    return max(nl + w, 0.0f) / (1.0f + w);
}
#endif

#if defined(WW_FN_vpCubeAt) || defined(WW_FN_ALL)
float3 vpCubeAt(float3 C_1[6], float3 N)
{
    float3 n2 = N * N;
    float3 cond3;
    if (N.x >= 0.0f)
    {
        cond3 = C_1[0];
    }
    else
    {
        cond3 = C_1[1];
    }
    float3 cond2;
    if (N.y >= 0.0f)
    {
        cond2 = C_1[2];
    }
    else
    {
        cond2 = C_1[3];
    }
    float3 cond;
    if (N.z >= 0.0f)
    {
        cond = C_1[4];
    }
    else
    {
        cond = C_1[5];
    }
    return ((cond3 * n2.x) + (cond2 * n2.y)) + (cond * n2.z);
}
#endif

#if defined(WW_FN_vpCubeAt_V2) || defined(WW_FN_ALL)
float3 vpCubeAt(float3 C[6], float3 N)
{
    float3 n2 = N * N;
    float3 cond3;
    if (N.x >= 0.0f)
    {
        cond3 = C[0];
    }
    else
    {
        cond3 = C[1];
    }
    float3 cond2;
    if (N.y >= 0.0f)
    {
        cond2 = C[2];
    }
    else
    {
        cond2 = C[3];
    }
    float3 cond;
    if (N.z >= 0.0f)
    {
        cond = C[4];
    }
    else
    {
        cond = C[5];
    }
    return ((cond3 * n2.x) + (cond2 * n2.y)) + (cond * n2.z);
}
#endif

#if defined(WW_FN_vpLightsBegin) || defined(WW_FN_ALL)
int vpLightsBegin(float3 P, float3 posView, out int n)
{
    n = cellLightCount;
    bool cond3 = !cellClusterOn;
    bool cond2;
    if (!cond3)
    {
        cond2 = (volPartRed & 1024) != 0;
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = volRect.z <= 0.0f;
    }
    else
    {
        cond = cond2;
    }
    bool condPart2;
    if (!cond)
    {
        condPart2 = volRect.w <= 0.0f;
    }
    else
    {
        condPart2 = cond;
    }
    if (condPart2)
    {
        return -1;
    }
    float4 c = mul(float4(posView, 1.0f), projectionMatrix);
    bool condPart = c.w <= 0.0f;
    bool volEnvHistTexel;
    if (!condPart)
    {
        float3 vpAmbKeepRet = abs(c.xyz);
        float3 partPick = c.w.xxx;
        volEnvHistTexel = any(bool3(vpAmbKeepRet.x > partPick.x, vpAmbKeepRet.y > partPick.y, vpAmbKeepRet.z > partPick.z));
    }
    else
    {
        volEnvHistTexel = condPart;
    }
    if (volEnvHistTexel)
    {
        return -1;
    }
    float2 px = volRect.xy + ((((c.xy / c.w.xx) * 0.5f) + 0.5f.xx) / volRect.zw);
    int3 d = int3(cellClusterDims.xyz + 0.5f.xxx);
    int2 t = clamp(int2(floor((px - cellClusterOrigin) / cellClusterDims.w.xx)), int2(0, 0), d.xy - int2(1, 1));
    float s = floor((log(max(dot(cellClusterDepth.xyz, P) + cellClusterDepth.w, 9.9999999747524270787835121154785e-07f)) - cellClusterLog.x) * cellClusterLog.y);
    int cl = (((clamp(int(s), 0, d.z - 1) * d.y) + t.y) * d.x) + t.x;
    int k = cellCluster.Load((2 * cl) + 1).x;
    if (k < 0)
    {
        return -1;
    }
    n = k;
    return cellClusterBase + cellCluster.Load(2 * cl).x;
}
#endif

#if defined(WW_FN_vpLit) || defined(WW_FN_ALL)
float3 vpLit(float3 posView, float3 Nview, float a)
{
    bool cond3 = (fxLitMode & 8) != 0;
    bool cond2;
    if (!cond3)
    {
        bool cond = (!cellInterior) && (!cellHasDir);
        bool condPart2;
        if (cond)
        {
            condPart2 = !vpLocal();
        }
        else
        {
            condPart2 = cond;
        }
        cond2 = condPart2;
    }
    else
    {
        cond2 = cond3;
    }
    if (cond2)
    {
        return 1.0f.xxx;
    }
    float condPart;
    if ((volPartRed & 2) != 0)
    {
        condPart = 1.0f;
    }
    else
    {
        condPart = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = condPart;
    float3 volEnvHistTexel = posView;
    float3 vpAmbKeepRet = posView;
    float3 partPick = Nview;
    float3 part = posView;
    float3 vpVeilKRet = cellWorldPos(volEnvHistTexel);
    float3 part7 = cellWorldDir(vpAmbKeepRet);
    float part6 = dot(fogView.xyz, posView) + fogView.w;
    float3 part5 = cellWorldDir(partPick);
    float part4 = m;
    float3 part3;
    float3 part2;
    vpLight(part, vpVeilKRet, part7, part6, part5, part4, part3, part2);
    float3 thin = part3;
    float3 thick = part2;
    thin += (vpLampMs * vpAirCardA);
    float3 vpLitPart3 = (thin * m) + (thick * (1.0f - m));
    float vpLitPart2 = lightingInfluence;
    bool vpLitPart = false;
    float3 vpLitRet = fxLawMix(vpLitPart3, vpLitPart2, vpLitPart);
    return vpLitRet;
}
#endif

#if defined(WW_FN_vpLit_V2) || defined(WW_FN_ALL)
float3 vpLit(float3 posView, float3 Nview, float a)
{
    bool cond3 = (fxLitMode & 8) != 0;
    bool cond2;
    if (!cond3)
    {
        bool cond = (!cellInterior) && (!cellHasDir);
        bool condPart2;
        if (cond)
        {
            condPart2 = !vpLocal();
        }
        else
        {
            condPart2 = cond;
        }
        cond2 = condPart2;
    }
    else
    {
        cond2 = cond3;
    }
    if (cond2)
    {
        return 1.0f.xxx;
    }
    float condPart;
    if ((volPartRed & 2) != 0)
    {
        condPart = 1.0f;
    }
    else
    {
        condPart = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = condPart;
    float3 volEnvHistTexel = posView;
    float3 vpAmbKeepRet = posView;
    float3 partPick = Nview;
    float3 part = posView;
    float3 vpVeilKRet = cellWorldPos(volEnvHistTexel);
    float3 part7 = cellWorldDir(vpAmbKeepRet);
    float part6 = dot(fogView.xyz, posView) + fogView.w;
    float3 part5 = cellWorldDir(partPick);
    float part4 = m;
    float3 part3;
    float3 part2;
    vpLight(part, vpVeilKRet, part7, part6, part5, part4, part3, part2);
    float3 thin = part3;
    float3 thick = part2;
    float3 vpLitPart3 = (thin * m) + (thick * (1.0f - m));
    float vpLitPart2 = lightingInfluence;
    bool vpLitPart = false;
    return fxLawMix(vpLitPart3, vpLitPart2, vpLitPart);
}
#endif

#if defined(WW_FN_vpOneLight) || defined(WW_FN_ALL)
float3 vpOneLight(float3 dW, float3 N, float3 to, float a)
{
    float cond3;
    if ((volPartRed & 2) != 0)
    {
        cond3 = 1.0f;
    }
    else
    {
        cond3 = 1.0f - clamp(a, 0.0f, 1.0f);
    }
    float m = cond3;
    float c = dot(to, dW);
    float cond2 = c;
    float cond = dot(N, to);
    float condPart2 = m;
    float condPart = c;
    return (vpPhase(cond2) * m) + (vpPhaseN(condPart) * ((1.0f - m) * vpWrap(cond, condPart2)));
}
#endif

#if defined(WW_FN_vpProbe) || defined(WW_FN_ALL)
float4 vpProbe(int slot, float3 posView, float a)
{
    float3 cond3 = posView;
    float3 P = cellWorldPos(cond3);
    float3 cond2 = posView;
    float3 dW = cellWorldDir(cond2);
    // lane SHADOWPAR1: FO4CS's bias 12 (1 + slope) / sin(elevation); else TERRSHADOW1's 0.05 T + 4
    float z = dot(fogView.xyz, posView) + fogView.w;
    float3 cond = float3(0.0f, 0.0f, 1.0f);
    float3 Zw = cellWorldDir(cond);
    float3 thin;
    float3 thick;
    if (slot <= 1)
    {
        float3 condPart2 = posView;
        float3 condPart = P;
        float3 volEnvHistTexel = dW;
        float vpAmbKeepRet = z;
        float3 partPick = Zw;
        float part = 0.0f;
        float3 vpVeilKRet;
        float3 part7;
        vpLight(condPart2, condPart, volEnvHistTexel, vpAmbKeepRet, partPick, part, vpVeilKRet, part7);
        thin = vpVeilKRet;
        thick = part7;
        bool3 part6 = (slot == 0).xxx;
        return float4(float3(part6.x ? thin.x : thick.x, part6.y ? thin.y : thick.y, part6.z ? thin.z : thick.z), 1.0f);
    }
    if (slot == 2)
    {
        float3 part5 = posView;
        float3 part4 = P;
        float3 part3 = dW;
        float part2 = z;
        float3 vpLitPart3 = -Zw;
        float vpLitPart2 = 0.0f;
        float3 vpLitPart;
        float3 vpLitRet;
        vpLight(part5, part4, part3, part2, vpLitPart3, vpLitPart2, vpLitPart, vpLitRet);
        thin = vpLitPart;
        thick = vpLitRet;
        return float4(thick, 1.0f);
    }
    if (slot == 3)
    {
        return float4(P, z);
    }
    if (slot == 4)
    {
        return float4(dW, length(posView) * fogDistScale);
    }
    if (slot == 5)
    {
        float4 c = mul(float4(posView, 1.0f), projectionMatrix);
        bool condPick;
        if (fogOn)
        {
            condPick = (volPartRed & 32) == 0;
        }
        else
        {
            condPick = fogOn;
        }
        bool cond5 = condPick && volOn;
        bool cond4;
        if (cond5)
        {
            cond4 = volSlices.w > 0.5f;
        }
        else
        {
            cond4 = cond5;
        }
        float4 vpProbeRet;
        if (cond4)
        {
            float3 vpProbePart2 = posView;
            float2 vpProbePart = ((c.xy / c.w.xx) * 0.5f) + 0.5f.xx;
            vpProbeRet = wwVolAirAtUv(vpProbePart2, vpProbePart);
        }
        else
        {
            vpProbeRet = float4(0.0f, 0.0f, 0.0f, -1.0f);
        }
        return vpProbeRet;
    }
    if (slot == 6)
    {
        float3 vpLitPart6 = posView;
        float3 vpLitPart5 = float3(0.0f, 0.0f, 1.0f);
        float vpLitPart4 = a;
        float3 vpLitRet2 = vpLit(vpLitPart6, vpLitPart5, vpLitPart4);
        return float4(vpLitRet2, a);
    }
    return float4(posView, a);
}
#endif
#endif	// WW_HLSL_SRC

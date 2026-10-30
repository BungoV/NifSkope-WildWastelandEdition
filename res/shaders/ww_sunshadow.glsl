/* Cascaded sun shadows, the receiver (lane CSM1; spec_cascaded_shadows.md 2.4 / 2.5).
 * Included only by programs that define WW_SUNSHADOW (pbrm_csm.frag,
 * lookdev_ground_csm.frag), so the default programs never carry a line of it.
 * Uploaded by gl/sunshadow.cpp wwSunShadowUniforms; the fit is echoed there. */

uniform sampler2DArrayShadow csmMap;
#ifdef WW_FRAME_UBO
// lane PERF3: the frame's cascade values, one buffer for every program (gl/sunshadow.cpp CsmFrameBlock mirrors it)
layout ( std140 ) uniform wwShadowFrame
{
	mat4 csmMat[3];
	vec3 csmInvRange;
	float csmForce;
	vec4 csmParams;
	vec4 csmSplitOffset;
	int csmProbe;
	int csmRed;
};
#else
uniform mat4 csmMat[3];		// view space -> (u, v, depth) of cascade i
uniform vec3 csmInvRange;	// 1 / (far_i - 150): a world-unit offset in depth units
uniform vec4 csmParams;		// 1 / view scale, shadow distance D, map size, blend band B
uniform vec4 csmSplitOffset;	// the two pair boundaries (800, 3000), receiver offsets A / B (0.275, 1.0)
uniform int csmProbe;		// 0 off, 1 hard tap, 2 cascade colour, 3 filtered + blend, 4 final factor, 5 selection
uniform int csmRed;		// 1 noblend, 2 nofade, 4 diffonly, 8 factorhalf (the OFF leak)
uniform float csmForce;		// WW_CSM_FORCE: >= 0 replaces the factor (the factor-site wiring test); -1 off
#endif

// lane TERRSHADOW1: the unified set (gl/shadowset.h). ussOn = 0 runs the CSM1 code below unchanged.
uniform int ussOn;
uniform int ussCsmOn;			// the cascade maps were built this frame
uniform vec4 ussCsmFilter;		// x Vogel filter on, y radius (texels), z taps, w receiver-plane depth bias on
uniform vec3 ussCsmBias;		// the receiver offset of each cascade, world units along the light
uniform int ussTerrOn;
uniform sampler2D ussTerrMap;		// RG16: x the height above which a point is lit, y below which it is shadowed
uniform mat4 ussWorldFromView;
uniform vec4 ussTerrXform;		// u = x X.x + X.y, v = y X.z + X.w
uniform vec4 ussTerrRange;		// zmin, z span, the receiver's bias (world up), the edge fade (uv)
uniform int ussContactOn;
uniform sampler2D ussContactMap;	// R8 contact visibility, screen space
uniform vec2 ussContactInvSize;
uniform int ussView;			// WW_SHADOW_VIEW: 1 cascades, 2 terrain, 3 contact, 4 composed, 5 weights
uniform int ussRed;			// 1 terrseam, 2 mulcompose
uniform vec4 ussCsmWorld;		// lane SHADOWPAR1: x world-unit radius on, yzw each cascade's radius (world units)
uniform vec3 ussCsmTexel;		// each cascade's texel (world units)
uniform float ussCsmPhase;		// the rotation's temporal phase (frame % 61; 0 without TAA)
uniform vec4 ussCsmStable;		// lane SHADOWSHIMMER1: x 1 the kernel rotation world-locked (0 the red: screen + frame), y the normal offset (texels)
// lane SHADOWSHIMMER3: ussCsmStable.z -1 = the red airslide (the air tail on SHADOWSHIMMER2's world-direction hash)
// lane SHADOWSHIMMER2: ussCsmStable.z 1 = the red (the terrain penumbra and the air tail rotate by the screen pixel; 0, also
// in a program that never gets it, = world-locked); .w the Off tier's stable bias: 0 the red (CSM1's 0.275 / 1.0 units, no
// normal offset), > 0 the normal offset in texels with ussCsmBias, < 0 ussCsmBias alone (the fog's froxels: no surface)
uniform vec3 ussCsmCellX;		// lane SHADOWSHIMMER1: each cascade's window corner in kernel lattice cells, wrapped (gl/sunshadowfit.h)
uniform vec3 ussCsmCellY;
uniform vec2 ussTail;			// lane SHADOWPAR1: x the air's far tail reads the terrain map, y the ambient's share of its light
uniform vec4 ussTerrAtlas;		// the near tier shares the far map's texture: far v scale, near u scale, near v offset, near v scale
uniform vec2 ussTerrSize;		// the far and near tiers' texels across (near 0: off)
uniform vec4 ussTerrNearXform;		// the near window: u = x X.x + X.y, v = y X.z + X.w
uniform vec4 ussTerrPen;		// x the disc penumbra on, y the far texel, z tan(elev - sun radius), w tan(elev + sun radius)
uniform vec4 ussTerrBias;		// x FO4CS's slope bias on, y max(sin elev, sin 1 deg), z the strength (low sun, moon), w the near texel
uniform vec4 ussTerrExtra;		// lane FAR5: v offsets of the terrain-only heights and sweep (0: none), their v scale, the ground band
uniform vec4 ussTerrSelf;		// lane FAR6: x the self-test on, y the relief band's low edge (0: no gate), z the margin cap, w the quantum
uniform vec2 ussDim;			// the ambient / specular dimming in terrain shadow (FO4CS 0.5 / 0.5; 0 off)
uniform int ussPbrDim;			// lane GAPLOOK1: 1 the PBR path dims its ambient + environment too, 2 red (the sun too)
// lane CLOUD3: the clouds' Beer shadow map (gl/cloudshadow.h); ussCloudOn 0 = no clouds, the factor exactly 1
uniform int ussCloudOn;
uniform sampler2D ussCloudMap;		// x height (km) where the cloud starts from the sun, y mean extinction /km, z whole depth
uniform mat4 ussCloudFromView;		// view space -> (u, v, height km); u, v across the sun
uniform vec4 ussCloudFar;		// x transmittance past the map, y edge fade (uv), z the air tail's cloud taps (Ultra), w the air's ambient share
float ussLastTerr = 1.0;		// the terrain visibility of the last composition (cell_lights.glsl dims by it)
vec3 csmNOff = vec3( 0.0 );		// lane SHADOWSHIMMER2: the Off tier's normal offset per world unit of a texel (wwSunShadowGeo)

// spec 2.5: the 16 Poisson taps on [0,1]^2, used as (p - 0.5) x 6 texels
const vec2 csmPoisson[16] = vec2[16](
	vec2( 0.493393, 0.394269 ), vec2( 0.798547, 0.885922 ), vec2( 0.247322, 0.926450 ), vec2( 0.051454, 0.140782 ),
	vec2( 0.831843, 0.009552 ), vec2( 0.428632, 0.017151 ), vec2( 0.015656, 0.749779 ), vec2( 0.758385, 0.496170 ),
	vec2( 0.223487, 0.562151 ), vec2( 0.011628, 0.406995 ), vec2( 0.241462, 0.304636 ), vec2( 0.430311, 0.727226 ),
	vec2( 0.981811, 0.278359 ), vec2( 0.407056, 0.500534 ), vec2( 0.123478, 0.463546 ), vec2( 0.809534, 0.682272 ) );

vec3 csmUvd( int i, vec3 posView )
{
	return ( csmMat[i] * vec4( posView, 1.0 ) ).xyz;
}

/*! lane SHADOWSHIMMER2 (CSMOFFBIAS): the Off tier's normal offset, ussCsmStable.w texels x sin(the light's angle to the
 *  surface), along the geometric normal turned toward the eye (ussCascades' rule; the light row of cascade 0) */
vec3 csmNormalOff( vec3 posView, vec3 dPx, vec3 dPy )
{
	vec3 n = cross( dPx, dPy );
	float ln = length( n );
	vec3 lv = vec3( csmMat[0][0][2], csmMat[0][1][2], csmMat[0][2][2] );
	if ( ln <= 1e-20 || dot( lv, lv ) <= 0.0 )
		return vec3( 0.0 );
	n = dot( n, posView ) > 0.0 ? -n / ln : n / ln;
	float c = dot( n, normalize( lv ) );
	return n * ( ussCsmStable.w * sqrt( max( 1.0 - c * c, 0.0 ) ) / csmParams.x );
}

//! one bilinear comparison tap (GL_LINEAR + COMPARE_REF_TO_TEXTURE): 1 lit, 0 shadowed
float csmHard( int i, vec3 posView, float offsetWorld )
{
	// lane SHADOWSHIMMER2: the Off tier's stable bias (csmNOff 0 and CSM1's offset with the red)
	vec3 q = csmUvd( i, posView + csmNOff * ussCsmTexel[i] );
	float off = ussCsmStable.w != 0.0 ? ussCsmBias[i] : offsetWorld;
	return texture( csmMap, vec4( q.xy, float( i ), q.z - off * csmInvRange[i] ) );
}

//! the 16-tap Poisson filter, radius 3 texels
float csmFiltered( int i, vec3 posView, float offsetWorld )
{
	vec3 q = csmUvd( i, posView + csmNOff * ussCsmTexel[i] );	// lane SHADOWSHIMMER2: as csmHard
	float zRef = q.z - ( ussCsmStable.w != 0.0 ? ussCsmBias[i] : offsetWorld ) * csmInvRange[i];
	float k = 6.0 / csmParams.z;
	float sum = 0.0;
	for ( int t = 0; t < 16; t++ )
		sum += texture( csmMap, vec4( q.xy + ( csmPoisson[t] - 0.5 ) * k, float( i ), zRef ) );
	return sum / 16.0;
}

//! the cascade pair and the blend weight at this view depth (spec 2.4)
void csmSelect( vec3 posView, out int a, out int b, out float t )
{
	float dv = -posView.z * csmParams.x;
	float bound;
	if ( dv < csmSplitOffset.y ) {
		a = 0; b = 1; bound = csmSplitOffset.x;
	} else {
		a = 1; b = 2; bound = csmSplitOffset.y;
	}
	float x = clamp( ( dv - bound ) / csmParams.w, 0.0, 1.0 );
	t = ( csmRed & 1 ) != 0 ? step( 0.5, x ) : smoothstep( 0.0, 1.0, x );
}

//! the filtered factor with the seam blend, before the distance fade
float csmBlended( vec3 posView )
{
	int a, b;
	float t;
	csmSelect( posView, a, b, t );
	float sA = t < 1.0 ? csmFiltered( a, posView, csmSplitOffset.z ) : 1.0;
	float sB = t > 0.0 ? csmFiltered( b, posView, csmSplitOffset.w ) : 1.0;
	return mix( sA, sB, t );
}

// ---- lane TERRSHADOW1: the composition ----

float ussIgn( vec2 p )
{
	return fract( 52.9829189 * fract( dot( p, vec2( 0.06711056, 0.00583715 ) ) ) );
}

//! (A) the Vogel-disk filter, rotated per world point (lane SHADOWSHIMMER1), with the receiver-plane depth bias;
//! nOff: the normal offset per world unit of this cascade's texel (view space; 0 off)
float ussVogel( int i, vec3 posView, float offsetWorld, vec3 dPx, vec3 dPy, vec3 nOff )
{
	vec3 q = csmUvd( i, posView + nOff * ussCsmTexel[i] );
	float zRef = q.z - offsetWorld * csmInvRange[i];
	vec2 rpdb = vec2( 0.0 );
	if ( ussCsmFilter.w > 0.5 ) {
		vec3 qx = mat3( csmMat[i] ) * dPx, qy = mat3( csmMat[i] ) * dPy;
		float det = qx.x * qy.y - qx.y * qy.x;
		if ( abs( det ) > 1e-24 )
			rpdb = vec2( qx.z * qy.y - qx.y * qy.z, qx.x * qy.z - qy.x * qx.z ) / det;
	}
	float R = ussCsmFilter.y * mix( 0.8, 1.2, clamp( float( i ) * 0.5, 0.0, 1.0 ) );
	int n = max( int( ussCsmFilter.z ), 1 );
	if ( ussCsmWorld.x > 0.5 ) {
		// lane SHADOWPAR1: FO4CS's radius in world units over this cascade's texel, its taps 32 (r / 3)^2 in 8..48
		R = clamp( ussCsmWorld[i + 1] / max( ussCsmTexel[i], 1e-6 ), 0.5, 16.0 );
		n = int( clamp( floor( 32.0 * ( R / 3.0 ) * ( R / 3.0 ) + 0.5 ), 8.0, 48.0 ) );
	}
	// lane SHADOWSHIMMER1: the rotation of the absolute light-space lattice cell (4 per texel, wrapped at 4096): the same
	// for a still world point in every frame and from every camera. Red (x 0): the pixel's, plus the TAA frame's phase
	vec2 rp = ussCsmStable.x > 0.5 ? mod( floor( q.xy * ( csmParams.z * 4.0 ) ) + vec2( ussCsmCellX[i], ussCsmCellY[i] ), 4096.0 )
		: gl_FragCoord.xy + ussCsmPhase * vec2( 32.665, 11.815 );
	float rot = 6.2831853 * ussIgn( rp );
	float k = 1.0 / csmParams.z;
	float lim = 2000.0 * csmInvRange[i];
	float sum = 0.0;
	for ( int t = 0; t < 48; t++ ) {
		if ( t >= n )
			break;
		float r = sqrt( ( float( t ) + 0.5 ) / float( n ) ) * R;
		float th = float( t ) * 2.39996323 + rot;
		vec2 o = vec2( cos( th ), sin( th ) ) * ( r * k );
		sum += texture( csmMap, vec4( q.xy + o, float( i ), zRef + clamp( dot( rpdb, o ), -lim, lim ) ) );
	}
	return sum / float( n );
}

//! (A) the cascades and their distance-fade weight w (1 inside, 0 past the reach)
float ussCascades( vec3 posView, vec3 dPx, vec3 dPy, out float w )
{
	w = 0.0;
	if ( ussCsmOn == 0 )
		return 1.0;
	float dv = -posView.z * csmParams.x;
	if ( dv > csmParams.y + csmParams.w )
		return 1.0;
	int a, b;
	float t;
	csmSelect( posView, a, b, t );
	float sA = 1.0, sB = 1.0;
	if ( ussCsmFilter.x > 0.5 ) {
		// lane SHADOWSHIMMER1: the normal offset, ussCsmStable.y texels of the cascade x sin(the light's angle to the
		// surface), along the geometric normal (the position's derivatives) turned toward the eye; once for both cascades
		vec3 nOff = vec3( 0.0 );
		if ( ussCsmStable.y > 0.0 ) {
			vec3 n = cross( dPx, dPy );
			float ln = length( n );
			vec3 lv = vec3( csmMat[a][0][2], csmMat[a][1][2], csmMat[a][2][2] );
			if ( ln > 1e-20 && dot( lv, lv ) > 0.0 ) {
				n = dot( n, posView ) > 0.0 ? -n / ln : n / ln;
				float c = dot( n, normalize( lv ) );
				nOff = n * ( ussCsmStable.y * sqrt( max( 1.0 - c * c, 0.0 ) ) / csmParams.x );
			}
		}
		if ( t < 1.0 )
			sA = ussVogel( a, posView, ussCsmBias[a], dPx, dPy, nOff );
		if ( t > 0.0 )
			sB = ussVogel( b, posView, ussCsmBias[b], dPx, dPy, nOff );
	} else {
		if ( t < 1.0 )
			sA = csmFiltered( a, posView, csmSplitOffset.z );
		if ( t > 0.0 )
			sB = csmFiltered( b, posView, csmSplitOffset.w );
	}
	float s = clamp( dot( posView, posView ) * csmParams.x * csmParams.x / ( csmParams.y * csmParams.y ), 0.0, 1.0 );
	float s2 = s * s;
	w = ( csmRed & 2 ) != 0 ? 1.0 : 1.0 - s2 * s2;
	return mix( sA, sB, t );
}

//! one tier's uv into the shared texture (lane SHADOWPAR1: the near tier sits under the far map)
vec2 ussTerrAtlasUv( vec2 uv, int tier )
{
	if ( ussTerrSize.y <= 0.0 && ussTerrExtra.z <= 0.0 )
		return uv;	// one tier: the map as TERRSHADOW1 made it
	float n = tier == 0 ? ussTerrSize.x : ussTerrSize.y;
	uv = clamp( uv, vec2( 0.5 / n ), vec2( 1.0 - 0.5 / n ) );	// no bleed across the tiers' seam
	return tier == 0 ? vec2( uv.x, uv.y * ussTerrAtlas.x ) : vec2( uv.x * ussTerrAtlas.y, ussTerrAtlas.z + uv.y * ussTerrAtlas.w );
}

//! the visibility at height z from the limb heights rg (0 = no shadow height: lit)
float ussTerrVis( vec2 rg, float z )
{
	float lo = ussTerrRange.x + rg.x * ussTerrRange.y;
	float hi = ussTerrRange.x + rg.y * ussTerrRange.y;
	return rg.x <= 0.0 ? 1.0 : ( lo - hi > 1e-3 ? clamp( ( z - hi ) / ( lo - hi ), 0.0, 1.0 ) : ( z >= lo ? 1.0 : 0.0 ) );
}

/*! one tier at its uv. lane SHADOWPAR1 (FO4CS FarFieldMaskCS): the disc penumbra -- the occluder's distance from the
 *  limbs' spread (lo - hi) / (tan hi - tan lo), half the sun's width at that distance in texels (<= 16), the centre
 *  and 8 Vogel taps */
float ussTerrTier( vec2 uv, int tier, float z, float texel, bool pen, vec2 wxy )
{
	vec2 rg = texture( ussTerrMap, ussTerrAtlasUv( uv, tier ) ).rg;
	float c = ussTerrVis( rg, z );
	if ( !pen || ussTerrPen.x < 0.5 || rg.x <= 0.0 )
		return c;
	float dist = max( rg.x - rg.y, 0.0 ) * ussTerrRange.y / max( ussTerrPen.w - ussTerrPen.z, 1e-6 );
	float r = min( dist * 0.0046251 / max( texel, 1e-6 ), 16.0 );	// tan(0.265 deg)
	if ( r <= 0.5 )
		return c;
	float n = tier == 0 ? ussTerrSize.x : ussTerrSize.y;
	if ( n <= 0.0 )
		n = 1.0 / max( ussTerrXform.x * texel, 1e-12 );	// one tier: N = span / texel
	// lane SHADOWSHIMMER2 (TERRPENSTABLE): the rotation of the world lattice cell (4 per tier texel, wrapped at 4096), the
	// cascades' hash: a still world point keeps it in every frame and from every camera. Red (z 1): the screen pixel's
	float rot = 6.2831853 * ussIgn( ussCsmStable.z > 0.5 ? gl_FragCoord.xy
		: mod( floor( wxy * ( 4.0 / max( texel, 1e-6 ) ) ), 4096.0 ) );
	float sum = c;
	for ( int t = 0; t < 8; t++ ) {
		float th = float( t ) * 2.39996323 + rot;
		vec2 o = vec2( cos( th ), sin( th ) ) * ( sqrt( ( float( t ) + 0.5 ) / 8.0 ) * r / n );
		sum += ussTerrVis( texture( ussTerrMap, ussTerrAtlasUv( clamp( uv + o, vec2( 0.0 ), vec2( 1.0 ) ), tier ) ).rg, z );
	}
	return sum / 9.0;
}

//! lane FAR5: a far-grid uv into one of the extra blocks under the tiers (v offset off)
vec2 ussTerrExtraUv( vec2 uv, float off )
{
	uv = clamp( uv, vec2( 0.5 / ussTerrSize.x ), vec2( 1.0 - 0.5 / ussTerrSize.x ) );
	return vec2( uv.x, off + uv.y * ussTerrExtra.z );
}

//! lane FAR5: the terrain-only height at uv (x; w 0 = none), the most its 4 neighbours differ (y) and FO4CS's
//! LocalGradient max(|hE - hW|, |hS - hN|) / 2 texels (z; 0 where a neighbour has no height)
vec4 ussTerrHeights( vec2 uv )
{
	float d = 1.0 / ussTerrSize.x;
	float c = texture( ussTerrMap, ussTerrExtraUv( uv, ussTerrExtra.x ) ).r;
	float hE = texture( ussTerrMap, ussTerrExtraUv( uv + vec2( d, 0.0 ), ussTerrExtra.x ) ).r;
	float hW = texture( ussTerrMap, ussTerrExtraUv( uv - vec2( d, 0.0 ), ussTerrExtra.x ) ).r;
	float hN = texture( ussTerrMap, ussTerrExtraUv( uv + vec2( 0.0, d ), ussTerrExtra.x ) ).r;
	float hS = texture( ussTerrMap, ussTerrExtraUv( uv - vec2( 0.0, d ), ussTerrExtra.x ) ).r;
	if ( c <= 0.0 || min( min( hE, hW ), min( hN, hS ) ) <= 0.0 )
		return vec4( ussTerrRange.x + c * ussTerrRange.y, 0.0, 0.0, c > 0.0 ? 1.0 : 0.0 );
	float s = ussTerrRange.y;
	float relief = max( max( abs( hE - c ), abs( hW - c ) ), max( abs( hN - c ), abs( hS - c ) ) ) * s;
	float g = max( abs( hE - hW ), abs( hS - hN ) ) * s / max( 2.0 * ussTerrPen.y, 1e-6 );
	return vec4( ussTerrRange.x + c * s, relief, g, 1.0 );
}

/*! lane FAR6 (FO4CS FarFieldMaskCS, the light-space self-test it ships on): how much of the full sweep's shadow counts
 *  here -- FarLodOcclusionFade past the self margin times FarLodReliefGate on the caster's height over the receiver */
float ussTerrSelfWeight( vec2 uv, float wz, vec4 h )
{
	vec2 rg = texture( ussTerrMap, ussTerrAtlasUv( uv, 0 ) ).rg;
	float cast = texture( ussTerrMap, ussTerrExtraUv( uv, ussTerrExtra.x ) ).g;
	if ( rg.x <= 0.0 || cast <= 0.0 )
		return 1.0;
	float s = ussTerrRange.x + rg.x * ussTerrRange.y;
	float hc = ussTerrRange.x + cast * ussTerrRange.y;
	float tanE = max( ussTerrPen.z, 1e-4 );
	float cosE = inversesqrt( 1.0 + tanE * tanE );
	// the gap to the first hit along the light: the caster's distance (s = hc - d tanE) over cos(elevation)
	float delta = max( hc - s, 0.0 ) / ( tanE * cosE );
	float grad = ussTerrPen.y * cosE * ( 1.0 + h.z * tanE );	// the stored depth's change per texel here
	float margin = 4.0 * ussTerrSelf.w + min( 1.5 * grad, ussTerrSelf.z );
	float fade = clamp( ( delta - margin ) / max( grad, 8.0 * ussTerrSelf.w ), 0.0, 1.0 );
	float lo = ussTerrSelf.y;
	return lo > 0.0 ? fade * smoothstep( 0.0, 1.0, ( hc - wz - lo ) / lo ) : fade;
}

//! (C) the terrain's shadow-height map; inMap = 1 inside it. grad: the receiver's slope (rise over run, the slope
//! bias); pen: the disc penumbra (off for the air's many taps)
float ussTerrain( vec3 posView, out float inMap, float grad, bool pen )
{
	inMap = 0.0;
	if ( ussTerrOn == 0 )
		return 1.0;
	vec3 wp = ( ussWorldFromView * vec4( posView, 1.0 ) ).xyz;
	vec2 uv = vec2( wp.x * ussTerrXform.x + ussTerrXform.y, wp.y * ussTerrXform.z + ussTerrXform.w );
	if ( any( lessThan( uv, vec2( 0.0 ) ) ) || any( greaterThan( uv, vec2( 1.0 ) ) ) )
		return 1.0;
	inMap = 1.0;
	// lane FAR5: bias.x 2 = FO4CS's LocalGradient from the terrain-only heights instead of the screen slope; 3 = 0.
	// Surface receivers only (pen): the air's taps would push the decal/GI-march programs over the instruction limit
	vec4 h = vec4( 0.0 );
	if ( pen && ( ussTerrBias.x > 1.5 || ussTerrExtra.y > 0.0 ) )
		h = ussTerrHeights( uv );
	if ( pen && ussTerrBias.x > 1.5 )
		grad = ussTerrBias.x > 2.5 ? 0.0 : h.z;
	// lane SHADOWPAR1: FO4CS's bias 12 (1 + slope) / sin(elevation); else TERRSHADOW1's 0.05 T + 4
	float z = wp.z + ( ussTerrBias.x > 0.5 ? 12.0 * ( 1.0 + clamp( grad, 0.0, 4.0 ) ) / ussTerrBias.y : ussTerrRange.z );
	float vis = ussTerrTier( uv, 0, z, ussTerrPen.y, pen, wp.xy );
	if ( ussTerrSize.y > 0.0 ) {
		// the near tier, faded in over 128 of its texels from its window's edge
		vec2 un = vec2( wp.x * ussTerrNearXform.x + ussTerrNearXform.y, wp.y * ussTerrNearXform.z + ussTerrNearXform.w );
		vec2 en = min( un, 1.0 - un ) * ussTerrSize.y / 128.0;
		float w = smoothstep( 0.0, 1.0, clamp( min( en.x, en.y ), 0.0, 1.0 ) );
		if ( w > 0.0 )
			vis = mix( vis, ussTerrTier( un, 1, z, ussTerrBias.w, pen, wp.xy ), w );
	}
	if ( ussTerrExtra.y > 0.0 ) {
		// lane FAR5 (FO4CS FarFieldMaskCS, the ground-band branch): a receiver above the ground band melts to the
		// terrain-only sweep, so object-LOD casters shade only the ground under them
		if ( h.w > 0.0 && ussTerrSelf.x > 0.5 ) {
			// lane FAR6: FO4CS composes its terrain sweep and the self-tested LOD term by min()
			vis = min( ussTerrVis( texture( ussTerrMap, ussTerrExtraUv( uv, ussTerrExtra.y ) ).rg, z ),
				mix( 1.0, vis, ussTerrSelfWeight( uv, wp.z, h ) ) );
		} else if ( h.w > 0.0 ) {
			float m = ussTerrExtra.w < 0.0 ? 1.0
				: smoothstep( 0.75 * ussTerrExtra.w, 1.25 * ussTerrExtra.w, wp.z - h.x - min( h.y, 4.0 * ussTerrPen.y ) );
			if ( m > 0.0 )
				vis = mix( vis, ussTerrVis( texture( ussTerrMap, ussTerrExtraUv( uv, ussTerrExtra.y ) ).rg, z ), m );
		}
	}
	vec2 e = min( uv, 1.0 - uv ) / max( ussTerrRange.w, 1e-6 );
	vis = mix( 1.0, vis, clamp( min( e.x, e.y ), 0.0, 1.0 ) );
	return mix( 1.0, vis, ussTerrBias.z );	// the low sun's fade and the moon's strength (1 when off)
}

//! the receiver's slope (rise over run) from its position's screen derivatives
float ussSlope( vec3 dPx, vec3 dPy )
{
	vec3 n = cross( dPx, dPy );
	float l = length( n );
	if ( l < 1e-20 || ussTerrBias.x < 0.5 )
		return 0.0;
	float nz = abs( ( ussWorldFromView * vec4( n / l, 0.0 ) ).z ) / max( length( ussWorldFromView[0].xyz ), 1e-20 );
	return sqrt( max( 1.0 - nz * nz, 0.0 ) ) / max( nz, 0.2 );
}

//! (B) the contact mask at this pixel
float ussContact()
{
	if ( ussContactOn == 0 )
		return 1.0;
	return texture( ussContactMap, gl_FragCoord.xy * ussContactInvSize ).r;
}

//! min( mix( 1, cascades, fade ), terrain, contact ): each producer fails to lit; min never darkens twice
float ussCompose( vec3 posView )
{
	vec3 dPx = dFdx( posView ), dPy = dFdy( posView );
	float w, inMap;
	float casc = ussCascades( posView, dPx, dPy, w );
	float terr = ussTerrain( posView, inMap, ussSlope( dPx, dPy ), true );
	float con = ussContact();
	if ( ( ussRed & 1 ) != 0 && w > 0.0 )
		terr = 1.0;	// red terrseam: the terrain only past the cascades' reach
	ussLastTerr = terr;
	float c = mix( 1.0, casc, w );
	if ( ( ussRed & 2 ) != 0 )
		return c * terr * con;	// red mulcompose
	return min( min( c, terr ), con );
}

//! lane CLOUD3: the clouds' transmittance toward the sun at posView, OD = min(whole, mean x depth below the top)
float ussCloudT( vec3 posView )
{
	if ( ussCloudOn == 0 )
		return 1.0;
	vec3 q = ( ussCloudFromView * vec4( posView, 1.0 ) ).xyz;
	vec3 m = textureLod( ussCloudMap, q.xy, 0.0 ).xyz;
	float t = exp( -min( m.z, m.y * max( m.x - q.z, 0.0 ) ) );
	vec2 e = abs( q.xy - 0.5 ) * 2.0;
	return mix( t, ussCloudFar.x, smoothstep( 1.0 - ussCloudFar.y, 1.0, max( e.x, e.y ) ) );
}

//! the geometry's shadow factor (cascades, terrain, contact): 1 lit, 0 fully shadowed
float wwSunShadowGeo( vec3 posView )
{
	if ( ( csmRed & 8 ) != 0 )
		return 0.5;
	if ( csmForce >= 0.0 )
		return csmForce;
	// lane SHADOWSHIMMER2 (CSMOFFBIAS): the Off tier's normal offset, before any per-pixel branch (derivatives)
	if ( ussCsmStable.w > 0.0 )
		csmNOff = csmNormalOff( posView, dFdx( posView ), dFdy( posView ) );
	if ( ussOn != 0 )
		return ussCompose( posView );
	float dv = -posView.z * csmParams.x;
	if ( dv > csmParams.y + csmParams.w )
		return 1.0;
	float sh = csmBlended( posView );
	if ( ( csmRed & 2 ) == 0 ) {
		float s = clamp( dot( posView, posView ) * csmParams.x * csmParams.x / ( csmParams.y * csmParams.y ), 0.0, 1.0 );
		float s2 = s * s;
		sh = 1.0 - ( 1.0 - sh ) * ( 1.0 - s2 * s2 );
	}
	return sh;
}

//! the sun's shadow factor: the geometry's x the clouds' (lane CLOUD3; x exactly 1 with the row off)
float wwSunShadow( vec3 posView )
{
	return wwSunShadowGeo( posView ) * ussCloudT( posView );
}

//! the probe views (WW_CSM_PROBE); false = draw normally. `shaded` is the display colour.
bool wwSunShadowProbe( vec3 posView, vec3 shaded, out vec3 outc )
{
	outc = shaded;
	if ( ussOn != 0 && ussView > 0 ) {
		// lane TERRSHADOW1: the debug ladder (raw grey, or the weights as numbers)
		vec3 dPx = dFdx( posView ), dPy = dFdy( posView );
		float w, inMap;
		float casc = ussCascades( posView, dPx, dPy, w );
		float terr = ussTerrain( posView, inMap, ussSlope( dPx, dPy ), true );
		float con = ussContact();
		if ( ussView == 1 )
			outc = vec3( mix( 1.0, casc, w ) );
		else if ( ussView == 2 )
			outc = vec3( terr );
		else if ( ussView == 3 )
			outc = vec3( con );
		else if ( ussView == 4 )
			outc = vec3( ussCompose( posView ) );
		else
			outc = vec3( w, inMap, con );
		return true;
	}
	if ( csmProbe <= 0 )
		return false;
	if ( ussCsmStable.w > 0.0 )
		csmNOff = csmNormalOff( posView, dFdx( posView ), dFdy( posView ) );	// lane SHADOWSHIMMER2: probes 1 / 3 as drawn
	float dv = -posView.z * csmParams.x;
	int a, b;
	float t;
	csmSelect( posView, a, b, t );
	if ( csmProbe == 1 ) {
		int i = t < 0.5 ? a : b;
		outc = vec3( dv > csmParams.y + csmParams.w ? 1.0 : csmHard( i, posView, i == a ? csmSplitOffset.z : csmSplitOffset.w ) );
	} else if ( csmProbe == 2 ) {
		const vec3 tint[3] = vec3[3]( vec3( 1.0, 0.25, 0.25 ), vec3( 0.25, 1.0, 0.25 ), vec3( 0.3, 0.45, 1.0 ) );
		vec3 c = dv > csmParams.y + csmParams.w ? vec3( 1.0 ) : mix( tint[a], tint[b], t );
		outc = shaded * c;
	} else if ( csmProbe == 3 ) {
		outc = vec3( dv > csmParams.y + csmParams.w ? 1.0 : csmBlended( posView ) );
	} else if ( csmProbe == 5 ) {
		// the selection as numbers: R = cascade A / 2, G = cascade B / 2, B = the blend weight;
		// past D + B all three are 1
		outc = dv > csmParams.y + csmParams.w ? vec3( 1.0 ) : vec3( float( a ) * 0.5, float( b ) * 0.5, t );
	} else {
		outc = vec3( wwSunShadow( posView ) );
	}
	return true;
}

/* lane SHADOWPAR1 (bungo 10-21): the far air tail (lookdev_fog.glsl) carries the last froxel slice's light J past the
 * froxels; behind a ridge the sun part of it is shadowed. The scale of J: the terrain visibility along the tail (8 taps,
 * equal shares of the light the tail sends to the eye) over the visibility at the froxels' far edge, the ambient share
 * a left unshadowed. dirView a unit view-space direction, t0 < t1 world distances, tau the tail's optical depth. */
#ifdef WW_LOOKDEV_FOG_GLSL	// fo4_csm.frag casts without the fog
float wwAirTailShadow( vec3 dirView, float t0, float t1, float tau )
{
	bool terr = ussOn != 0 && ussTail.x > 0.0 && ussTerrOn != 0;
	bool cloud = ussCloudOn != 0 && ussCloudFar.z > 0.0;	// lane CLOUD3: Ultra's cloud taps (the far aerial perspective)
	if ( ( !terr && !cloud ) || t1 <= t0 )
		return 1.0;
	float a = clamp( terr ? ussTail.y : ussCloudFar.w, 0.0, 1.0 );
	float inMap;
	vec3 p0 = dirView * ( t0 / fogDistScale );
	float v0 = ( terr ? ussTerrain( p0, inMap, 0.0, false ) : 1.0 ) * ( cloud ? ussCloudT( p0 ) : 1.0 );
	float e = 1.0 - exp( -max( tau, 0.0 ) );
	/* lane SHADOWSHIMMER3 (AIRTAILSLIDE): the start of the strata hashed from the WORLD point the tail ends at (the
	 * surface this pixel shades), on a lattice of power-of-two cells of about 1/2048 of its distance (at least 1 unit),
	 * IGN of (x + 37 z, y + 61 z) wrapped at 4096: a still world point keeps it on a turn and on a slide (a slide moves it
	 * only where the distance crosses a power of two). Red airslide (ussCsmStable.z -1): SHADOWSHIMMER2's hash of the
	 * ray's WORLD direction (octahedral, 2048 cells a unit, wrapped at 4096), which a slide moves; red terr (z 1): the pixel's */
	vec2 key;
	if ( ussCsmStable.z < -0.5 ) {
		vec3 dw = mat3( ussWorldFromView ) * dirView;
		if ( dot( dw, dw ) <= 1e-20 )
			dw = dirView;
		dw /= abs( dw.x ) + abs( dw.y ) + abs( dw.z );
		vec2 oct = dw.z >= 0.0 ? dw.xy : ( 1.0 - abs( dw.yx ) ) * vec2( dw.x >= 0.0 ? 1.0 : -1.0, dw.y >= 0.0 ? 1.0 : -1.0 );
		key = mod( floor( oct * 2048.0 ), 4096.0 );
	} else {
		vec3 w1 = ( ussWorldFromView * vec4( dirView * ( t1 / fogDistScale ), 1.0 ) ).xyz;
		float cell = exp2( max( ceil( log2( max( t1, 1.0 ) / 2048.0 ) ), 0.0 ) );	// t1: game units, as the world's
		vec3 c = floor( w1 / cell );
		key = mod( c.xy + c.z * vec2( 37.0, 61.0 ), 4096.0 );
	}
	float jit = ussIgn( ussCsmStable.z > 0.5 ? gl_FragCoord.xy : key );
	float acc = 0.0;
	for ( int i = 0; i < 8; i++ ) {
		float q = ( float( i ) + jit ) / 8.0;
		float u = tau > 1e-4 ? -log( max( 1.0 - q * e, 1e-6 ) ) / tau : q;
		vec3 p = dirView * ( mix( t0, t1, clamp( u, 0.0, 1.0 ) ) / fogDistScale );
		acc += ( terr ? ussTerrain( p, inMap, 0.0, false ) : 1.0 ) * ( cloud ? ussCloudT( p ) : 1.0 );
	}
	return ( a + ( 1.0 - a ) * acc / 8.0 ) / max( a + ( 1.0 - a ) * v0, 0.25 );
}
#endif

//! lane SHADOWPAR1 (FO4CS bsdf_light_sun): the ambient (cool-tinted toward the sky) and the specular cube dimmed in
//! terrain shadow, floors 1 - strength; the strengths fade with the low sun (ussDim, 0 off)
void ussTerrainDim( out vec3 amb, out float spec )
{
	amb = vec3( 1.0 );
	spec = 1.0;
	if ( ussOn == 0 )
		return;
	float fl = 1.0 - ussDim.x;
	float m = max( ussLastTerr, fl );
	if ( ussDim.x > 0.0 && m < 1.0 ) {
		float shift = min( 0.35 * clamp( ( 1.0 - m ) / max( 1.0 - fl, 1e-6 ), 0.0, 1.0 ), ( 1.0 - m ) / ( m * 0.5414 ) );
		amb = max( m * mix( vec3( 1.0 ), vec3( 0.7770, 1.0116, 1.5414 ), shift ), vec3( 0.777 * fl ) );
	}
	if ( ussDim.y > 0.0 )
		spec = max( ussLastTerr, 1.0 - ussDim.y );
}

// Lookdev weather fog (lane FOG1): the engine fog formula, a line-for-line port
// of the composite shader as transcribed in scratchpad/pbrprep1_20260924/spec_fog.md
// 2.4 (FO4CS ambient_ibl_pass.hlsl F4FXApplyEngineFog), fed the cb12[41..46]
// packing (fogK) that gl/lookdevstage.cpp builds from the WTHR (esmweather.cpp
// wwFogAt). Applied per fragment to scene geometry in LINEAR light, before the
// exposure and the view transform (studioOutput); the sky dome never takes the game's
// fog (lane VOLAIR1: it takes the air, cell_volsky.frag, wwVolAirSky below).
// The fog-sun term's colour and intensity are INFERRED (the lookdev sun: its
// direction, the Sunlight row, intensity 1); the power is fDirectionalFogPower.
// Needs: posView = the fragment's view-space position (game units x view scale).

#define WW_LOOKDEV_FOG_GLSL 1	// lane SHADOWPAR1: ww_sunshadow.glsl's air-tail reader needs fogDistScale
uniform bool fogOn;
uniform vec4 fogK[6];		// cb12[41..46]
uniform vec4 fogView;		// world z above the ground = dot( fogView.xyz, posView ) + fogView.w
uniform float fogDistScale;	// view units -> game units (1 / the view scale)
uniform vec4 fogSun;		// view space, unit, TO the sun; .w = intensity
uniform vec4 fogSunColour;	// linear; .w = fDirectionalFogPower
uniform vec4 fogProbe;		// gates only: .z = 0 off, 1 alpha, 2 colour / 2, 3 hb (at d = .x, z = .y); 5 geometry echo (d / 4096, 0.5 + z / 2000);
				// 6 / 7 per fragment (lane FOG2): (alpha, hb, 0) / colour ^ 1/2.2; 8 the geometry, wide (d / 16384, 0.5 + z / 32768)
uniform int fogRed;		// gates only: 1 maxclamp, 2 noescape, 4 height0

/* lane SKYA (ATMO2): the physical sky's aerial perspective (src/gl/atmosphere.h, Hillaire 2020 sec. 5.4). atApTex
 * holds per froxel the light the air sends toward the eye from the camera to slice k (rgb, the sky's scene units)
 * and the mean transmittance (a); slice k's centre sits at maxKm ((k + 0.5) / n)^2, so the texture's depth
 * coordinate is sqrt(d / maxKm). Off (atAp.x 0): every path below is the one before the lane. */
#include "atmo_apfog.glsl"
uniform sampler3D atApTex;
uniform vec4 atAp;		// 1 = on, maxKm, km per view unit, slices
uniform vec4 atApProj;		// the projection the volume was built for: P00, P11, P20, P21
uniform float atApDouble;	// red "double": the fog laid over the whole aerial perspective

bool wwApOn()
{
	return atAp.x > 0.5;
}

// (L, T) of the sky's air between the eye and posView (view space)
vec4 wwAtmoApAt( vec3 posView )
{
	float dKm = length( posView ) * atAp.z;
	float zv = max( -posView.z, 1e-6 );
	vec2 ndc = vec2( atApProj.x * posView.x + atApProj.z * posView.z, atApProj.y * posView.y + atApProj.w * posView.z ) / zv;
	vec2 uv = clamp( ndc * 0.5 + 0.5, 0.0, 1.0 );
	float n = atAp.w;
	float s = sqrt( clamp( dKm / atAp.y, 0.0, 1.0 ) );
	vec4 v;
	if ( s < 0.5 / n ) {
		// nearer than slice 0's centre: its (L, 1 - T) scaled by the distance (both linear there)
		v = texture( atApTex, vec3( uv, 0.5 / n ) );
		float f = s * n * 2.0;
		f *= f;
		v = vec4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = texture( atApTex, vec3( uv, s ) );
	}
	return vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
}

// fog alpha, height blend and colour (before the sun term) of one fragment
float wwFogEval( float d, float z, out float hb, out vec3 fogCol )
{
	if ( ( fogRed & 4 ) != 0 )
		z = 0.0;
	float ramp = d * fogK[0].x - fogK[0].z;
	float f = clamp( ramp, 0.0, 1.0 );
	vec2 pair = clamp( z * fogK[5].xy - fogK[5].zw, 0.0, 1.0 );
	hb = mix( pair.x, pair.y, f );
	float mx = fogK[2].w;
	float clampT = ( ramp > 0.75 && ( fogRed & 1 ) == 0 ) ? min( ( f - 0.75 ) * 4.0 * ( 1.0 - mx ) + mx, 1.0 ) : mx;
	float escape = ( ramp < 0.015 && ( fogRed & 2 ) == 0 ) ? f * 66.666672 : 1.0;
	float I = f > 0.0 ? min( clampT, pow( f, fogK[1].w ) ) : 0.0;
	float weight = hb * fogK[3].w + ( 1.0 - hb );
	fogCol = mix( mix( fogK[1].rgb, fogK[3].rgb, I ), mix( fogK[2].rgb, fogK[4].rgb, I ), hb );
	return weight * I * escape;
}

/* lane VOLAIR1 (src/gl/cellvolair.h): the air IN PLACE of the game's fog. volTex holds, per froxel column, the
 * light the air sends toward the eye from the near plane to the far edge of slice k (rgb) and the transmittance
 * over the same stretch (a); the slices are exponential from volSlices.x over ln(far / near) = volSlices.y. A
 * surface at distance d shows colour x T(d) + L(d), linear in log-depth. Linear light. */
uniform bool volOn;
uniform sampler3D volTex;
uniform vec4 volRect;		// the viewport's origin, 1 / its size
uniform sampler2D volTile;	// the air pass's stage 0: per froxel column the farthest, nearest opaque ray distance (game units)
uniform int volFarClamp;	// 1: the lookup clamped to its column's farthest (red nofarclamp, and the programs without it: 0)
uniform vec4 volSlices;		// near, ln(far / near), slices, 1 = applied (0 under the red "off": the game's fog drawn)
uniform int volProbe;		// gates only: 1 the light term L alone, raw; 2 the transmittance T, grey; 3 the sun's T to the surface
uniform int volFogBlend;	// test only (WW_CELL_VOLAIR_FOGBLEND=1, option B): the game's fog drawn over the lit air
				// with its own alpha (0 nearer than its start distance); 0 = option A, the real light only

uniform vec4 volFar;		// lane FARGRID1: the far grid's start (the near grid's end), ln(end / start), slices, 1 = on
uniform vec4 volPack;		// the packed volume's shares: the near grid's width, height, the far grid's
uniform vec4 volPackDims;	// the packed volume's texels: width, height, depth; the near grid's slices

uniform vec4 volAirMed;		// the pass's medium (cell_volair.frag airMed): sigma0, zb, 1 / H, the top (the fog's height frame)
uniform vec4 volAirLayer;	// its near band and ground layer (airLayer): the near share w, zn, 1 / Hn, the ground strength s
uniform vec4 volAirGround;	// (airGround): the ground layer's anchor zg, 1 / its thickness
uniform int volAirFlags;	// 1 the surfaces' sun through the air (FOGSUN1; red nosunt clears it), 2 the far tail (red
				// froxonly clears it), 4 the records' colour on the tail (red physcol clears it), 8 the sky
				// through the air (cell_volsky.frag; red nosky clears it), 16 the effects and particles through the
				// air (wwFogFx; red fxfog clears it), 32 the tricubic read (FOGTRICUBIC1, WW_CELL_VOLAIR_TRICUBIC=1),
				// 64 the ground layer on the terrain grid (FOGTERRAIN2; red WW_FOG_TERRAIN_RED=absolute clears it)
uniform sampler2D volAirTerr;	// FOGTERRAIN2: the pass's low-pass terrain (cell_airmed.glsl airTerr), world z
uniform vec4 volAirTerrU;	// its uv at view point p: (dot( U.xyz, p ) + U.w, dot( V.xyz, p ) + V.w)
uniform vec4 volAirTerrV;
uniform float volAirTerrRef;	// the world z of the fog frame's ground (airTerrRef)

// the column between heights lo < hi of one term B(z) = min(1, exp(-(z - z0) invH)): flat under z0, exponential
// above (invH 0: flat everywhere); no cancellation of two large columns. Shared with cell_volair.frag airCol.
float wwAirColOne( float lo, float hi, float z0, float invH )
{
	if ( hi <= lo )
		return 0.0;
	if ( invH <= 0.0 )
		return hi - lo;
	float below = max( min( hi, z0 ) - lo, 0.0 );
	float a = max( lo, z0 );
	if ( hi <= a )
		return below;
	float w = hi - a;
	float u = w * invH;
	float e1 = u < 1e-3 ? 1.0 - u * ( 0.5 - u / 6.0 ) : ( 1.0 - exp( -u ) ) / u;
	return below + exp( -( a - z0 ) * invH ) * w * e1;
}

/* the column of the whole density (cellvolair.cpp VaLayer): (1 - w) far band + w near band + s ground layer, empty
 * above the top; exactly the far band's column when w = s = 0 */
float wwAirColLaw( float lo, float hi, vec4 med, vec4 layer, vec4 ground )
{
	hi = min( hi, med.w );
	float c = wwAirColOne( lo, hi, med.y, med.z );
	if ( layer.x <= 0.0 && layer.w <= 0.0 )
		return c;
	if ( layer.x > 0.0 )
		c = ( 1.0 - layer.x ) * c + layer.x * wwAirColOne( lo, hi, layer.y, layer.z );
	if ( layer.w > 0.0 )
		c += layer.w * wwAirColOne( lo, hi, ground.x, ground.y );
	return c;
}

// the density per unit sigma0 at height z (cell_volair.frag airRho)
float wwAirRhoLaw( float z, vec4 med, vec4 layer, vec4 ground )
{
	if ( z > med.w )
		return 0.0;
	float r = med.z > 0.0 ? min( 1.0, exp( -( z - med.y ) * med.z ) ) : 1.0;
	if ( layer.x <= 0.0 && layer.w <= 0.0 )
		return r;
	if ( layer.x > 0.0 )
		r = ( 1.0 - layer.x ) * r + layer.x * ( layer.z > 0.0 ? min( 1.0, exp( -( z - layer.y ) * layer.z ) ) : 1.0 );
	if ( layer.w > 0.0 )
		r += layer.w * ( ground.y > 0.0 ? min( 1.0, exp( -( z - ground.x ) * ground.y ) ) : 1.0 );
	return r;
}

// FOGTERRAIN2: the ground layer's anchor at view point p, the froxels' (cell_airmed.glsl airGroundAt); flag 64 off: absolute
vec4 wwAirGroundAt( vec3 p )
{
	if ( ( volAirFlags & 64 ) == 0 )
		return volAirGround;
	vec2 uv = vec2( dot( volAirTerrU.xyz, p ) + volAirTerrU.w, dot( volAirTerrV.xyz, p ) + volAirTerrV.w );
	return vec4( volAirGround.x + texture( volAirTerr, uv ).r - volAirTerrRef, volAirGround.yzw );
}

// the column of air between heights lo < hi per unit sigma0, the surfaces' copy of the pass's medium, its ground layer
// anchored by `ground` (wwAirGroundAt)
float wwAirCol( float lo, float hi, vec4 ground )
{
	return wwAirColLaw( lo, hi, volAirMed, volAirLayer, ground );
}

// the records' haze colour at distance d, height z, view direction dirView (cell_volair.frag airEnvColour), unit luminance
vec3 wwAirRecordColour( float d, float z, vec3 dirView, out bool ok );
#ifdef WW_SUNSHADOW
float wwAirTailShadow( vec3 dirView, float t0, float t1, float tau );	// ww_sunshadow.glsl (lane SHADOWPAR1)
#endif

/* lane FARGRID1: with the far grid on, one volume holds both grids: the near grid in layers 0..n-1 over its share of
 * the width and height, the far grid after it over its own share. (L, T) of one grid at slice coordinate f (0 at its
 * near edge), the bilinear kept inside the grid's share (its own clamp to edge). */
vec4 wwGridAt( vec2 uv, vec2 share, float base, float n, float f )
{
	vec2 lo = 0.5 / volPackDims.xy;
	vec2 st = clamp( uv * share, lo, share - lo );
	vec4 v;
	if ( f < 1.0 ) {
		v = texture( volTex, vec3( st, ( base + 0.5 ) / volPackDims.z ) );
		v = vec4( v.rgb * max( f, 0.0 ), 1.0 - ( 1.0 - v.a ) * max( f, 0.0 ) );
	} else {
		v = texture( volTex, vec3( st, ( base + min( f, n ) - 0.5 ) / volPackDims.z ) );
	}
	return vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
}

// the light per unit (1 - T) of a grid's last slice (the closed-form tail's J), `last` its (L, T) at the end
vec3 wwGridTailJ( vec2 uv, vec2 share, float base, float n, vec4 last )
{
	vec4 p = wwGridAt( uv, share, base, n, n - 1.0 );
	float dT = p.a - last.a;
	return dT > 1e-7 ? max( ( last.rgb - p.rgb ) / dT, vec3( 0.0 ) ) : vec3( 0.0 );
}

// near grid -> far grid -> closed-form tail: (L, T) at distance d (game units) along posView
vec4 wwVolAirAtFar( float d, vec2 uv, vec3 posView )
{
	float nN = volPackDims.w;
	if ( d <= volSlices.x )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	vec4 v = wwGridAt( uv, volPack.xy, 0.0, nN, log( d / volSlices.x ) / volSlices.y * nN );
	if ( d <= volFar.x )
		return v;
	float fF = log( d / volFar.x ) / volFar.y * volFar.z;
	vec4 w = wwGridAt( uv, volPack.zw, nN, volFar.z, fF );
	vec4 o = vec4( v.rgb + v.a * w.rgb, v.a * w.a );
	if ( fF <= volFar.z || ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
		return o;
	// the closed-form tail past the far grid (wwVolAirAt's, from the far grid's last slice)
	float far = volFar.x * exp( volFar.y );
	vec3 J = wwGridTailJ( uv, volPack.zw, nN, volFar.z, w );
	float z1 = dot( fogView.xyz, posView ) + fogView.w;
	float zc = fogView.w;
	float z0 = zc + ( z1 - zc ) * ( far / d );
	float len = d - far;
	float dz = z1 - z0;
	vec4 gT = wwAirGroundAt( posView * ( 0.5 * ( far + d ) / max( length( posView ) * fogDistScale, 1e-12 ) ) );	// the tail's middle
	float tau = abs( dz ) < 0.5 ? volAirMed.x * len * wwAirCol( z0, z0 + 1.0, gT ) : volAirMed.x * len * wwAirCol( min( z0, z1 ), max( z0, z1 ), gT ) / abs( dz );
	float Tt = exp( -tau );
	if ( ( volAirFlags & 4 ) != 0 ) {
		vec3 dirV = posView / max( length( posView ), 1e-12 );
		bool ok0, ok1;
		vec3 c0 = wwAirRecordColour( far, z0, dirV, ok0 );
		vec3 c1 = wwAirRecordColour( 0.5 * ( far + d ), 0.5 * ( z0 + z1 ), dirV, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, vec3( 1e-6 ) );
	}
	return vec4( o.rgb + o.a * J * ( 1.0 - Tt ), o.a * Tt );
}

/* (L, T) of the air between the eye and posView. Past the froxels' far edge (bungo 10-14, the far tail: far
 * mountains and the far LOD) the same medium continues in closed form: T x= exp(-sigma0 x the column along the
 * segment / its slope), L += T_edge x J x (1 - T_tail), J the light the last slice's air scattered (its L and T steps,
 * so the tail starts continuous with the froxels), carried to the tail's middle in the records' colour. A ray that
 * climbs meets thinner air: a peak keeps more contrast than its foot. */
/* FOGTRICUBIC1 (bungo 10-21): a cubic B-spline read of the froxels, from linear fetches placed between texels (the
 * fractional offsets carry the weights). Smooths the grid's blocks; xyOnly keeps the slice exact (the far tail's and the
 * sky's step J needs the two slices themselves). Flag 32 off: the plain trilinear read. */
vec4 wwVolTex( vec3 p, bool xyOnly )
{
	if ( ( volAirFlags & 32 ) == 0 )
		return texture( volTex, p );
	vec3 n = vec3( textureSize( volTex, 0 ) );
	vec3 c = p * n - 0.5;
	vec3 i = floor( c );
	vec3 f = c - i;
	vec3 w0 = ( ( -f + 3.0 ) * f - 3.0 ) * f + 1.0;	// x 6: (1 - f)^3
	vec3 w1 = ( 3.0 * f - 6.0 ) * f * f + 4.0;
	vec3 w2 = ( ( -3.0 * f + 3.0 ) * f + 3.0 ) * f + 1.0;
	vec3 w3 = f * f * f;
	vec3 g0 = w0 + w1, g1 = w2 + w3;
	vec3 h0 = ( i - 0.5 + w1 / g0 ) / n, h1 = ( i + 1.5 + w3 / g1 ) / n;
	g0 /= 6.0;
	g1 /= 6.0;
	if ( xyOnly ) {
		return g0.y * ( g0.x * texture( volTex, vec3( h0.x, h0.y, p.z ) ) + g1.x * texture( volTex, vec3( h1.x, h0.y, p.z ) ) )
			+ g1.y * ( g0.x * texture( volTex, vec3( h0.x, h1.y, p.z ) ) + g1.x * texture( volTex, vec3( h1.x, h1.y, p.z ) ) );
	}
	vec4 a = g0.y * ( g0.x * texture( volTex, vec3( h0.x, h0.y, h0.z ) ) + g1.x * texture( volTex, vec3( h1.x, h0.y, h0.z ) ) )
		+ g1.y * ( g0.x * texture( volTex, vec3( h0.x, h1.y, h0.z ) ) + g1.x * texture( volTex, vec3( h1.x, h1.y, h0.z ) ) );
	vec4 b = g0.y * ( g0.x * texture( volTex, vec3( h0.x, h0.y, h1.z ) ) + g1.x * texture( volTex, vec3( h1.x, h0.y, h1.z ) ) )
		+ g1.y * ( g0.x * texture( volTex, vec3( h0.x, h1.y, h1.z ) ) + g1.x * texture( volTex, vec3( h1.x, h1.y, h1.z ) ) );
	return g0.z * a + g1.z * b;
}

vec4 wwVolAirAtUv( vec3 posView, vec2 uv )	// lane VOLPART1: uv = the column (a particle's probe: its centre's)
{
	float d = length( posView ) * fogDistScale;
	/* 10-16: no farther than the column's farthest opaque surface (the obscurance pass's depth). At a thin sliver's
	 * edge the interpolated position can lie far past the depth the same fragment writes (a trim at 596 shaded as 763):
	 * that read the froxels the depth skip had left empty, and an air no surface there sits behind. */
	if ( volFarClamp != 0 )
		d = min( d, texture( volTile, uv ).r );
	if ( volFar.w > 0.5 )
		return wwVolAirAtFar( d, uv, posView );
	if ( d <= volSlices.x )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	float f = log( d / volSlices.x ) / volSlices.y * volSlices.z;
	vec4 v;
	if ( f < 1.0 ) {
		v = wwVolTex( vec3( uv, 0.5 / volSlices.z ), true );
		v = vec4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = wwVolTex( vec3( uv, ( min( f, volSlices.z ) - 0.5 ) / volSlices.z ), f > volSlices.z );
	}
	v = vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
	if ( f <= volSlices.z || ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
		return v;
	// the far tail
	float far = volSlices.x * exp( volSlices.y );
	vec4 p = wwVolTex( vec3( uv, ( volSlices.z - 1.5 ) / volSlices.z ), true );
	float dT = p.a - v.a;
	vec3 J = dT > 1e-7 ? max( ( v.rgb - p.rgb ) / dT, vec3( 0.0 ) ) : vec3( 0.0 );
	float z1 = dot( fogView.xyz, posView ) + fogView.w;
	float zc = fogView.w;
	float z0 = zc + ( z1 - zc ) * ( far / d );
	float len = d - far;
	float dz = z1 - z0;
	vec4 gT = wwAirGroundAt( posView * ( 0.5 * ( far + d ) / max( length( posView ) * fogDistScale, 1e-12 ) ) );	// the tail's middle
	float tau = abs( dz ) < 0.5 ? volAirMed.x * len * wwAirCol( z0, z0 + 1.0, gT ) : volAirMed.x * len * wwAirCol( min( z0, z1 ), max( z0, z1 ), gT ) / abs( dz );
	float Tt = exp( -tau );
#ifdef WW_SUNSHADOW
	J *= wwAirTailShadow( posView / max( length( posView ), 1e-12 ), far, d, tau );
#endif
	if ( ( volAirFlags & 4 ) != 0 ) {
		vec3 dirV = posView / max( length( posView ), 1e-12 );
		bool ok0, ok1;
		vec3 c0 = wwAirRecordColour( far, z0, dirV, ok0 );
		vec3 c1 = wwAirRecordColour( 0.5 * ( far + d ), 0.5 * ( z0 + z1 ), dirV, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, vec3( 1e-6 ) );
	}
	return vec4( v.rgb + v.a * J * ( 1.0 - Tt ), v.a * Tt );
}

// (L, T) of the air between the eye and posView, at this fragment's column
vec4 wwVolAirAt( vec3 posView )
{
	return wwVolAirAtUv( posView, ( gl_FragCoord.xy - volRect.xy ) * volRect.zw );
}

/* (L, T) of the whole air along a sky ray (bungo 10-14: the far tower stood out against an unfogged sky). The sky is a
 * surface at infinite distance: the froxels to their far edge, then the far tail of wwVolAirAt run to the end of the
 * air -- climbing, to the air's top (the column over the sine of the elevation: finite); level or down, the air never
 * ends (T 0, the fog's own colour). It is the limit of wwVolAirAt as the distance grows, so far geometry and the sky
 * behind it take the same air. The tail's colour is read halfway to where the ray leaves the air. */
vec4 wwVolAirSky( vec3 dirView )
{
	vec2 uv = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw;
	float n = volSlices.z;
	vec4 v;
	float far;
	vec3 J;
	if ( volFar.w > 0.5 ) {
		// lane FARGRID1: the near grid's end, then the far grid's, the tail from the far grid's last slice
		vec4 a = wwGridAt( uv, volPack.xy, 0.0, volPackDims.w, volPackDims.w );
		vec4 b = wwGridAt( uv, volPack.zw, volPackDims.w, volFar.z, volFar.z );
		v = vec4( a.rgb + a.a * b.rgb, a.a * b.a );
		if ( ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
			return v;
		far = volFar.x * exp( volFar.y );
		J = wwGridTailJ( uv, volPack.zw, volPackDims.w, volFar.z, b );
	} else {
	v = wwVolTex( vec3( uv, ( n - 0.5 ) / n ), true );
	v = vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
	if ( ( volAirFlags & 2 ) == 0 || volAirMed.x <= 0.0 )
		return v;
	far = volSlices.x * exp( volSlices.y );
	vec4 p = wwVolTex( vec3( uv, ( n - 1.5 ) / n ), true );
	float dT = p.a - v.a;
	J = dT > 1e-7 ? max( ( v.rgb - p.rgb ) / dT, vec3( 0.0 ) ) : vec3( 0.0 );
	}
	float s = dot( fogView.xyz, dirView ) / max( fogDistScale, 1e-12 );	// the sine of the elevation
	float z0 = fogView.w + s * far;
	float top = volAirMed.w;
	float Tt, dEnd;
	if ( s > 1e-4 ) {
		dEnd = far + min( max( top - z0, 0.0 ) / s, 1e7 );
		// FOGTERRAIN2: the ground under the climb's middle (a long level ray reads the window's clamped edge)
		Tt = exp( -volAirMed.x * wwAirCol( z0, top, wwAirGroundAt( dirView * ( 0.5 * ( far + dEnd ) / max( fogDistScale, 1e-12 ) ) ) ) / s );
	} else {
		Tt = ( s >= 0.0 && z0 >= top ) ? 1.0 : 0.0;
		dEnd = far + 1e7;
	}
	if ( ( volAirFlags & 4 ) != 0 ) {
		bool ok0, ok1;
		float dm = 0.5 * ( far + dEnd );
		vec3 c0 = wwAirRecordColour( far, z0, dirView, ok0 );
		vec3 c1 = wwAirRecordColour( dm, z0 + s * ( dm - far ), dirView, ok1 );
		if ( ok0 && ok1 )
			J *= c1 / max( c0, vec3( 1e-6 ) );
	}
	return vec4( v.rgb + v.a * J * ( 1.0 - Tt ), v.a * Tt );
}

/* FOGSUN1 (bungo 10-14): the sun reaching a surface through the air -- the air's transmittance from the surface's
 * height to its top along the sun, closed form (the air's own sun term, cell_volair.frag airTup): in heavy fog the
 * direct sun is dimmed by the medium; the sky's share the air scatters is its in-scattering. 1 where the air is off.
 * Red nosunt: the sun not dimmed. The grazing guard mu >= 0.02 is the air's own (a flat medium has no finite column at
 * the horizon). */
float wwVolAirSunT( vec3 posView )
{
	if ( !fogOn || !volOn || volSlices.w < 0.5 || fogSun.w <= 0.0 || volAirMed.x <= 0.0 || ( volAirFlags & 1 ) == 0 )
		return 1.0;
	float lo = dot( fogView.xyz, posView ) + fogView.w;
	if ( lo >= volAirMed.w )
		return 1.0;
	float mu = max( dot( fogView.xyz, fogSun.xyz ) / max( length( fogView.xyz ), 1e-12 ), 0.02 );
	return exp( -volAirMed.x * wwAirCol( lo, volAirMed.w, wwAirGroundAt( posView ) ) / mu );
}

vec3 wwFogBase( vec3 preFog, vec3 posView, float tAp, bool apOn );

/* linear in, linear out. Lane SKYA (ATMO2): with the aerial perspective on, the surface goes through the sky's air
 * first, then through the cell fog's residual (atmo_apfog.glsl: the combined transmittance is the min of the two, no
 * haze counted twice); one fog path for both, so no program carries the fog code twice. AP off: today's math. */
vec3 wwFog( vec3 preFog, vec3 posView )
{
	bool apOn = wwApOn();
	vec4 ap = apOn ? wwAtmoApAt( posView ) : vec4( 0.0, 0.0, 0.0, 1.0 );
	if ( apOn )
		preFog = preFog * ap.a + ap.rgb;
	if ( !fogOn )
		return preFog;
	if ( volOn && volSlices.w > 0.5 ) {
		vec4 a = wwVolAirAt( posView );
		if ( apOn ) {
			bool dbl = atApDouble > 0.5;
			float tr = wwApFogResidualT( ap.a, a.a, dbl );
			return preFog * tr + ( dbl ? a.rgb : wwApFogResidualL( a.rgb, a.a, tr ) );
		}
		vec3 lit = preFog * a.a + a.rgb;
		if ( volFogBlend == 1 )
			return wwFogBase( lit, posView, 1.0, false );
		if ( volFogBlend == 2 ) {	// red blendnear (gates only): weighted by the air's 1 - T, which has no start distance
			float hb;
			vec3 fc;
			wwFogEval( length( posView ) * fogDistScale, dot( fogView.xyz, posView ) + fogView.w, hb, fc );
			return mix( lit, fc, 1.0 - a.a );
		}
		return lit;
	}
	return wwFogBase( preFog, posView, ap.a, apOn );
}

vec3 wwAirRecordColour( float d, float z, vec3 dirView, out bool ok )
{
	float hb;
	vec3 fc;
	wwFogEval( d, z, hb, fc );
	float sunW = fogSun.w > 0.0 ? pow( max( dot( dirView, fogSun.xyz ), 0.0 ), fogSunColour.w ) * fogSun.w : 0.0;
	vec3 c = max( mix( fc, fogSunColour.rgb, sunW ), vec3( 0.0 ) );
	float lc = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
	ok = lc > 1e-8;
	return ok ? c / lc : vec3( 1.0 );
}

// the game's fog alone; with the aerial perspective on, its alpha cut to the residual: 1 - alpha' = min(1, (1 - alpha) / Tap)
vec3 wwFogBase( vec3 preFog, vec3 posView, float tAp, bool apOn )
{
	float d = length( posView ) * fogDistScale;
	float z = dot( fogView.xyz, posView ) + fogView.w;
	float hb;
	vec3 fogCol;
	float alpha = wwFogEval( d, z, hb, fogCol );
	if ( apOn )
		alpha = 1.0 - wwApFogResidualT( tAp, 1.0 - alpha, atApDouble > 0.5 );
	vec3 viewDir = posView / max( length( posView ), 1e-6 );
	float sunW = pow( max( dot( viewDir, fogSun.xyz ), 0.0 ), fogSunColour.w ) * fogSun.w;
	vec3 sunlit = mix( fogCol, fogSunColour.rgb, sunW );
	float gray = dot( preFog, vec3( 1.0 / 3.0 ) );
	vec3 resolved = alpha < fogK[2].w ? mix( sunlit, vec3( gray ), gray ) : sunlit;
	return mix( preFog, resolved, alpha );
}

// lane VOLAIR1: an effect's or a particle's linear colour through the same fog as the opaque draws (wwFog), so a
// card on a far wall fades with the wall. An additive draw takes the transmittance only: its light adds over a
// surface that already holds the in-scatter. Red fxfog (flag 16 clear) and the air off: the game's fog alone, as before.
// Lane SKYA (ATMO2): with the aerial perspective on, a lit effect takes wwFog's split; an additive one the sky's
// transmittance times the fog's residual.
vec3 wwFogFx( vec3 lin, vec3 posView, bool additive )
{
	bool apOn = wwApOn();
	if ( !fogOn && !apOn )
		return lin;
	bool volFx = volOn && volSlices.w > 0.5 && ( volAirFlags & 16 ) != 0;
	if ( !additive && ( volFx || apOn ) )
		return max( wwFog( lin, posView ), vec3( 0.0 ) );
	float tAp = apOn ? wwAtmoApAt( posView ).a : 1.0;
	if ( !fogOn )
		return lin * tAp;
	float hb;
	vec3 fc;
	float f = wwFogEval( length( posView ) * fogDistScale, dot( fogView.xyz, posView ) + fogView.w, hb, fc );
	if ( !volFx ) {
		if ( apOn )
			return lin * tAp * wwApFogResidualT( tAp, 1.0 - f, atApDouble > 0.5 );
		return additive ? lin * ( 1.0 - f ) : mix( lin, fc, f );
	}
	float tv = wwVolAirAt( posView ).a;
	if ( apOn )
		return lin * tAp * wwApFogResidualT( tAp, tv, atApDouble > 0.5 );
	lin *= tv;
	return volFogBlend == 1 ? lin * ( 1.0 - f ) : lin;
}

// gates only: true when the fragment must write `o` raw (no exposure, no view transform)
bool wwFogProbe( vec3 posView, out vec3 o )
{
	o = vec3( 0.0 );
	if ( fogOn && volOn && ( volProbe == 1 || volProbe == 2 ) ) {	// lane VOLAIR1: the air's L or T at this fragment
		vec4 a = wwVolAirAt( posView );
		o = volProbe == 1 ? a.rgb : vec3( a.a );
		return true;
	}
	if ( fogOn && volOn && volProbe == 3 ) {	// FOGSUN1: the sun's transmittance through the air to this fragment, grey
		o = vec3( wwVolAirSunT( posView ) );
		return true;
	}
	if ( !fogOn || fogProbe.z < 0.5 )
		return false;
	if ( fogProbe.z > 8.5 ) {
		// lane VOLAIR1 (the far tail's gate), 16 bits in R (high byte) and G (low): 9 = log2(d) / 20, 10 = (z + 65536)
		// / 131072 (d to ~1e6 units, z in 2-unit steps); B = 0
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		float x = fogProbe.z < 9.5 ? log2( max( d, 1.0 ) ) / 20.0 : ( z + 65536.0 ) / 131072.0;
		float q = floor( clamp( x, 0.0, 1.0 ) * 65535.0 + 0.5 );
		o = vec3( floor( q / 256.0 ) / 255.0, mod( q, 256.0 ) / 255.0, 0.0 );
		return true;
	}
	if ( fogProbe.z > 7.5 ) {
		// lane FOG2, 8: the geometry this fragment's fog read, wide. The gate drops the pixels whose top surface
		// is not the one the position probes saw (a translucent card that serves this probe and not theirs)
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		o = vec3( d / 16384.0, 0.5 + z / 32768.0, 0.0 );
		return true;
	}
	if ( fogProbe.z > 5.5 ) {
		// lane FOG2, per fragment: 6 = (alpha, height blend, 0), 7 = the fog colour ^ (1 / 2.2)
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		float hb;
		vec3 fogCol;
		float alpha = wwFogEval( d, z, hb, fogCol );
		o = fogProbe.z < 6.5 ? vec3( alpha, hb, 0.0 ) : pow( max( fogCol, vec3( 0.0 ) ), vec3( 1.0 / 2.2 ) );
		return true;
	}
	if ( fogProbe.z > 4.5 ) {
		// the geometry the fog reads: R = d / 4096, G = 0.5 + z / 2000
		float d = length( posView ) * fogDistScale;
		float z = dot( fogView.xyz, posView ) + fogView.w;
		o = vec3( d / 4096.0, 0.5 + z / 2000.0, 0.0 );
		return true;
	}
	float hb;
	vec3 fogCol;
	float alpha = wwFogEval( fogProbe.x, fogProbe.y, hb, fogCol );
	if ( fogProbe.z < 1.5 )
		o = vec3( alpha );
	else if ( fogProbe.z < 2.5 )
		o = fogCol * 0.5;
	else
		o = vec3( hb );
	return true;
}

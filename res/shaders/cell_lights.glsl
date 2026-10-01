// lane PRTP3 (cell lights): the cell's placed lights, interior ambient and directional, per
// docs/PRTP2_LIGHT_MODEL.md. Included by fo4_default.frag only when WW_CELLLIGHTS is defined
// (fo4_cell.frag); src/gl/celllights.cpp sets every uniform.

uniform bool cellOn;
uniform samplerBuffer cellLights;	// 5 texels a light: pos+radius, color+cosOuter (-2 omni), dir+cone, bias scale exponent flags
									// (1 noSpec, 2 noRim, 4 ignoreRoughness), shadow slot (-1 none) kind near-clip xlig-bias
uniform int cellLightCount;
uniform vec4 cellRow[3];			// world = (dot(row.xyz, posView) + row.w), the view's inverse
uniform bool cellHasDalc;
uniform vec4 cellDalc[3];			// per channel: (p - n) / 2 per axis, mean of the six (byte / 255)
uniform bool cellHasDir;
uniform vec3 cellDirColor;			// linear
uniform vec3 cellDirTo;				// world, TO the light
uniform bool cellInterior;
uniform vec3 cellCenter;
uniform int cellProbe;
uniform int cellRed;				// 1 linear: the radial curve without its 2.2; 8 lambert, 16 normalised, 32 norim,
									// 64 rimflags (the lights' rim / roughness flags ignored)
// lane PRTPGI: the bake relit by these lights (src/probegi.h), six axis slabs of dims.z each, x fastest;
// rgb = irradiance x valid, a = valid (so a filtered sample divides by its own valid)
uniform bool cellGiOn;
uniform sampler3D cellGi;
uniform vec3 cellGiOrigin;
uniform float cellGiVoxel;
uniform vec3 cellGiDims;

// lane IMGS1: the cell's imagespace, the game's own HDR -> display chain (src/gl/celllights.h)
uniform bool cellIsOn;
uniform float cellIsExposure;		// clamp( middle gray / (adapted + 0.001), min, max ), the CPU's
uniform float cellIsE;				// HNAM Tonemap E: the curve's toe numerator
uniform float cellIsAdapted;		// the frame's mean luminance, the contrast pivot
uniform vec3 cellIsCine;			// CNAM saturation, brightness, contrast
uniform vec4 cellIsTint;			// TNAM amount, r, g, b
uniform bool cellIsLutOn;
uniform sampler3D cellIsLut;
uniform int cellIsRed;				// 1 nolut, 2 noexp, 4 nograde, 8 nobloom (the CPU drops cellIsBloomOn)
// lane BLOOM1: the measure's blurred bright pass, a quarter of the view; rect = viewport origin, 1 / size
uniform bool cellIsBloomOn;
uniform sampler2D cellIsBloom;
uniform vec4 cellIsBloomRect;
// lane SHADOW1: one depth cube a shadow-casting light (distance to the caster / radius); texel = 2 / face
uniform bool cellShadowOn;
uniform samplerCubeArrayShadow cellShadow;
uniform float cellShadowTexel;

// sqrt-of-linear in (this program's convention), display out. Shaders011.fxp, the tonemap PS and the LUT PS.
vec3 cellImageSpace( vec3 sqrtColor )
{
	vec3 x = max( sqrtColor, vec3( 0.0 ) );
	x = x * x;
#ifndef WW_CELL_FX	// lane EFX2: an effect blends over a surface that already took the bloom
	if ( cellIsBloomOn )	// the tonemap PS adds the bloom target before its exposure multiply
		x += texture( cellIsBloom, ( gl_FragCoord.xy - cellIsBloomRect.xy ) * cellIsBloomRect.zw ).rgb;
#endif
	x = x * ( ( cellIsRed & 2 ) != 0 ? 1.0 : cellIsExposure ) * 2.0;
	float E = cellIsE;
	vec3 c = ( x * ( 0.15 * x + 0.05 ) + 0.2 * E ) / ( x * ( 0.15 * x + 0.5 ) + 0.06 ) - E / 0.3;
	c /= ( 0.2 * E + 19.376 ) * 0.040856 - E / 0.3;		// the curve at W 11.2
	if ( ( cellIsRed & 4 ) == 0 ) {
		float luma = dot( c, vec3( 0.2125, 0.7154, 0.0721 ) );
		c = mix( vec3( luma ), c, cellIsCine.x );
		c = mix( c, luma * cellIsTint.yzw, cellIsTint.x );
		c = cellIsCine.z * ( cellIsCine.y * c - cellIsAdapted ) + cellIsAdapted;
	}
	c = pow( max( c, vec3( 0.0 ) ), vec3( 1.0 / 2.2 ) );
	if ( cellIsLutOn && ( cellIsRed & 1 ) == 0 )
		c = texture( cellIsLut, c * 0.9375 + 0.03125 ).rgb;
	return c;
}

vec3 cellWorldPos( vec3 posView )
{
	vec4 p = vec4( posView, 1.0 );
	return vec3( dot( cellRow[0], p ), dot( cellRow[1], p ), dot( cellRow[2], p ) );
}

vec3 cellWorldDir( vec3 dirView )
{
	return normalize( vec3( dot( cellRow[0].xyz, dirView ), dot( cellRow[1].xyz, dirView ), dot( cellRow[2].xyz, dirView ) ) );
}

// PRTP2 section 1: x = saturate(d / r); atten = pow(1 - saturate(scale * x^exponent + bias), 2.2)
float cellRadial( float d, float r, vec3 bse )
{
	float x = clamp( d / max( r, 0.001 ), 0.0, 1.0 );
	float xe = bse.z > 0.0 ? pow( x, bse.z ) : 1.0;
	float k = 1.0 - clamp( bse.y * xe + bse.x, 0.0, 1.0 );
	return ( cellRed & 1 ) != 0 ? k : pow( k, 2.2 );
}

/* lane SHADOW1: light i's shadow factor at P (normal N, facing it): 1 lit, 0 shadowed. The point lifted
 * 1.5 texels along N (the slope bias), then its distance - 1 unit against 3x3 taps a texel apart (each
 * a hardware 2x2 compare), / 9, as the game's 9 taps. A hemisphere lights nothing behind its plane. */
float cellShadowF( int i, vec3 P, vec3 N )
{
	vec4 t4 = texelFetch( cellLights, i * 5 + 4 );
	if ( !cellShadowOn || t4.y < 0.5 )
		return 1.0;
	vec4 t0 = texelFetch( cellLights, i * 5 );
	if ( t4.y > 1.5 && t4.y < 2.5 && dot( P - t0.xyz, texelFetch( cellLights, i * 5 + 2 ).xyz ) < 0.0 )
		return 0.0;	// the mask's paraboloid: behind the hemisphere's plane is unlit (in a slot or not)
	if ( t4.x < -0.5 )
		return 1.0;	// a shadow light beyond the slot budget: unshadowed (the game's budget is unread)
	vec3 r = P - t0.xyz;
	vec3 rn = r + N * ( 1.5 * length( r ) * cellShadowTexel );
	float d = length( rn );
	float ref = ( d - 1.0 ) / max( t0.w, 0.001 );
	vec3 a = rn / max( d, 0.001 );
	vec3 u = normalize( cross( a, abs( a.z ) < 0.9 ? vec3( 0.0, 0.0, 1.0 ) : vec3( 1.0, 0.0, 0.0 ) ) );
	vec3 v = cross( a, u );
	float s = 0.0;
	for ( int y = -1; y <= 1; y++ )
		for ( int x = -1; x <= 1; x++ )
			s += texture( cellShadow, vec4( a + ( float( x ) * u + float( y ) * v ) * cellShadowTexel, t4.x ), ref );
	return s / 9.0;
}

/* probe 7: the factors of shadow slots 0, 1, 2 in r, g, b as 2/255 + f x 253/255; 0 where that light is out
 * of reach or faces away (N.L under 0.05), or no light holds the slot */
vec3 cellShadowProbe( vec3 P, vec3 N )
{
	vec3 o = vec3( 0.0 );
	for ( int i = 0; i < cellLightCount; i++ ) {
		float slot = texelFetch( cellLights, i * 5 + 4 ).x;
		if ( slot < -0.5 || slot > 2.5 )
			continue;
		vec4 t0 = texelFetch( cellLights, i * 5 );
		vec3 Lv = t0.xyz - P;
		float d = length( Lv );
		if ( d >= t0.w || dot( N, Lv / max( d, 0.001 ) ) < 0.05 )
			continue;
		o[int( slot + 0.5 )] = ( 2.0 + cellShadowF( i, P, N ) * 253.0 ) / 255.0;
	}
	return o;
}

// the bounce's irradiance at P, normal N: the three facing slabs blended by n^2, sampled half a
// voxel off the surface (the grid's voxels behind a wall are its other room's)
vec3 cellGiE( vec3 P, vec3 N )
{
	vec3 g = ( P + N * ( 0.5 * cellGiVoxel ) - cellGiOrigin ) / cellGiVoxel;
	vec2 xy = g.xy / cellGiDims.xy;
	float z = clamp( g.z, 0.5, cellGiDims.z - 0.5 );
	float depth = 6.0 * cellGiDims.z;
	vec3 n2 = N * N;
	vec4 s = n2.x * texture( cellGi, vec3( xy, ( z + ( N.x >= 0.0 ? 0.0 : 1.0 ) * cellGiDims.z ) / depth ) )
	       + n2.y * texture( cellGi, vec3( xy, ( z + ( N.y >= 0.0 ? 2.0 : 3.0 ) * cellGiDims.z ) / depth ) )
	       + n2.z * texture( cellGi, vec3( xy, ( z + ( N.z >= 0.0 ? 4.0 : 5.0 ) * cellGiDims.z ) / depth ) );
	return s.a > 0.01 ? max( s.rgb / s.a, vec3( 0.0 ) ) : vec3( 0.0 );
}

/* light i at world point P, normal N: its colour x the radial curve x the spot cone (no N.L), and
 * the direction to it; zero when out of reach or behind the surface. The PBR path's per-light term. */
vec3 cellLightE( int i, vec3 P, vec3 N, out vec3 L, out bool noSpec )
{
	vec4 t0 = texelFetch( cellLights, i * 5 );
	vec3 Lv = t0.xyz - P;
	float d = length( Lv );
	L = Lv / max( d, 0.001 );
	noSpec = true;
	if ( d >= t0.w || dot( N, L ) <= 0.0 )
		return vec3( 0.0 );
	vec4 t1 = texelFetch( cellLights, i * 5 + 1 );
	vec4 t3 = texelFetch( cellLights, i * 5 + 3 );
	float a = cellRadial( d, t0.w, t3.xyz );
	if ( t1.w > -1.5 ) {
		vec4 t2 = texelFetch( cellLights, i * 5 + 2 );
		float base = clamp( 1.0 - ( 1.0 - dot( -L, t2.xyz ) ) / max( 1.0 - t1.w, 1e-4 ), 0.0, 1.0 );
		a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
	}
	if ( a > 0.0 )
		a *= cellShadowF( i, P, N );
	noSpec = ( int( t3.w + 0.5 ) & 1 ) != 0;
	return t1.rgb * a;
}

// the harness probes for a program without the legacy BRDF helpers (pbrm_cell)
vec3 cellProbeRaw( vec3 P, vec3 N )
{
	if ( cellProbe == 5 )
		return cellGiOn ? clamp( cellGiE( P, N ) * 0.31830989, 0.0, 1.0 ) : vec3( 0.0 );
	if ( cellProbe == 1 ) {
		vec3 E = vec3( 0.0 );
		for ( int i = 0; i < cellLightCount; i++ ) {
			vec3 L;
			bool ns;
			vec3 c = cellLightE( i, P, N, L, ns );
			E += c * max( dot( N, L ), 0.0 );
		}
		return clamp( E * 0.25, 0.0, 1.0 );
	}
	if ( cellProbe == 7 )
		return cellShadowProbe( P, N );
	vec3 q = clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
	if ( cellProbe == 2 )
		return floor( q / 256.0 ) / 255.0;
	if ( cellProbe == 3 )
		return mod( q, 256.0 ) / 255.0;
	return N * 0.5 + 0.5;
}

#if !defined( WW_CELL_PBR ) && !defined( WW_CELL_FX )	// lane EFX2: the effect program takes none of the surface lobes
/* The game's own light specular (the shipped deferred point/spot light shaders, read op for op):
 * normalized Blinn-Phong, n = 2^(gloss x 10 + 1), D = NdotH^n (n + 2) / 2pi, Schlick Fresnel with
 * F0 = 0.2, a Cook-Torrance geometry select with the 1/NdotV folded in, x 1/4, clamped at 15, x pi.
 * The caller multiplies the light's irradiance (N.L inside) and the material's mask and colour. */
float cellSpecGame( vec3 N, vec3 L, vec3 V, float gloss )
{
	float n = exp2( gloss * 10.0 + 1.0 );
	vec3 H = normalize( V + L );
	float NdotL = clamp( dot( N, L ), 0.0, 1.0 );
	float NdotV = clamp( dot( N, V ), 0.0, 1.0 );
	float VdotH = clamp( dot( V, H ), 0.0, 1.0 );
	float NdotH = clamp( dot( N, H ), 0.0, 1.0 );
	float D = pow( NdotH, n ) * ( n + 2.0 ) * 0.159155;
	float m = min( NdotL, NdotV );
	float G = ( VdotH >= 2.0 * NdotH * m ) ? 2.0 * NdotH * ( NdotV == m ? 1.0 : NdotL / max( NdotV, 1e-4 ) ) / max( VdotH, 1e-4 )
	                                       : 1.0 / max( NdotV, 1e-4 );
	float f1 = 1.0 - VdotH;
	float f4 = f1 * f1 * f1 * f1;
	float F = min( ( 1.0 - f1 * f4 ) * 0.2 + f4 * f1, 1.0 );
	return min( D * G * F * 0.25, 15.0 ) * 3.141593;
}

/* Lane ON1: the game's legacy diffuse (the same shaders, the spec / gloss branch, and its sun): Oren-Nayar
 * with sigma = 1 - gloss, A = 1 - 0.5 s2 / (s2 + 0.57), B = 0.45 s2 / (s2 + 0.09), the azimuth cosine from the
 * UNnormalised tangent-plane projections, x sinL sinV / max(NdotL, NdotV). The factor that multiplies NdotL. */
float cellOren( vec3 N, vec3 L, vec3 V, float NdotL, float gloss )
{
	if ( ( cellRed & 8 ) != 0 )
		return 1.0;	// WW_CELL_LIT_RED=lambert
	float s2 = ( 1.0 - gloss ) * ( 1.0 - gloss );
	float A = 1.0 - 0.5 * s2 / ( s2 + 0.57 );
	float B = 0.45 * s2 / ( s2 + 0.09 );
	float NdotV = dot( N, V );
	vec3 tV = V - N * NdotV, tL = L - N * NdotL;
	float cosPhi = dot( tV, tL );
	if ( ( cellRed & 16 ) != 0 )
		cosPhi /= max( length( tV ) * length( tL ), 1e-6 );	// WW_CELL_LIT_RED=normalised: the textbook azimuth
	float geom = sqrt( clamp( ( 1.0 - NdotL * NdotL ) * ( 1.0 - NdotV * NdotV ), 0.0, 1.0 ) ) / max( max( NdotL, NdotV ), 1e-4 );
	return max( cosPhi, 0.0 ) * B * geom + A;
}

/* Lane RIM1: the game's back-light term, added to the diffuse by every light and the sun (legacy materials; the
 * PBR branch drops it): saturate(dot(V, -L)) x (1 - NdotV)^0.01 x (1 - gloss), times NdotL and the light like the
 * diffuse (the shadow scales it too). Bright where the camera looks toward the lamp across a rough surface. */
float cellRim( vec3 N, vec3 L, vec3 V, float gloss )
{
	if ( ( cellRed & ( 8 | 32 ) ) != 0 )
		return 0.0;	// WW_CELL_LIT_RED=lambert / norim
	float edge = pow( clamp( 1.0 - dot( N, V ), 0.0, 1.0 ), 0.01 );
	return clamp( dot( V, -L ), 0.0, 1.0 ) * edge * ( 1.0 - gloss );
}

/* the placed lights at world point P, world normal N: the irradiance (Lambert, what the GI, the reflection and
 * probe 1 read), the game's Oren-Nayar diffuse (what the albedo takes) and the specular sum */
vec3 cellRimSum;	// lane RIM1: the rim part of diffOn alone, for probe 10 (set by cellSumLights)
void cellSumLights( vec3 P, vec3 N, vec3 Vw, float gloss, out vec3 diff, out vec3 diffOn, out vec3 spec )
{
	diff = vec3( 0.0 );
	diffOn = vec3( 0.0 );
	cellRimSum = vec3( 0.0 );
	spec = vec3( 0.0 );
	for ( int i = 0; i < cellLightCount; i++ ) {
		vec4 t0 = texelFetch( cellLights, i * 5 );
		vec3 Lv = t0.xyz - P;
		float d = length( Lv );
		if ( d >= t0.w )
			continue;
		vec3 L = Lv / max( d, 0.001 );
		float NdotL = dot( N, L );
		if ( NdotL <= 0.0 )
			continue;
		vec4 t1 = texelFetch( cellLights, i * 5 + 1 );
		vec4 t3 = texelFetch( cellLights, i * 5 + 3 );
		float a = cellRadial( d, t0.w, t3.xyz );
		if ( t1.w > -1.5 ) {
			// PRTP2 section 2: base = saturate(1 - (1 - dot(-L, dir)) / (1 - cosOuter)), cone = min(base^falloff, 1)
			vec4 t2 = texelFetch( cellLights, i * 5 + 2 );
			float base = clamp( 1.0 - ( 1.0 - dot( -L, t2.xyz ) ) / max( 1.0 - t1.w, 1e-4 ), 0.0, 1.0 );
			a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
		}
		if ( a > 0.0 )
			a *= cellShadowF( i, P, N );	// lane SHADOW1
		vec3 E = t1.rgb * a * NdotL;
		diff += E;
		// lane RIM1: No Rim Lighting (2) drops the back-light; Ignore Roughness (4) also takes Lambert's diffuse
		int fl = int( t3.w + 0.5 ) & ( ( cellRed & 64 ) != 0 ? 1 : 7 );	// WW_CELL_LIT_RED=rimflags
		float rim = ( fl & 6 ) != 0 ? 0.0 : cellRim( N, L, Vw, gloss );
		diffOn += E * ( ( ( fl & 4 ) != 0 ? 1.0 : cellOren( N, L, Vw, NdotL, gloss ) ) + rim );
		cellRimSum += E * rim;
		if ( ( fl & 1 ) == 0 )
			spec += E * cellSpecGame( N, L, Vw, gloss );
	}
}


#endif

// PRTP2 section 4: ambient(n) = pow(max(dot(row, (n, 1)), 0), 2.2) per channel
vec3 cellAmbient( vec3 N )
{
	vec4 n1 = vec4( N, 1.0 );
	return pow( max( vec3( dot( cellDalc[0], n1 ), dot( cellDalc[1], n1 ), dot( cellDalc[2], n1 ) ), vec3( 0.0 ) ), vec3( 2.2 ) );
}

#if !defined( WW_CELL_PBR ) && !defined( WW_CELL_FX )

/* The lit colour, in the program's sqrt-of-linear space. Interior: the cell's ambient, directional
 * and placed lights replace the viewport light. Exterior: the placed lights add to what the
 * viewport (or the Lookdev sun) already lit. */
vec3 cellLit( vec3 color, vec3 albedo, vec3 normalView, vec3 posView, vec3 Vview, float specMask, vec3 specCol,
              float alphaR, float kSmith, vec3 emissive, vec3 envSpec )
{
	vec3 P = cellWorldPos( posView );
	vec3 N = cellWorldDir( normalView );
	vec3 Vw = cellWorldDir( Vview );
	vec3 diff, diffOn, spec;
	float gloss = 1.0 - sqrt( alphaR );	// the program's rough = 1 - gloss, alphaR = rough^2
	cellSumLights( P, N, Vw, gloss, diff, diffOn, spec );
	vec3 alb = albedo * albedo;
	vec3 gi = cellGiOn ? cellGiE( P, N ) * 0.31830989 : vec3( 0.0 );
	vec3 add = alb * ( diffOn + gi ) + spec * specMask * specCol;	// albedo x the game's diffuse; the GI stays Lambert
	if ( !cellInterior )
		return sqrt( color * color + add );
	// the light reaching this point (a white surface would show it): the cubemap reflection is lit by it,
	// so a dark corner's reflection is dark (the viewport path scales it by its own light the same way)
	vec3 E = diff + gi;
	vec3 Ed = diffOn + gi;	// what the albedo takes: the direct terms through Oren-Nayar (lane ON1)
	if ( cellHasDalc ) {
		vec3 amb = cellAmbient( N );
		E += amb;
		Ed += amb;
	}
	if ( cellHasDir ) {
		vec3 Ld = normalize( cellDirTo );
		float nl = dot( N, Ld );
		vec3 dir = cellDirColor * max( nl, 0.0 );
		E += dir;
		Ed += dir * ( cellOren( N, Ld, Vw, nl, gloss ) + cellRim( N, Ld, Vw, gloss ) );
	}
	vec3 lin = alb * Ed + spec * specMask * specCol + envSpec * envSpec * E;
	return sqrt( max( lin, vec3( 0.0 ) ) ) + emissive;
}

// the harness probes (WW_CELL_LIT_PROBE): raw values, no tonemap
vec3 cellProbeOut( vec3 normalView, vec3 posView, float alphaR, float kSmith )
{
	vec3 P = cellWorldPos( posView );
	vec3 N = cellWorldDir( normalView );
	if ( cellProbe == 5 )
		return cellGiOn ? clamp( cellGiE( P, N ) * 0.31830989, 0.0, 1.0 ) : vec3( 0.0 );
	if ( cellProbe == 1 || cellProbe == 8 || cellProbe == 10 ) {
		// 1: the irradiance / 4; 8 (lanes ON1, RIM1): the game's diffuse (Oren-Nayar + rim) / 4, seen from the camera;
		// 10 (lane RIM1): the rim alone x 4, sixteen times probe 8's reach, so the per-light rim flags show
		vec3 diff, diffOn, spec;
		cellSumLights( P, N, cellProbe == 1 ? N : cellWorldDir( -posView ), 1.0 - sqrt( alphaR ), diff, diffOn, spec );
		if ( cellProbe == 10 )
			return clamp( cellRimSum * 4.0, 0.0, 1.0 );
		return clamp( ( cellProbe == 8 ? diffOn : diff ) * 0.25, 0.0, 1.0 );
	}
	if ( cellProbe == 9 )
		return vec3( 1.0 - sqrt( alphaR ), 0.0, 0.0 );	// lane ON1: the gloss the diffuse used
	if ( cellProbe == 7 )
		return cellShadowProbe( P, N );
	// the position, 16 bits an axis over the 65536-unit box around the centre
	vec3 q = clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
	if ( cellProbe == 2 )
		return floor( q / 256.0 ) / 255.0;
	if ( cellProbe == 3 )
		return mod( q, 256.0 ) / 255.0;
	return N * 0.5 + 0.5;
}
#endif

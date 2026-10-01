// lane PRTP3 (cell lights): the cell's placed lights, interior ambient and directional, per
// docs/PRTP2_LIGHT_MODEL.md. Included by fo4_default.frag only when WW_CELLLIGHTS is defined
// (fo4_cell.frag); src/gl/celllights.cpp sets every uniform.

uniform bool cellOn;
uniform samplerBuffer cellLights;	// 4 texels a light: pos+radius, color+cosOuter (-2 omni), dir+cone, bias scale exponent noSpec
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
uniform int cellRed;				// 1 linear: the radial curve without its 2.2

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

// the placed lights at world point P, world normal N: diffuse irradiance and the specular sum
void cellSumLights( vec3 P, vec3 N, vec3 Vw, float alphaR, float kSmith, out vec3 diff, out vec3 spec )
{
	diff = vec3( 0.0 );
	spec = vec3( 0.0 );
	float NdotV = max( dot( N, Vw ), 1e-4 );
	for ( int i = 0; i < cellLightCount; i++ ) {
		vec4 t0 = texelFetch( cellLights, i * 4 );
		vec3 Lv = t0.xyz - P;
		float d = length( Lv );
		if ( d >= t0.w )
			continue;
		vec3 L = Lv / max( d, 0.001 );
		float NdotL = dot( N, L );
		if ( NdotL <= 0.0 )
			continue;
		vec4 t1 = texelFetch( cellLights, i * 4 + 1 );
		vec4 t3 = texelFetch( cellLights, i * 4 + 3 );
		float a = cellRadial( d, t0.w, t3.xyz );
		if ( t1.w > -1.5 ) {
			// PRTP2 section 2: base = saturate(1 - (1 - dot(-L, dir)) / (1 - cosOuter)), cone = min(base^falloff, 1)
			vec4 t2 = texelFetch( cellLights, i * 4 + 2 );
			float base = clamp( 1.0 - ( 1.0 - dot( -L, t2.xyz ) ) / max( 1.0 - t1.w, 1e-4 ), 0.0, 1.0 );
			a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
		}
		vec3 E = t1.rgb * a * NdotL;
		diff += E;
		if ( t3.w < 0.5 ) {
			vec3 H = normalize( L + Vw );
			float NdotH = max( dot( N, H ), 1e-4 );
			float F = fresnelSchlick( max( dot( Vw, H ), 1e-4 ), 0.04 );
			spec += E * ( D_GGX( NdotH, alphaR ) * G1( NdotL, kSmith ) * G1( NdotV, kSmith ) * F
			              / max( 4.0 * NdotL * NdotV, 0.001 ) );
		}
	}
}

// PRTP2 section 4: ambient(n) = pow(max(dot(row, (n, 1)), 0), 2.2) per channel
vec3 cellAmbient( vec3 N )
{
	vec4 n1 = vec4( N, 1.0 );
	return pow( max( vec3( dot( cellDalc[0], n1 ), dot( cellDalc[1], n1 ), dot( cellDalc[2], n1 ) ), vec3( 0.0 ) ), vec3( 2.2 ) );
}

/* The lit colour, in the program's sqrt-of-linear space. Interior: the cell's ambient, directional
 * and placed lights replace the viewport light. Exterior: the placed lights add to what the
 * viewport (or the Lookdev sun) already lit. */
vec3 cellLit( vec3 color, vec3 albedo, vec3 normalView, vec3 posView, vec3 Vview, float specMask, vec3 specCol,
              float alphaR, float kSmith, vec3 emissive )
{
	vec3 P = cellWorldPos( posView );
	vec3 N = cellWorldDir( normalView );
	vec3 Vw = cellWorldDir( Vview );
	vec3 diff, spec;
	cellSumLights( P, N, Vw, alphaR, kSmith, diff, spec );
	vec3 alb = albedo * albedo;
	vec3 add = alb * diff + spec * specMask * specCol;
	if ( !cellInterior )
		return sqrt( color * color + add );
	vec3 lin = add;
	if ( cellHasDalc )
		lin += alb * cellAmbient( N );
	if ( cellHasDir )
		lin += alb * cellDirColor * max( dot( N, normalize( cellDirTo ) ), 0.0 );
	return sqrt( max( lin, vec3( 0.0 ) ) ) + emissive;
}

// the harness probes (WW_CELL_LIT_PROBE): raw values, no tonemap
vec3 cellProbeOut( vec3 normalView, vec3 posView, float alphaR, float kSmith )
{
	vec3 P = cellWorldPos( posView );
	vec3 N = cellWorldDir( normalView );
	if ( cellProbe == 1 ) {
		vec3 diff, spec;
		cellSumLights( P, N, N, alphaR, kSmith, diff, spec );
		return clamp( diff * 0.25, 0.0, 1.0 );
	}
	// the position, 16 bits an axis over the 65536-unit box around the centre
	vec3 q = clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
	if ( cellProbe == 2 )
		return floor( q / 256.0 ) / 255.0;
	if ( cellProbe == 3 )
		return mod( q, 256.0 ) / 255.0;
	return N * 0.5 + 0.5;
}

// lane WATERREFL1 (src/gl/cellwater.h): physical water reflections and sun glare (fo4_waterlook.prog only).
// Roughness: LEAN mapping (Olano and Baker, I3D 2010): each noise layer has a second-moment texture (t.x^2, t.y^2 of
// the decoded slope, mipmapped and sampled like the layer), so the slope variance the filter averaged away comes back
// as roughness; the layers the game fades out past 8192 u keep their whole variance. Plus the pixel's own normal
// footprint (Tokuyoshi and Kaplanyan 2019, "Improved Geometric Specular Antialiasing"). Fresnel: F0 0.0204 (water,
// IOR 1.333) with Frostbite's rough Schlick (Lagarde and de Rousiers 2014). Sun: GGX with height-correlated Smith.
uniform bool waterReflOn;		// the Water reflections row
uniform bool waterGlintOn;		// the Water sun glare row
uniform int waterReflRed;		// 1 record (the WATR's F0 and reflectivity cap), 2 farcolour (the 8192 u colour swap),
								// 4 sharp (no roughness in the reflection), 8 noclouds (the plain sky gradient)
uniform int waterGlintRed;		// 1 game (today's sun + fixed-direction sparkle), 2 nolean (no slope variance), 4 noshoulder
uniform vec4 waterGlintK;		// the glare's highlight shoulder (x the sun colour), F0, footprint clamp, sharp-SSR alpha
uniform sampler2D waterMoment0;	// per noise layer: (E[t.x^2], E[t.y^2]) of the decoded slope t = 2 texel - 1
uniform sampler2D waterMoment1;
uniform sampler2D waterMoment2;
uniform sampler2D waterCloudMap;	// lane CLOUD1's map (R low, G mid, A high band coverage); waterCloudGeo.w 0 = no map
uniform vec4 waterCloudGeo;		// the map's centre x, y, half extent (game units), valid
uniform vec4 waterCloudZ;		// the low, mid, high bands' base z
uniform vec4 waterCloudCov;		// the bands' coverage outside the map; w unused
uniform vec3 waterCloudTint;

const float kWaterPi = 3.14159265;

float wwLayerVar( sampler2D m, vec2 uv, vec2 tap )
{
	vec2 t = tap * 2.0 - 1.0;
	vec2 e = texture( m, uv ).xy;
	return max( e.x - t.x * t.x, 0.0 ) + max( e.y - t.y * t.y, 0.0 );
}

// the whole layer's variance (its last mip: the texture's mean moment; the mean slope of a tiling noise is ~0)
float wwLayerVarAll( sampler2D m )
{
	vec2 e = textureLod( m, vec2( 0.5 ), 20.0 ).xy;
	return e.x + e.y;
}

/* GGX alpha^2 of the water at this pixel: the record's own lobe (sun spec power, Phong to Blinn x 4, then Beckmann
 * alpha = sqrt( 2 / ( n + 2 ) )), the slope variance the filtered layers lost, the faded layers' whole variance,
 * all through the game's flatten, and the footprint term on the drawn normal N */
float wwWaterAlpha2( vec2 uv0, vec2 uv1, vec2 uv2, vec2 tap0, vec2 tap1, vec2 tap2, vec4 Amp, float dfade,
	float flatten, float specPow, vec3 N, bool lean )
{
	float a0 = 2.0 / ( 4.0 * max( specPow, 1.0 ) + 2.0 );
	if ( !lean )
		return a0;
	float v = Amp.x * Amp.x * wwLayerVar( waterMoment0, uv0, tap0 )
		+ dfade * dfade * ( Amp.y * Amp.y * wwLayerVar( waterMoment1, uv1, tap1 ) + Amp.z * Amp.z * wwLayerVar( waterMoment2, uv2, tap2 ) );
	float z = 1.0 + ( Amp.y + Amp.z ) * dfade;
	float lost = ( 1.0 - dfade * dfade ) * ( Amp.y * Amp.y * wwLayerVarAll( waterMoment1 ) + Amp.z * Amp.z * wwLayerVarAll( waterMoment2 ) );
	float zl = 1.0 + Amp.y + Amp.z;
	float slopeVar = ( v / ( z * z ) + lost / ( zl * zl ) ) * flatten * flatten;
	vec2 s = N.xy / max( N.z, 0.05 );
	vec2 dx = dFdx( s ), dy = dFdy( s );
	float foot = min( 0.5 * ( dot( dx, dx ) + dot( dy, dy ) ), waterGlintK.z );
	return a0 + slopeVar + foot;
}

// Fresnel with roughness (Frostbite: the rough surface's grazing reflectance stops short of 1)
float wwWaterFresnel( float NoV, float alpha, float F0 )
{
	float c = 1.0 - clamp( NoV, 0.0, 1.0 );
	float c2 = c * c;
	return F0 + ( max( 1.0 - alpha, F0 ) - F0 ) * c2 * c2 * c;
}

// the sun's glare: GGX x height-correlated Smith x Schlick, the FO4 lighting scale (Lambert = albedo x sun x NoL, so
// the BRDF carries pi), then a highlight shoulder below the tone map's clip: x / ( 1 + x / K ), never brighter
vec3 wwWaterGlare( vec3 Ns, vec3 V, vec3 L, vec3 sun, float a2, float F0, bool shoulder )
{
	vec3 Vo = -V;
	float NoL = dot( Ns, L ), NoV = max( dot( Ns, Vo ), 1e-3 );
	if ( NoL <= 0.0 )
		return vec3( 0.0 );
	vec3 H = normalize( L + Vo );
	float NoH = clamp( dot( Ns, H ), 0.0, 1.0 ), LoH = clamp( dot( L, H ), 0.0, 1.0 );
	float d = NoH * NoH * ( a2 - 1.0 ) + 1.0;
	float D = a2 / ( kWaterPi * d * d );
	float vis = 0.5 / ( NoL * sqrt( NoV * NoV * ( 1.0 - a2 ) + a2 ) + NoV * sqrt( NoL * NoL * ( 1.0 - a2 ) + a2 ) );
	float c = 1.0 - LoH;
	float F = F0 + ( 1.0 - F0 ) * c * c * c * c * c;
	float g = kWaterPi * D * vis * F * NoL;
	if ( shoulder )
		g = g / ( 1.0 + g / waterGlintK.x );
	return sun * g;
}

float wwWaterCloudBand( vec3 P, vec3 R, float z, float outside, int ch )
{
	float t = ( z - P.z ) / R.z;
	if ( t <= 0.0 )
		return 0.0;
	vec2 uv = ( P.xy + t * R.xy - waterCloudGeo.xy ) / ( 2.0 * waterCloudGeo.z ) + 0.5;
	if ( any( lessThan( uv, vec2( 0.0 ) ) ) || any( greaterThan( uv, vec2( 1.0 ) ) ) )
		return outside;
	return texture( waterCloudMap, uv )[ch];
}

/* the reflected sky with the weather's clouds: the reflected ray meets each band's base over CLOUD1's map, as the
 * volume clouds read it. The volume clouds draw after the water (over the toned frame), so the march never sees them.
 * APPROXIMATE colour: a lit cloud at 1.5 x the brightest sky colour's luminance, x the weather's tint */
vec3 wwWaterCloudSky( vec3 sky, vec3 P, vec3 R )
{
	if ( waterCloudGeo.w < 0.5 || ( waterReflRed & 8 ) != 0 || R.z <= 0.0 )
		return sky;
	float cl = wwWaterCloudBand( P, R, waterCloudZ.x, waterCloudCov.x, 0 );
	float cm = wwWaterCloudBand( P, R, waterCloudZ.y, waterCloudCov.y, 1 );
	float ch = wwWaterCloudBand( P, R, waterCloudZ.z, waterCloudCov.z, 3 );
	float cov = 1.0 - ( 1.0 - cl ) * ( 1.0 - cm ) * ( 1.0 - ch );
	cov *= smoothstep( 0.0, 0.08, R.z );	// the far band crowds into the horizon line: let it go to the sky there
	const vec3 lumW = vec3( 0.2126, 0.7152, 0.0722 );
	float l = max( max( dot( waterSky[0].rgb, lumW ), dot( waterSky[1].rgb, lumW ) ), dot( waterSky[2].rgb, lumW ) );
	return mix( sky, waterCloudTint * ( 1.5 * l ), clamp( cov, 0.0, 1.0 ) );
}

// the reflected scene: the march where it is confident (sharp on calm water, its blur as the waves roughen), else the
// sky (the fallback off screen, at the edges and behind the camera; the march's confidence fades between them)
vec3 wwWaterReflEnv( vec3 sky, vec4 raw, vec4 fin, bool haveSsr, float alpha )
{
	if ( !haveSsr )
		return sky;
	float w = ( waterReflRed & 4 ) != 0 ? 0.9 : smoothstep( waterGlintK.w, 8.0 * waterGlintK.w, alpha );
	vec4 s = mix( raw, fin, w );
	return mix( sky, max( s.rgb, vec3( 0.0 ) ), clamp( s.a, 0.0, 1.0 ) );
}

/* the surface: the body seen through by 1 - F, the reflection by F, the same law near and far (the game swaps to one
 * WATR colour past 8192 u). Reds: record = the game's F0 and its reflectivity cap; farcolour = the game's far swap */
vec3 wwWaterReflMix( vec3 body, vec3 env, float F, float Fgame, float reflectivity, vec3 far, float dfade )
{
	vec3 c = ( waterReflRed & 1 ) != 0 ? mix( body, env, Fgame * reflectivity ) : mix( body, env, F );
	return ( waterReflRed & 2 ) != 0 ? mix( far, c, dfade ) : c;
}

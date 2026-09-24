p = r'E:\Projects\NifskopeWildWastelandEdition\res\shaders\pbrm_default.frag'
s = open(p, 'rb').read().decode()


def rep(old, new):
    global s
    assert s.count(old) == 1, old[:80]
    s = s.replace(old, new)


rep("""//   F0_ior  = ((ior-1)/(ior+1))^2        ior = constant, or the spec map's A x iorMax
//   F_diel  = weight * Schlick( tint * F0_ior, F90 = 1 )   (OpenPBR: weight 0 = no lobe)
//   F_metal = F82( base, tint )           (the editor's; tint 1 = plain Schlick)""",
"""//   F0_ior  = ((ior-1)/(ior+1))^2        ior = constant, or the spec map's A x iorMax
//   F_diel  = Schlick( F0' = weight * tint * F0_ior, F90' = sat( 50 F0' ) )
//             OpenPBR v1.1.1's IOR remap (lane PBRR4): the weight scales F0, so the
//             remapped IOR's Fresnel keeps its grazing rise; weight 0 = ior' 1 = no lobe
//   F_metal = weight * F82( base, tint )  (the editor's; tint 1 = plain Schlick)""")
rep("""// Not here: emission law, tint masks and opacity modes (R4), the editor's
// vegetation/parallax/surface states, coat/sheen/anisotropy (merge, s3.3).""",
"""//   tint    = max( 0, (1 - sum m) + sum c m ), base *= tint   (lane PBRR4, ED:2310-2316)
//   emit    = colour * mask * luminance/100, a map REPLACES the constant (ED:2519-2521)
//   opacity = the .pbrm composition (ED:2282-2283, 2336-2337, 2921)
// Not here: the editor's vegetation/parallax/surface states, coat/sheen/anisotropy,
// refraction (merge, s3.3).""")
rep("""uniform sampler2D SpecColorMap; // v6: sRGB RGB specular tint (bit 25), A = IOR / iorMax (bit 30)
""", """uniform sampler2D SpecColorMap; // v6: sRGB RGB specular tint (bit 25), A = IOR / iorMax (bit 30)
uniform sampler2D TintMaskMap;  // linear RGBA: the four tint masks (special layer 0)
""")
rep("""uniform vec3 pbrEmissiveColor;
uniform float pbrEmissiveIntensity;
""", """uniform vec3 pbrEmissiveColor;		// raw sRGB as authored; decoded pow 2.2 here (the editor's)
uniform float pbrEmissiveIntensity;	// luminance / 100
uniform float pbrEmissiveMask;		// the constant A mask
uniform bool pbrEmissiveMapColor;	// the map's RGB replaces the constant (!overrideColor)
uniform bool pbrEmissiveMapMask;	// the map's A replaces the constant mask (!overrideMask)

// Tint masks (lane PBRR4): the editor's special layer 0 (ED:2310-2316)
uniform bool pbrTintEnabled;
uniform int pbrTintUseTex;			// bit c: mask c comes from TintMaskMap
uniform vec4 pbrTintMasks;			// the constant masks
uniform vec3 pbrTintColors[4];		// raw sRGB (Q13: the editor does not decode them)
uniform int pbrTintMode;			// 0 Normalize, 1 Add, 2 Priority RGBA

// Composition (lane PBRR4): -1 = the NIF alpha property (derived/legacy shapes);
// 0 Opaque, 1 Alpha Test, 2 Alpha Blend, 3 Premultiplied, 4 Additive, 5 Multiply,
// 6 Physical Transmission, 7 Water (6/7 blend like 2 until the merge)
uniform int pbrComposition;
uniform float pbrGlobalOpacity;
uniform float pbrAlphaThreshold;
uniform bool pbrAlphaConst;			// alphaSource "Constant"
""")
rep("""uniform int r3Red;					// WW_R3_RED: 1 noms, 2 nosplit, 4 fo4csweight, 8 notint
""", """uniform int r3Red;					// WW_R3_RED: 1 noms, 2 nosplit, 4 fo4csweight, 8 notint, 16 f90scaled, 32 lambert
uniform int r4Red;					// WW_R4_RED: 1 nodiv, 2 emitmul, 4 notintmask, 8 emitraw, 16 nocomp
""")
rep("""// The whole specular Fresnel of the surface at cosine c
vec3 surfaceF( Surface s, float c )
{
	// OpenPBR: the weight scales the dielectric Fresnel, F90 included, so weight 0 has
	// no specular lobe. Red "fo4csweight": the runtime's F0-only weight (F90 stays 1).
	vec3 fd = ( ( r3Red & 4 ) != 0 ) ? fresnelSchlick( c, s.weight * s.f0d ) : s.weight * fresnelSchlick( c, s.f0d );
	return mix( fd, fresnelF82( c, s.base, s.tint ), s.metal );
}

// The specular F0 the multiscatter term takes (FO4CS: lerp(F0_diel, base, metal))
vec3 surfaceF0( Surface s )
{
	return mix( s.weight * s.f0d, s.base, s.metal );
}""", """/* The specular weight, OpenPBR v1.1.1 (bungo RULED 2026-09-24, lane PBRR4): the weight
 * remaps the IOR, F0' = clamp( w F0(ior) ), ior' = (1 + sqrt F0') / (1 - sqrt F0'), and the
 * dielectric Fresnel is the one of ior'. Its Schlick form keeps F90 = 1 for any real ior'
 * and must reach 0 at ior' = 1 (weight 0 = no lobe): F90' = sat( 50 F0' ), the 2% knee
 * below which no real dielectric sits. So weight 0.5 halves the head-on reflectance and
 * keeps the grazing rise (80 deg: 0.970 of weight 1 at ior 1.5). Reds: "fo4csweight" =
 * the runtime's F90 = 1 at every weight, "f90scaled" = lane PBRR3's w x Schlick (F90 scaled). */
vec3 dielF0( Surface s ) { return clamp( s.weight * s.f0d, 0.0, 1.0 ); }
vec3 dielF90( Surface s )
{
	if ( ( r3Red & 4 ) != 0 )
		return vec3( 1.0 );
	return clamp( 50.0 * dielF0( s ), 0.0, 1.0 );
}

// The whole specular Fresnel of the surface at cosine c
vec3 surfaceF( Surface s, float c )
{
	vec3 fd;
	if ( ( r3Red & 16 ) != 0 ) {
		fd = s.weight * fresnelSchlick( c, s.f0d );
	} else {
		vec3 F0 = dielF0( s );
		fd = F0 + ( dielF90( s ) - F0 ) * pow5( clamp( 1.0 - c, 0.0, 1.0 ) );
	}
	// metals: the weight scales the F82 conductor Fresnel (the specular colour is its edge tint)
	return mix( fd, s.weight * fresnelF82( c, s.base, s.tint ), s.metal );
}

// The specular F0 the multiscatter term takes (FO4CS: lerp(F0_diel, base, metal))
vec3 surfaceF0( Surface s )
{
	return mix( s.weight * s.f0d, s.weight * s.base, s.metal );
}""")
rep("""	vec3 diel = ( ( r3Red & 4 ) != 0 ) ? s.weight * s.f0d * dfg.x + dfg.y : s.weight * ( s.f0d * dfg.x + dfg.y );
	vec3 metl = s.base * dfg.x + dfg.y * s.tint;	// the editor's F82 split-sum: B x tint""",
"""	// the split sum of Schlick( F0, F90 ) is F0 A + F90 B
	vec3 diel = ( ( r3Red & 16 ) != 0 ) ? s.weight * ( s.f0d * dfg.x + dfg.y ) : dielF0( s ) * dfg.x + dielF90( s ) * dfg.y;
	vec3 metl = s.weight * ( s.base * dfg.x + dfg.y * s.tint );	// the editor's F82 split-sum: B x tint""")
rep("""		// Burley (FO4CS), with the specular energy removed
		float f90 = 0.5 + 2.0 * s.rough * LoH * LoH;
		float fd = ( 1.0 + ( f90 - 1.0 ) * pow5( 1.0 - NoL ) ) * ( 1.0 + ( f90 - 1.0 ) * pow5( 1.0 - NoV ) );""",
"""		// Burley, the game's (FO4CS wt-spec1 F4FX/Lighting/truepbr_brdf.hlsli:57-66, the
		// 1 - F keep :74-83), with the specular energy removed. Red "lambert": fd = 1.
		float f90 = 0.5 + 2.0 * s.rough * LoH * LoH;
		float fd = ( 1.0 + ( f90 - 1.0 ) * pow5( 1.0 - NoL ) ) * ( 1.0 + ( f90 - 1.0 ) * pow5( 1.0 - NoV ) );
		if ( ( r3Red & 32 ) != 0 )
			fd = 1.0;""")
rep("""		if ( hasFeature( F_OPACITY ) )
			op = bc.a * pbrOpacity;
	}
	base *= C.rgb;

	vec4 color = vec4( base, 1.0 );
	if ( alphaFlags > 0 ) {
		float a = C.a * op * alpha;
		int m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}
""", """		if ( hasFeature( F_OPACITY ) )
			op = ( pbrComposition >= 0 ) ? bc.a : bc.a * pbrOpacity;	// the editor's baseAlpha replaces (ED:2258)
	}

	// --- tint masks (ED:2310-2316, the law verbatim) ---
	if ( pbrTintEnabled && ( r4Red & 4 ) == 0 ) {
		vec4 masks = pbrTintMasks;
		if ( pbrTintUseTex != 0 ) {
			vec4 tm = texture( TintMaskMap, offset );
			if ( ( pbrTintUseTex & 1 ) != 0 ) masks.r = tm.r;
			if ( ( pbrTintUseTex & 2 ) != 0 ) masks.g = tm.g;
			if ( ( pbrTintUseTex & 4 ) != 0 ) masks.b = tm.b;
			if ( ( pbrTintUseTex & 8 ) != 0 ) masks.a = tm.a;
		}
		float total = dot( masks, vec4( 1.0 ) );
		if ( pbrTintMode == 0 && total > 1.0 && ( r4Red & 1 ) == 0 )
			masks /= total;
		else if ( pbrTintMode == 2 )
			masks = vec4( masks.r, masks.g * ( 1.0 - masks.r ), masks.b * ( 1.0 - masks.r ) * ( 1.0 - masks.g ),
				masks.a * ( 1.0 - masks.r ) * ( 1.0 - masks.g ) * ( 1.0 - masks.b ) );
		vec3 tintMix = max( vec3( 0.0 ), vec3( 1.0 - dot( masks, vec4( 1.0 ) ) ) + pbrTintColors[0] * masks.r
			+ pbrTintColors[1] * masks.g + pbrTintColors[2] * masks.b + pbrTintColors[3] * masks.a );
		base *= tintMix;
	}
	base *= C.rgb;

	vec4 color = vec4( base, 1.0 );
	if ( pbrComposition >= 0 && ( r4Red & 16 ) == 0 ) {
		// the .pbrm composition (ED:2282-2283, 2336-2337); vertex alpha is not read (the editor's)
		float opacity = ( pbrAlphaConst ? 1.0 : op ) * pbrGlobalOpacity;
		if ( pbrComposition == 0 )
			opacity = 1.0;
		else if ( pbrComposition == 1 ) {
			if ( opacity < pbrAlphaThreshold )
				discard;
			opacity = 1.0;
		}
		color.a = opacity;
	} else if ( alphaFlags > 0 ) {
		float a = C.a * op * alpha;
		int m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}
""")
rep("""	color.rgb = ( r3Term == 1 ) ? outDiff : ( r3Term == 2 ) ? outSpec : outDiff + outSpec;

	// --- emissive ---
	if ( pbrEmissiveIntensity > 0.0 && r3Term == 0 ) {
		vec3 emissive = pbrEmissiveColor * pbrEmissiveIntensity;
		if ( hasFeature( F_EMISSIVE ) ) {
			vec4 em = texture( EmissiveMap, offset );
			emissive *= ( emissiveIsSrgbTex ? em.rgb : srgbToLinear( em.rgb ) ) * em.a;
		}
		color.rgb += emissive * glowScaleSRGB;
	}
""", """	color.rgb = ( r3Term == 1 ) ? outDiff : ( r3Term == 2 ) ? outSpec : outDiff + outSpec;
	if ( r3Term == 3 )	// gate s1b: the surface Fresnel at N.V
		color.rgb = surfaceF( s, NdotV );

	// --- emission (ED:2519-2521): colour x mask x luminance/100, added before exposure,
	// never the legacy glow scale. A map REPLACES the constant colour / mask unless that
	// channel is overridden. Red "emitmul": the old constant x map law; "emitraw": no /100.
	if ( pbrEmissiveIntensity > 0.0 && r3Term == 0 ) {
		vec3 ecol = pow( max( pbrEmissiveColor, vec3( 0.0 ) ), vec3( 2.2 ) );
		float emask = pbrEmissiveMask;
		if ( hasFeature( F_EMISSIVE ) ) {
			vec4 em = texture( EmissiveMap, offset );
			vec3 emap = emissiveIsSrgbTex ? em.rgb : srgbToLinear( em.rgb );
			if ( ( r4Red & 2 ) != 0 ) {
				ecol *= emap;
				emask *= em.a;
			} else {
				if ( pbrEmissiveMapColor ) ecol = emap;
				if ( pbrEmissiveMapMask ) emask = em.a;
			}
		}
		color.rgb += ecol * emask * pbrEmissiveIntensity * ( ( r4Red & 8 ) != 0 ? 100.0 : 1.0 );
	}
""")
rep("""		color.rgb = tonemap( sqrt( max( color.rgb, vec3( 0.0 ) ) ) );
	}
""", """		color.rgb = tonemap( sqrt( max( color.rgb, vec3( 0.0 ) ) ) );
	}
	// Premultiplied: the mapped colour times opacity (ED:2921), blended ONE, 1 - SRC_ALPHA
	if ( pbrComposition == 3 && ( r4Red & 16 ) == 0 )
		color.rgb *= color.a;
""")
open(p, 'wb').write(s.encode())
print("frag ok")

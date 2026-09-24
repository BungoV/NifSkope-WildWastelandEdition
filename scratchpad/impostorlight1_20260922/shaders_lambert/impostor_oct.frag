#version 410 core

/* ---------------------------------------------------------------------------
 * The octahedral impostor card, fragment stage. Lane IMPOSTORSHOW, 2026-09-19.
 *
 * This is the drawing half of docs/LODGEN_IMPOSTOR_SPEC.md and every clause it
 * implements is named where it is implemented. The frame SELECTION arithmetic
 * is not here -- it is in src/impostoroct.cpp, where a gate can compile it on
 * its own and diff it against a Python reference. What is here is what only a
 * pixel can do: the three frames' disagreement about where the surface is, and
 * the height channel that settles it.
 *
 * WHAT THIS SHADER DOES NOT DO, and will not until bungo says so.
 * The material sheets -- the mask (GSAOS / RMAOS) and the emissive -- are
 * LOADED, VALIDATED and SHOWN AS DEBUG CHANNELS, and they are not shaded.
 * There is no gloss term, no specular term, no metal, no subsurface and no
 * emission added to the lit result, because a legacy impostor must be lit the
 * way Fallout 4 lights it and a pbr one the way FO4CS's PBRM does, and neither
 * renderer exists in NifSkope yet (bungo, 2026-09-19 05:4x). Writing a
 * plausible-looking stand-in here would be the exact thing that makes the real
 * one impossible to land later, because every picture taken in between would
 * have to be retaken and nobody would know which ones.
 *
 * THE SEAM where that renderer arrives is ONE place and it is named:
 * `IMPOSTOR_MATERIAL_SEAM` below. It already has the four sheets unpacked into
 * named locals, in the units the spec says they are in. A renderer takes them
 * and returns a colour. Nothing else in this file has to move.
 *
 * DEPTH AND AO ARE USED, NOT ONLY SHOWN (bungo, 2026-09-19 05:5x). Neither
 * needs a material model: height is geometry and AO is a scalar on the ambient
 * term. So the card parallaxes, writes a real gl_FragDepth and therefore
 * INTERSECTS the world as a volume, blends its three frames ghost-free, and
 * multiplies the viewer's own ambient by the baked AO.
 * ------------------------------------------------------------------------- */

#include "uniforms.glsl"

uniform mat4 modelViewMatrix;      // the same one the vertex stage used

// --- the four sheets of spec 42..48 -----------------------------------------
uniform sampler2D ColourSheet;     // _d / _bc : RGB colour (unlit), A coverage
uniform sampler2D NormalSheet;     // _n       : R nX, G nY, B height, A sway
uniform sampler2D MaskSheet;       // _gsaos / _rmaos : R, G, B AO, A subsurface
uniform sampler2D EmissiveSheet;   // _g / _e  : RGB emissive colour, BC1

uniform bool  hasMaskSheet;
uniform bool  hasEmissiveSheet;
uniform bool  familyPbr;           // false = legacy (gloss/specular), true = pbr
uniform float emissiveScale;       // spec 88..95 -- carried, never folded in

// --- the frames this pixel blends (chosen CPU-side, src/impostoroct.cpp) -----
// Each frame k carries its rect on the sheet, its weight, and its own camera
// basis -- three DIFFERENT view spaces, which is the whole difficulty.
uniform vec4  frameRect[3];        // u0, v0, du, dv
uniform float frameWeight[3];
uniform vec3  frameRight[3];
uniform vec3  frameUp[3];
uniform vec3  frameFwd[3];
uniform int   frameCount;          // 3 normally; 1 when blending is off

/* PER-FRAME POSITIONING: where frame k's quad SITS, relative to `cardCenter`,
 * along that frame's own right and up (`card.frameOffset`, lodgen.cpp:2999).
 * The bake slides each view's silhouette to its own frame's centre so a frame
 * does not have to pay for the union of every view's box; this slides it back.
 * Zero for a set baked before the law, which is exactly that older behaviour. */
uniform vec2  frameOffset[3];

// --- the card ---------------------------------------------------------------
uniform vec3  cardCenter;
uniform vec2  cardHalf;
uniform float cardDepthSpan;       // spec 285: 3 x max(bound radius, 1024)
uniform float cardMipCap;          // spec 243: mips stop at 8 texels a side

/* THE COVERAGE CONTRACT, as the SET declares it, in 0..1 (lodgen.cpp:3031).
 * `cardCovBase <= 0` means the set declares none: the alpha is already the
 * fraction and `coverageOf` returns it floored, which is the older rule at
 * lodgenaggregate.cpp:117. */
uniform float cardCovFloor;
uniform float cardCovBase;

/*! The colour sheet's alpha -> the coverage FRACTION the bake measured.
 *
 *  The exact inverse of `coverageEncode` (lodgenaggregate.cpp:122..129), and
 *  the same line the manifest note states (lodgen.cpp:3033):
 *      cov = floor + (a - base) x (255 - floor) / (255 - base)
 *  written in 0..1. Below `base` there is no coverage to recover -- the bake
 *  wrote 0 there -- so it returns 0 rather than a negative extrapolation.
 *
 *  WHY IT MATTERS HERE AND NOT ONLY AT THE TEST: the blend below averages
 *  these numbers. Averaging encoded alphas is averaging positions on a curve,
 *  not quantities, and the result is not the coverage of anything.
 */
float coverageOf( float a )
{
	if ( cardCovBase <= 0.0 )
		return a < ( 16.0 / 255.0 ) ? 0.0 : a;
	if ( a < cardCovBase )
		return 0.0;
	return clamp( cardCovFloor + ( a - cardCovBase ) * ( 1.0 - cardCovFloor )
			/ ( 1.0 - cardCovBase ), cardCovFloor, 1.0 );
}

// --- what the viewer is asking for ------------------------------------------
uniform float alphaThreshold;      // default 128/255, vanilla's LOD alpha test (IMPOSTORFIN1)
uniform bool  useHeightBlend;      // the ghost-free blend, spec 286
uniform bool  useDepthOffset;      // gl_FragDepth, spec 286
uniform bool  useBakedAo;          // spec 293..295
uniform int   debugChannel;        // 0 = lit; otherwise one sheet channel raw

/* SWAY, AND WHAT A CARD CAN HONESTLY SHOW OF IT (spec 288..290).
 *
 * The sway weight is h^2 x (0.35 + 0.65 r) per TEXEL, and the law it feeds is
 * the chunk builder's, applied to VERTICES. A card is four vertices, so there
 * is nothing to displace: the weight varies across the quad and the quad does
 * not.
 *
 * What a card CAN do is shear its own sampling -- move the texel it reads
 * sideways in proportion to that texel's sway weight -- which makes the crown
 * lean while the trunk stands and is what the eye reads as wind. It is an
 * APPROXIMATION OF THE LAW, not the law: the real thing moves geometry and
 * this moves a lookup, so a silhouette swayed this way is clipped by the
 * unswayed quad at the extremes. It is marked here so no picture taken with it
 * is later quoted as "the sway bake, drawn".
 *
 * `swayAmplitude` 0 is still, and still is the default: a moving picture is
 * not evidence of anything and every gate runs at 0.
 */
uniform float swayAmplitude;
uniform float swayPhase;           // radians; the harness steps it, not a clock

in vec3 cardPosModel;
in vec2 cardQuadUv;
in vec3 cardViewRay;

out vec4 fragColor;

/* Where a point on (or near) the card lands inside frame k.
 *
 * The frame is an ORTHOGRAPHIC photograph along frameFwd[k], so a point's
 * place in it is just its coordinates in that frame's own basis, divided by
 * the card's extents. `d` comes back as the point's signed distance along the
 * frame's view direction, which is what the height channel is measured in. */
vec2 frameUvOf( int k, vec3 pModel, out float d )
{
	vec3 r = pModel - cardCenter;
	/* ...measured from where THIS FRAME'S QUAD SITS, not from the card's one
	 * centre. Subtracting the frame's own offset is what undoes the slide the
	 * bake applied when it packed this view; skipping it puts every frame's
	 * picture in the wrong place by up to 13% of a frame, and puts the THREE
	 * blended frames in three different wrong places, which does not blur the
	 * card, it disperses it. See `ImpostorCardSet::frameOffset`. */
	vec2 st = vec2( dot( r, frameRight[k] ) - frameOffset[k].x,
	                dot( r, frameUp[k] )    - frameOffset[k].y );
	d = dot( r, frameFwd[k] );
	// -half..+half  ->  0..1, y up in the world and v down on the sheet
	vec2 uv = vec2( st.x / ( 2.0 * cardHalf.x ) + 0.5,
	                0.5 - st.y / ( 2.0 * cardHalf.y ) );
	return frameRect[k].xy + clamp( uv, 0.0, 1.0 ) * frameRect[k].zw;
}

/* CLAUSE C5 (spec 49) and SPEC GAP #6: unpack the frame's half-packed normal,
 * rebuild Z as the positive root, and rotate it out of THIS frame's view space
 * into model space BEFORE it is blended with the others. */
vec3 frameNormalModel( int k, vec4 n )
{
	vec2 xy = n.xy * 2.0 - 1.0;
	float z = sqrt( max( 0.0, 1.0 - dot( xy, xy ) ) );
	return normalize( frameRight[k] * xy.x + frameUp[k] * xy.y + frameFwd[k] * z );
}

void main()
{
	/* ------------------------------------------------------------------
	 * 1. THE GHOST-FREE BLEND (spec 286: height is "used for ... ghost-free
	 *    frame blending", and then the spec stops -- SPEC GAP #5).
	 *
	 * The three frames photographed the object from three directions. Sampled
	 * at the card plane they disagree about where the surface is by up to the
	 * object's own depth, and averaging that disagreement is exactly the
	 * double-edged ghost everyone sees on a blended impostor.
	 *
	 * The height channel is the cure and it is already baked: 0.5 is the card
	 * plane and (h - 0.5) x depthSpan is the offset along the frame's view
	 * direction (spec 284..286). So each frame is asked where ITS surface is,
	 * the fragment's point is moved onto that surface along the VIEW RAY, and
	 * the frame is resampled there. All three then agree on a point in space
	 * before any of them is averaged.
	 *
	 * One iteration. A second buys almost nothing at LOD distance and doubles
	 * the sheet reads, which is the one budget a LOD path has.
	 * ------------------------------------------------------------------ */
	vec4  colour   = vec4( 0.0 );
	vec3  normal   = vec3( 0.0 );
	vec4  mask     = vec4( 0.0 );
	vec3  emissive = vec3( 0.0 );
	float height   = 0.0;
	float sway     = 0.0;
	float wsum     = 0.0;
	float depthOffset = 0.0;

	vec3 ray = normalize( cardViewRay );

	for ( int k = 0; k < 3; k++ ) {
		if ( k >= frameCount )
			break;
		float w = frameWeight[k];
		if ( w <= 0.0 )
			continue;

		float d;
		vec2 uv = frameUvOf( k, cardPosModel, d );

		vec3 p = cardPosModel;
		if ( useHeightBlend ) {
			/* Where frame k says the surface is, in world units along its own
			 * view direction, and the point on the view ray that reaches it. */
			float h = textureLod( NormalSheet, uv, 0.0 ).b;
			/* THE SIGN, and it is SPEC GAP #4 -- spec 284..286 gives the height
			 * channel a unit and never says which way it points, so it had to be
			 * derived, and this lane derived it backwards first.
			 *
			 * `d` is measured along `frameFwd[k]`, and frameFwd is row 2 of the
			 * bake's Euler matrix: it points from the object TOWARD the camera
			 * (src/impostoroct.cpp:290). So a point in FRONT of the card plane
			 * has d > 0. The baked height runs the OTHER WAY: the bake writes
			 * `gl_FragCoord.z` (res/shaders/fo4_default.frag:310), which grows
			 * with distance FROM the camera, so h > 0.5 is BEHIND the plane.
			 * The surface frame k reports therefore sits at
			 *     d = -( h - 0.5 ) x depthSpan
			 * and the leading minus is the entire correction.
			 *
			 * WHAT THE WRONG SIGN DID, so nobody restores it: a backwards
			 * parallax does not merely fail to remove the frames' disagreement,
			 * it DOUBLES it -- every sample lands as far on the wrong side as the
			 * right one lay on the correct side -- and the card came apart into a
			 * wide sparse spray. Measured on TreeMapleblasted05 N=4, azim 45
			 * elev 15, silhouette IoU against the mesh at five apparent sizes:
			 *     256 px  0.1675 wrong / 0.3114 blend OFF
			 *     128 px  0.1762 wrong / 0.3135 blend OFF
			 *      64 px  0.2112 wrong / 0.3151 blend OFF
			 *      32 px  0.2087 wrong / 0.2917 blend OFF
			 *      16 px  0.1786 wrong / 0.3750 blend OFF
			 * That is the signature, and it is the refuter this sign is checked
			 * by: a correction applied the right way round cannot be WORSE than
			 * not applying it at all. The pixel depth offset in section 2 has the
			 * opposite-looking sign for the same reason -- it moves along `ray`,
			 * which points AWAY from the camera, so there the plus is correct. */
			float want = -( h - 0.5 ) * cardDepthSpan;
			float denom = dot( ray, frameFwd[k] );
			/* A grazing frame gives a denominator near zero and a correction
			 * that would fly off the card. Its weight is small there anyway
			 * (a grazing frame is a corner of the blend triangle), so the
			 * correction is simply skipped rather than clamped to a number
			 * that would be a guess. */
			if ( abs( denom ) > 0.15 ) {
				p = cardPosModel + ray * ( ( want - d ) / denom );
				uv = frameUvOf( k, p, d );
			}
		}

		/* The sway shear, if the viewer asked for one: read this texel's own
		 * weight, then move the lookup sideways in the frame's x by that much.
		 * Clamped to the frame's rect so a shear can never read a neighbour's
		 * frame -- the gutter is four texels (spec 239) and a wind shear is
		 * bigger than four texels. */
		if ( swayAmplitude != 0.0 ) {
			float sw = textureLod( NormalSheet, uv, 0.0 ).a;
			float dx = sw * swayAmplitude * sin( swayPhase ) * frameRect[k].z;
			uv.x = clamp( uv.x + dx, frameRect[k].x, frameRect[k].x + frameRect[k].z );
		}

		/* CLAUSE C6 / C28 (spec 49, 317..321): coverage is the cut-out and it
		 * lives on the COLOUR sheet and nowhere else, and after the bake's
		 * downsample it is a FRACTION, not a cut-out. */
		vec4 c = textureLod( ColourSheet, uv, 0.0 );
		vec4 n = textureLod( NormalSheet, uv, 0.0 );

		/* DECODED FIRST. `c.a` is the encoded alpha, not the coverage: see
		 * `coverageOf` above and `ImpostorCardSet::covFloor`. Everything below
		 * -- the weight, the average, the test -- is the FRACTION. */
		float cov = coverageOf( c.a );

		/* The weight is the blend weight TIMES this frame's coverage, and the
		 * sum is normalised at the end. Without the coverage factor a frame
		 * that sees sky through a gap drags the other two towards transparent
		 * black, which is the second half of the ghosting. */
		float wc = w * cov;

		colour.rgb += c.rgb * wc;
		colour.a   += cov * w;          // coverage itself is a plain average
		normal     += frameNormalModel( k, n ) * wc;
		height     += n.b * wc;
		sway       += n.a * wc;
		if ( hasMaskSheet )
			mask += textureLod( MaskSheet, uv, 0.0 ) * wc;
		if ( hasEmissiveSheet )
			emissive += textureLod( EmissiveSheet, uv, 0.0 ).rgb * wc;
		wsum += wc;
	}

	if ( wsum > 1e-5 ) {
		colour.rgb /= wsum;
		normal     /= wsum;
		height     /= wsum;
		sway       /= wsum;
		mask       /= wsum;
		emissive   /= wsum;
	}
	normal = ( dot( normal, normal ) > 1e-8 ) ? normalize( normal ) : frameFwd[0];

	/* THE DEFAULT IS VANILLA'S (lane IMPOSTORFIN1, 2026-09-22): the viewer
	 * sends 128/255, the threshold on every alpha-tested shape of every
	 * vanilla .BTO chunk (flags 0x12EC: a hard GREATER test, never a blend),
	 * and the colour is then written opaque below -- the same hard cut-out.
	 *
	 * CLAUSE C28. The spec gives 0.5 for full crowns and says a consumer
	 * "tests lower, or blends, for bare trees" without naming the number --
	 * SPEC GAP #7. The number is the viewer's, not this shader's: it arrives
	 * as `alphaThreshold` so a picture can show the same card at both and
	 * bungo can say which one a bare Commonwealth maple wants. */
	if ( colour.a < alphaThreshold )
		discard;

	/* ------------------------------------------------------------------
	 * 2. THE PIXEL DEPTH OFFSET (spec 286, SPEC GAP #4 on its sign).
	 *
	 * Without this a card is a flat sheet of paper: it intersects the ground
	 * in a straight line and slices through anything it overlaps. With it the
	 * card occupies the depth its own height channel says it does, so a tree
	 * standing half behind a wall is half behind the wall.
	 *
	 * The offset is along the VIEW RAY, away from the camera, which is the
	 * sign the spec should state and does not.
	 * ------------------------------------------------------------------ */
	if ( useDepthOffset ) {
		depthOffset = ( height - 0.5 ) * cardDepthSpan;
		vec3 pDepth = cardPosModel + ray * depthOffset;
		vec4 clip = projectionMatrix * ( modelViewMatrix * vec4( pDepth, 1.0 ) );
		gl_FragDepth = ( clip.z / clip.w ) * 0.5 + 0.5;
	} else {
		gl_FragDepth = gl_FragCoord.z;
	}

	/* ------------------------------------------------------------------
	 * 3. DEBUG CHANNELS. Every sheet channel on its own, unshaded, because a
	 *    sheet that is loaded and never looked at is a sheet nobody can say is
	 *    right. These are the ONLY thing the mask and emissive sheets feed
	 *    until the material renderer lands.
	 * ------------------------------------------------------------------ */
	if ( debugChannel != 0 ) {
		vec3 dbg = vec3( 0.0 );
		if      ( debugChannel == 1 ) dbg = colour.rgb;                 // colour
		else if ( debugChannel == 2 ) dbg = vec3( colour.a );           // coverage
		else if ( debugChannel == 3 ) dbg = normal * 0.5 + 0.5;         // normal, model space
		else if ( debugChannel == 4 ) dbg = vec3( height );             // height, raw 0..1
		else if ( debugChannel == 5 ) dbg = vec3( sway );               // sway weight
		else if ( debugChannel == 6 ) dbg = vec3( mask.r );             // gloss / roughness
		else if ( debugChannel == 7 ) dbg = vec3( mask.g );             // specular / metallic
		else if ( debugChannel == 8 ) dbg = vec3( mask.b );             // AO
		else if ( debugChannel == 9 ) dbg = vec3( mask.a );             // subsurface mask
		else if ( debugChannel == 10 ) dbg = emissive * emissiveScale;  // emissive x its multiple
		else if ( debugChannel == 11 ) {
			/* Which of the three frames won here, as a colour. The one picture
			 * that shows the frame grid actually turning. */
			dbg = vec3( frameWeight[0], frameWeight[1], frameWeight[2] );
		} else if ( debugChannel == 12 ) {
			// the depth offset in card-depth units, signed, red back / blue front
			float t = clamp( depthOffset / max( cardDepthSpan, 1.0 ) * 2.0, -1.0, 1.0 );
			dbg = vec3( max( t, 0.0 ), 0.0, max( -t, 0.0 ) );
		}
		fragColor = vec4( dbg, 1.0 );
		return;
	}

	/* ------------------------------------------------------------------
	 * 4. IMPOSTOR_MATERIAL_SEAM -- the one place the future renderer plugs in.
	 *
	 * Everything the two families carry is unpacked and in its own unit right
	 * here:
	 *
	 *     colour.rgb   the unlit colour, sRGB like its source (spec 51)
	 *     normal       the blended geometric normal, MODEL space, unit
	 *     mask.r       legacy: gloss          pbr: roughness
	 *     mask.g       legacy: specular       pbr: metallic
	 *     mask.b       ambient occlusion      (used below, no material needed)
	 *     mask.a       subsurface mask        (a material label, spec 296)
	 *     emissive     the emissive colour; the MULTIPLE is `emissiveScale`
	 *     familyPbr    which of the two spellings the above are in
	 *
	 * A Fallout 4 legacy path takes the first five; an FO4CS PBRM path takes
	 * the pbr spelling. Until one of them exists this stays what it is: the
	 * viewer's own diffuse lighting off the baked normal, which is honest
	 * about being the viewer's and cannot be mistaken for either game's.
	 * ------------------------------------------------------------------ */
	/* THE LIGHT TERMS ARE THE MESH PATH'S, NOT THE RAW UNIFORMS (lane
	 * IMPOSTORLOOK1, 2026-09-19). `fo4_default.vert` hands its fragment stage
	 * `A = vec4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale )` and
	 * `D = vec4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale )` -- the
	 * square roots are the FO4 path's gamma convention and the 0.375 is its
	 * ambient scale. This shader used the uniforms raw, so a card and the mesh
	 * it replaces were lit by two different lights in one framebuffer.
	 *
	 * AND THE SAME SPACE (lane IMPOSTORLIGHT1, 2026-09-22). `normal` is MODEL
	 * space (frameNormalModel above) and `lightSourcePosition` is VIEW space --
	 * the mesh path lights `n * normalMatrix` with `normalMatrix` the view
	 * rotation (glshape.cpp:658, fo4_default.vert:46,59). The old loop dotted
	 * the two raw, so under the headlight a card was lit by its model-space UP
	 * component: crown tops bright, the trunk faces that point at the camera --
	 * the mesh's brightest pixels -- dark. MEASURED on the five cardRes-512
	 * subjects at their bake directions: that normal agreed with the mesh's in
	 * sign 41..51 % on x and y (a coin toss), mean angle 69..88 deg; the same
	 * normal taken to view space agrees 80..99 %, 5..22 deg. The diffuse rung of
	 * the brightness ladder was where card/mesh fell from 0.97..1.01 (albedo,
	 * ambient) to 0.49..0.91.
	 *
	 * ONE-SIDED, like the mesh. The old `abs( dot )` lit the side facing AWAY
	 * from the light as brightly as the lit side. The mesh never does that, not
	 * even on a two-sided material: fo4_default.frag:404 flips a back face's
	 * normal to face the VIEWER and then clamps N.L at zero (:422), and the
	 * bake photographs exactly that flipped normal (channel 8, :305). So the
	 * baked normal already carries the two-sided flag's whole effect, and the
	 * clamp below is right for every material. Vanilla's own flag (the BGSM's
	 * bTwoSided, read by scratchpad/impostorlight1_20260922/material_flags.py):
	 * all three trees 1 (every shape, the trunk included -- one atlas material),
	 * the rock 0; none of the five has back-lighting or subsurface set, so no
	 * light-through term is owed either.
	 *
	 * The mesh path lights with light 0 only (fo4_default.vert:59,63), and so
	 * does this; its diffuse is Oren-Nayar at roughness 1 - smoothness times
	 * (1 - Fresnel), and the card carries the smoothness in the mask sheet. */
	mat3  toView = mat3( modelViewMatrix );
	vec3  nView  = normalize( toView * normal );
	vec3  V = ( projectionMatrix[3][3] == 1.0 ) ? vec3( 0.0, 0.0, 1.0 )
	        : normalize( -( modelViewMatrix * vec4( cardPosModel, 1.0 ) ).xyz );
	float smoothness = hasMaskSheet ? clamp( familyPbr ? 1.0 - mask.r : mask.r, 0.0, 1.0 ) : 0.0;
	vec3  lit = vec3( 0.0 );
	vec3  L = lightSourcePosition[0].xyz;
	if ( dot( L, L ) > 1e-8 ) {
		L = normalize( L );
		float NdotL = dot( nView, L );
		float diff = max( NdotL, 0.0 );
		lit = sqrt( max( lightSourceDiffuse[0].rgb, vec3( 0.0 ) ) ) * diff;
	}

	/* CLAUSE C25 (spec 293..295): the AO the bake measured from the height
	 * neighbourhood, multiplied into the AMBIENT term only -- never into the
	 * direct term, where it would double-count the shading the light already
	 * does. No material model is needed for this, which is why it is here and
	 * the gloss beside it is not. */
	float ao = ( useBakedAo && hasMaskSheet ) ? mask.b : 1.0;
	vec3 ambient = sqrt( max( lightSourceAmbient.rgb, vec3( 0.0 ) ) ) * 0.375 * ao;

	/* AND THE TONEMAP, which this shader was the only one in the tree without.
	 * Verbatim `fo4_default.frag`'s curve, with `A.a` = toneMapScale and
	 * `D.a` = brightnessScale, which is what its vertex stage packs there. */
	vec3 lc = colour.rgb * ( lit + ambient );
	{
		const float a = 0.15, b = 0.50, c = 0.10, d = 0.20, e = 0.02, f = 0.30;
		vec3 z = lc * lc * brightnessScale * ( toneMapScale * 4.22978723 );
		z = ( z * ( a * z + b * c ) + d * e ) / ( z * ( a * z + b ) + d * f ) - e / f;
		lc = sqrt( z / max( toneMapScale * 0.93333333, 1e-6 ) );
	}
	fragColor = vec4( lc, 1.0 );
}

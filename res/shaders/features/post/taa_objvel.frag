#version 410 core

/* lane TAAINPUTS1: the object vector, previous minus current in texture units (game_taa_mv.frag's convention),
 * and the own-motion flag in the mask's B. The alpha test and the grass fade drop what the frame dropped. */

uniform sampler2D BaseMap;
uniform vec2 uvScale;
uniform vec2 uvOffset;
uniform int alphaTest;
uniform float alpha;
uniform float alphaThreshold;
uniform vec3 wwGrassFade;
/* lane TREELEAF3 (src/gl/wwfxcov.h, its GLSL mirror): 0 = today's test, base.a x alpha; 4 = an effect shape's composed
 * alpha as fo4_effectshader.frag tests it (alpha = baseColor.a^2), + 1 x the vertex alpha, + 2 the palette's alpha
 * at (base.a, the rest); lane TREELEAF4 (FXCOVRESID1): + 8 x the falloff (the normal-map normal), + 16 x the cell's
 * soft fade (the opaque pass's depth): fo4_effectshader.frag's own lines, in its order */
uniform int fxMode;
uniform sampler2D GreyscaleMap;
uniform sampler2D NormalMap;
uniform vec4 falloffParams;		// start angle, stop angle, start opacity, stop opacity (the colour pass's uniform)
uniform bool fxOrtho;			// the colour pass's projectionMatrix[3][3] == 1 (ViewDir = +z)
uniform sampler2D fxDepth;		// Scene::fxDepthTexId: the opaque pass's depth, the viewport's corner at texel 0
uniform ivec2 fxDepthOrigin;	// the colour pass's viewportDimensions.xy
uniform vec2 fxProj;			// the colour pass's projectionMatrix[2][2], [3][2]
uniform float fxDistScale;		// view units -> game units (celllights.cpp)
uniform float fxSoftDepth;		// game units
/* lane TREELEAF4 (TAABLENDMOVER1): a BLENDED mover draws twice. covMode 1: the object vector only where its composed
 * alpha >= covThreshold; covMode 2: the rest of its triangles, into the mask's R (reactive) -- the offline gate
 * (tests/treeleaf4/blendmover_gate.py, the real taa2.frag) measured the background behind a moving card 15-65 % closer
 * to the reference than the whole-card vector, the card itself within 2 %. 0 = today's (every covered pixel). */
uniform int covMode;
uniform float covThreshold;

in vec4 cCur;
in vec4 cPrev;
in vec3 viewPos;
in vec2 texCoord;
in float vAlpha;
in mat3 btnMatrix;

layout ( location = 0 ) out vec2 vel;
layout ( location = 1 ) out vec4 flag;

// fo4_effectshader.frag's falloff (its normal, V and two lines, unchanged); the back-face flip leaves |N.V| as it is
float fxFalloff()
{
	vec3 normal = normalize( texture( NormalMap, texCoord * uvScale + uvOffset ).rgb * 2.0 - 1.0 );
	normal.b = sqrt( max( 1.0 - dot( normal.rg, normal.rg ), 0.0 ) );
	mat3 btn = mat3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );
	normal = normalize( btn * normal );
	vec3 V = normalize( fxOrtho ? vec3( 0.0, 0.0, 1.0 ) : -viewPos );
	float falloff = smoothstep( falloffParams.x, falloffParams.y, abs( dot( normal, V ) ) );
	return mix( max( falloffParams.z, 0.0 ), min( falloffParams.w, 1.0 ), falloff );
}

// fo4_effectshader.frag cellFxFade(), line for line
float fxSoftFade()
{
	float d = texelFetch( fxDepth, ivec2( gl_FragCoord.xy ) - fxDepthOrigin, 0 ).r;
	float sceneW = fxProj.y / ( fxProj.x + ( 2.0 * d - 1.0 ) );
	float k = fxDistScale / fxSoftDepth;
	float fragW = 1.0 / gl_FragCoord.w;
	float soft = clamp( ( sceneW - fragW ) * k, 0.0, 1.0 );
	float t = clamp( ( fragW * k - 0.075 ) * 2.352941, 0.0, 1.0 );
	return soft * t * t * ( 3.0 - 2.0 * t );
}

void main()
{
	if ( wwGrassFade.z > 0.5 && length( viewPos ) - wwGrassFade.x >= max( wwGrassFade.y, 1e-3 ) )
		discard;
	if ( alphaTest != 0 || covMode != 0 ) {
		float a = texture( BaseMap, texCoord * uvScale + uvOffset ).a;
		if ( fxMode != 0 ) {
			float k = ( fxMode & 1 ) != 0 ? vAlpha : 1.0;
			float f = ( fxMode & 8 ) != 0 ? fxFalloff() : 1.0;
			float s = ( fxMode & 16 ) != 0 ? fxSoftFade() : 1.0;
			a = ( fxMode & 2 ) != 0 ? textureLod( GreyscaleMap, vec2( a, ( ( 1.0 * f ) * k ) * alpha * s ), 0.0 ).a
				: ( ( a * f ) * k ) * alpha * s;
		} else {
			a *= alpha;
		}
		// the reactive draw covers the texels the colour pass drops too (the card shows nothing there either)
		if ( alphaTest != 0 && covMode != 2 && a < alphaThreshold )
			discard;
		if ( covMode == 1 && a < covThreshold )
			discard;
		if ( covMode == 2 && !( a < covThreshold ) )
			discard;
	}
	if ( cCur.w <= 0.0 || cPrev.w <= 0.0 ) {
		vel = vec2( 0.0 );	// behind the eye last frame: no usable history
	} else {
		vel = ( cPrev.xy / cPrev.w - cCur.xy / cCur.w ) * 0.5;
	}
	flag = vec4( covMode == 2 ? 1.0 : 0.0, 0.0, 1.0, 0.0 );
}

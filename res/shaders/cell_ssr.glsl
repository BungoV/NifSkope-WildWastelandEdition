// lane SSR1: the screen-space reflection a cell-lit draw reads (src/gl/cellssr.cpp, cell_ssr.frag).
// Included after cell_lights.glsl by the two cell programs.

uniform bool cellSsrOn;			// an opaque draw of a flagged material, with this frame's reflections bound
uniform bool cellSsrMat;		// the material's flag: environment-mapped AND its material file's reflections switch
uniform sampler2D cellSsr;		// half the view, top row first, bilinear: rgb the reflected color, a the confidence
uniform vec4 cellSsrRect;		// the viewport: x, y, 1 / w, 1 / h

vec4 cellSsrTexel()
{
	vec2 uv = ( gl_FragCoord.xy - cellSsrRect.xy ) * cellSsrRect.zw;
	return texture( cellSsr, vec2( uv.x, 1.0 - uv.y ) );
}

/* The game's composite: lerp(cube, reflection, min(confidence, 1)) x the env scale of lane CUBE1's term
 * (cell_lights.glsl cellCubeGame). cubeK is that term (cube x k); the caller multiplies the diffuse light.
 * Probe 60, the reflections' own scene pass, carries no reflection term at all (the game marches the frame
 * before its env term). */
vec3 cellSsrMix( vec3 cubeK, float gloss, float spec, float envScale )
{
	if ( cellProbe == 60 )
		return vec3( 0.0 );
	if ( !cellSsrOn )
		return cubeK;
	vec4 s = cellSsrTexel();
	float k = 3.0 * clamp( spec, 0.0, 1.0 ) * min( sqrt( clamp( clamp( gloss, 0.0, 1.0 ) - 0.3, 0.0, 1.0 ) ), 1.0 )
		* clamp( envScale, 0.0, 50.0 );
	return mix( cubeK, s.rgb * k, min( s.a, 1.0 ) );
}

// probe 60: the scene pass (linear lit color without reflection, obscurance and fog; alpha the flag).
// probe 61 (the gate's): the reflection as this draw read it, the square roots of c = min(confidence, 1),
// green x c and the mean of rgb x c; black elsewhere, with alpha 0 (a blended draw over a reflecting surface, a
// decal or a glass, leaves that surface's value standing, as it leaves its reflection standing in the picture)
vec4 cellSsrProbeOut( vec3 sceneLinear )
{
	if ( cellProbe == 60 )
		return vec4( max( sceneLinear, vec3( 0.0 ) ), cellSsrMat ? 1.0 : 0.0 );
	if ( !cellSsrOn )
		return vec4( 0.0 );
	vec4 s = cellSsrTexel();
	float c = clamp( s.a, 0.0, 1.0 );
	return vec4( sqrt( clamp( vec3( c, s.g * c, dot( s.rgb, vec3( 1.0 / 3.0 ) ) * c ), 0.0, 1.0 ) ), 1.0 );
}

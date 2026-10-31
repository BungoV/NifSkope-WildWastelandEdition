// Screen-space reflection read by cell-lit draws (src/gl/cellssr.cpp, cell_ssr.frag).
// Included after cell_lights.glsl.

uniform bool cellSsrOn;			// an opaque draw of a flagged material, with this frame's reflections bound
uniform bool cellSsrMat;		// the material's flag: environment-mapped AND its material file's reflections switch
uniform sampler2D cellSsr;		// half the view, top row first, bilinear: rgb the reflected color, a the confidence
uniform vec4 cellSsrRect;		// the viewport: x, y, 1 / w, 1 / h

vec4 cellSsrTexel()
{
	vec2 uv = ( gl_FragCoord.xy - cellSsrRect.xy ) * cellSsrRect.zw;
	return texture( cellSsr, vec2( uv.x, 1.0 - uv.y ) );
}

/* The game's composite: lerp(cube, reflection, min(confidence, 1)) x env scale (cell_lights.glsl cellCubeGame).
 * Probe 60, the reflections' own scene pass, carries no reflection: the game marches before its env term. */
vec3 cellSsrMix( vec3 cubeK, float gloss, float spec, float envScale )
{
	if ( cellProbe == 60 )
		return vec3( 0.0 );
	if ( !cellSsrOn )
		return cubeK;
	vec4 s = cellSsrTexel();
	cellReflAdd *= 1.0 - min( s.a, 1.0 );	// lane REFLPROBE1: the march's confidence takes the probes' share too
	float k = 3.0 * clamp( spec, 0.0, 1.0 ) * min( sqrt( clamp( clamp( gloss, 0.0, 1.0 ) - 0.3, 0.0, 1.0 ) ), 1.0 )
		* clamp( envScale, 0.0, 50.0 );
	return mix( cubeK, s.rgb * k, min( s.a, 1.0 ) );
}

// probe 60: the scene pass (linear lit color without reflection, obscurance and fog; alpha the flag).
// probe 61: sqrt of c = min(confidence, 1), green x c, mean rgb x c; alpha 0 elsewhere so blended draws keep it
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

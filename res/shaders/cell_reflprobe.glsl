// lane REFLPROBE1 (src/gl/cellreflprobe.h): the placed reflection probes a cell-lit surface reads.
// Included after cell_lights.glsl by the two cell programs. The atlas comes in on a sampler the draw leaves idle
// (GreyscaleMap without a palette): fo4_cellcsm sits at the 32 samplers a fragment stage may hold.

uniform bool cellReflOn;		// this draw reads the probes (the row on, not red, an env-mapped material)
uniform int cellReflCount;		// probes in the arrays (the camera's first)
uniform vec4 cellReflPos[33];	// xyz the capture point (world), w the atlas tile
uniform vec4 cellReflMin[33];	// the box (world), w the fade band
uniform vec4 cellReflMax[33];	// w: 1 the camera's probe (takes what the boxes leave), 0 a placed one
uniform vec4 cellReflAtlas;		// 1 / atlas width, 1 / atlas height, tile size (px), tiles across
uniform float cellReflLods;		// the atlas's last mip

float cellReflW = 0.0, cellReflFall = 0.0;	// probe 66: the boxes' weight, the camera probe's share

vec2 cellReflOct( vec3 d )
{
	vec3 n = d / ( abs( d.x ) + abs( d.y ) + abs( d.z ) );
	vec2 f = n.xy;
	if ( n.z < 0.0 )
		f = ( 1.0 - abs( n.yx ) ) * vec2( n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0 );
	return f * 0.5 + 0.5;
}

vec3 cellReflSample( sampler2D atlas, int i, vec3 P, vec3 R, float lod )
{
	vec3 bmin = cellReflMin[i].xyz, bmax = cellReflMax[i].xyz;
	// Lagarde's parallax: the ray to the box's far wall, seen from the capture point
	vec3 Rs = vec3( abs( R.x ) < 1e-5 ? 1e-5 : R.x, abs( R.y ) < 1e-5 ? 1e-5 : R.y, abs( R.z ) < 1e-5 ? 1e-5 : R.z );
	vec3 tf = max( ( bmax - P ) / Rs, ( bmin - P ) / Rs );
	float t = min( tf.x, min( tf.y, tf.z ) );
	vec3 d = t > 0.0 ? P + R * t - cellReflPos[i].xyz : R;
	float T = cellReflAtlas.z;
	float tile = cellReflPos[i].w;
	vec2 org = vec2( mod( tile, cellReflAtlas.w ), floor( tile / cellReflAtlas.w ) ) * T;
	float inset = 0.5 * exp2( ceil( lod ) ) / T;	// the coarser mip's half texel: no bleed from the next tile
	vec2 uv = clamp( cellReflOct( normalize( d ) ), vec2( inset ), vec2( 1.0 - inset ) );
	return textureLod( atlas, ( org + uv * T ) * cellReflAtlas.xy, lod ).rgb;
}

/* Lagarde's blend: each box's fade x its parallax-corrected sample, normalised past 1; the camera's probe takes
 * what is left. The result is light already, so it is added (cellReflAdd) at lane CUBE1's strength k and NOT x the
 * diffuse light; the return scales the material cube's term (0 where the probes cover it). */
float cellReflProbe( sampler2D atlas, vec3 posView, vec3 reflView, float gloss, float spec, float envScale )
{
	if ( !cellReflOn || !cellCubeGameOn() || ( cellProbe != 0 && cellProbe != 66 ) )
		return 1.0;
	vec3 P = cellWorldPos( posView );
	vec3 R = normalize( cellWorldDir( reflView ) );
	float g = clamp( gloss, 0.0, 1.0 );
	float lod = clamp( ( 1.0 - g ) * 6.0 - 1.0, 0.0, cellReflLods );	// lane CUBE1's mip on a 128 cube; ours are 64
	vec3 sum = vec3( 0.0 );
	float W = 0.0;
	int fall = -1;
	for ( int i = 0; i < cellReflCount; i++ ) {
		if ( cellReflMax[i].w > 0.5 ) {
			fall = i;
			continue;
		}
		vec3 dmin = min( P - cellReflMin[i].xyz, cellReflMax[i].xyz - P );
		float w = clamp( min( dmin.x, min( dmin.y, dmin.z ) ) / max( cellReflMin[i].w, 1.0 ), 0.0, 1.0 );
		if ( w <= 0.0 )
			continue;
		sum += w * cellReflSample( atlas, i, P, R, lod );
		W += w;
	}
	if ( W > 1.0 ) {
		sum /= W;
		W = 1.0;
	}
	cellReflW = W;
	if ( fall >= 0 && W < 1.0 ) {
		cellReflFall = 1.0 - W;
		sum += cellReflFall * cellReflSample( atlas, fall, P, R, lod );
		W = 1.0;
	}
	float k = 3.0 * clamp( spec, 0.0, 1.0 ) * min( sqrt( clamp( g - 0.3, 0.0, 1.0 ) ), 1.0 ) * clamp( envScale, 0.0, 50.0 );
	cellReflAdd = sum * k;
	return 1.0 - W;
}

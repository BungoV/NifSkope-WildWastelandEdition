#version 410 core

/* lane AODECAL1: one copy's baked AO volume (src/aovolume.h; design docs/cloud/AODECAL1_DESIGN.md) over the
 * opaque pass. aoMode 0: the copy's own triangles, nothing but its id (the stencil does the masking). aoMode 1:
 * the footprint box's back faces; each covered pixel's receiver is rebuilt from the opaque pass (view normal,
 * linear depth in game units) and taken to the copy's model space; out = AO(m, n), multiplied into the target
 * (red "add": out = AO - 1, added). The lookup is aovol::lookup's, term for term. */

layout ( location = 0 ) out vec4 outAo;
layout ( location = 1 ) out vec4 outId;

uniform int aoMode;
uniform float aoSelf;		// the copy's index + 1 (mask mode)
uniform bool aoAdd;			// red "add"
uniform sampler2D aoGbuf;	// full size: view normal, linear depth (game units); 1e6 = background
uniform sampler3D aoVol;	// RGBA32F, dims.x x dims.y x 3 dims.z: slabs k0-3, k4-7, k8
uniform vec4 aoProj;		// P00, P11, P20, P21
uniform vec2 aoSize;		// the target's size
uniform float aoSc;			// the view's scale
uniform vec4 aoM[3];		// view -> model rows (M | b)
uniform vec3 aoN[3];		// view -> model normal rows
uniform vec3 aoLo;
uniform vec3 aoHi;
uniform ivec3 aoDims;
uniform float aoFade;
uniform vec3 aoC;
uniform float aoCoef[9];
uniform float aoRcut;

void sh9( vec3 d, out float y[9] )
{
	y[0] = 0.282095;
	y[1] = 0.488603 * d.y;
	y[2] = 0.488603 * d.z;
	y[3] = 0.488603 * d.x;
	y[4] = 1.092548 * d.x * d.y;
	y[5] = 1.092548 * d.y * d.z;
	y[6] = 0.315392 * ( 3.0 * d.z * d.z - 1.0 );
	y[7] = 1.092548 * d.x * d.z;
	y[8] = 0.546274 * ( d.x * d.x - d.y * d.y );
}

void voxel( ivec3 i, float w, inout float K[9] )
{
	vec4 a = texelFetch( aoVol, i, 0 );
	vec4 b = texelFetch( aoVol, ivec3( i.xy, i.z + aoDims.z ), 0 );
	float c = texelFetch( aoVol, ivec3( i.xy, i.z + 2 * aoDims.z ), 0 ).r;
	K[0] += w * a.x; K[1] += w * a.y; K[2] += w * a.z; K[3] += w * a.w;
	K[4] += w * b.x; K[5] += w * b.y; K[6] += w * b.z; K[7] += w * b.w;
	K[8] += w * c;
}

float aoLookup( vec3 m, vec3 n )
{
	float K[9] = float[9]( 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 );
	ivec3 i0;
	vec3 f;
	for ( int a = 0; a < 3; a++ ) {
		float cell = ( aoHi[a] - aoLo[a] ) / float( aoDims[a] );
		float g = clamp( ( m[a] - aoLo[a] ) / cell - 0.5, 0.0, float( aoDims[a] - 1 ) );
		i0[a] = min( int( floor( g ) ), aoDims[a] - 2 );
		f[a] = g - float( i0[a] );
	}
	for ( int dz = 0; dz < 2; dz++ )
		for ( int dy = 0; dy < 2; dy++ )
			for ( int dx = 0; dx < 2; dx++ ) {
				float w = ( dx == 1 ? f.x : 1.0 - f.x ) * ( dy == 1 ? f.y : 1.0 - f.y ) * ( dz == 1 ? f.z : 1.0 - f.z );
				voxel( i0 + ivec3( dx, dy, dz ), w, K );
			}
	float Y[9];
	sh9( n, Y );
	const float band[9] = float[9]( 1.0, 2.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0, 0.25, 0.25, 0.25, 0.25, 0.25 );
	float occ = 0.0;
	for ( int j = 0; j < 9; j++ )
		occ += band[j] * K[j] * 3.5449077 * Y[j];	// 2 sqrt(pi)
	vec3 inner3 = min( m - aoLo, aoHi - m );
	float fd = clamp( min( inner3.x, min( inner3.y, inner3.z ) ) / aoFade, 0.0, 1.0 );
	float far = 0.0;
	vec3 w = m - aoC;
	float r = max( length( w ), 1e-6 );
	if ( r < aoRcut ) {
		vec3 u = w / r;
		float yu[9];
		sh9( u, yu );
		float s = 0.0;
		for ( int j = 0; j < 9; j++ )
			s += yu[j] * aoCoef[j];
		far = min( max( s, 0.0 ) / ( r * r ), 1.0 ) * max( -dot( n, u ), 0.0 );
	}
	return clamp( 1.0 - ( occ * fd + far * ( 1.0 - fd ) ), 0.0, 1.0 );
}

void main()
{
	if ( aoMode == 0 ) {
		outAo = vec4( 1.0 );
		outId = vec4( aoSelf );
		return;
	}
	vec4 g = texelFetch( aoGbuf, ivec2( gl_FragCoord.xy ), 0 );
	if ( g.w >= 1.0e5 ) {
		outAo = vec4( aoAdd ? 0.0 : 1.0 );
		return;
	}
	vec2 ndc = gl_FragCoord.xy / aoSize * 2.0 - 1.0;
	float zv = g.w * aoSc;
	vec4 pv = vec4( zv * ( ndc.x + aoProj.z ) / aoProj.x, zv * ( ndc.y + aoProj.w ) / aoProj.y, -zv, 1.0 );
	vec3 m = vec3( dot( aoM[0], pv ), dot( aoM[1], pv ), dot( aoM[2], pv ) );
	vec3 n = normalize( vec3( dot( aoN[0], g.xyz ), dot( aoN[1], g.xyz ), dot( aoN[2], g.xyz ) ) );
	float ao = aoLookup( m, n );
	outAo = vec4( aoAdd ? ao - 1.0 : ao );
	outId = vec4( 0.0 );
}

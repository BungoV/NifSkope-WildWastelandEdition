#version 410 core

/* lane PERFFIX3 D (2026-11-01): a precipitation particle in the cell view's linear frame, lit per PARTICLE
 * (cell_precip_gpu.geom: gA, gB). Per fragment only the texture, the opacity and the near-eye fade remain:
 * colour = albedo x gA + gB (= wwFogFx( albedo x lit ), the fog being affine in the colour). Alpha blended, no depth
 * write (the scene's depth tests it). */

uniform sampler2D pcTex;
uniform bool pcHasTex;
uniform int pcFar;
uniform vec4 pcAtlas;		// cell uv size, the deepest mip a near quad reads, the one-texel-a-cell mip
uniform vec2 pcNear;		// view units from the eye: gone at x, whole at y (a flake never covers the lens)

in vec2 gUv;
in vec2 gQ;
in vec3 gView;
flat in vec4 gCol;
flat in vec4 gBox;
flat in vec3 gA;
flat in vec3 gB;
#ifdef PC_LOOK
/* lane PRECIPLOOK1 (cell_precip_look.prog). pcLook 1 = a raindrop: no texture; the opacity is the time-averaged
 * coverage of the drop's disc (radius hd) swept over the streak (length tl) during the shutter, which conserves the
 * disc's area x time (Garg & Nayar 2006 streak model); its light flickers along the streak with the drop's n = 2
 * oscillation (gDrop.z depth, gDrop.w cycles). The water lens itself (Fresnel + refraction) is the phase table the
 * geometry stage lit it with (cell_volpart.glsl pcDropPi), so the albedo is 1: no grey drop.
 * pcLook 2 = a snow flake with its normal map: the thin share per texel m = clamp( pcTrans (1 - a) / 0.4 ), the
 * thick share turned by the normal toward the key light (wrap( N.L, w ) / wrap( L.z, w ), w = pcTrans), and a small ice
 * glint at N.H (the flake's facets: Blinn 64, F0 0.02). pcClump (t, k): the merged clump atlas's density threshold
 * (alpha = clamp( (d - t) / k )): low t = loose dry, high t = compact wet; pcAlbedo the material's colour. */
uniform int pcLook;
uniform sampler2D pcTexN;
uniform bool pcHasN;
uniform vec2 pcClump;
uniform float pcAlbedo;
uniform float pcTrans;
flat in vec4 gDrop;
flat in float gOsc;
flat in vec3 gThin;
flat in vec3 gThick;
flat in vec4 gDomL;
flat in vec3 gDomE;
flat in vec2 gAx;

float pcWrap( float x, float w )
{
	return max( ( x + w ) / ( 1.0 + w ), 0.0 );
}
#endif

out vec4 fragColor;

void main()
{
	vec4 t;
#ifdef PC_LOOK
	if ( pcLook == 1 ) {
		// the swept disc: along the streak s, across u (view units), the quad's half sizes hd + tl / 2 and hd
		float hd = gDrop.x, tl = gDrop.y;
		float s = gQ.x * ( hd + 0.5 * tl ), u = gQ.y * hd;
		float c2 = hd * hd - u * u;
		float cov = 0.0;
		if ( c2 > 0.0 ) {
			float c = sqrt( c2 );
			if ( tl > 1e-4 * hd )
				cov = clamp( ( min( s + c, 0.5 * tl ) - max( s - c, -0.5 * tl ) ) / tl, 0.0, 1.0 );
			else
				cov = abs( s ) < c ? 1.0 : 0.0;
		}
		float a = clamp( cov * gCol.a, 0.0, 1.0 ) * smoothstep( pcNear.x, pcNear.y, -gView.z );
		if ( a < 1.0 / 512.0 )
			discard;
		float tf = tl > 1e-4 * hd ? clamp( s / tl + 0.5, 0.0, 1.0 ) : 0.5;
		float fl = 1.0 + gDrop.z * sin( 6.2831853 * gDrop.w * tf + gOsc );
		fragColor = vec4( max( gCol.rgb * gA * fl + gB, vec3( 0.0 ) ), a );
		return;
	}
#endif
	if ( pcFar != 0 ) {
		// a far dot: the cell's mean (gCol) over a soft round footprint whose mean over the quad is 1
		t = vec4( 1.0, 1.0, 1.0, exp( -2.5 * dot( gQ, gQ ) ) / 0.2985 );
	} else if ( pcHasTex ) {
		float lod = min( textureQueryLod( pcTex, gUv ).x, pcAtlas.z );
		t = textureLod( pcTex, clamp( gUv, gBox.xy, gBox.zw ), lod );
#ifdef PC_LOOK
		t.a = clamp( ( t.a - pcClump.x ) / pcClump.y, 0.0, 1.0 );
#endif
	} else {
		t = vec4( 1.0, 1.0, 1.0, clamp( 1.0 - dot( gQ, gQ ), 0.0, 1.0 ) );
	}
	float a = clamp( t.a * gCol.a, 0.0, 1.0 ) * smoothstep( pcNear.x, pcNear.y, -gView.z );
	if ( a < 1.0 / 512.0 )
		discard;
	vec3 alb = t.rgb * gCol.rgb;	// linear: the sRGB texture decodes on read
#ifdef PC_LOOK
	alb *= pcAlbedo;
	if ( pcLook == 2 && pcHasN && pcFar == 0 ) {
		float lod = min( textureQueryLod( pcTexN, gUv ).x, pcAtlas.z );
		vec3 nt = textureLod( pcTexN, clamp( gUv, gBox.xy, gBox.zw ), lod ).rgb;
		vec3 nm = vec3( 2.0 * nt.r - 1.0, 1.0 - 2.0 * nt.g, 2.0 * nt.b - 1.0 );	// DirectX green: y up in the image
		vec2 ay = vec2( -gAx.y, gAx.x );	// the image's down is the quad's +v
		vec3 N = normalize( vec3( gAx * nm.x - ay * nm.y, max( nm.z, 0.0 ) ) );
		vec3 L = gDomL.xyz;
		float m = clamp( pcTrans * ( 1.0 - t.a ) / 0.4, 0.0, 1.0 );
		float r = min( pcWrap( dot( N, L ), pcTrans ) / max( pcWrap( L.z, pcTrans ), 0.05 ), 4.0 );
		vec3 thickF = gThick * mix( 1.0, r, gDomL.w );
		vec3 V = normalize( -gView );
		vec3 H = normalize( L + V );
		vec3 spark = gDomE * ( 0.02 * ( 72.0 / 8.0 ) * pow( max( dot( N, H ), 0.0 ), 64.0 ) * max( dot( N, L ), 0.0 ) );
		fragColor = vec4( max( alb * ( m * gThin + ( 1.0 - m ) * thickF ) + spark + gB, vec3( 0.0 ) ), a );
		return;
	}
#endif
	fragColor = vec4( max( alb * gA + gB, vec3( 0.0 ) ), a );
}


uniform sampler2D uAtlasC;
uniform sampler2D uAtlasN;
uniform float uMaxLod;
uniform int uDecalNormals;
layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
void main()
{
	vec3 w, l;
	Rec r;
	if ( !unproject( w, l, r ) )
		discard;
	vec2 uv = vec2( ( l.x - r.box0.x ) / ( r.box0.z - r.box0.x ), ( r.box0.w - l.y ) / ( r.box0.w - r.box0.y ) );
	vec2 a = mix( r.atlas.xy, r.atlas.zw, uv );
	float pix = uOrtho != 0 ? uPixelK : uPixelK * distance( w, uEye );
	float lod = clamp( log2( max( 1.0, pix / ( r.box1.z * r.posScale.w ) ) ), 0.0, uMaxLod );
	vec4 c = textureLod( uAtlasC, a, lod );
	vec4 n = textureLod( uAtlasN, a, lod );
	float wgt = 1.0;
	if ( uOption == 1 )
		wgt = 1.0 - smoothstep( uFade.x, uFade.y, distance( w, uEye ) );
	oAlbedo = vec4( c.rgb, c.a * wgt );
	vec3 nw = normalize( qrot( r.quat, n.rgb * 2.0 - 1.0 ) );
	oNormal = vec4( nw * 0.5 + 0.5, uDecalNormals != 0 ? n.a * c.a * wgt : 0.0 );
}

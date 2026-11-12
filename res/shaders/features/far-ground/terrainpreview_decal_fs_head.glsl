
flat in int vRec;
uniform sampler2D uDepth;
uniform mat4 uInvViewProj;
uniform vec2 uViewport;
uniform vec3 uEye;
uniform vec2 uFade;
uniform int uOption;
uniform int uOrtho;
uniform float uPixelK;
bool unproject( out vec3 w, out vec3 l, out Rec r )
{
	r = recs[vRec];
	float d = texelFetch( uDepth, ivec2( gl_FragCoord.xy ), 0 ).r;
	if ( d >= 1.0 )
		return false;
	vec4 ndc = vec4( gl_FragCoord.xy / uViewport * 2.0 - 1.0, d * 2.0 - 1.0, 1.0 );
	vec4 wp = uInvViewProj * ndc;
	w = wp.xyz / wp.w;
	vec4 qi = vec4( -r.quat.xyz, r.quat.w );
	l = qrot( qi, w - r.posScale.xyz ) / r.posScale.w;
	return l.x >= r.box0.x && l.x <= r.box0.z && l.y >= r.box0.y && l.y <= r.box0.w
		&& l.z >= r.box1.x && l.z <= r.box1.y;
}

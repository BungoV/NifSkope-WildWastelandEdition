
uniform mat4 uViewProj;
flat out int vRec;
const int kIdx[36] = int[36]( 0,2,1, 1,2,3, 4,5,6, 5,7,6, 0,1,4, 1,5,4, 2,6,3, 3,6,7, 0,4,2, 2,4,6, 1,3,5, 3,7,5 );
void main()
{
	Rec r = recs[gl_InstanceID];
	int c = kIdx[gl_VertexID];
	vec3 t = vec3( float( c & 1 ), float( ( c >> 1 ) & 1 ), float( ( c >> 2 ) & 1 ) );
	vec3 l = vec3( mix( r.box0.x, r.box0.z, t.x ), mix( r.box0.y, r.box0.w, t.y ), mix( r.box1.x, r.box1.y, t.z ) );
	vec3 w = r.posScale.xyz + qrot( r.quat, l * r.posScale.w );
	vRec = gl_InstanceID;
	gl_Position = uViewProj * vec4( w, 1.0 );
}

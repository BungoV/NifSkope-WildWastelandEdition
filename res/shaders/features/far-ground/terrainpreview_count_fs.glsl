
layout(location = 0) out vec4 oCount;
void main()
{
	vec3 w, l;
	Rec r;
	bool inside = unproject( w, l, r );
	oCount = vec4( 1.0, inside ? 1.0 : 0.0, 0.0, 1.0 );
}

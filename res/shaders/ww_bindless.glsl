// lane BINDLESS2: the per-draw row (src/gl/cellbindless.h WwBl::Row, std430, 176 bytes): the draw's matrices and the
// material's seven 2D maps as resident texture handles. wwDrawIdx is the draw's row in the ring at binding 12.
struct WwDraw
{
	mat4 mv;
	vec4 nm[3];
	uvec2 tex[8];
};
layout( std430, binding = 12 ) readonly buffer WwDrawBuf
{
	WwDraw wwDraws[];
};
uniform uint wwDrawIdx;

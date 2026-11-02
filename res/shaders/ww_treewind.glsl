/* lane WINDTREE1: the game's tree wind on a loaded cell's Tree_Anim shapes (Todd's treat), op for op as
 * src/gl/celltreewind.h (wwTreeWindVertex). Set per draw by src/gl/cellgrass.cpp (wwCellGrassUniforms):
 * wwTreeWind  = (wind angle, 0, previous phase, phase)
 * wwTreeWind2 = (min speed * 300, max speed * 300, wind frequency, 1 on a tree-wind shape / 0 on every other)
 * wwTreeWind3 = (the reference's position in model space, its world x + y + z)
 * wwTreeWind4 = (2 x bound radius x scale, leaf amplitude x scale, leaf frequency, 0)
 * wwTreeWind5 = (the reference's rotation x (1, 1, 1), 0): the object normal's sum from the world normal
 * The weld carries the vertex alpha (the sway weight) in the bitangent's length: 2 + alpha. Normals are not bent. */
uniform vec4 wwTreeWind;
uniform vec4 wwTreeWind2;
uniform vec4 wwTreeWind3;
uniform vec4 wwTreeWind4;
uniform vec4 wwTreeWind5;

vec3 wwTreeWindApply( vec3 p, vec3 nrm, float bitLen, mat4 mv )
{
	float a = bitLen > 1.5 ? clamp( bitLen - 2.0, 0.0, 1.0 ) : 0.0;
	vec3 n = nrm * inversesqrt( max( dot( nrm, nrm ), 1.0e-30 ) );
	float s = dot( n, wwTreeWind5.xyz );
	float base = 3.0 * s + wwTreeWind3.w;
	float F = wwTreeWind4.z * 20.0;
	float dist = length( ( mv * vec4( wwTreeWind3.xyz, 1.0 ) ).xyz ) / length( mv[0].xyz );
	float A = wwTreeWind4.y * clamp( ( 4600.0 - dist ) * 0.001, 0.0, 1.0 ) * 0.035;
	float halfR = ( wwTreeWind2.y - wwTreeWind2.x ) * 0.5;
	float sp = wwTreeWind2.x + halfR * sin( wwTreeWind.w * ( ( F * wwTreeWind2.z ) * 5.0 ) + base );
	vec3 off = p + ( n * ( sp * A ) ) * a - wwTreeWind3.xyz;
	float h = ( off.x + off.y + off.z ) / wwTreeWind4.x;
	float c = sin( ( wwTreeWind.w * wwTreeWind2.z ) * F + ( h * -1.5 + wwTreeWind3.w ) );
	float g = wwTreeWind2.x + halfR * ( 0.3 * sin( 3.14159274 * c ) + sin( 6.28318548 * c ) + 2.0 );
	float b = ( h * g / 3.0 ) * a;
	vec3 q = off + vec3( cos( wwTreeWind.x ) * b, sin( wwTreeWind.x ) * b, 0.0 );
	float ql = length( q );
	return ( ql > 0.0 ? q * ( length( off ) / ql ) : q ) + wwTreeWind3.xyz;
}

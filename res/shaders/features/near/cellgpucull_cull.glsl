
layout( local_size_x = 64 ) in;
struct Meshlet { vec4 sphere; vec4 cone; uvec4 tri; };
struct Job { mat4 mv; vec4 p; };
layout( std430, binding = 0 ) readonly buffer HizBuf { float hiz[]; };
layout( std430, binding = 1 ) readonly buffer MeshletBuf { Meshlet ml[]; };
layout( std430, binding = 2 ) readonly buffer JobBuf { Job jobs[]; };
layout( std430, binding = 3 ) readonly buffer ItemBuf { uvec2 items[]; };
layout( std430, binding = 4 ) writeonly buffer CmdBuf { uint cmds[]; };
layout( std430, binding = 5 ) buffer CensusBuf { uint census[8]; };
uniform mat4 proj;
uniform vec4 planes[5];
uniform uint numItems;
uniform ivec2 vpSize;
uniform int levels;
uniform int lvlOff[16];
uniform ivec2 lvlDim[16];
uniform int mode;	// 1 occlusion, 2 cone, 4 frustum, 8 red hard, 16 red nocull, 32 census
uniform float depthEps;
void main()
{
	uint i = gl_GlobalInvocationID.x;
	if ( i >= numItems )
		return;
	uvec2 it = items[i];
	Meshlet m = ml[it.y];
	Job j = jobs[it.x];
	vec3 c = ( j.mv * vec4( m.sphere.xyz, 1.0 ) ).xyz;
	float r = m.sphere.w * j.p.x;
	int why = 0;
	if ( ( mode & 4 ) != 0 ) {
		float margin = r * 1.001 + 1e-5 * length( c );
		for ( int k = 0; k < 5; k++ ) {
			if ( dot( planes[k].xyz, c ) + planes[k].w < -margin ) {
				why = 1;
				break;
			}
		}
	}
	if ( why == 0 && ( mode & 2 ) != 0 && j.p.y != 0.0 && m.cone.w <= 1.0 ) {
		vec3 a = normalize( mat3( j.mv ) * m.cone.xyz ) * j.p.y;
		if ( dot( c, a ) >= m.cone.w * length( c ) + r )
			why = 2;
	}
	if ( why == 0 && ( mode & 1 ) != 0 ) {
		bool front = true;
		vec2 lo = vec2( 1e30 ), hi = vec2( -1e30 );
		for ( int k = 0; k < 8; k++ ) {
			vec3 o = vec3( ( k & 1 ) != 0 ? r : -r, ( k & 2 ) != 0 ? r : -r, ( k & 4 ) != 0 ? r : -r );
			vec4 q = proj * vec4( c + o, 1.0 );
			if ( q.w <= 0.0 || q.z < -q.w ) {
				front = false;
				break;
			}
			lo = min( lo, q.xy / q.w );
			hi = max( hi, q.xy / q.w );
		}
		if ( front ) {
			vec2 p0 = ( lo * 0.5 + 0.5 ) * vec2( vpSize ) - 1.0;
			vec2 p1 = ( hi * 0.5 + 0.5 ) * vec2( vpSize ) + 1.0;
			if ( p1.x >= 0.0 && p1.y >= 0.0 && p0.x < float( vpSize.x ) && p0.y < float( vpSize.y ) ) {
				ivec2 a = clamp( ivec2( floor( p0 ) ), ivec2( 0 ), vpSize - 1 );
				ivec2 b = clamp( ivec2( floor( p1 ) ), ivec2( 0 ), vpSize - 1 );
				int span = max( b.x - a.x, b.y - a.y ) + 1;
				int L = 0;
				while ( L < levels - 1 && ( 1 << L ) < span )
					L++;
				ivec2 ta = a >> L, tb = b >> L;
				float h = 0.0;
				for ( int y = ta.y; y <= tb.y; y++ )
					for ( int x = ta.x; x <= tb.x; x++ )
						h = max( h, hiz[lvlOff[L] + y * lvlDim[L].x + x] );
				float z = ( mode & 8 ) != 0 ? c.z : c.z + r;
				vec4 q = proj * vec4( 0.0, 0.0, z, 1.0 );
				if ( ( q.z / q.w ) * 0.5 + 0.5 > h + depthEps )
					why = 3;
			}
		}
	}
	if ( ( mode & 16 ) != 0 )
		why = 0;
	uint o = i * 5u;
	cmds[o] = m.tri.y * 3u;
	cmds[o + 1u] = why == 0 ? 1u : 0u;
	cmds[o + 2u] = m.tri.x * 3u;
	cmds[o + 3u] = 0u;
	cmds[o + 4u] = 0u;
	if ( ( mode & 32 ) != 0 ) {
		atomicAdd( census[why], 1u );
		if ( why == 0 )
			atomicAdd( census[4], m.tri.y );
	}
}

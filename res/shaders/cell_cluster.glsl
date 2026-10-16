// Clustered light lists (src/gl/cellcluster.h): a pixel walks only the lights its cluster lists, in the flat
// loop's order, so the sum is the same bits. Included by cell_lights.glsl. Use:
//     int n; int b = cellLightsBegin( P, n );
//     for ( int k = 0; k < n; k++ ) { int i = cellLightAt( b, k ); ... }

uniform bool cellClusterOn;             // false: every pixel walks every light (the flat loop)
uniform isamplerBuffer cellCluster;     // per cluster (offset, count), count -1 = walk every light; then the indices
uniform vec4 cellClusterDims;           // tiles x, tiles y, slices, tile size in pixels (whole numbers)
uniform vec2 cellClusterOrigin;         // the viewport's origin, in window pixels
uniform vec4 cellClusterDepth;          // view depth = dot( xyz, world P ) + w
uniform vec2 cellClusterLog;            // slice = floor( ( log( depth ) - x ) * y )
uniform int cellClusterBase;            // where the indices start (2 x the cluster count)

int cellClusterOf( vec3 P )
{
	ivec3 d = ivec3( cellClusterDims.xyz + 0.5 );
	ivec2 t = clamp( ivec2( floor( ( gl_FragCoord.xy - cellClusterOrigin ) / cellClusterDims.w ) ), ivec2( 0 ), d.xy - 1 );
	float s = floor( ( log( max( dot( cellClusterDepth.xyz, P ) + cellClusterDepth.w, 1e-6 ) ) - cellClusterLog.x ) * cellClusterLog.y );
	return ( clamp( int( s ), 0, d.z - 1 ) * d.y + t.y ) * d.x + t.x;
}

// The first index of P's list and its length; -1 with n = every light when there is no list.
int cellLightsBegin( vec3 P, out int n )
{
	n = cellLightCount;
	if ( !cellClusterOn )
		return -1;
	int c = cellClusterOf( P );
	int k = texelFetch( cellCluster, 2 * c + 1 ).r;
	if ( k < 0 )
		return -1;
	n = k;
	return cellClusterBase + texelFetch( cellCluster, 2 * c ).r;
}

int cellLightAt( int base, int k )
{
	return base < 0 ? k : texelFetch( cellCluster, base + k ).r;
}

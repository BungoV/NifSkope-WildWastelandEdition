
void main()
{
	int i = int( gl_GlobalInvocationID.x );
	if ( i >= uN )
		return;
	int so = i * 16;
	vec3 n = vec3( surf[so], surf[so + 1], surf[so + 2] );
	vec3 E = vec3( 0.0 );
	float ws = 0.0;
	for ( int k = feedStart[i]; k < feedStart[i + 1]; k++ ) {
		int rec = floatBitsToInt( feed[k * 4 + 2] );
		float vf = rec >= 0 ? doorF( rec, 0, true ) : 1.0;
		if ( vf <= 0.0 )
			continue;
		int p = floatBitsToInt( feed[k * 4] );
		float w = feed[k * 4 + 1] * vf;
		ws += w;
		for ( int a = 0; a < 3; a++ ) {
			int ax = 2 * a + ( n[a] >= 0.0 ? 0 : 1 );
			int po = p * 18 + ax * 3;
			E += w * n[a] * n[a] * vec3( pe[po], pe[po + 1], pe[po + 2] );
		}
	}
	vec3 alb = uRedGrow != 0 ? vec3( 1.5 ) : vec3( surf[so + 4], surf[so + 5], surf[so + 6] );
	vec3 B1 = vec3( surf[so + 12], surf[so + 13], surf[so + 14] );
	vec3 Bn = B1 + ( ws > 0.0 ? alb * ( E / ws ) / 3.14159265358979 : vec3( 0.0 ) );
	vec3 Bo = vec3( bsrc[i * 4], bsrc[i * 4 + 1], bsrc[i * 4 + 2] );
	bdst[i * 4] = Bn.x;
	bdst[i * 4 + 1] = Bn.y;
	bdst[i * 4 + 2] = Bn.z;
	vec3 ch = abs( Bn - Bo );
	atomicMax( ctl[0], floatBitsToUint( max( max( ch.x, ch.y ), ch.z ) ) );
	atomicMax( ctl[1], floatBitsToUint( max( max( max( Bn.x, Bn.y ), Bn.z ), 0.0 ) ) );
}

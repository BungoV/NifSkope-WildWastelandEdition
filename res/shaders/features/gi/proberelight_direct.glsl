
void main()
{
	int i = int( gl_GlobalInvocationID.x );
	if ( i >= uN )
		return;
	vec3 E = vec3( 0.0 );
	for ( int k = pairStart[i]; k < pairStart[i + 1]; k++ ) {
		int li = floatBitsToInt( pairs[k * 8] );
		float d = pairs[k * 8 + 1], nl = pairs[k * 8 + 2], dl = pairs[k * 8 + 3];
		int rec = floatBitsToInt( pairs[k * 8 + 4] );
		int lo = li * 16;
		vec3 col = vec3( lights[lo + 4], lights[lo + 5], lights[lo + 6] );
		float r = lights[lo + 3];
		if ( col == vec3( 0.0 ) || d >= r )
			continue;
		float x = clamp( d / max( r, 0.001 ), 0.0, 1.0 );
		float a;
		float kind = lights[lo + 15];   // lane LIGHTPHYS2: wwlp::lightTerm, decided per light on the CPU
		if ( kind < 0.5 ) {
			float xe = lights[lo + 9] > 0.0 ? pow( x, lights[lo + 9] ) : 1.0;
			float kk = 1.0 - clamp( lights[lo + 8] * xe + lights[lo + 7], 0.0, 1.0 );
			a = kk > 0.0 ? pow( kk, 2.2 ) : 0.0;
		} else if ( kind < 1.5 ) {   // wwlp::physRaw
			float s = lights[lo + 14], x2 = x * x;
			float w = clamp( 1.0 - x2 * x2, 0.0, 1.0 ), den = x2 + s * s;
			a = den > 0.0 ? lights[lo + 13] * ( w * w / den ) : 0.0;
		} else {   // wwlp::advAtt
			float t = clamp( ( x - 0.8 ) / ( 1.0 - 0.8 ), 0.0, 1.0 );
			float w = kind > 2.5 ? 1.0 : 1.0 - t * t * ( 3.0 - 2.0 * t );
			float m = max( lights[lo + 14], x );
			a = lights[lo + 13] * w / ( m * m );
		}
		if ( lights[lo + 12] > 0.5 ) {
			float base = clamp( 1.0 - ( 1.0 - dl ) / max( 1.0 - lights[lo + 11], 1e-4 ), 0.0, 1.0 );
			a *= base > 0.0 ? min( pow( base, max( lights[lo + 10], 1e-3 ) ), 1.0 ) : 0.0;
		}
		if ( a * nl <= 0.0 )
			continue;
		vec3 f = vec3( doorF( rec, 0, false ), doorF( rec, 1, false ), doorF( rec, 2, false ) );
		E += col * ( a * nl ) * f;
	}
	int so = i * 16;
	if ( uHasDir != 0 && surf[so + 3] > 0.0 )
		E += uDirColor * surf[so + 3];
	if ( uSunOn != 0 && surf[so + 7] > 0.0 )
		E += uSun * surf[so + 7];
	vec3 B = vec3( surf[so + 4], surf[so + 5], surf[so + 6] ) * E + vec3( surf[so + 8], surf[so + 9], surf[so + 10] );
	surf[so + 12] = B.x;
	surf[so + 13] = B.y;
	surf[so + 14] = B.z;
	bsrc[i * 4] = B.x;
	bsrc[i * 4 + 1] = B.y;
	bsrc[i * 4 + 2] = B.z;
}

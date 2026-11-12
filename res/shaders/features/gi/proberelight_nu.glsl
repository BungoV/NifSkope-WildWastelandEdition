
void main()
{
	int j = int( gl_GlobalInvocationID.x );
	if ( j >= uN )
		return;
	float sum = 0.0, sq = 0.0;
	for ( int k = linkStart[j]; k < linkStart[j + 1]; k++ ) {
		int o = k * 12;
		int s = floatBitsToInt( links[o] );
		int rec = floatBitsToInt( links[o + 11] );
		vec3 w = vec3( bsrc[s * 4], bsrc[s * 4 + 1], bsrc[s * 4 + 2] ) * vec3( links[o + 2], links[o + 3], links[o + 4] ) * links[o + 1];
		if ( rec >= 0 )
			w *= vec3( doorF( rec, 0, false ), doorF( rec, 1, false ), doorF( rec, 2, false ) );
		float cs = 0.0;
		for ( int a = 0; a < 6; a++ )
			cs += links[o + 5 + a];
		float x = dot( w, vec3( 0.2126, 0.7152, 0.0722 ) ) * cs / 6.0;
		sum += x;
		sq += x * x;
	}
	float sky = 0.0;
	if ( uSkyOn != 0 ) {
		for ( int a = 0; a < 6; a++ )
			sky += dot( vec3( probeK[j * 20 + 2 + a * 3], probeK[j * 20 + 3 + a * 3], probeK[j * 20 + 4 + a * 3] ),
				vec3( 0.2126, 0.7152, 0.0722 ) );
		sky /= 6.0;
	}
	float kU = probeK[j * 20];
	float m1 = kU * sum + sky;
	bdst[j] = m1 > 0.0 ? min( kU * sqrt( sq ) / m1, 1.0 ) : 0.0;
}

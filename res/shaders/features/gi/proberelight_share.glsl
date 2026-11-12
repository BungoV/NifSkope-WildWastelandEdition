
void main()
{
	int j = int( gl_GlobalInvocationID.x );
	if ( j >= uN )
		return;
	float acc[18];
	for ( int q = 0; q < 18; q++ )
		acc[q] = pe[j * 18 + q];
	float as = 0.0, nu = bdst[j];
	for ( int k = feedStart[j]; k < feedStart[j + 1]; k++ ) {
		int rec = floatBitsToInt( feed[k * 4 + 2] );
		float vf = rec >= 0 ? doorF( rec, 0, true ) : 1.0;
		float a = nu * feed[k * 4 + 1] * vf;
		if ( a <= 0.0 )
			continue;
		as += a;
		int p = floatBitsToInt( feed[k * 4] );
		for ( int q = 0; q < 18; q++ )
			acc[q] += a * pe[p * 18 + q];
	}
	for ( int q = 0; q < 18; q++ )
		bsrc[j * 18 + q] = acc[q] / ( 1.0 + as );
}

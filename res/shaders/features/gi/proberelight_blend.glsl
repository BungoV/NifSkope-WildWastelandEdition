
void main()
{
	int s = int( gl_GlobalInvocationID.x );
	if ( s >= uN )
		return;
	int b = feedStart[s], e = feedStart[s + 1];
	if ( b == e )
		return;
	float acc[18];
	for ( int q = 0; q < 18; q++ )
		acc[q] = 0.0;
	float wsum = 0.0;
	for ( int k = b; k < e; k++ ) {
		int rec = floatBitsToInt( feed[k * 4 + 2] );
		float vf = rec >= 0 ? doorF( rec, 0, true ) : 1.0;
		if ( vf <= 0.0 )
			continue;
		int p = floatBitsToInt( feed[k * 4] );
		float w = feed[k * 4 + 1] * vf;
		wsum += w;
		for ( int q = 0; q < 18; q++ )
			acc[q] += w * pe[p * 18 + q];
	}
	int v = slots[s * 2], which = slots[s * 2 + 1];
	int base = which * uNVox * 24;
	for ( int a = 0; a < 6; a++ ) {
		int o = base + ( a * uNVox + v ) * 4;
		for ( int c = 0; c < 3; c++ )
			grid[o + c] = wsum > 0.0 ? acc[a * 3 + c] / wsum : 0.0;
		grid[o + 3] = 1.0;
	}
}

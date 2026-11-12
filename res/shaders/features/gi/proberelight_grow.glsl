
void main()
{
	int g = uFirst + int( gl_GlobalInvocationID.x );
	if ( int( gl_GlobalInvocationID.x ) >= uN )
		return;
	int s = linkStart[g];
	int b = pairStart[g], e = pairStart[g + 1];
	float acc[18];
	for ( int q = 0; q < 18; q++ )
		acc[q] = 0.0;
	for ( int k = b; k < e; k++ ) {
		int q = feedStart[k];
		int base = slots[q * 2 + 1] * uNVox * 24;
		for ( int a = 0; a < 6; a++ ) {
			int o = base + ( a * uNVox + slots[q * 2] ) * 4;
			for ( int c = 0; c < 3; c++ )
				acc[a * 3 + c] += grid[o + c];
		}
	}
	int base = slots[s * 2 + 1] * uNVox * 24;
	float n = float( e - b );
	for ( int a = 0; a < 6; a++ ) {
		int o = base + ( a * uNVox + slots[s * 2] ) * 4;
		for ( int c = 0; c < 3; c++ )
			grid[o + c] = acc[a * 3 + c] / n;
		grid[o + 3] = 1.0;
	}
}


void main()
{
	int j = int( gl_GlobalInvocationID.x );
	if ( j >= uN )
		return;
	float E[18];
	for ( int q = 0; q < 18; q++ )
		E[q] = 0.0;
	for ( int k = linkStart[j]; k < linkStart[j + 1]; k++ ) {
		int o = k * 12;
		int s = floatBitsToInt( links[o] );
		int rec = floatBitsToInt( links[o + 11] );
		vec3 B = vec3( bsrc[s * 4], bsrc[s * 4 + 1], bsrc[s * 4 + 2] );
		vec3 w = B * vec3( links[o + 2], links[o + 3], links[o + 4] ) * links[o + 1];
		if ( rec >= 0 )
			w *= vec3( doorF( rec, 0, false ), doorF( rec, 1, false ), doorF( rec, 2, false ) );
		for ( int a = 0; a < 6; a++ ) {
			float ca = links[o + 5 + a];
			E[a * 3] += w.x * ca;
			E[a * 3 + 1] += w.y * ca;
			E[a * 3 + 2] += w.z * ca;
		}
	}
	float kU = probeK[j * 20];
	for ( int q = 0; q < 18; q++ ) {
		float e = E[q] * kU;
		if ( uSkyOn != 0 )
			e += probeK[j * 20 + 2 + q];
		pe[j * 18 + q] = e;
	}
}

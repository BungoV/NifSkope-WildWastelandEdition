// lane LIGHTNING2 (LIGHTNINGSKY1): a storm flash's light at p (world km x, y, height) seen along d (from the eye), per
// unit of the colour the caller hands in: max(phi + 3 g D grad(phi).d, 0), phi = the diffusion Green's function of the
// strike minus its images across the Low band's extrapolated base and top. The same field as cloud_composite.frag's
// cvLtField; twin: src/gl/cloudlightning.h fieldAt (which also carries the norm; the caller's colour carries it here).
// src = (x, y, height km, 1 / delta), prm = (r0^2 km^2, 2 (base - z_b), 2 (top + z_b), 3 g D km).
float ldLtField( vec4 src, vec4 prm, vec3 p, vec3 d )
{
	float phi = 0.0, gd = 0.0;
	for ( int i = 0; i < 3; i++ ) {
		float zs = i == 0 ? src.z : ( i == 1 ? prm.y : prm.z ) - src.z;
		vec3 a = p - vec3( src.xy, zs );
		float r = sqrt( dot( a, a ) + prm.x );
		float e = exp( -r * src.w ) / r;
		float s = i == 0 ? 1.0 : -1.0;
		phi += s * e;
		gd += s * ( -e * ( src.w + 1.0 / r ) / r ) * dot( a, d );
	}
	return max( phi + prm.w * gd, 0.0 );
}

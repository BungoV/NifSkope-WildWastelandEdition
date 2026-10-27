// lane WATERUNDER1: the camera-underwater look (src/gl/cellunderwater.h). Pure: the offline gate
// (notes/waterunder1/tools/uw_readback.py) draws cell_underwater.frag, which includes this, through the real driver.
//   uwFog  = ( WATR uwFog amount, near, far, the surface height H, world z )
//   fogCol = the fog's colour, linear: half the water sky's horizon + half the WATR underwater colour (the game's own
//            underwater surface variant, fo4_water.frag), so the fog meets the surface seen from below without a seam
//   tint   = the WATR underwater colour, linear: the filter the light takes over `far` units of water (the probe
//            bake's glass rule, probebake.cpp waterFog), so the scene and the fog darken with depth below H
//   red    = 1 notint (no depth darkening), 2 nofog (no view fog)
//   fwd    = the camera's forward, world: the fog ramps over the view depth L . fwd, the game's (its image-space depth
//            of field shader, lane WATERUNDER3); vec3( 0 ) = over the ray length (WATERUNDER1, red radial)
// Out: the frame becomes scene * mul + add (dual-source blend, one pass).

void wwUnderwater( vec3 eye, vec3 Pn, vec3 Pw, bool farPlane, vec4 uwFog, vec3 fogCol, vec3 tint, int red, vec3 fwd,
	out vec3 add, out vec3 mul )
{
	add = vec3( 0.0 );
	mul = vec3( 1.0 );
	float H = uwFog.w;
	if ( Pn.z >= H )
		return;	// this pixel's ray starts above the surface: the waterline splits the screen per pixel
	vec3 d = Pw - eye;
	float L = length( d );
	vec3 dir = d / max( L, 1e-6 );
	if ( farPlane )
		L = 1.0e9;	// nothing drew there: the fog's full amount
	if ( dir.z > 1e-6 )
		L = min( L, ( H - eye.z ) / dir.z );	// the ray leaves the water at the surface (its underside is fogged too)
	float far = max( uwFog.z, 1e-3 );
	float span = max( uwFog.z - uwFog.y, 1e-3 );
	float Lf = dot( fwd, fwd ) > 0.0 ? L * max( dot( dir, fwd ), 0.0 ) : L;
	float a = clamp( uwFog.x * clamp( ( Lf - uwFog.y ) / span, 0.0, 1.0 ), 0.0, 1.0 );
	float dv = clamp( H - ( eye.z + dir.z * L ), 0.0, far );	// the light's path down to the point
	float dc = clamp( H - eye.z, 0.0, far );	// and down to the eye: the fog's own light
	vec3 Tv = vec3( 1.0 ) - clamp( uwFog.x, 0.0, 1.0 ) * ( dv / far ) * ( vec3( 1.0 ) - tint );
	vec3 Tc = vec3( 1.0 ) - clamp( uwFog.x, 0.0, 1.0 ) * ( dc / far ) * ( vec3( 1.0 ) - tint );
	if ( ( red & 1 ) != 0 ) {
		Tv = vec3( 1.0 );
		Tc = vec3( 1.0 );
	}
	if ( ( red & 2 ) != 0 )
		a = 0.0;
	add = fogCol * Tc * a;
	mul = Tv * ( 1.0 - a );
}

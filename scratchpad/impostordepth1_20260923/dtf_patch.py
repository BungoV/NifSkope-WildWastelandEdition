# Dev-only: coverage DECODED PER TEXEL then filtered (4 texelFetch), instead of the hardware filtering the ENCODED alpha.
import sys
src = open(sys.argv[1], 'rb').read().decode()
fn = '''
float covDecodedBilinear( vec2 uv )
{
	ivec2 sz = textureSize( ColourSheet, 0 );
	vec2  p  = uv * vec2( sz ) - 0.5;
	ivec2 i0 = ivec2( floor( p ) );
	vec2  f  = p - floor( p );
	ivec2 mx = sz - ivec2( 1 );
	float a00 = coverageOf( texelFetch( ColourSheet, clamp( i0, ivec2( 0 ), mx ), 0 ).a );
	float a10 = coverageOf( texelFetch( ColourSheet, clamp( i0 + ivec2( 1, 0 ), ivec2( 0 ), mx ), 0 ).a );
	float a01 = coverageOf( texelFetch( ColourSheet, clamp( i0 + ivec2( 0, 1 ), ivec2( 0 ), mx ), 0 ).a );
	float a11 = coverageOf( texelFetch( ColourSheet, clamp( i0 + ivec2( 1, 1 ), ivec2( 0 ), mx ), 0 ).a );
	return mix( mix( a00, a10, f.x ), mix( a01, a11, f.x ), f.y );
}
'''
anchor = '// --- what the viewer is asking for'
assert src.count(anchor) == 1
src = src.replace(anchor, fn + '\n' + anchor)
n0 = src.count('coverageOf( textureLod( ColourSheet, uq, 0.0 ).a )')
n1 = src.count('coverageOf( textureLod( ColourSheet, u2, 0.0 ).a )')
n2 = src.count('float cov = missed ? 0.0 : coverageOf( c.a );')
assert (n0, n1, n2) == (1, 1, 1), (n0, n1, n2)
src = src.replace('coverageOf( textureLod( ColourSheet, uq, 0.0 ).a )', 'covDecodedBilinear( uq )')
src = src.replace('coverageOf( textureLod( ColourSheet, u2, 0.0 ).a )', 'covDecodedBilinear( u2 )')
src = src.replace('float cov = missed ? 0.0 : coverageOf( c.a );', 'float cov = missed ? 0.0 : covDecodedBilinear( uv );')
open(sys.argv[2], 'wb').write(src.encode())
print('patched')

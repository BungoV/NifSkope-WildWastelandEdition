# IMPOSTORDEPTH1: coverage decoded per texel, then filtered -- behind uniform coverageDecodedFilter (default off).
P = 'E:/Projects/NifskopeWildWastelandEdition/res/shaders/impostor_oct.frag'
src = open(P, 'rb').read().decode()
cr0 = src.count('\r')
fn = '''
/* COVERAGE, DECODED THEN FILTERED (lane IMPOSTORDEPTH1, 2026-09-23).
 * `coverageDecodedFilter` false -- GL's default, what every caller that does
 * not set it gets -- is the hardware's bilinear over the ENCODED alpha, then
 * `coverageOf`: the picture before the lane. True reads the four texels with
 * texelFetch, decodes EACH, and filters the fractions.
 *
 * WHY: the encoding jumps from 0 to `cardCovBase` (160) at the floor, so a
 * filtered ENCODED alpha between an empty texel and a 0.8-coverage texel
 * crosses the 128 cut about 0.89 of the way across instead of 0.63 -- every
 * silhouette edge loses about a quarter texel, and a trunk four texels wide
 * draws at 0.87 of the mesh's width even from its own frame's angle
 * (measured: nearest-frame trunk ratio median 0.870 -> 0.944 on the 8x8 maple).
 * Same argument as `coverageOf`'s own comment: average quantities, not
 * positions on the encoding's curve. Cost: 4 fetches in place of 1 at every
 * coverage read. Mip 0 only, which is all this shader samples. */
uniform bool  coverageDecodedFilter;

float covAt( vec2 uv )
{
	if ( !coverageDecodedFilter )
		return coverageOf( textureLod( ColourSheet, uv, 0.0 ).a );
	ivec2 sz = textureSize( ColourSheet, 0 );
	vec2  p  = uv * vec2( sz ) - 0.5;
	vec2  fl = floor( p );
	ivec2 i0 = ivec2( fl );
	vec2  f  = p - fl;
	ivec2 mx = sz - ivec2( 1 );
	float a00 = coverageOf( texelFetch( ColourSheet, clamp( i0,                ivec2( 0 ), mx ), 0 ).a );
	float a10 = coverageOf( texelFetch( ColourSheet, clamp( i0 + ivec2( 1, 0 ), ivec2( 0 ), mx ), 0 ).a );
	float a01 = coverageOf( texelFetch( ColourSheet, clamp( i0 + ivec2( 0, 1 ), ivec2( 0 ), mx ), 0 ).a );
	float a11 = coverageOf( texelFetch( ColourSheet, clamp( i0 + ivec2( 1, 1 ), ivec2( 0 ), mx ), 0 ).a );
	return mix( mix( a00, a10, f.x ), mix( a01, a11, f.x ), f.y );
}
'''
anchor = '// --- what the viewer is asking for'
assert src.count(anchor) == 1
src = src.replace(anchor, fn + '\n' + anchor)
reps = [
    ('coverageOf( textureLod( ColourSheet, uq, 0.0 ).a )', 'covAt( uq )'),
    ('coverageOf( textureLod( ColourSheet, u2, 0.0 ).a )', 'covAt( u2 )'),
    ('float cov = missed ? 0.0 : coverageOf( c.a );', 'float cov = missed ? 0.0 : covAt( uv );'),
]
for a, b in reps:
    assert src.count(a) == 1, a
    src = src.replace(a, b)
assert src.count('\r') == cr0
open(P, 'wb').write(src.encode())
print('ok')

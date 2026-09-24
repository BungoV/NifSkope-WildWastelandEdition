"""VTNORMAL1: pin the chunk-sheet DDS reader's length to the rung's contraction.
fmaprobe.py re-encoded his whole sheet under each contraction of
e*e + n*n + up*up: the rung's bytes are fma(up,up,fma(n,n,e*e)) on 4194304 of
4194304 texels, build 2's are fma(up,up,fma(e,e,n*n)). Spelled out with
std::fma, the compiler has no choice left. The vector loop gets the same form."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
s = open(P, 'rb').read().decode('utf-8')
assert s.count('\r') == 0

old = """		float n = float( src[2] ) / 255.0f * 2.0f - 1.0f;   // B north
		const float inv = 1.0f / qMax( std::sqrt( e * e + n * n + up * up ), 1e-6f );
		out[i] = lodgenTerrainMsnPixel( Vector3( e * inv, n * inv, up * inv ) );"""
new = """		float n = float( src[2] ) / 255.0f * 2.0f - 1.0f;   // B north
		/* The length is spelled as the fused form the rung's compiler chose
		 * (lane VTNORMAL1, fmaprobe.py: 4194304 of 4194304 texels of a sheet
		 * agree with it). Written as a plain sum, -O3 -march=haswell is free to
		 * contract it either way, and a code change elsewhere in this file
		 * flipped it: 2 texels of a 2048 sheet moved one step. */
		const float inv = 1.0f / qMax( std::sqrt( std::fma( up, up, std::fma( n, n, e * e ) ) ), 1e-6f );
		out[i] = lodgenTerrainMsnPixel( Vector3( e * inv, n * inv, up * inv ) );"""
if s.count(old) != 1:
	sys.exit('anchor 1 count %d' % s.count(old))
s = s.replace(old, new)
old2 = """			const float n = float( vs[2] ) / 255.0f * 2.0f - 1.0f;
			const float inv = 1.0f / qMax( std::sqrt( e * e + n * n + up * up ), 1e-6f );"""
new2 = """			const float n = float( vs[2] ) / 255.0f * 2.0f - 1.0f;
			const float inv = 1.0f / qMax( std::sqrt( std::fma( up, up, std::fma( n, n, e * e ) ) ), 1e-6f );"""
if s.count(old2) != 1:
	sys.exit('anchor 2 count %d' % s.count(old2))
s = s.replace(old2, new2)
open(P, 'wb').write(s.encode('utf-8'))
print('fma ok, CR', s.count('\r'), 'cmath', '#include <cmath>' in s)

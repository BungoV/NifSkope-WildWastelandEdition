# CARDFIX1 step 5 (IMPOSTORRING1), part 4: the NifSkope card drawer draws a RING set.
# The two neighbouring azimuths, weighted by angle (IMPOSTORRING1 brief job 3: "blends the TWO
# neighbouring azimuths (not three), weights by angle"); views above the horizon use the ring frames as
# they are (no top frames). The grid path is untouched: every ring branch is behind set.ring().
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'
P = 'src/gl/impostordraw.cpp'
T = chr(9)

b = open(ROOT + P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')


def rep(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, (old[:80], c, n)
    s = s.replace(old, new)


# 1. selection
rep(T + 'camDir[0] = d[0]; camDir[1] = d[1]; camDir[2] = d[2];\n\n',
    T + 'camDir[0] = d[0]; camDir[1] = d[1]; camDir[2] = d[2];\n\n'
    + T + 'if ( set.ring() ) {\n'
    + T * 2 + '/* THE HORIZON RING (lane CARDFIX1 step 5, IMPOSTORRING1). Frame v was\n'
    + T * 2 + ' * photographed from azimuth 360*v/V at elevation 0, so the two frames that\n'
    + T * 2 + ' * bracket the camera\'s azimuth are the neighbours, and each weighs by how\n'
    + T * 2 + ' * near it is: t = the fraction of a step past the lower one. Elevation\n'
    + T * 2 + ' * picks nothing -- a ring has no top frames, and a view from above reads\n'
    + T * 2 + ' * the horizon frames as they are (the error that costs is measured, not\n'
    + T * 2 + ' * hidden). The stronger frame goes first, which is the one the crisp end\n'
    + T * 2 + ' * draws alone. Slot 2 repeats it at weight 0: a ring blends TWO. */\n'
    + T * 2 + 'const int V = set.views;\n'
    + T * 2 + 'const float twoPi = 6.28318530718f;\n'
    + T * 2 + 'float phi = std::atan2( d[1], d[0] );\n'
    + T * 2 + 'if ( phi < 0.0f )\n'
    + T * 3 + 'phi += twoPi;\n'
    + T * 2 + 'const float f = phi / twoPi * float( V );\n'
    + T * 2 + 'const float fl = std::floor( f );\n'
    + T * 2 + 'const float t = f - fl;\n'
    + T * 2 + 'const int i0 = ( ( int( fl ) % V ) + V ) % V, i1 = ( i0 + 1 ) % V;\n'
    + T * 2 + 'if ( t <= 0.5f ) {\n'
    + T * 3 + 'idx[0] = i0; w[0] = 1.0f - t; idx[1] = i1; w[1] = t;\n'
    + T * 2 + '} else {\n'
    + T * 3 + 'idx[0] = i1; w[0] = t; idx[1] = i0; w[1] = 1.0f - t;\n'
    + T * 2 + '}\n'
    + T * 2 + 'idx[2] = idx[0]; w[2] = 0.0f;\n'
    + T * 2 + '// THE RED CONTROL, the ring\'s form of the grid\'s quarter turn: every frame\n'
    + T * 2 + '// becomes the view from 90 degrees round.\n'
    + T * 2 + 'if ( opt.shuffleFrames )\n'
    + T * 3 + 'for ( int k = 0; k < 3; k++ )\n'
    + T * 4 + 'idx[k] = ( idx[k] + V / 4 ) % V;\n'
    + T * 2 + 'return;\n'
    + T + '}\n\n')

# 2. the describe block: a ring has no grid cell
rep(T + 'float fi = 0.0f, fj = 0.0f;\n' + T + 'ImpostorOct::dirToGrid( look, set.oct, &fi, &fj );\n',
    T + 'float fi = 0.0f, fj = 0.0f;\n'
    + T + 'if ( !set.ring() )\n'
    + T * 2 + 'ImpostorOct::dirToGrid( look, set.oct, &fi, &fj );\n')
rep(T + 'out << QStringLiteral( "cell %1 %2 of %3" ).arg( double( fi ) ).arg( double( fj ) ).arg( set.oct );\n',
    T + 'if ( set.ring() )\n'
    + T * 2 + 'out << QStringLiteral( "ring of %1 views, camera azimuth %2 deg" ).arg( set.views )\n'
    + T * 4 + '.arg( double( std::atan2( camDir[1], camDir[0] ) * 57.2957795f ) );\n'
    + T + 'else\n'
    + T * 2 + 'out << QStringLiteral( "cell %1 %2 of %3" ).arg( double( fi ) ).arg( double( fj ) ).arg( set.oct );\n')
rep('.arg( idx[k] % set.oct ).arg( idx[k] / set.oct )', '.arg( idx[k] % set.cols() ).arg( idx[k] / set.cols() )')

# 3. the frame count: a ring blends two
rep(T + 'const int frameCount = rs.frameCount;\n' + T + 'if ( frameCount == 3 && rs.sharpen != 1.0f ) {\n',
    T + '// a RING set blends its two neighbouring azimuths, never three (CARDFIX1 step 5)\n'
    + T + 'const int frameCount = set.ring() ? qMin( rs.frameCount, 2 ) : rs.frameCount;\n'
    + T + 'if ( frameCount > 1 && rs.sharpen != 1.0f ) {\n')

# 4. the per-frame uniforms
rep(T + 'const float du = 1.0f / float( set.oct );\n',
    T + 'const float du = 1.0f / float( set.cols() );\n')
rep(T * 2 + 'const int i = idx[k] % set.oct;\n' + T * 2 + 'const int j = idx[k] / set.oct;\n',
    T * 2 + 'const int i = idx[k] % set.cols();\n' + T * 2 + 'const int j = idx[k] / set.cols();\n')
rep(T * 3 + 'ImpostorOct::frameRect( i, j, set.oct, &u0, &v0, &dU, &dV );\n',
    T * 3 + 'if ( set.ring() ) {\n'
    + T * 4 + '// the ring\'s one row: frame v at [v/V, (v+1)/V) x [0, 1)\n'
    + T * 4 + 'u0 = float( i ) * du; v0 = 0.0f; dU = du; dV = 1.0f;\n'
    + T * 3 + '} else {\n'
    + T * 4 + 'ImpostorOct::frameRect( i, j, set.oct, &u0, &v0, &dU, &dV );\n'
    + T * 3 + '}\n')
rep(T * 3 + 'ImpostorOct::frameDir( i, j, set.oct, dir );\n' + T * 3 + 'ImpostorOct::frameBasis( dir, right, up, fwd );\n',
    T * 3 + 'if ( set.ring() ) {\n'
    + T * 4 + '/* The ring\'s own eye (docs/LODGEN_LODM_FORMAT.md 3a): ( cos p, sin p, 0 ),\n'
    + T * 4 + ' * p = 2 pi v / V; frameBasis then gives right ( -sin p, cos p, 0 ) and up\n'
    + T * 4 + ' * ( 0, 0, 1 ) -- the bake\'s camera for that frame, under the spec law\n'
    + T * 4 + ' * (a ring set is only ever baked under it). */\n'
    + T * 4 + 'const float p = 6.28318530718f * float( i ) / float( set.views );\n'
    + T * 4 + 'dir[0] = std::cos( p ); dir[1] = std::sin( p ); dir[2] = 0.0f;\n'
    + T * 4 + 'ImpostorOct::g_convention = ImpostorOct::Convention::SpecLiteral;\n'
    + T * 3 + '} else {\n'
    + T * 4 + 'ImpostorOct::frameDir( i, j, set.oct, dir );\n'
    + T * 3 + '}\n'
    + T * 3 + 'ImpostorOct::frameBasis( dir, right, up, fwd );\n')

out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(ROOT + P, 'wb').write(out)
print('patched', P)

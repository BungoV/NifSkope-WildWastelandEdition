# CARDFIX1 step 5 (IMPOSTORRING1), part 1: the bake hook photographs a HORIZON RING.
# bungo 2026-09-23 04:4x, RULED: "for fo4cs use the convention was 22.5 degrees per take" -- tree cards
# are 16 azimuths at elevation 0, uniform. WW_IMPOSTOR_RING=V: V frames in ONE row, frame v at azimuth
# 360*v/V -- the aggregate's ring layout (docs/LODGEN_LODM_FORMAT.md 3a, LODGEN_CARD_SHEETS.md 10.2).
# The sidecar says `ring V ...` INSTEAD of `oct N ...` (an old lodgen then finds no grid, rather than
# reading a 16x16 one), `frameoff v 0 ox oy` per frame, and `ringview v azim elev` per frame: the camera
# the renderer actually held for that frame, read back from GLView::viewTransform() -- the gate's echo.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'
P = 'src/nifskope_ui.cpp'
T = chr(9)

b = open(ROOT + P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')


def rep(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, (old[:70], c, n)
    s = s.replace(old, new)


# 1. the arm
rep('const int octN = qEnvironmentVariableIntValue( "WW_IMPOSTOR_OCT" );\n' + T * 5 + 'if ( octN >= 2 && octN <= 16 ) {\n',
    'const int octN = qEnvironmentVariableIntValue( "WW_IMPOSTOR_OCT" );\n'
    + T * 5 + '/* THE HORIZON RING (lane CARDFIX1 step 5 = IMPOSTORRING1; bungo 2026-09-23\n'
    + T * 5 + ' * 04:4x, RULED: "for fo4cs use the convention was 22.5 degrees per take").\n'
    + T * 5 + ' * WW_IMPOSTOR_RING=V photographs V azimuths at elevation 0, frame v at\n'
    + T * 5 + ' * 360*v/V, in ONE row: the aggregate\'s ring layout (docs/LODGEN_LODM_FORMAT.md\n'
    + T * 5 + ' * 3a), not a third one. It wins over WW_IMPOSTOR_OCT. Everything else -- the\n'
    + T * 5 + ' * matte, the 4x offscreen arm, per-frame offsets, one scale, the size ladder,\n'
    + T * 5 + ' * the coverage contract -- is the grid bake\'s, frame for frame. */\n'
    + T * 5 + 'const int ringV = qEnvironmentVariableIntValue( "WW_IMPOSTOR_RING" );\n'
    + T * 5 + 'const bool ringOn = ringV >= 4 && ringV <= 64;\n'
    + T * 5 + 'const int gCols = ringOn ? ringV : octN, gRows = ringOn ? 1 : octN;\n'
    + T * 5 + 'if ( ringOn || ( octN >= 2 && octN <= 16 ) ) {\n')

# 2. the camera per frame
rep('auto viewDir = [octN]( int i, int j, float & rx, float & rz ) {\n',
    'auto viewDir = [octN, ringOn, ringV]( int i, int j, float & rx, float & rz ) {\n'
    + T * 7 + 'if ( ringOn ) {\n'
    + T * 8 + '// the ring: elevation 0, azimuth 360*i/V, the same camera law as below\n'
    + T * 8 + 'rx = -90.0f;\n'
    + T * 8 + 'rz = 270.0f - 360.0f * float( i ) / float( ringV );\n'
    + T * 8 + 'return;\n'
    + T * 7 + '}\n')

# 3. the per-frame arrays, plus the camera echo
rep('QVector<float> frameOffX( octN * octN, 0.0f ), frameOffY( octN * octN, 0.0f );\n',
    'QVector<float> frameOffX( gCols * gRows, 0.0f ), frameOffY( gCols * gRows, 0.0f );\n'
    + T * 6 + '// the ring\'s camera echo: what the renderer held for each frame, in degrees\n'
    + T * 6 + 'QVector<float> echoAz( gCols * gRows, -999.0f ), echoEl( gCols * gRows, -999.0f );\n')

# 4. the loops and indices
rep('j < octN; j++', 'j < gRows; j++', 3)
rep('i < octN; i++', 'i < gCols; i++', 3)
rep('const int v = j * octN + i;', 'const int v = j * gCols + i;')
rep('[j * octN + i]', '[j * gCols + i]', 4)
rep('const int S_W = octN * tw, S_H = octN * th;', 'const int S_W = gCols * tw, S_H = gRows * th;')

# 5. the echo, in pass two, right where the frame's camera is set
rep('const float ox = frameOffX[j * gCols + i], oy = frameOffY[j * gCols + i];\n',
    'const float ox = frameOffX[j * gCols + i], oy = frameOffY[j * gCols + i];\n'
    + T * 8 + 'if ( ringOn ) {\n'
    + T * 9 + '/* THE ECHO: the camera paintGL will use, read back from the view, not\n'
    + T * 9 + ' * the angle asked for. Its rotation\'s ROW 2 is where the camera sits\n'
    + T * 9 + ' * (the derivation at viewDir above), so azimuth and elevation are that\n'
    + T * 9 + ' * vector\'s own. A wrong rz law shows here as a wrong azimuth. */\n'
    + T * 9 + 'const Transform vt = skope->ogl->viewTransform();\n'
    + T * 9 + 'const float ex = vt.rotation( 2, 0 ), ey = vt.rotation( 2, 1 ), ez = vt.rotation( 2, 2 );\n'
    + T * 9 + 'float az = std::atan2( ey, ex ) * 180.0f / 3.14159265f;\n'
    + T * 9 + 'if ( az < 0.0f )\n'
    + T * 10 + 'az += 360.0f;\n'
    + T * 9 + 'echoAz[j * gCols + i] = az;\n'
    + T * 9 + 'echoEl[j * gCols + i] = std::asin( qBound( -1.0f, ez, 1.0f ) ) * 180.0f / 3.14159265f;\n'
    + T * 8 + '}\n')

# 6. the sidecar's layout line
rep('ms << "oct " << octN << " " << tw << " " << th',
    'ms << ( ringOn ? "ring " : "oct " ) << ( ringOn ? ringV : octN ) << " " << tw << " " << th')
rep('ms << "frameclamped " << clamped << "\\n";\n',
    'ms << "frameclamped " << clamped << "\\n";\n'
    + T * 6 + '/* THE RING\'S CAMERA ECHO, one line per frame: `ringview v azim elev`, in\n'
    + T * 6 + ' * degrees, read back from the view at the moment the frame was drawn.\n'
    + T * 6 + ' * Unknown to every reader, which skips lines it does not name. */\n'
    + T * 6 + 'if ( ringOn )\n'
    + T * 7 + 'for ( int v = 0; v < gCols; v++ )\n'
    + T * 8 + 'ms << "ringview " << v << " " << echoAz[v] << " " << echoEl[v] << "\\n";\n')

out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(ROOT + P, 'wb').write(out)
print('patched', P, 'octN left:', s.count('octN'))

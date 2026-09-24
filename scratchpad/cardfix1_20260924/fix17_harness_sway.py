# CARDFIX1 step 6: the preview harness can draw the card SWAYING (the drawer's existing
# swayAmplitude / swayPhase uniforms, which nothing drove) -- for the GIF bungo asked for.
# WW_IMPOSTOR_SWAY_AMP (default 0 = still, every existing caller unchanged) and
# WW_IMPOSTOR_SWAY_PHASE (radians). Logged by name. LF-only.
P = 'E:/Projects/NifskopeWWE-cardfix1/src/impostorpreviewtest.cpp'
T = chr(9)
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')
old = (T + 'if ( s.opt.shuffleFrames )\n'
       + T * 2 + 's.log << QStringLiteral( "SHUFFLED: the frame choice is deliberately wrong (red control)" );\n')
assert s.count(old) == 1
new = old + (
    '\n' + T + '/* THE SWAY, on request (CARDFIX1 step 6): the drawer\'s wind shear at a fixed\n'
    + T + ' * amplitude and phase, so a run of phases makes a moving picture. 0 = still. */\n'
    + T + 's.opt.swayAmplitude = float( qEnvironmentVariable( "WW_IMPOSTOR_SWAY_AMP" ).toDouble() );\n'
    + T + 's.opt.swayPhase = float( qEnvironmentVariable( "WW_IMPOSTOR_SWAY_PHASE" ).toDouble() );\n'
    + T + 'if ( s.opt.swayAmplitude != 0.0f )\n'
    + T * 2 + 's.log << QStringLiteral( "sway: amplitude %1, phase %2 rad" ).arg( s.opt.swayAmplitude ).arg( s.opt.swayPhase );\n')
s = s.replace(old, new)
out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('patched', P)

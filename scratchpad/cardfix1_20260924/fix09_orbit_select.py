# CARDFIX1 step 5 (IMPOSTORRING1), part 6: the preview harness can say, per orbit view, WHICH frames the
# drawer chose and at what weights (WW_IMPOSTOR_ORBIT_SELECT=1, off by default -- every existing caller's
# log is unchanged). tests/spells/impostor_ring.sh row R6 checks the ring's two-neighbour, weight-by-angle
# rule against these lines with arithmetic of its own.
ROOT = 'E:/Projects/NifskopeWWE-cardfix1/'
P = 'src/impostorpreviewtest.cpp'
T = chr(9)

b = open(ROOT + P, 'rb').read()
cr = b.count(b'\r')
s = b.decode('utf-8')

old = (T * 5 + 'if ( meshCov == 0 || frac > 0.95 ) {\n'
       + T * 6 + 'st.log << QStringLiteral( "  EXCLUDED: mesh coverage %1 is degenerate" )\n')
assert s.count(old) == 1, s.count(old)
new = (T * 5 + '// THE SELECTION, per view, on request (CARDFIX1 step 5): the frames the\n'
       + T * 5 + '// drawer chose here and their weights, so a gate can check the rule.\n'
       + T * 5 + 'if ( qEnvironmentVariableIntValue( "WW_IMPOSTOR_ORBIT_SELECT" ) != 0 )\n'
       + T * 6 + 'for ( const QString & line : ImpostorDraw::describeSelection( scene, st.set, st.offset, st.opt ) )\n'
       + T * 7 + 'st.log << QStringLiteral( "  select %1" ).arg( line );\n\n'
       + old)
s = s.replace(old, new)
out = s.encode('utf-8')
assert out.count(b'\r') == cr
open(ROOT + P, 'wb').write(out)
print('patched', P)

P = 'E:/Projects/NifskopeWildWastelandEdition/src/impostorpreviewtest.cpp'
b = open(P, 'rb').read(); s = b.decode('utf-8')
old = '\ts.log << QStringLiteral( "depth search: %1" ).arg( rs.searchSteps > 0\n'
new = '\ts.log << QStringLiteral( "depth search: %1" ).arg( !rs.parallax\n\t\t\t? QStringLiteral( "none -- the frame is drawn flat, not moved by its depth" )\n\t\t\t: rs.searchSteps > 0\n'
assert s.count(old) == 1
s = s.replace(old, new)
assert s.encode().count(b'\r') == 0
open(P, 'wb').write(s.encode()); print('ok')

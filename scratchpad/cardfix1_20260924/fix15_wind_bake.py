# CARDFIX1 step 6 (IMPOSTORWIND1 job 3, sway A), part 2: the bake's sway law.
# bungo 2026-09-24 21:1x RULED sway A: _n.A = W x h, W the tree's own vertex-alpha wind weight (channel
# 11 G, 0 on a shape without the tree-animation flag), h the linear height up from the silhouette's
# bottom row. A model with NO tree-animation shape keeps the synthetic h^2 (0.35 + 0.65 r) byte for byte.
# The sidecar says which: `sway model` or `sway synthetic`. nifskope_ui.cpp is LF-only.
P = 'E:/Projects/NifskopeWWE-cardfix1/src/nifskope_ui.cpp'
T = chr(9)
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')


def rep(old, new):
    global s
    c = s.count(old)
    assert c == 1, (old[:70], c)
    s = s.replace(old, new)


rep(T * 6 + 'wwLodMaskByTree = anyTree ? 1 : 0;\n'
    + T * 6 + 'ms << "mask " << ( anyTree ? "tree" : "alpha" ) << "\\n";\n',
    T * 6 + 'wwLodMaskByTree = anyTree ? 1 : 0;\n'
    + T * 6 + 'ms << "mask " << ( anyTree ? "tree" : "alpha" ) << "\\n";\n'
    + T * 6 + '/* THE SWAY SOURCE (IMPOSTORWIND1 sway A, bungo 2026-09-24 21:1x): a\n'
    + T * 6 + ' * model with a tree-animation shape sways by its OWN wind weight, read\n'
    + T * 6 + ' * off channel 11 G; one without keeps the synthetic law. The same test\n'
    + T * 6 + ' * the mask rule uses, so the two can never disagree about a model. */\n'
    + T * 6 + 'ms << "sway " << ( anyTree ? "model" : "synthetic" ) << "\\n";\n')

old = (T * 10 + '// sway: h^2 * (0.35 + 0.65 r), h up from the coverage\'s bottom row\n'
       + T * 10 + 'const float h = float( bottom - y ) / rows;\n'
       + T * 10 + 'const float rr = qMin( 1.0f, std::fabs( float( x ) - cxCol ) / halfSpan );\n'
       + T * 10 + 'const int sway = qBound( 0, int( h * h * ( 0.35f + 0.65f * rr ) * 255.0f + 0.5f ), 255 );\n')
new = (T * 10 + '/* sway, h up from the coverage\'s bottom row. SWAY A (IMPOSTORWIND1): a\n'
       + T * 10 + ' * model with a tree-animation shape writes W x h, W its own vertex-alpha\n'
       + T * 10 + ' * wind weight (channel 11 G, un-premultiplied like the mask; 0 on a shape\n'
       + T * 10 + ' * the game never moves). Else the synthetic h^2 * (0.35 + 0.65 r), unchanged\n'
       + T * 10 + ' * to the byte. */\n'
       + T * 10 + 'const float h = float( bottom - y ) / rows;\n'
       + T * 10 + 'int sway;\n'
       + T * 10 + 'if ( wwLodMaskByTree ) {\n'
       + T * 11 + 'const int w = unp( qGreen( tM.pixel( x, y ) ) );\n'
       + T * 11 + 'sway = qBound( 0, int( float( w ) * h + 0.5f ), 255 );\n'
       + T * 10 + '} else {\n'
       + T * 11 + 'const float rr = qMin( 1.0f, std::fabs( float( x ) - cxCol ) / halfSpan );\n'
       + T * 11 + 'sway = qBound( 0, int( h * h * ( 0.35f + 0.65f * rr ) * 255.0f + 0.5f ), 255 );\n'
       + T * 10 + '}\n')
rep(old, new)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('patched', P)

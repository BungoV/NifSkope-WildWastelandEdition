R = 'E:/Projects/NifskopeWWE-seam1/'
BS = chr(92)


def patch(path, pairs):
    s = open(R + path, newline='').read()
    cr = s.count('\r')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, (path, n, old[:80])
        s = s.replace(old, new)
    assert s.count('\r') == cr
    open(R + path, 'w', newline='').write(s)
    print('ok', path)


src = open(R + 'src/nifcli.cpp', newline='').read()
L = [l for l in src.split('\n') if '[--vt-height]' in l and '<<' in l]
assert len(L) == 1
L = L[0]
col = L.index('a fourth')
pre = '\t\t  << "         '


def u(opt, text):
    first = pre + opt
    return first + ' ' * (col - len(first)) + text + BS + 'n"'


def cont(text):
    return '\t\t  << "' + ' ' * (col - len('\t\t  << "')) + text + BS + 'n"'


new_lines = [
    u('[--vt-fill-vanilla]', 'blend the ground no LAND record paints'),
    cont("toward Bethesda's own terrain LOD colour"),
    cont('for those cells (read at bake time from'),
    cont('the load order, never shipped), tone-'),
    cont('matched where painted land meets it.'),
    cont('OFF by default.'),
]
patch('src/nifcli.cpp', [
    ('\t\telse if ( t == QLatin1String( "--vt-height" ) ) lgVt.height = true;\n',
     '\t\telse if ( t == QLatin1String( "--vt-height" ) ) lgVt.height = true;\n'
     '\t\telse if ( t == QLatin1String( "--vt-fill-vanilla" ) ) lgVt.vanillaFill = true;\n'),
    (L + '\n', '\n'.join(new_lines) + '\n' + L + '\n'),
])
N = BS + 'n'
patch('src/lodgenmanager.cpp', [
    ('\t\t\txB( f, "LodgenVtCoverInColorCheck", QStringLiteral( "vtCoverInColor" ),',
     '\t\t\txB( f, "LodgenVtFillVanillaCheck", QStringLiteral( "vtFillVanilla" ),\n'
     '\t\t\t\ttr( "Fill unpainted ground with vanilla\'s colour" ), false,\n'
     '\t\t\t\ttr( "Ground no landscape record paints is blended toward the game\'s own' + N + '"\n'
     '\t\t\t\t\t"terrain LOD colour, matched in tone where painted ground meets it.' + N + '"\n'
     '\t\t\t\t\t"Command line: --vt-fill-vanilla" ) );\n'
     '\t\t\txB( f, "LodgenVtCoverInColorCheck", QStringLiteral( "vtCoverInColor" ),'),
    ('"vtHeight", "vtCoverInColor", "vtHalfAux" } ) {',
     '"vtHeight", "vtFillVanilla", "vtCoverInColor", "vtHalfAux" } ) {'),
    ('\t\to.height = xb( "vtHeight" );\n',
     '\t\to.height = xb( "vtHeight" );\n\t\to.vanillaFill = xb( "vtFillVanilla" );\n'),
])

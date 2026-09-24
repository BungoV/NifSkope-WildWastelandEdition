"""Apply scratchpad/native0_20260910/HOOKUP_CHANGE_NEEDED.md exactly, blocks taken from the
note's own fenced code by index. Dry run unless --apply. Every anchor: count == 1 or refuse.
Files are LF-only (0 CR) and must stay so."""
import re, sys, hashlib
APPLY = '--apply' in sys.argv
R = 'E:/Projects/NifskopeWildWastelandEdition/'
note = open(R+'scratchpad/native0_20260910/HOOKUP_CHANGE_NEEDED.md', encoding='utf-8').read()
B = [b for _, b in re.findall(r'```(\w*)\n(.*?)```', note, re.S)]
assert len(B) == 15, len(B)

def load(p):
    b = open(R+p, 'rb').read()
    assert b.count(b'\r') == 0, (p, 'has CR')
    return b.decode('utf-8')

edits = {}   # path -> list of (anchor, replacement, label)
def E(path, anchor, repl, label):
    edits.setdefault(path, []).append((anchor, repl, label))

# ---- A. src/lodgen.cpp
L = 'src/lodgen.cpp'
E(L, '#include "esmdata.h"\n', '#include "esmdata.h"\n' + B[1], 'A1 include after esmdata.h')
rung3 = '/* ================= rung 3: per-placement AO bake'
E(L, rung3, B[2] + '\n' + rung3, 'A2 loader before rung 3 (after the anonymous namespace)')
E(L, B[4], B[5] + B[4], 'A3 placement before swaying')
E(L, B[6], B[6] + B[7], 'A4 lighting after ao')
# ---- lodgen.h
H = 'src/lodgen.h'
E(H, '#include <QStringList>\n', '#include <QStringList>\n#include <vector>\n', 'H <vector>')
E(H, 'bool lodgenIsTreeModel( const QString & model );\n',
     'bool lodgenIsTreeModel( const QString & model );\n' + B[3], 'H loader decl')
# ---- B. src/nifcli.cpp
N = 'src/nifcli.cpp'
E(N, '#include "lodgen.h"\n', '#include "lodgen.h"\n' + B[8] + '#include "lodifile.h"\n', 'B1 includes')
E(N, '\tQString lgLodmCheck, lgLodvCheck;\n', '\tQString lgLodmCheck, lgLodvCheck;\n' + B[9], 'B2 option vars')
E(N, '\t\telse if ( t == QLatin1String( "--lodm-check" ) ) lgLodmCheck = next();\n',
     '\t\telse if ( t == QLatin1String( "--lodm-check" ) ) lgLodmCheck = next();\n' + B[10], 'B3 parse')
U = '\t\t  << "  lodgen ... --terrain-region ... [--slot-fallback]\\n"\n'
E(N, U, B[11] + U, 'B4 usage')
E(N, 'int cardAuxDiv )', 'int cardAuxDiv, const QString & nativeDir, const QString & nativeVerifyLodo,\n\tconst QString & nativeVerifyLodi, const QString & nativeFixture )', 'B5 signature')
E(N, '\tif ( !lodmCheck.isEmpty() ) {\n', B[12] + '\tif ( !lodmCheck.isEmpty() ) {\n', 'B5 early commands')
E(N, '\t\tQDir().mkpath( outDir );\n', '\t\tQDir().mkpath( outDir );\n' + B[13], 'B5 arming')
E(N, '\t\tif ( arrays && !writtenBto.isEmpty() ) {\n', B[14] + '\t\tif ( arrays && !writtenBto.isEmpty() ) {\n', 'B5 writing')
E(N, '\t\t\tlgCardAuxDiv );', '\t\t\tlgCardAuxDiv, lgNativeDir, lgNativeVerifyLodo, lgNativeVerifyLodi, lgNativeFixture );', 'B6 call')

ok = True
for path, lst in edits.items():
    s = load(path); before = s.encode('utf-8')
    print(path, 'before: bytes', len(before), 'LF', before.count(b'\n'), 'sha', hashlib.sha256(before).hexdigest()[:16])
    for anchor, repl, label in lst:
        n = s.count(anchor)
        print('  %-52s count=%d %s' % (label, n, 'OK' if n == 1 else 'REFUSE'))
        if n != 1: ok = False; continue
        s = s.replace(anchor, repl)
    after = s.encode('utf-8')
    assert b'\r' not in after
    print('  after: bytes', len(after), 'LF', after.count(b'\n'), 'dLF', after.count(b'\n') - before.count(b'\n'))
    if APPLY and ok:
        open(R+path, 'wb').write(after)
        print('  WRITTEN', path)
print('ALL ANCHORS OK' if ok else 'REFUSED: an anchor was not unique', '| mode', 'APPLY' if APPLY else 'DRY-RUN')
sys.exit(0 if ok else 1)

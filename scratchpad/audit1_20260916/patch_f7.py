"""AUDIT1 step 6, F7: the exe reports `aggregateStride 0` for a version-5 file
whose header holds 48.

F3/F4 made the aggregate RULES reach version 5, and the doctored files are now
refused. The REPORTING site was not in that change and still carries the lone
version-4 ternary, so `--native-verify` on bake/aggreal -- 97 aggregates, stride
48 in the bytes -- prints `lodi aggregateStride 0` beside `aggregateCount 97`.
Its five siblings in the same block (`aggregateCount`, `coveredCount`,
`aggregateViews`, `aggSwitchPx`, `aggBandRatio`) all print the field straight.

The ternary cannot simply be dropped: `aggregateStride` DEFAULTS to
LODI_AGGREGATE_STRIDE (src/lodifile.h:377), not to 0, so a version-3 file, which
never reads the word, would start printing 48 -- a different lie. The word is
printed exactly when the reader READ it, which is v4 or v5.

Refuter, both directions, on files this lane already has:
  bake/aggreal          v5, aggregateCount 97 -> must report 48
  bake/sanctuary_fo4cs  v5, aggregateCount 0  -> must report what its bytes hold
"""
import io
import os
import sys
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodifile.cpp'
OLD = ('\t\t<< QString( "aggregateStride %1" ).arg( h.version == '
       'LODI_VERSION_AGGREGATE ? h.aggregateStride : quint16( 0 ) )')
NEW = ('\t\t/* v4 AND v5 (lane AUDIT1, 2026-09-17): both read the word at 0xC8, so\n'
       '\t\t * both report it. NOT an unguarded `h.aggregateStride` -- the field\n'
       '\t\t * defaults to LODI_AGGREGATE_STRIDE rather than to 0, so a version-3\n'
       '\t\t * file, which never reads the word, would report 48 out of thin air.\n'
       '\t\t * Its five siblings below print straight because their defaults are 0. */\n'
       '\t\t<< QString( "aggregateStride %1" ).arg(\n'
       '\t\t\t( h.version == LODI_VERSION_AGGREGATE || h.version == LODI_VERSION_PLACEMENT_AO )\n'
       '\t\t\t\t? h.aggregateStride : quint16( 0 ) )')


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    n = s.count(OLD)
    if n != 1:
        print('ABORT: the describe anchor appears %d times' % n)
        return 1
    out = s.replace(OLD, NEW, 1)
    if out.count('\r') != s.count('\r'):
        print('ABORT: CR count moved')
        return 1
    d = os.path.dirname(P)
    f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d,
                                    delete=False, suffix='.tmp')
    f.write(out)
    f.close()
    os.replace(f.name, P)
    print('src/lodifile.cpp: %d -> %d bytes, CR %d, LF %d'
          % (len(s), len(out), out.count('\r'), out.count('\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

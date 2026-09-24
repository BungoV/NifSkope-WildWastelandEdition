"""Re-stamp the LAND1 provenance block against today's two-part exe.

Written with the Write tool, not a heredoc: the anchors carry backslashes
(an escaped pipe in a markdown table) and apostrophes, both of which a heredoc
eats.  Every replacement asserts it matched exactly once.
"""
import os
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition'
DOC = os.path.join(ROOT, 'docs/LODGEN_TERRAIN_VT.md')

BAR = chr(92) + '|'   # an escaped pipe, built rather than written


def sub(text, old, new, label):
    n = text.count(old)
    assert n == 1, 'anchor %s matched %d times, expected 1' % (label, n)
    return text.replace(old, new)


def main():
    b = open(DOC, 'rb').read()
    assert b.count(b'\r') == 0, 'doc is not LF-only'
    s = b.decode('utf-8')
    before = len(s)

    # 1. the stale hash table -> today's.
    s = sub(s,
            '| `src/lodgen.cpp` | `e0c7b23f0fba593a` | 523,229 | 12016 |\n'
            '| `src/lodgen.h` | `a912b948fe79ebbf` | 71,894 | 1271 |\n'
            '| `src/nifcli.cpp` | `e39869679ec64713` | 317,886 | 7003 |\n',
            '| `src/lodgen.cpp` | `3b928277034b0406` | 535,632 | 12320 |\n'
            '| `src/lodgen.h` | `b03d36199f8d50a7` | 75,006 | 1342 |\n'
            '| `src/nifcli.cpp` | `1b1a696ee324c8eb` | 335,580 | 7350 |\n',
            'hash table')

    # 2. the exe the numbers were taken on: Part A's, then Part B's.
    s = sub(s,
            'into `scratchpad/land1_20260912/out/` made by `release/NifSkope.exe` (07:42:22,\n'
            '21,861,376 bytes, sha1 `902223bd99dba4bfaf5d621fe36eb12fc4cf0272`) -- 119 sweep',
            'into `scratchpad/land1_20260912/out/` made by `release/NifSkope.exe` (07:42:22,\n'
            '21,861,376 bytes, sha1 `902223bd99dba4bfaf5d621fe36eb12fc4cf0272`), and section 2.5h\'s\n'
            'from bakes by the exe that carries BOTH of this lane\'s parts (08:42:33,\n'
            '21,935,616 bytes, sha1 `1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`, objects\n'
            '`GeneratedFiles/.obj/lodgen.o` and `nifcli.o` both 08:42:31) -- 119 sweep',
            'exe stamp')

    # 3. line numbers that Part B moved. Each anchor text re-found today.
    s = sub(s, '| `lodgen.cpp:6297` |', '| `lodgen.cpp:6304` |', 'scale refusal')
    s = sub(s, '| `nifcli.cpp:6175, 6201` |', '| `nifcli.cpp:6516, 6542` |', 'land-guide flag')
    s = sub(s, '| `nifcli.cpp:6204, 6205` |', '| `nifcli.cpp:6551, 6552` |', 'scale/slope flags')

    # 4. Part B's own rows, appended to the same anchor table.
    tail = ('| 5 `--land-guide-scale` and `--land-guide-slope` | `nifcli.cpp:6551, 6552` | '
            '`else if ( t == QLatin1String( "--land-guide-scale" ) ) '
            'lodgenSetLandGuideScale( next().toFloat() );` |\n')
    rows = (
        '| INCR1 the ledger structs, and the refusal enum beside them | `lodgen.h:1287, 1296, 1335` | '
        '`struct LodgenLedgerEntry` / `struct LodgenLedger` / `LODGEN_INCR_NO_LEDGER,` |\n'
        '| INCR1 the per-chunk input digest, and that its ring loop is ONE cell wide on all four sides | '
        '`lodgen.cpp:12070` | `QString lodgenChunkInputDigest( const EsmWorld & world, int dim, int cx, int cy,` |\n'
        '| INCR1 the LTEX texture-set row of the dependency map -- the blind spot a loose-file override '
        'would otherwise have walked through | `lodgen.cpp:12107, 12140, 12144` | '
        '`auto feedLtex = [&]( quint32 form ) {` |\n'
        '| INCR1 an asset is digested by its BYTES, through the same reader the bake uses, which is why '
        '`--data-root` may sit on the skip list | `lodgen.cpp:12032` | '
        '`static QString lodgenLedgerAssetDigest( const QString & dataRoot, const QString & relPath,` |\n'
        '| INCR1 the switch digest, and the skip list it drops -- `--vt`, `--native` and '
        '`--vanilla-lod-root` are NOT on it | `nifcli.cpp:2498, 2525` | '
        '`static const char * const gLgSwitchSkip[] = {` |\n'
        '| INCR1 the four refusals, each naming itself and exiting before a byte is written | '
        '`nifcli.cpp:3681, 3690, 3700, 3727` | `refused: --incremental has nothing to diff against -- ` |\n'
        '| INCR1 the whole-region refusal names THREE passes, not five: the merge and the far-ring '
        'simplify are per-file loops and are not refused | `nifcli.cpp:3727` | '
        '`refused: --atlas, --arrays and --impostors each build ONE region-wide ` |\n'
        '| INCR1 the one-cell neighbour widening, applied on top of the digest\'s own ring | '
        '`nifcli.cpp:3791` | `if ( j.cx <= dx + d && dx <= j.cx + d && j.cy <= dy + d && dy <= j.cy + d ) {` |\n'
        '| INCR1 the census line, printed every run | `nifcli.cpp:3804` | `"%6 by neighbour)" )` |\n'
        '| INCR1 the ledger is written LAST, AFTER the merge has rewritten every `.BTO` -- the defect '
        'gate B3 caught | `nifcli.cpp:4060, 4113` | `THE LEDGER GOES HERE, LAST` |\n')
    s = sub(s, tail, tail + rows, 'part B rows')

    out = s.encode('utf-8')
    assert out.count(b'\r') == 0, 'CR crept in'
    open(DOC, 'wb').write(out)
    print('docs/LODGEN_TERRAIN_VT.md %d -> %d bytes, CR 0, LF %d'
          % (before, len(out), out.count(b'\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

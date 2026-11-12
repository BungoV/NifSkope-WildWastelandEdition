"""Per-stage DXC level selection (HLSLBULK1 R8).
  python -I -X utf8 tests/hlsllive/optsel.py <tree> <a4 log or a4.txt> [--level -O0]
Every stage of a program the A4 gate reported FAIL at the default level (stamps.DEFAULT_OPT, -O3) is written to
res/shaders/hlsl/optlevels.txt as `<stage file name>\\t<level>` (default -O0), keeping the lines already there. A stage shared with a
passing program drops to the lower level too (one .spv per stage file). Then rebuild (build_all.py) and rerun A4: a program that still
fails at -O0 is a real port bug (particles_cell, stf_default, cell_volair base). Prints how many stages moved."""
import os
import re
import sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import a2_iface
import stamps


def main():
    a = sys.argv[1:]
    tree = a[0].rstrip('/')
    level = a[a.index('--level') + 1] if '--level' in a else '-O0'
    failed = set()
    for ln in open(a[1], encoding='utf-8', errors='replace'):
        m = re.match(r'FAIL\s+\S+\s+(\S+\.prog\S*)', ln)
        if m:
            failed.add(m.group(1))
    table = os.path.join(tree, 'res/shaders/hlsl/optlevels.txt')
    cur = {k: v[0] for k, v in stamps.load_table(table, 2).items()}
    moved = 0
    seen = 0
    for _pf, st, _vd, name in a2_iface.items(tree):
        if name not in failed:
            continue
        seen += 1
        for _e, path, _s in st:
            b = os.path.basename(path)
            if cur.get(b) != level:
                cur[b] = level
                moved += 1
    open(table, 'w', newline='\n').write(''.join('%s\t%s\n' % kv for kv in sorted(cur.items())))
    print('optsel: %d FAIL programs of %d named, %d stages set to %s, %d lines in optlevels.txt' % (seen, len(failed), moved, level, len(cur)))


main()

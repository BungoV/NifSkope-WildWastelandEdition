"""lane TIMING1: every Fusion Shaders row of the Display panel (src/cellworkspace.cpp, the list after
'// Fusion Shaders') has a row in src/ui/cellpresettable.h, and the table names no row the panel lacks.
usage: python -I coverage.py <tree> [--red]   (--red drops the table's first row: it must FAIL)"""
import re
import sys


def panel_rows(tree):
    src = open(tree + '/src/cellworkspace.cpp', encoding='utf-8', errors='replace').read()
    looks = open(tree + '/src/gl/celllooks.cpp', encoding='utf-8', errors='replace').read()
    pin = dict(re.findall(r'case WwLook::(\w+): return "(WW_\w+)";', looks))
    names = re.findall(r'\{ "[^"]*", "(\w+)", "WW/Cell[^"]*" \},', looks)
    hdr = open(tree + '/src/gl/celllooks.h', encoding='utf-8', errors='replace').read()
    m = re.search(r'enum class WwGroup[^{]*\{(.*?)\};', hdr, re.S)
    enum = [x.strip().split('=')[0].strip() for x in re.sub(r'//[^\n]*', '', m.group(1)).split(',') if x.strip()]
    group = {g: names[i] for i, g in enumerate(enum) if i < len(names)}
    a = src.index('// Fusion Shaders')
    body = src[a:src.index('showPanel = wwMenuAsDisplay', a)]
    # PRTOFF1: with the PRT off (main) the GI group's row is the Ray-traced GI's
    body = re.sub(r'if \( wwCellPrtOn\(\) \)\s*head\( F, "CellWorkspaceCellGi".*?\n\s*else[^\n]*\n', '', body, flags=re.S)
    keys = []
    for ln in body.split('\n'):
        s = ln.strip()
        h = re.match(r'head\( F, "([^"]*)"(?:, "([^"]*)")?', s)
        if h:
            keys.append(h.group(1) or h.group(2))
            continue
        g = re.match(r'master\( F, WwGroup::(\w+) \)', s)
        if g:
            keys.append('CellWorkspaceGroup_' + group[g.group(1)])
            continue
        k = re.match(r'kid\( "([^"]*)", tr\( "[^"]+" \)(?:, "([^"]*)")?', s)
        if k:
            keys.append(k.group(1) or k.group(2))
            continue
        lk = re.match(r'look\( WwLook::(\w+),', s)
        if lk:
            keys.append('CellWorkspaceLook_' + pin[lk.group(1)])
    return keys


def main():
    tree = sys.argv[1]
    red = '--red' in sys.argv
    table = re.findall(r'^\t\t\{ "(CellWorkspace\w+)",', open(tree + '/src/ui/cellpresettable.h', encoding='utf-8').read(), re.M)
    if red:
        table = table[1:]
    rows = panel_rows(tree)
    missing = [k for k in rows if k not in table]
    extra = [k for k in table if k not in rows]
    print('panel Fusion Shaders rows %d, table rows %d%s' % (len(rows), len(table), ' (RED: first table row dropped)' if red else ''))
    for k in missing:
        print('FAIL a panel row the table lacks:', k)
    for k in extra:
        print('FAIL a table row the panel lacks:', k)
    ok = not missing and not extra and len(rows) > 40
    print('COVERAGE PASS' if ok else 'COVERAGE FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())

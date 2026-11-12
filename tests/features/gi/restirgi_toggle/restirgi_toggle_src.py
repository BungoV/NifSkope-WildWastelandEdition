"""RTEMIS1 toggle (bungo 11-12): ReSTIR GI is its own Show row, off by default, at any RT quality.
Checks the source: the "ReSTIR GI" tick beside the RT quality menu (no tooltip, live), the reader (WW_CELL_RESTIRGI pin,
WW/CellRestirGi default false), the gate (no RT quality in it but under WW_CELL_RESTIRGI_GATE_RED=preset), the stage rows,
and no "High = + ReSTIR GI" text left in the RT quality menu.
usage: python -I restirgi_toggle_src.py <tree> [--rev <git rev>]   (--rev reads the files at that commit: the red is the
merge before the toggle, 96f059d5, which must FAIL)"""
import re
import subprocess
import sys

GIT = 'E:/Tools/Git/cmd/git.exe'


def reader(tree, rev):
    def rd(rel):
        if rev:
            return subprocess.run([GIT, '-C', tree, 'show', '%s:%s' % (rev, rel)], capture_output=True).stdout.decode('utf-8', 'replace')
        return open(tree + '/' + rel, encoding='utf-8', errors='replace').read()
    return rd


def main():
    tree = sys.argv[1]
    rev = sys.argv[sys.argv.index('--rev') + 1] if '--rev' in sys.argv else None
    rd = reader(tree, rev)
    ws, cell, inc = rd('src/cellworkspace.cpp'), rd('src/features/gi/rtddgicell.cpp'), rd('src/wwswitchstages.inc')
    res = []

    def check(name, ok):
        res.append(ok)
        print('%s %s' % ('PASS' if ok else 'FAIL', name))

    a = ws.find('"CellWorkspaceRtPreset"')
    m = re.search(r'\{\s*QAction \* (\w+) = tick\( showMenu, tr\( "ReSTIR GI" \), "CellWorkspaceRestirGi", rtddgiRestirGiOn\(\) \);(.*?)\n\t\}', ws, re.S)
    check('row "ReSTIR GI" (CellWorkspaceRestirGi) is a Show-menu tick', m is not None)
    if m:
        body = m.group(2)
        check('row sits after the RT quality menu', 0 <= a < m.start())
        check('row has no tooltip (no menu text)', 'setToolTip' not in body)
        check('row greys while WW_CELL_RESTIRGI pins it', 'setEnabled( !rtddgiRestirGiPinned() )' in body)
        check('row is live: toggled -> rtddgiRestirGiSetOn + updateShaders', 'rtddgiRestirGiSetOn( on )' in body and 'updateShaders()' in body)
    rt = ws[a:ws.find('} );\n\t}', a)] if a >= 0 else ''
    check('RT quality menu text no longer says High = + ReSTIR GI', a >= 0 and '+ ReSTIR GI' not in rt.replace('\r', ''))

    on = re.search(r'bool rtddgiRestirGiOn\(\)\n\{(.*?)\n\}', cell.replace('\r', ''), re.S)
    check('reader rtddgiRestirGiOn exists', on is not None)
    if on:
        check('reader: WW_CELL_RESTIRGI pins it first', 'wwEnvIsSet( "WW_CELL_RESTIRGI" )' in on.group(1))
    check('key WW/CellRestirGi defaults false', re.search(r'value\( QStringLiteral\( "WW/CellRestirGi" \), false \)', cell) is not None)
    gd = re.search(r'bool rtddgiRestirGiShaderDefine\(\)\n\{(.*?)\n\}', cell.replace('\r', ''), re.S)
    why = re.search(r'static QString restirGiOffWhy\(\)\n\{(.*?)\n\}', cell.replace('\r', ''), re.S)
    gate = (gd.group(1) if gd else '') + (why.group(1) if why else '')
    check('gate reads the row (rtddgiRestirGiOn)', 'rtddgiRestirGiOn()' in gate)
    pre = gate.split('WW_CELL_RESTIRGI_GATE_RED')
    check('gate reads the RT quality only under WW_CELL_RESTIRGI_GATE_RED=preset',
        len(pre) > 1 and 'rtddgiRtPreset()' not in pre[0] and 'rtddgiRtPreset()' not in gate.split('} else if')[-1])
    for k in ('WW_CELL_RESTIRGI', 'WW_CELL_RESTIRGI_GATE_RED'):
        check('stage row %s = 4 (shaders: live, a set reloads them)' % k, re.search(r'^\{ "%s", 4,' % k, inc, re.M) is not None)
    n, bad = len(res), res.count(False)
    print('restirgi toggle: %d checks, %d failed%s' % (n, bad, ' (rev %s)' % rev if rev else ''))
    print('TOGGLE PASS' if bad == 0 and n >= 13 else 'TOGGLE FAIL')
    return 0 if bad == 0 and n >= 13 else 1


sys.exit(main())

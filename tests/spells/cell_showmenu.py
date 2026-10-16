"""Lane SHOWMENU1's judge: the Cell panel's Show dropdown is the one home of every viewport switch.

Reads what the window showed (a dump from the in-app driver WW_SHOWMENU_GATE, or from
cell_showmenu_uia.ps1 for an exe that has no driver) and the viewport PNGs, and decides.
It shares no code with the panel.

  python cell_showmenu.py layout <dump>                       menu sections + order, Water once, no boxes
  python cell_showmenu.py pixels <dir> <name> [<name> ...]    base/flip/back per name, plus magenta
  python cell_showmenu.py persist <end dump> <relaunch dump>  every tick and submenu state survived
Exit 0 = PASS, 1 = FAIL. Each mode prints one RESULT line.
"""
import sys

WANT = [
    ('World', ['Cell borders', 'Ground', 'Water', 'Markers', 'Disabled references', 'Grass', 'Particles',
               'Previs', 'Culling', 'Far LOD', 'Far LOD source']),
    ('Lighting', ['Cell lights', 'GI', 'Sky visibility', 'Far light', 'Far light mode', 'Light shadows', 'AO',
                  'AO decals', 'GPU relight']),
    ('Effects', ['Imagespace', 'Temporal AA', 'SSR', 'SSR outdoors', 'Depth of field', 'Motion blur', 'Bloom']),
]
# what the old panel called the two drop-downs' jobs: their current values / item names
COMBO_WORDS = {'Vanilla', 'FO4CS', 'Combined'}


def read(path):
    rows = []
    for line in open(path, encoding='utf-8-sig', errors='replace'):
        line = line.rstrip('\r\n')
        if line:
            rows.append(line.split('\t'))
    return rows


def layout(path):
    rows = read(path)
    got, section = [], ''
    for r in rows:
        if r[0] == 'SECTION':
            section = r[1] if len(r) > 1 else ''
        elif r[0] in ('TICK', 'SUBMENU', 'VALUE'):   # VALUE: a label | field row
            got.append((section, r[2].replace('&', '')))
    want = [(s, l) for s, labels in WANT for l in labels]
    every = {l for _, labels in WANT for l in labels}
    boxes = [r for r in rows if r[0] == 'BOX']
    viewport_boxes = [r for r in boxes if len(r) > 3 and (r[3] in every or r[3] in COMBO_WORDS)]
    menu_labels = [l for _, l in got]
    water = menu_labels.count('Water') + sum(1 for r in viewport_boxes if r[3] == 'Water')
    missing = [l for _, l in want if l not in menu_labels]
    dup = sorted({l for l in menu_labels if menu_labels.count(l) > 1})
    subs = {}
    for r in rows:
        if r[0] == 'SUBITEM':
            subs.setdefault(r[1], []).append(r[3] == '1')
    sub_ok = all(subs.get(n) and sum(subs[n]) == 1 for n in ('Far LOD source', 'Far light mode'))
    print('menu entries %d (want %d), in order with sections: %s' % (len(got), len(want), got == want))
    print('missing from the menu: %d %s' % (len(missing), missing))
    print('duplicated in the menu: %s' % dup)
    print('Water count (menu + boxes): %d' % water)
    print('viewport boxes left in the panel: %d %s' % (len(viewport_boxes), [r[2] for r in viewport_boxes]))
    print('other boxes in the dock: %s' % [r[2] for r in boxes if r not in viewport_boxes])
    print('submenus exclusive, one ticked: %s %s' % (sub_ok, subs))
    ok = got == want and not missing and not dup and water == 1 and not viewport_boxes and sub_ok
    print('RESULT layout %s' % ('PASS' if ok else 'FAIL'))
    return ok


def pixels(d, names):
    from PIL import Image
    import numpy as np

    def load(n):
        return np.asarray(Image.open('%s/%s.png' % (d, n)).convert('RGB')).astype(np.int16)

    def changed(a, b):
        return int((np.abs(a - b).max(axis=2) > 8).sum())

    def magenta(a):
        return int(((a[:, :, 0] > 200) & (a[:, :, 1] < 60) & (a[:, :, 2] > 200)).sum())

    base, base2 = load('base'), load('base2')
    noise = changed(base, base2)
    total = base.shape[0] * base.shape[1]
    print('viewport %dx%d; base vs base2 (frame noise): %d px' % (base.shape[1], base.shape[0], noise))
    ok = True
    mag = {'base': magenta(base)}
    for n in names:
        f, b = load(n + '_flip'), load(n + '_back')
        c_flip, c_back = changed(base, f), changed(base, b)
        mag[n + '_flip'], mag[n + '_back'] = magenta(f), magenta(b)
        # the flip must move more than 0.1% of the frame and 10x the noise; the back must return to noise
        good = c_flip > max(total // 1000, 10 * noise) and c_back <= max(noise * 2, total // 2000)
        ok &= good
        print('%-10s flip changed %8d px   back changed %8d px   %s' % (n, c_flip, c_back, 'PASS' if good else 'FAIL'))
    print('magenta px per shot: %s' % mag)
    ok &= all(v == 0 for v in mag.values())
    print('RESULT pixels %s' % ('PASS' if ok else 'FAIL'))
    return ok


def persist(a, b):
    def states(p):
        out = {}
        for r in read(p):
            if r[0] == 'TICK':
                out[r[2]] = r[4]
            elif r[0] == 'SUBITEM':
                out[r[1] + '/' + r[2]] = r[3]
        return out
    sa, sb = states(a), states(b)
    # The five cell rows follow the .wwcell the view was built from (as on main), not the last click.
    spec = {'Cell borders', 'Ground', 'Water', 'Markers', 'Disabled references'}
    keys = [k for k in set(sa) | set(sb) if k not in spec]
    diff = {k: (sa.get(k), sb.get(k)) for k in keys if sa.get(k) != sb.get(k)}
    print('states compared: %d (the 5 cell-file rows left out); differing after relaunch: %s' % (len(keys), diff))
    ok = len(keys) >= 25 and not diff
    print('RESULT persist %s' % ('PASS' if ok else 'FAIL'))
    return ok


if __name__ == '__main__':
    mode = sys.argv[1]
    if mode == 'layout':
        r = layout(sys.argv[2])
    elif mode == 'pixels':
        r = pixels(sys.argv[2], sys.argv[3:])
    elif mode == 'persist':
        r = persist(sys.argv[2], sys.argv[3])
    else:
        raise SystemExit(__doc__)
    sys.exit(0 if r else 1)

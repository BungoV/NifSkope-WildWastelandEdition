"""Lane BAKEINCR1 (2026-10-04): bake a rectangle of exterior cells' probes, whole or incrementally.

Each asked cell is one headless NifSkope bake (the 5x5 block + the far soup, BAKEBLOCK1) into its
own folder <root>/cell_<x>_<y>/ -- its sector .tbk files and the bakeincr.txt record of everything
the bake read (src/probeincr.h). One folder a cell because a cell's bake also writes the border
sectors of its neighbours, and those names collide between neighbouring bakes.

  --mode full         rebake every cell
  --mode check        ask the exe which cells are dirty (WW_CELL_BAKEINCR_CHECK), bake nothing
  --mode incremental  check, then rebake only the dirty cells; the clean ones are left untouched

usage: ww_bake_world.py --exe <NifSkope.exe> --plugins <a.esm,b.esp> --world Commonwealth
         --rect x0,y0,x1,y1 --root <dir> --mode full|check|incremental
         [--dataroot <dir>] [--port 14893] [--scope <settings scope>] [--times <tsv>] [--verdict <file>]

The environment is passed through to every run (WW_WINDOW_AT, WW_LODGEN_RESOURCES, red knobs...).
Prints the census line "incremental: N of M cells dirty (...)" and one "baked" line. No process is
ever killed: a run that does not end is waited for.
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
FIXTURE = os.path.join(REPO, 'tests', 'fixtures', 'empty.wwcell')
SCOPE_SEED = os.path.join(REPO, 'tests', 'spells', 'settings_scope_game.py')


def fresh_scope(scope, tmpdir):
    """A wiped settings scope seeded with this machine's game manager state (tests/spells/_harness.sh rule)."""
    key = r'HKCU\Software\NifTools\NifSkope 2.0 ' + scope
    q = dict(stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(['reg', 'delete', key, '/f'], **q)
    subprocess.run(['reg', 'add', key + r'\Settings', '/v', 'Version', '/t', 'REG_SZ', '/d', '1', '/f'], **q)
    subprocess.run(['reg', 'add', key, '/v', 'Game Manager Version', '/t', 'REG_DWORD', '/d', '2', '/f'], **q)
    reg = os.path.join(tmpdir, 'scope.reg')
    if subprocess.run([sys.executable, SCOPE_SEED, scope, reg], **q).returncode == 0:
        subprocess.run(['reg', 'import', reg], **q)
    return scope


def drop_scope(scope):
    subprocess.run(['reg', 'delete', r'HKCU\Software\NifTools\NifSkope 2.0 ' + scope, '/f'],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def cells(rect):
    x0, y0, x1, y1 = rect
    return [(x, y) for y in range(min(y0, y1), max(y0, y1) + 1) for x in range(min(x0, x1), max(x0, x1) + 1)]


def run_check(a, verdict):
    runs = os.path.join(a.root, '_runs')
    os.makedirs(runs, exist_ok=True)
    env = dict(os.environ)
    env['WW_SETTINGS_SCOPE'] = fresh_scope(a.scope, runs)
    if a.dataroot:
        env['WW_CELL_DATAROOT'] = a.dataroot
    env['WW_CELL_BAKEINCR_CHECK'] = '|'.join([a.plugins, a.world, ','.join(map(str, a.rect)), a.root, verdict])
    t0 = time.perf_counter()
    with open(os.path.join(runs, 'check.notes'), 'wb') as nf:
        rc = subprocess.run([a.exe], env=env, stdout=nf, stderr=subprocess.STDOUT).returncode
    wall = time.perf_counter() - t0
    dirty, census = [], ''
    if rc == 0 and os.path.isfile(verdict):
        for ln in open(verdict, encoding='utf-8'):
            c = ln.split()
            if ln.startswith('cell ') and len(c) >= 4 and c[3] == 'dirty':
                dirty.append((int(c[1]), int(c[2])))
            elif ln.startswith('incremental:'):
                census = ln.strip()
    return rc, dirty, census, wall


def bake(a, x, y, times):
    cell = os.path.join(a.root, 'cell_%d_%d' % (x, y))
    run = os.path.join(a.root, '_runs', 'cell_%d_%d' % (x, y))
    for d in (cell, run):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    env = dict(os.environ)
    env.update({
        'WW_CELL_OPEN': '%s|%s|%d,%d|1' % (a.plugins, a.world, x, y),
        'WW_CELL_PROBES': os.path.join(run, 'probes.tsv'), 'WW_CELL_PROBES_HIDE': '1',
        'WW_CELL_PROBE_BAKE': cell,
        'WW_RENDER_SHOT': os.path.join(run, 'shot.png'), 'WW_RENDER_SIZE': '320x200',
        'WW_RENDER_VIEW': '1', 'WW_RENDER_CLEAN': '1',
        'WW_SETTINGS_SCOPE': fresh_scope(a.scope, run),
    })
    if a.dataroot:
        env['WW_CELL_DATAROOT'] = a.dataroot
    t0 = time.perf_counter()
    with open(os.path.join(run, 'shot.notes'), 'wb') as nf:
        rc = subprocess.run([a.exe, '--port', str(a.port), FIXTURE], env=env, stdout=nf,
                            stderr=subprocess.STDOUT).returncode
    wall = time.perf_counter() - t0
    ok = rc == 0 and os.path.isfile(os.path.join(cell, 'bakeincr.txt'))
    if times:
        with open(times, 'a') as f:
            f.write('%s\t%d\t%d\t%d\t%.2f\t%s\n' % (a.tag, x, y, rc, wall, 'ok' if ok else 'NO RECORD'))
    return ok, wall


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('--exe', required=True)
    p.add_argument('--plugins', required=True)
    p.add_argument('--world', default='Commonwealth')
    p.add_argument('--rect', required=True, type=lambda s: [int(v) for v in s.split(',')])
    p.add_argument('--root', required=True)
    p.add_argument('--mode', required=True, choices=['full', 'check', 'incremental'])
    p.add_argument('--dataroot', default='')
    p.add_argument('--port', type=int, default=14893)
    p.add_argument('--scope', default='bakeworld')
    p.add_argument('--times', default='')
    p.add_argument('--verdict', default='')
    p.add_argument('--tag', default='')
    # a rect like -15,17,-12,19 starts with '-': glue it to its flag so argparse does not read it as an option
    argv = sys.argv[1:]
    for i in range(len(argv) - 1):
        if argv[i] == '--rect':
            argv[i] = '--rect=' + argv[i + 1]
            argv[i + 1] = None
    a = p.parse_args([v for v in argv if v is not None])
    if len(a.rect) != 4:
        p.error('--rect wants x0,y0,x1,y1')
    a.root = os.path.abspath(a.root)
    a.tag = a.tag or a.mode
    os.makedirs(a.root, exist_ok=True)
    all_cells = cells(a.rect)
    t0 = time.perf_counter()
    if a.mode == 'full':
        todo = all_cells
        print('full: %d of %d cells' % (len(todo), len(todo)))
    else:
        verdict = a.verdict or os.path.join(a.root, '_runs', 'verdict.txt')
        rc, todo, census, cwall = run_check(a, verdict)
        if rc != 0 or not census:
            print('check FAILED rc=%s (see %s)' % (rc, os.path.join(a.root, '_runs', 'check.notes')))
            drop_scope(a.scope)
            return 1
        print('%s; check wall %.1f s' % (census, cwall))
        if a.mode == 'check':
            drop_scope(a.scope)
            return 0
    failed, bake_s = [], 0.0
    for x, y in todo:
        ok, wall = bake(a, x, y, a.times)
        bake_s += wall
        if not ok:
            failed.append((x, y))
    drop_scope(a.scope)
    print('baked %d cells in %.1f s (bakes %.1f s), kept %d, failed %d%s' % (
        len(todo), time.perf_counter() - t0, bake_s, len(all_cells) - len(todo), len(failed),
        (' ' + ' '.join('%d,%d' % c for c in failed)) if failed else ''))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())

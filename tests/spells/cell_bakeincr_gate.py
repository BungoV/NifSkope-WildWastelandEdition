"""Lane BAKEINCR1 (2026-10-04): the incremental probe bake gate. Run it through cell_bakeincr.sh.

A 6x5 region around the asked cell (default -17,15..-12,19 around Concord -15,17), every cell its
own 5x5 + far bake (tools/ww_bake_world.py). Edits come from cell_bakeincr_esp.py, into scratch.

  FULL0    full bake on the master alone                                   -> root A
  NULL     check A at once: 0 dirty (the bake's record and the check agree)
  INCR_R   copy A, one reference moved (bakeincr_ref.esp), incremental     -> root AR
           GREEN: the dirty set is EXACTLY the 25 cells of the edit's 5x5; the 5 kept cells are
           byte-identical to FULL0
  FULL1    full bake with the edit                                         -> root B
           GREEN: AR == B, every .tbk and every record, byte for byte
  RED nonbr     check A with the edit, neighbour rule off: dirty must be the edit's cell alone,
                and that incremental result (A + the cell rebaked, = B's cell) must differ from
                FULL1 somewhere -- the red is caught
  LIGHT    check AR with a LIGH record + a light reference edited and WW_LOOKDEV_HOUR=6: 0 dirty;
           a fresh bake of the edit's cell under them equals the kept one
  MODEL    copy AR, one model overridden (WW_LODGEN_RESOURCES), incremental -> root AM
           GREEN: dirty == the cells whose 5x5 holds the model's only reference (from the master)
  RED nomodels  check AR with the override, models ignored: 0 dirty, and AM's rebakes differ from
                AR's kept cells -- the red is caught
  LOD      a copy of the far LOD (hard links; the .lodl itself copied): unedited -> 0 dirty;
           one far cell's water raised 32 cells west of the region's west column -> exactly that
           column dirty (the cells whose far ring reaches it); one of them rebaked to show the bytes
  TIME     the one-object edit's incremental wall time against the full bake's

usage: cell_bakeincr_gate.py <exe> <out dir> [--from <step>]
"""
import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
DRIVER = os.path.join(REPO, 'tools', 'ww_bake_world.py')
ESPGEN = os.path.join(HERE, 'cell_bakeincr_esp.py')
ESM = r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm'
DATA = r'E:/Tools/Fallout 4/DataUnpacked/Data'
LODDIR = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth'
C = (-15, 17)
RECT = (-17, 15, -12, 19)
FAILS = []


def say(s):
    print(s, flush=True)


def verdict(name, ok, detail):
    say('%s %s: %s' % ('PASS' if ok else 'FAIL', name, detail))
    if not ok:
        FAILS.append(name)


def all_cells():
    x0, y0, x1, y1 = RECT
    return [(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)]


def block(c, half=2):
    return {(x, y) for (x, y) in all_cells() if abs(x - c[0]) <= half and abs(y - c[1]) <= half}


def cell_files(root, c):
    d = os.path.join(root, 'cell_%d_%d' % c)
    out = {}
    if os.path.isdir(d):
        for n in sorted(os.listdir(d)):
            if n.endswith('.tbk') or n == 'bakeincr.txt':
                out[n] = hashlib.sha1(open(os.path.join(d, n), 'rb').read()).hexdigest()
    return out


def tbk_only(files):
    return {k: v for k, v in files.items() if k.endswith('.tbk')}


def drive(exe, plugins, root, mode, tag, out, extra_env=None, rect=RECT):
    env = dict(os.environ)
    env.update(extra_env or {})
    cmd = [sys.executable, DRIVER, '--exe', exe, '--plugins', plugins, '--world', 'Commonwealth',
           '--rect', ','.join(map(str, rect)), '--root', root, '--mode', mode, '--dataroot', DATA,
           '--scope', 'bakeincr1_gate', '--times', os.path.join(out, 'times.tsv'), '--tag', tag,
           '--port', os.environ.get('PORT', '14893'),
           '--verdict', os.path.join(out, 'verdict_%s.txt' % tag)]
    t0 = time.perf_counter()
    p = subprocess.run(cmd, env=env, capture_output=True, text=True)
    wall = time.perf_counter() - t0
    for ln in p.stdout.splitlines():
        say('  [%s] %s' % (tag, ln))
    dirty = set()
    vf = os.path.join(out, 'verdict_%s.txt' % tag)
    if mode != 'full' and os.path.isfile(vf):
        for ln in open(vf, encoding='utf-8'):
            c = ln.split()
            if ln.startswith('cell ') and c[3] == 'dirty':
                dirty.add((int(c[1]), int(c[2])))
    return p.returncode, dirty, wall


def copy_root(src, dst):
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns('_runs'))


def fmt(cells):
    return ' '.join('%d,%d' % c for c in sorted(cells)) or '-'


def facts(path):
    f = {}
    for ln in open(path, encoding='utf-8'):
        k, _, v = ln.strip().partition(' ')
        f[k] = v
    return f


def lod_copy(dst):
    """The far LOD as hard links (no bytes copied, his files never written), the .lodl a real copy."""
    shutil.rmtree(dst, ignore_errors=True)
    for dp, dns, fns in os.walk(LODDIR):
        rel = os.path.relpath(dp, LODDIR)
        os.makedirs(os.path.join(dst, rel), exist_ok=True)
        for n in fns:
            s, d = os.path.join(dp, n), os.path.join(dst, rel, n)
            if n.lower().endswith('.lodl'):
                shutil.copyfile(s, d)
            else:
                os.link(s, d)
    return os.path.join(dst, 'Commonwealth.lodl')


def lod_raise_water(lodl, cx, cy):
    with open(lodl, 'r+b') as f:
        h = f.read(0x70)
        minx, miny, maxx, maxy = struct.unpack_from('<iiii', h, 0x08)
        ocell = struct.unpack_from('<Q', h, 0x68)[0]
        assert minx <= cx <= maxx and miny <= cy <= maxy, 'far cell outside the .lodl'
        at = ocell + ((cy - miny) * (maxx - minx + 1) + (cx - minx)) * 16
        f.seek(at)
        lo, hi, wh, wt, fl = struct.unpack('<fffHH', f.read(16))
        f.seek(at)
        f.write(struct.pack('<fffHH', lo, hi, hi + 2000.0, wt, fl | 1))
    return lo, hi, wh, fl


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('out')
    ap.add_argument('--from', dest='start', default='full0')
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    steps = ['full0', 'null', 'incr_r', 'full1', 'nonbr', 'light', 'model', 'lod']
    run = steps[steps.index(a.start):]
    ed = os.path.join(out, 'edits')
    if not os.path.isfile(os.path.join(ed, 'facts.txt')):
        subprocess.run([sys.executable, ESPGEN, ESM, DATA, ed, '%d,%d' % C, ','.join(map(str, RECT))], check=True)
    F = facts(os.path.join(ed, 'facts.txt'))
    say('edits: ' + '; '.join('%s %s' % kv for kv in F.items()))
    PL0 = ESM
    PLR = ESM + ',' + os.path.join(ed, 'bakeincr_ref.esp')
    PLL = PLR + ',' + os.path.join(ed, 'bakeincr_light.esp')
    A, AR, B, AM = (os.path.join(out, n) for n in ('A', 'AR', 'B', 'AM'))
    cells = all_cells()
    walls = {}

    if 'full0' in run:
        rc, _, walls['full0'] = drive(a.exe, PL0, A, 'full', 'full0', out)
        verdict('FULL0', rc == 0 and all(cell_files(A, c) for c in cells), 'rc %d, %d cells' % (rc, len(cells)))
    if 'null' in run:
        rc, d, _ = drive(a.exe, PL0, A, 'check', 'null', out)
        verdict('NULL', rc == 0 and not d, 'a check right after the full bake: dirty %d (%s)' % (len(d), fmt(d)))
    if 'incr_r' in run:
        copy_root(A, AR)
        rc, d, walls['incr_r'] = drive(a.exe, PLR, AR, 'incremental', 'incr_r', out)
        exp = block(C)
        verdict('GREEN dirty set', rc == 0 and d == exp, 'dirty %d, expected %d; missing %s; extra %s' % (
            len(d), len(exp), fmt(exp - d), fmt(d - exp)))
        kept = [c for c in cells if c not in d]
        same = [c for c in kept if cell_files(AR, c) == cell_files(A, c)]
        verdict('GREEN kept untouched', len(same) == len(kept) == len(cells) - len(exp),
                '%d of %d kept cells byte-identical to FULL0' % (len(same), len(kept)))
    if 'full1' in run:
        rc, _, walls['full1'] = drive(a.exe, PLR, B, 'full', 'full1', out)
        diff = [c for c in cells if not cell_files(B, c) or cell_files(AR, c) != cell_files(B, c)]
        verdict('GREEN incremental == full', rc == 0 and not diff,
                '%d of %d cells byte-identical (every .tbk and record); differ: %s' % (len(cells) - len(diff), len(cells), fmt(diff)))
        moved = [c for c in cells if tbk_only(cell_files(A, c)) != tbk_only(cell_files(B, c))]
        say('  info: the edit moved .tbk bytes in %d cells: %s' % (len(moved), fmt(moved)))
    if 'nonbr' in run:
        rc, d, _ = drive(a.exe, PLR, A, 'check', 'red_nonbr', out, {'WW_CELL_BAKEINCR_RED': 'nonbr'})
        stale = [c for c in cells if c not in d and tbk_only(cell_files(A, c)) != tbk_only(cell_files(B, c))]
        verdict('RED nonbr caught', rc == 0 and d == {C} and len(stale) > 0,
                'red dirty %s; its incremental world would keep %d stale cells vs FULL1: %s' % (fmt(d), len(stale), fmt(stale)))
    if 'light' in run:
        env = {'WW_LOOKDEV_HOUR': '6'}
        rc, d, _ = drive(a.exe, PLL, AR, 'check', 'light', out, env)
        L = os.path.join(out, 'L')
        rc2, _, _ = drive(a.exe, PLL, L, 'full', 'light_fresh', out, env, rect=(C[0], C[1], C[0], C[1]))
        fresh_same = tbk_only(cell_files(L, C)) == tbk_only(cell_files(AR, C)) and bool(tbk_only(cell_files(L, C)))
        verdict('GREEN light/time dirty nothing', rc == 0 and rc2 == 0 and not d and fresh_same,
                'dirty %d (%s); fresh bake of %d,%d under the light edit + hour 6 == kept: %s' % (
                    len(d), fmt(d), C[0], C[1], 'Y' if fresh_same else 'N'))
    if 'model' in run:
        res = os.path.join(ed, 'res')
        env = {'WW_LODGEN_RESOURCES': res}
        copy_root(AR, AM)
        rc, d, walls['incr_m'] = drive(a.exe, PLR, AM, 'incremental', 'incr_m', out, env)
        exp = {tuple(int(v) for v in s.split(',')) for s in F['model_expect'].split()}
        verdict('GREEN model dirty set', rc == 0 and d == exp, 'model %s: dirty %d, expected %d; missing %s; extra %s' % (
            F['model'], len(d), len(exp), fmt(exp - d), fmt(d - exp)))
        rc, dr, _ = drive(a.exe, PLR, AR, 'check', 'red_nomodels', out, dict(env, WW_CELL_BAKEINCR_RED='nomodels'))
        stale = [c for c in exp if tbk_only(cell_files(AM, c)) != tbk_only(cell_files(AR, c))]
        verdict('RED nomodels caught', rc == 0 and not dr and len(stale) > 0,
                'red dirty %d; it would keep %d stale cells: %s' % (len(dr), len(stale), fmt(stale)))
    if 'lod' in run:
        lodl = lod_copy(os.path.join(out, 'lod'))
        env = {'WW_CELL_FAR_LOD': lodl}
        rc, d, _ = drive(a.exe, PLR, AR, 'check', 'lod_same', out, env)
        verdict('GREEN LOD copy clean', rc == 0 and not d, 'unedited far LOD from another path: dirty %d (%s)' % (len(d), fmt(d)))
        fx, fy = RECT[0] - 32, C[1]
        lo, hi, wh, fl = lod_raise_water(lodl, fx, fy)
        say('  far cell %d,%d: lo %.0f hi %.0f water %.0f flags %d -> water %.0f, flag 1' % (fx, fy, lo, hi, wh, fl, hi + 2000))
        rc, d, _ = drive(a.exe, PLR, AR, 'check', 'lod_edit', out, env)
        exp = {c for c in cells if c[0] - 32 <= fx}
        verdict('GREEN LOD dirty set', rc == 0 and d == exp, 'dirty %d (%s), expected %s' % (len(d), fmt(d), fmt(exp)))
        D = os.path.join(out, 'LD')
        one = sorted(exp)[0]
        rc2, _, _ = drive(a.exe, PLR, D, 'full', 'lod_one', out, env, rect=(one[0], one[1], one[0], one[1]))
        moved = tbk_only(cell_files(D, one)) != tbk_only(cell_files(AR, one))
        say('  info: cell %d,%d rebaked under the far edit: .tbk bytes %s' % (one[0], one[1], 'MOVED' if moved else 'unchanged'))
    if 'incr_r' in walls and 'full1' in walls:
        say('TIME one-object edit: incremental %.1f s (check + 25 bakes) vs full %.1f s (30 bakes): %.1f s saved (%.0f%%)' % (
            walls['incr_r'], walls['full1'], walls['full1'] - walls['incr_r'],
            100.0 * (walls['full1'] - walls['incr_r']) / walls['full1']))
    say('GATE %s%s' % ('GREEN' if not FAILS else 'RED', '' if not FAILS else ' (' + ', '.join(FAILS) + ')'))
    return 0 if not FAILS else 1


if __name__ == '__main__':
    sys.exit(main())

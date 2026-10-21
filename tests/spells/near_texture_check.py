"""Lane NEAR2 (2026-09-26): gate T -- does every near-library material's DIFFUSE texture resolve, and if not, why.

bungo, 2026-09-26, on the NEAR1 Boston render: "Purple trees in your render" / "If they're purple, it means
they're missing textures". This gate counts, per material of `<ws>.near.textures.txt`, whether its diffuse
resolves, from an INDEPENDENT reading: the stack is rebuilt in Python off the MO2 profile
(lodgen_loadorder_check.expected), with loose files and BOTH archive kinds (near_texture_stack.TexStack: GNRL
for BGSMs, DX10 for textures; LAST entry wins, a loose file beats every archive, inside one entry an archive
takes its plugin's load-order index). The BGSM is read here, not taken from the bake.

A material's diffuse is: the BGSM's first texture when the material names a BGSM (what the game and the viewer
both read), else the NIF's slot 0. Classes (INDEPENDENT: a material is counted in every one that applies, so a
fix to one class cannot hide another; `ok` = none applies):

  missing         the diffuse (or the BGSM) is in no entry of the bake's stack: a real missing file
  bake-path       the path the sidecar STORES for slot 0 is not the resolvable path: it does not open as written
                  (no `textures\\` in front, an author's absolute build path) or it names another file than the
                  BGSM does
  renderer-stack  the bake's stack has it, the stack the VIEWER was given does not (the BGSM or the diffuse):
                  the viewer draws the material without its textures (magenta)
  undrawable      it resolves, but is a kind the viewer cannot draw as a diffuse (a cube map, or a DXGI format
                  outside BC1/BC2/BC3/BC5/BC7/B8G8R8A8/R8G8B8A8)
  ok

`--renderer data` = the viewer sees the game's Data folder only (a fresh settings scope's Game Manager folders,
tests/spells/settings_scope_game.py); `--renderer lib` = the viewer is given the library's own
`<ws>.near.resources.txt` (WW_LODGEN_RESOURCES), falling back to data when the library has none.
`--shot-log <log>` also counts the viewer's OWN misses ("not found in archives") on BGSMs and diffuse maps.

Refuters (`--sabotage`, each must turn the gate red on a library that passes):
  strip-prefix   store every slot-0 path without its `textures\\`            -> bake-path > 0
  no-resources   ignore the library's resources.txt (the viewer sees Data)  -> renderer-stack > 0
  drop-entry     the bake stack loses its last mod entry that alone serves a  -> missing grows
                 diffuse this library draws

Verdict: PASS when bake-path == 0, renderer-stack == 0, undrawable == 0 and missing <= --missing-max
(pre-registered from the bake's own "tex ... missing" lines: a real missing file is data, not a defect).

usage: near_texture_check.py --lib <dir> [--renderer data|lib] [--shot-log L] [--missing-max N] [--sabotage K]
       [--profile P] [--mods M] [--data D] [--world Commonwealth] [--list N]
"""
import argparse
import os
import struct
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lodgen_loadorder_check as lo   # noqa: E402
import near_texture_stack as ts       # noqa: E402

BS = chr(92)
DRAWABLE = {70, 71, 72, 73, 74, 75, 76, 77, 78, 82, 83, 84, 87, 88, 91, 94, 95, 96, 97, 98, 99, 28, 29}


def bgsm_textures(b):
    """The BGSM's texture list (as stored), or None when it does not read that far."""
    try:
        if b[:4] != b'BGSM':
            return None
        o = 4
        ver, = struct.unpack_from('<I', b, o); o += 4
        o += 4 + 16 + 4 + 1 + 8 + 6 + 1 + 3 + 6 + 1
        if ver < 10:
            o += 4
        o += 1
        if ver >= 6:
            o += 1
        tex = []
        for _ in range(10 if ver >= 17 else 9):
            n, = struct.unpack_from('<I', b, o); o += 4
            tex.append(b[o:o + n].rstrip(b'\0').decode('latin-1')); o += n
        return tex
    except struct.error:
        return None


def material_rel(lodm):
    """A material name as the game resolves it: `materials\\...`, anything before an embedded one dropped."""
    p = lodm.replace('/', BS).lower().strip().lstrip(BS)
    if not p.startswith('materials' + BS):
        at = p.find(BS + 'materials' + BS)
        p = p[at + 1:] if at >= 0 else 'materials' + BS + p
    return p


def texture_rel(t):
    p = t.replace('/', BS).lower().strip().lstrip(BS)
    if not p.startswith('textures' + BS):
        at = p.find(BS + 'textures' + BS)
        p = p[at + 1:] if at >= 0 else 'textures' + BS + p
    return p


def read_sidecar(path):
    mats = []
    cur = None
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.rstrip('\n')
        if line.startswith('mat '):
            f = line.split(' ')
            lodm = line.split(' lodm ', 1)[1] if ' lodm ' in line else ''
            cur = {'id': int(f[1]), 'lodm': lodm, 'slots': {}}
            mats.append(cur)
        elif line.startswith('  slot ') and cur is not None:
            f = line.strip().split(' ')
            # slot <k> <path...> <bucket>: the path may hold spaces
            k = int(f[1])
            path_ = ' '.join(f[2:-1])
            cur['slots'][k] = None if path_ == '-' else path_
    return mats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--lib', required=True)
    ap.add_argument('--world', default='Commonwealth')
    ap.add_argument('--renderer', default='lib', choices=['data', 'lib'])
    ap.add_argument('--shot-log', default='')
    ap.add_argument('--missing-max', type=int, default=-1)
    ap.add_argument('--sabotage', default='')
    ap.add_argument('--profile', default='E:/Projects/Fallout 4 Mods/profiles/Default')
    ap.add_argument('--mods', default='E:/Projects/Fallout 4 Mods/mods')
    ap.add_argument('--data', default='X:/Programs/Steam/steamapps/common/Fallout 4/Data')
    ap.add_argument('--list', type=int, default=6)
    a = ap.parse_args()

    stem = os.path.join(a.lib, a.world + '.near')
    mats = read_sidecar(stem + '.textures.txt')
    pl, stack, *_ = lo.expected(a.profile, a.mods, a.data)
    stack = [s.replace(BS, '/') for s in stack]

    bake = ts.TexStack(stack, 'bake', plugins=pl)
    if a.sabotage == 'drop-entry':
        # the last mod entry that is the ONLY provider of a diffuse this library draws (an entry whose files all
        # have a lower provider cannot make anything missing, so a blind "last entry" is not a refuter)
        need = set()
        for m in mats:
            lodm = m['lodm']
            if lodm.lower().endswith('.bgsm'):
                b = bake.read(material_rel(lodm))
                tl = bgsm_textures(b) if b else None
                if tl and tl[0]:
                    need.add(texture_rel(tl[0]))
            elif m['slots'].get(0):
                need.add(texture_rel(m['slots'][0]))
        served = {}
        for d in need:
            w = bake.where(d)
            if w is None:
                continue
            if w.startswith('loose:'):
                served.setdefault(w[6:], set()).add(d)
            else:
                ent = next((e for e in stack if os.path.isfile(os.path.join(e, w[4:]))), None)
                if ent:
                    served.setdefault(ent, set()).add(d)
        for i in range(len(stack) - 1, 0, -1):
            if stack[i] not in served:
                continue
            trial = ts.TexStack(stack[:i] + stack[i + 1:], 'bake-drop', plugins=pl)
            lost = sorted(d for d in served[stack[i]] if trial.where(d) is None)
            if lost:
                print('sabotage drop-entry: the bake stack loses %s (%d diffuse maps it alone serves, e.g. %s)'
                      % (stack[i], len(lost), lost[0]))
                stack = stack[:i] + stack[i + 1:]
                bake = trial
                break

    res_path = stem + '.resources.txt'
    lib_res = []
    if os.path.isfile(res_path):
        lib_res = [ln.strip() for ln in open(res_path, encoding='utf-8') if ln.strip() and not ln.startswith('#')]
    if a.renderer == 'lib' and lib_res and a.sabotage != 'no-resources':
        view = ts.TexStack(lib_res, 'viewer:lib', plugins=pl)
        view_label = 'the library resources.txt (%d entries)' % len(lib_res)
    else:
        view = ts.TexStack([a.data], 'viewer:data', plugins=pl)
        view_label = 'Data only' + (' (sabotage no-resources)' if a.sabotage == 'no-resources' else
                                    '' if a.renderer == 'data' else ' (the library has no resources.txt)')

    classes = Counter()
    examples = {}
    cache = {}
    for m in mats:
        lodm = m['lodm']
        stored = m['slots'].get(0)
        if a.sabotage == 'strip-prefix' and stored:
            s0 = stored.lower()
            stored = s0[len('textures' + BS):] if s0.startswith('textures' + BS) else s0
        flags = {}
        diffuse = None
        is_bgsm = lodm.lower().endswith('.bgsm')
        if is_bgsm:
            mrel = material_rel(lodm)
            if mrel not in cache:
                b = bake.read(mrel)
                cache[mrel] = bgsm_textures(b) if b else None
            tl = cache[mrel]
            if tl is None:
                flags['missing'] = mrel + ' (BGSM)'
            else:
                diffuse = texture_rel(tl[0]) if tl and tl[0] else None
        else:
            diffuse = texture_rel(stored) if stored else None
        if not flags and not diffuse:
            flags['no-diffuse'] = lodm
        if diffuse and bake.where(diffuse) is None:
            flags['missing'] = diffuse
        if diffuse and 'missing' not in flags:
            st = (stored or '').replace('/', BS).lower()
            if st != diffuse:
                flags['bake-path'] = '%s stored as %s' % (diffuse, stored)
            if is_bgsm and view.where(material_rel(lodm)) is None:
                flags['renderer-stack'] = material_rel(lodm) + ' (BGSM)'
            elif view.where(diffuse) is None:
                flags['renderer-stack'] = diffuse
            h = bake.header(diffuse)
            if h is not None and (h[4] or h[0] not in DRAWABLE):
                flags['undrawable'] = '%s dxgi %d cube %s' % (diffuse, h[0], h[4])
        if not flags:
            flags['ok'] = ''
        for cls, why in flags.items():
            classes[cls] += 1
            examples.setdefault(cls, []).append('mat %d %s' % (m['id'], why))

    print('near texture gate: library %s, %d materials; bake stack %d entries (%d archived files); viewer stack: %s'
          % (a.lib, len(mats), len(stack), len(bake.arch), view_label))
    for c in ('ok', 'missing', 'bake-path', 'renderer-stack', 'undrawable', 'no-diffuse'):
        print('  %-15s %d' % (c, classes.get(c, 0)))
        if c != 'ok':
            for e in examples.get(c, [])[:a.list]:
                print('      ' + e)
    if a.shot_log and os.path.isfile(a.shot_log):
        miss = set()
        for ln in open(a.shot_log, encoding='utf-8', errors='replace'):
            if 'not found in archives' in ln and '"' in ln:
                miss.add(ln.split('"')[1].lower())
        bg = sorted(x for x in miss if x.endswith('.bgsm'))
        dd = sorted(x for x in miss if x.endswith('_d.dds'))
        print('  viewer log %s: %d distinct misses, %d BGSMs, %d diffuse maps%s'
              % (os.path.basename(a.shot_log), len(miss), len(bg), len(dd),
                 (' -- ' + ', '.join((bg + dd)[:a.list])) if bg or dd else ''))
    bad = classes.get('bake-path', 0) + classes.get('renderer-stack', 0) + classes.get('undrawable', 0)
    miss_ok = a.missing_max < 0 or classes.get('missing', 0) <= a.missing_max
    verdict = 'PASS' if bad == 0 and miss_ok else 'FAIL'
    print('near texture gate: bake-path %d, renderer-stack %d, undrawable %d, missing %d (max %s) -> %s'
          % (classes.get('bake-path', 0), classes.get('renderer-stack', 0), classes.get('undrawable', 0),
             classes.get('missing', 0), a.missing_max if a.missing_max >= 0 else 'not set', verdict))
    return 0 if verdict == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())

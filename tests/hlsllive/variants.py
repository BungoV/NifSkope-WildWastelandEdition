"""HLSLBULK1 R6: variant HLSL. The loader injects `#define WW_X 1` into a source when its marker text is present and the
feature flag is on (glcontext.cpp Shader::load); the converted HLSL is flat, so each define set the live path can meet is
converted as its own file: res/shaders/hlsl/<module>/<stem>_<stage>__<A+B>.hlsl (names sorted), and
res/shaders/hlsl/variants.txt lists one line per program variant: `<prog>\t<A+B union>\t<stem.ext>=<stage tag> ...`.
  python -I -X utf8 tests/hlsllive/variants.py <tree> [--dry] [--only substr]
Flag sets met in a default run: WW_FRAME_UBO always (WW_FRAME_UBO_RED=1 is a red), WW_CELL_RTDDGI off/on, WW_CELL_RESTIR
(only with RTDDGI), WW_CELL_AIRZONE, WW_CELL_AIR_SKYVIEW. Other defines (views, debug views, fog leak, air bounce, water
probe, PRT, march, the RTDDGI cascades/one-read reds) are not built: the loader finds no key and falls back to GLSL.
Uses notes/hlslbulk1/tools/convert.py (the GLSL -> HLSL converter) and its census.json: a generator, not part of the build."""
import itertools
import json
import os
import re
import sys

NOTES = 'E:/Projects/Claude/notes/hlslbulk1/tools'
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, NOTES)
import remap
import convert

FLAGS = ['WW_CELL_RTDDGI', 'WW_CELL_RESTIR', 'WW_CELL_AIRZONE', 'WW_CELL_AIR_SKYVIEW']


def flag_sets():
    out = []
    for bits in itertools.product([0, 1], repeat=len(FLAGS)):
        s = {'WW_FRAME_UBO'} | {f for f, b in zip(FLAGS, bits) if b}
        if 'WW_CELL_RESTIR' in s and 'WW_CELL_RTDDGI' not in s:
            continue
        out.append(sorted(s))
    return out


def main():
    a = sys.argv[1:]
    tree = a[0].rstrip('/')
    dry = '--dry' in a
    only = a[a.index('--only') + 1] if '--only' in a else None
    census = json.load(open(os.path.join(NOTES, '..', 'census.json')))
    byname = {}
    for f in census['files']:
        byname.setdefault(os.path.basename(f), []).append(f)
    scratch = os.path.join(os.environ.get('TEMP', '.'), 'hlslbulk1_var')
    os.makedirs(scratch, exist_ok=True)
    lines, jobs, srcs = [], {}, {}
    for pf, st in remap.programs(tree):
        if only and only not in os.path.basename(pf):
            continue
        listed = open(os.path.join(tree, pf), encoding='utf-8', errors='replace').read()
        names = []
        for ln in open(os.path.join(tree, pf), encoding='utf-8', errors='replace'):
            if ln.startswith('shaders'):
                names = [w for w in ln.split()[1:]]
        stages = []
        for s in names:
            cands = byname.get(os.path.basename(s), [])
            near = [c for c in cands if os.path.dirname(c) == os.path.dirname(pf)]
            pick = (near or cands or [None])[0]
            ext = os.path.splitext(s)[1][1:]
            if pick is None or ext not in convert.SUFFIX:
                stages = None
                break
            if pick not in srcs:
                try:
                    srcs[pick] = convert.expand(tree, pick, byname)
                except Exception:
                    srcs[pick] = ''
            stages.append((ext, pick, os.path.splitext(os.path.basename(s))[0]))
        if not stages:
            continue
        seen = set()
        for V in flag_sets():
            tags = [convert.applied(srcs[p], V) if srcs[p].startswith('#version') else [] for _e, p, _s in stages]
            union = sorted({d for t in tags for d in t})
            if not union or '+'.join(union) in seen:
                continue
            seen.add('+'.join(union))
            for (e, p, s), t in zip(stages, tags):
                if t:
                    jobs[(p, '+'.join(t))] = t
            lines.append('%s\t%s\t%s' % (os.path.basename(pf), '+'.join(union),
                                         ' '.join('%s.%s=%s' % (s, e, '+'.join(t)) for (e, p, s), t in zip(stages, tags))))
    print('variants: %d program variants, %d stage files to convert' % (len(lines), len(jobs)))
    if dry:
        return
    ok = bad = 0
    for (p, tag), t in sorted(jobs.items()):
        try:
            out, err = convert.convert(tree, p, byname, scratch, variant=tuple(t))
        except Exception as e:
            out, err = None, 'exception: %s' % e
        if err:
            bad += 1
            print('CONV FAIL %s@%s %s' % (p, tag, err), flush=True)
        else:
            ok += 1
    print('CONV variants ok %d fail %d' % (ok, bad))
    open(os.path.join(tree, 'res/shaders/hlsl/variants.txt'), 'w', newline='\n').write('\n'.join(sorted(lines)) + '\n')


main()

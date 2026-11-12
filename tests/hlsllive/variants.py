"""HLSLBULK1 R6: variant HLSL. The loader injects `#define WW_X 1` into a source when its marker text is present and the
feature flag is on (glcontext.cpp Shader::load); the converted HLSL is flat, so each define set the live path can meet is
converted as its own file: res/shaders/hlsl/<module>/<stem>_<stage>__<A+B>.hlsl (names sorted), and
res/shaders/hlsl/variants.txt lists one line per program variant: `<prog>\t<A+B union>\t<stem.ext>=<stage tag> ...`.
  python -I -X utf8 tests/hlsllive/variants.py <tree> [--dry] [--only substr]
R7: the sets come from the app. fly94.log's 'hlsl fallback' keys all carry WW_CELL_FOGLEAK (on by default) and, with the Ray-traced
GI row, WW_CELL_RTDDGI_CASCADES: ALWAYS / RT_STATES / FLAGS below list every combination of the shipped-on rows. Not built:
probe/pass views, debug views, GI march, PRT, water info probe and the reds (FOGLEAK=0, cascades one, one-read): GLSL fallback.
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

ALWAYS = ['WW_FRAME_UBO', 'WW_CELL_FOGLEAK']   # on in a default run (FOGLEAK since FOGLEAK1; WW_CELL_FOGLEAK=0 is a red)
# the Ray-traced GI row and the RT quality preset (rtddgicell.cpp): off, or on with the cascades (kPresetCascades 3/3/3/4, always
# > 1 unless the red WW_CELL_RTDDGI_CASCADES_RED=one), and RESTIR from preset Medium up (it needs RTDDGI)
RT_STATES = [[], ['WW_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES'], ['WW_CELL_RTDDGI', 'WW_CELL_RTDDGI_CASCADES', 'WW_CELL_RESTIR']]
# HLSLRESTIR1: the ReSTIR GI row (WW_CELL_RESTIRGI, off by default) at any RT quality while RTDDGI runs (rtddgicell.cpp restirGiOffWhy)
RT_STATES += [s + ['WW_CELL_RESTIRGI'] for s in RT_STATES[1:]]
# rows that inject a define: Air zones row (AIRZONE), the air's sky visibility (AIR_SKYVIEW, on since DARKAIR2), the lamps' floor bounce
# (AIR_BOUNCE). Not built: probe/pass views (VIEWS), debug views (DEBUG_VIEWS), GI march / PRT / water info probe (reds, harness).
FLAGS = ['WW_CELL_AIRZONE', 'WW_CELL_AIR_SKYVIEW', 'WW_CELL_AIR_BOUNCE']


def flag_sets():
    out = []
    for rt in RT_STATES:
        for bits in itertools.product([0, 1], repeat=len(FLAGS)):
            out.append(sorted(set(ALWAYS) | set(rt) | {f for f, b in zip(FLAGS, bits) if b}))
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

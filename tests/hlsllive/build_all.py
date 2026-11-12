"""Build the whole SPIR-V set for the live path (HLSLBULK1 R5, A1) and link-test every program against the real driver.
  python -I -X utf8 tests/hlsllive/build_all.py <tree> <outdir> [--only substr] [--defines A,B]
Writes <outdir>/spirv/<one .spv per distinct stage shader>, <outdir>/spirv/index.txt (one line per stage shader file name:
`<glsl file name>\\t<spv file>`), <outdir>/spirv/programs.txt (one line per program: `<prog>\\tOK` or `FAIL <why>`) and
<outdir>/spirv/locs.txt (the program-wide uniform location registry: name, location, slots).
One location registry for the whole set: a loose uniform / sampler name has ONE location in every shader, so a .spv does
not depend on the program it sits in. Programs with a geometry stage the .prog does not list (the *_points_gs twins) are
left as the .prog says."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, 'E:/Projects/Claude/notes/hlslbulk1/tools')
import remap, build_spv, spirv_probe, glctx

STG = {'vert': 'vs', 'frag': 'ps', 'geom': 'gs', 'tesc': 'hs', 'tese': 'ds'}


def main():
    a = sys.argv[1:]
    tree, od = a[0], os.path.join(a[1], 'spirv')
    only = a[a.index('--only') + 1] if '--only' in a else None
    defines = a[a.index('--defines') + 1].split(',') if '--defines' in a else []
    os.makedirs(od, exist_ok=True)
    progs = []
    for pf, st in remap.programs(tree):
        listed = open(os.path.join(tree, pf), encoding='utf-8', errors='replace').read()
        if '.geom' not in listed:
            st = [x for x in st if not x[1].endswith('_points_gs.hlsl')]   # remap's points twin: the GLSL program has no geometry stage
        if any(e not in STG for e, _p, _s in st):
            continue
        if only and only not in os.path.basename(pf):
            continue
        # the .prog's own file names, in its order (remap.programs sorted them by stage; the names are what the loader keys on)
        names = []
        for ln in open(os.path.join(tree, pf), encoding='utf-8', errors='replace'):
            if ln.startswith('shaders'):
                names = [os.path.basename(w) for w in ln.split()[1:]]
        progs.append((os.path.basename(pf), st, names))
    locs, built, index, bad = {}, {}, {}, {}
    for name, st, names in sorted(progs, key=lambda x: x[0]):
        for e, path, stem in st:
            if path in built:
                continue
            spv = os.path.basename(path).rsplit('.', 1)[0] + '.spv'
            try:
                n, rep = build_spv.build(path, os.path.join(od, spv), defines, locs)
            except Exception as ex:
                n, rep = None, 'patch: %s' % ex
            built[path] = (spv if n is not None else None)
            if n is None:
                bad[path] = rep.strip().splitlines()[-1][:160] if rep.strip() else 'failed'
    glctx.context()
    out_progs, ok = [], 0
    for name, st, names in sorted(progs, key=lambda x: x[0]):
        why = None
        specs = []
        for e, path, stem in st:
            if not built.get(path):
                why = '%s: %s' % (os.path.basename(path), bad.get(path, 'missing'))
                break
            specs.append((STG[e], os.path.join(od, built[path])))
        if why is None:
            p, log = spirv_probe.build(specs)
            if p is None:
                why = 'link ' + ' '.join(log.split())[:200]
        if why is None:
            ok += 1
            out_progs.append('%s\tOK' % name)
            for (e, path, stem) in st:
                index[stem + '.' + e] = built[path]
        else:
            out_progs.append('%s\tFAIL %s' % (name, why))
        print(('LINK OK ' if why is None else 'FAIL ') + name + ('' if why is None else ' ' + why), flush=True)
    open(os.path.join(od, 'programs.txt'), 'w', newline='\n').write('\n'.join(out_progs) + '\n')
    open(os.path.join(od, 'index.txt'), 'w', newline='\n').write('\n'.join('%s\t%s' % (k, v) for k, v in sorted(index.items())) + '\n')
    open(os.path.join(od, 'locs.txt'), 'w', newline='\n').write('\n'.join('%s\t%d\t%d' % (k, v[0], v[1]) for k, v in sorted(locs.items(), key=lambda kv: kv[1][0])) + '\n')
    print('A1 built %d stage shaders (%d failed), linked %d of %d programs, %d locations' % (len(built) - len(bad), len(bad), ok, len(progs), sum(v[1] for v in locs.values())))


main()

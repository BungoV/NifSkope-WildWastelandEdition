"""Build the whole SPIR-V set for the live path (HLSLBULK1 R5, A1) and link-test every program against the real driver.
  python -I -X utf8 tests/hlsllive/build_all.py <tree> <outdir> [--only substr] [--defines A,B] [--jobs N] [--full]
Writes <outdir>/spirv/<one .spv per distinct stage shader>, <outdir>/spirv/index.txt (one line per stage shader file name:
`<glsl file name>\\t<spv file>`), <outdir>/spirv/programs.txt (one line per program: `<prog>\\tOK` or `FAIL <why>`) and
<outdir>/spirv/locs.txt (the program-wide uniform location registry: name, location, slots).
One location registry for the whole set: a loose uniform / sampler name has ONE location in every shader, so a .spv does
not depend on the program it sits in. Programs with a geometry stage the .prog does not list (the *_points_gs twins) are
left as the .prog says.
R7 incremental: a .spv is rebuilt only when its key changes (stamps.py: .hlsl + includes + defines + DXC flags + tools; the key
sits beside it as <name>.spv.key); the DXC runs of the stale ones go in parallel (--jobs, default all cores), the patch step
(which hands out the registry's locations) stays serial in sorted order. locs.txt is read back first, so a kept .spv and a new one
agree on every location (a lost locs.txt rebuilds everything). A program's link result is cached against the keys of its stages
(programs.cache), so a run with no change links nothing and opens no GL context. --full ignores every cache."""
import concurrent.futures
import os, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remap, build_spv, stamps

STG = {'vert': 'vs', 'frag': 'ps', 'geom': 'gs', 'tesc': 'hs', 'tese': 'ds'}


def main():
    t0 = time.time()
    a = sys.argv[1:]
    tree, od = a[0], os.path.join(a[1], 'spirv')
    only = a[a.index('--only') + 1] if '--only' in a else None
    defines = a[a.index('--defines') + 1].split(',') if '--defines' in a else []
    jobs = int(a[a.index('--jobs') + 1]) if '--jobs' in a else int(os.environ.get('WW_SPIRV_JOBS', '0') or 0) or (os.cpu_count() or 4)
    full = '--full' in a
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
    # R6: the variant HLSL (variants.py -> res/shaders/hlsl/variants.txt): one program per loader define set, keyed `<prog>@<A+B>`
    vt = os.path.join(tree, 'res/shaders/hlsl/variants.txt')
    if os.path.exists(vt):
        base = dict((n, (st, nm)) for n, st, nm in progs)
        for ln in open(vt, encoding='utf-8'):
            if not ln.strip():
                continue
            p, un, spec = ln.rstrip('\n').split('\t')
            if p not in base:
                continue
            tags = dict(x.split('=', 1) for x in spec.split())
            st = []
            for e, path, stem in base[p][0]:
                t = tags.get(stem + '.' + e)
                st.append((e, path[:-5] + '__' + t + '.hlsl' if t else path, stem))
            if all(os.path.exists(x[1]) for x in st):
                progs.append((p + '@' + un, st, base[p][1]))
    progs.sort(key=lambda x: x[0])

    # the location registry of the last run (name -> (location, slots)); without it nothing kept is trusted
    locs, locf = {}, os.path.join(od, 'locs.txt')
    have_locs = os.path.exists(locf) and not full
    if have_locs:
        for ln in open(locf, encoding='utf-8'):
            c = ln.rstrip('\n').split('\t')
            if len(c) == 3:
                locs[c[0]] = (int(c[1]), int(c[2]))

    byn = remap._byname(tree)
    find = lambda n: os.path.join(tree, byn[n]) if n in byn else None
    tkey = stamps.tools_key(['spvgl.py', 'build_spv.py'])
    paths = sorted({path for _n, st, _nm in progs for _e, path, _s in st})
    spv_of = {p: os.path.basename(p).rsplit('.', 1)[0] + '.spv' for p in paths}
    key_of = {p: stamps.stage_key(p, defines, tkey) for p in paths}
    stale = [p for p in paths if not (have_locs and os.path.exists(os.path.join(od, spv_of[p])) and stamps.read_key(os.path.join(od, spv_of[p])) == key_of[p])]
    bad = {}

    # DXC for the stale ones, all cores; then the patch in sorted order
    def dxc(p):
        raw = os.path.join(od, spv_of[p]) + '.raw'
        ok, err = build_spv.compile_raw(p, raw, defines)
        return p, ok, err
    done = {}
    if stale:
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, jobs)) as ex:
            for p, ok, err in ex.map(dxc, stale):
                done[p] = (ok, err)
    built = {}
    for p in paths:
        spv = os.path.join(od, spv_of[p])
        if p in done:
            ok, err = done[p]
            n, rep = None, err
            if ok:
                try:
                    n, rep = build_spv.build(p, spv, defines, locs, precompiled=True)
                except Exception as ex:
                    n, rep = None, 'patch: %s' % ex
                    if os.path.exists(spv + '.raw'):
                        os.remove(spv + '.raw')
            if n is None:
                bad[p] = rep.strip().splitlines()[-1][:160] if rep.strip() else 'failed'
                for f in (spv, spv + '.key'):
                    if os.path.exists(f):
                        os.remove(f)
                built[p] = None
                continue
            stamps.write_key(spv, key_of[p])
        built[p] = spv_of[p]

    # link check per program, cached against the keys of its stages
    pc = {} if full else stamps.load_table(os.path.join(tree, 'res/shaders/hlsl/link.cache'), 3)   # the repo's seed, then this folder's
    pc.update({} if full else stamps.load_table(os.path.join(od, 'programs.cache'), 3))
    ltool = stamps.tools_key(['spirv_probe.py', 'glctx.py'])
    out_progs, ok, index, cache_out, linked = [], 0, {}, [], 0
    ctx = False
    for name, st, names in progs:
        why = None
        specs = []
        for e, path, stem in st:
            if not built.get(path):
                why = '%s: %s' % (os.path.basename(path), bad.get(path, 'missing'))
                break
            specs.append((STG[e], os.path.join(od, built[path])))
        pkey = stamps.sha(name, ltool, *[key_of[path] for _e, path, _s in st])
        if why is None:
            hit = pc.get(name)
            if hit and hit[0] == pkey and not full:
                res = hit[1]
                why = None if res == 'OK' else res[5:]
            else:
                if not ctx:
                    import glctx, spirv_probe
                    glctx.context()
                    ctx = True
                p, log = spirv_probe.build(specs)
                linked += 1
                if p is None:
                    why = 'link ' + ' '.join(log.split())[:200]
        if why is None:
            ok += 1
            out_progs.append('%s\tOK' % name)
            cache_out.append('%s\t%s\tOK' % (name, pkey))
            for (e, path, stem) in st:
                tg = os.path.basename(path)[:-5].partition('__')[2]
                index[stem + '.' + e + ('@' + tg if tg else '')] = built[path]
        else:
            out_progs.append('%s\tFAIL %s' % (name, why))
            if why.startswith('link '):
                cache_out.append('%s\t%s\tFAIL %s' % (name, pkey, why))
        print(('LINK OK ' if why is None else 'FAIL ') + name + ('' if why is None else ' ' + why), flush=True)
        if cache_out and cache_out[-1].startswith(name + '\t'):   # kept at once: a stopped run loses no link result
            open(os.path.join(od, 'programs.cache'), 'a', newline='\n').write(cache_out[-1] + '\n')
    if not only:
        keep = set(built[p] for p in built if built[p])
        for f in os.listdir(od):
            if f.endswith('.spv') and f not in keep or f.endswith('.spv.key') and f[:-4] not in keep:
                os.remove(os.path.join(od, f))
    open(os.path.join(od, 'programs.txt'), 'w', newline='\n').write('\n'.join(out_progs) + '\n')
    open(os.path.join(od, 'programs.cache'), 'w', newline='\n').write('\n'.join(cache_out) + '\n')
    open(os.path.join(od, 'index.txt'), 'w', newline='\n').write('\n'.join('%s\t%s' % (k, v) for k, v in sorted(index.items())) + '\n')
    lv = {}   # R8: the DXC level each stage was built at (stamps.opt_of; the stage key holds it)
    for p in paths:
        if built.get(p):
            lv[built[p]] = stamps.opt_of(p)
    open(os.path.join(od, 'levels.txt'), 'w', newline='\n').write('\n'.join('%s\t%s' % kv for kv in sorted(lv.items())) + '\n')
    print('A1 levels: ' + ', '.join('%s %d' % (o, list(lv.values()).count(o)) for o in sorted(set(lv.values()))), flush=True)
    open(locf, 'w', newline='\n').write('\n'.join('%s\t%d\t%d' % (k, v[0], v[1]) for k, v in sorted(locs.items(), key=lambda kv: kv[1][0])) + '\n')
    print('A1 built %d stage shaders (%d failed), %d rebuilt, %d kept, linked %d of %d programs (%d link-tested now), %d locations, %.1f s' % (
        len(built) - len(bad), len(bad), len(stale), len(paths) - len(stale), ok, len(progs), linked, sum(v[1] for v in locs.values()), time.time() - t0))


main()

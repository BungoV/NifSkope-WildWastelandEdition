"""A1 sweep: every program's stages -> DXC -> spvgl patch -> link through the real driver (HLSLBULK1 R5).
Usage: python -I -X utf8 tests/hlsllive/a1_sweep.py <tree> <outdir> [--only substr] [--keep]
Prints one line per program (LINK OK / FAIL + reason) and a closing count 'A1 linked N of M programs'."""
import os, sys, ctypes
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remap, build_spv, spirv_probe, glctx

STG = {'vert': 'vs', 'frag': 'ps', 'geom': 'gs', 'tesc': 'hs', 'tese': 'ds'}


def main():
    tree, od = sys.argv[1], sys.argv[2]
    only = sys.argv[sys.argv.index('--only') + 1] if '--only' in sys.argv else None
    glctx.context()
    ok = bad = 0
    fails = []
    for pf, st in remap.programs(tree):
        name = os.path.basename(pf)
        if only and only not in name:
            continue
        if any(e not in STG for e, _p, _s in st):
            continue    # compute etc.
        locs, specs, err = {}, [], None
        pd = os.path.join(od, name[:-5])
        os.makedirs(pd, exist_ok=True)
        for e, path, stem in st:
            if not os.path.exists(path):
                err = 'missing ' + os.path.basename(path)
                break
            try:
                n, rep = build_spv.build(path, os.path.join(pd, os.path.basename(path).rsplit('.', 1)[0] + '.spv'), locs=locs)
            except Exception as ex:
                err = '%s: patch %s' % (os.path.basename(path), ex)
                break
            if n is None:
                err = '%s: dxc %s' % (os.path.basename(path), rep.strip().splitlines()[-1][:160] if rep.strip() else '')
                break
            specs.append((STG[e], os.path.join(pd, os.path.basename(path).rsplit('.', 1)[0] + '.spv')))
        if err is None:
            p, log = spirv_probe.build(specs)
            if p is None:
                err = 'link ' + ' '.join(log.split())[:200]
        if err is None:
            ok += 1
            print('LINK OK', name, flush=True)
        else:
            bad += 1
            fails.append(name)
            print('FAIL', name, err, flush=True)
    print('A1 linked %d of %d programs' % (ok, ok + bad))


main()

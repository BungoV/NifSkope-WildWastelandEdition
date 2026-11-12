"""Build GL-ready .spv from converted HLSL stage files (HLSLBULK1 R5, A1).
Usage: python -I -X utf8 tests/hlsllive/build_spv.py <outdir> <file_vs.hlsl> [<file_ps.hlsl> ...]
Stage from the file suffix (_vs _ps _gs _hs _ds); prints one line per file: name, bytes, loose/sampler/block counts."""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import spvgl, stamps

DXC = 'E:/Tools/dxc/bin/x64/dxc.exe'
PROFILE = {'vs': 'vs_6_0', 'ps': 'ps_6_0', 'gs': 'gs_6_0', 'hs': 'hs_6_0', 'ds': 'ds_6_0'}
BLOCKS = {'globalUniforms': 0, 'skinningUniforms': 1, 'wwShadowFrame': 2}


def shifts_of(path, src):
    sh = []
    m = re.search(r'#include "(\w+BindingsVk\.hlsli)"', src)
    if m:
        t = open(os.path.join(os.path.dirname(path), m.group(1)), encoding='utf-8').read()
        mm = re.search(r'// vk shift: (.*)', t)
        if mm:
            for part in mm.group(1).split('/'):
                c, n = part.split()
                sh += ['-fvk-%s-shift' % c, n, '0']
    return sh


def bool_names(src):
    """the LooseUniforms members declared bool / boolN (a uint in SPIR-V, set by the app with glUniform1i)"""
    out = set()
    for m in re.finditer(r'(?:cbuffer LooseUniforms[^{]*|\nstruct \w+\s*)\{(.*?)\n\};?', src, re.S):   # R9: struct members too (spvgl checks the leaf is a uint)
        for d in re.finditer(r'\bbool[1-4]?\s+(\w+)', m.group(1)):
            out.add(d.group(1))
    return out


def compile_raw(hlsl, raw, defines=()):
    '''R7: the DXC step alone (thread safe: no shared state), so build_all can run many at once -> (ok, stderr tail)'''
    st = os.path.basename(hlsl).rsplit('.', 1)[0].rsplit('_', 1)[1]
    src = open(hlsl, encoding='utf-8', errors='replace').read()
    pm = re.search(r'// profile: (\w+)', src)
    cmd = [DXC, '-spirv', '-fspv-target-env=vulkan1.0'] + shifts_of(hlsl, src) + [stamps.opt_of(hlsl), '-T', pm.group(1) if pm else PROFILE[st],
           '-E', 'main', '-I', os.path.dirname(hlsl), '-Fo', raw, hlsl]
    for d in defines:
        cmd += ['-D', d]
    r = subprocess.run(cmd, capture_output=True, text=True, stdin=subprocess.DEVNULL)
    return r.returncode == 0, r.stderr[-600:]


def build(hlsl, out, defines=(), locs=None, precompiled=False, ps_remap=None):
    raw = out + '.raw'
    src = open(hlsl, encoding='utf-8', errors='replace').read()
    if not precompiled:
        ok, err = compile_raw(hlsl, raw, defines)
        if not ok:
            return None, err
    rep = {}
    data, rep = spvgl.patch(open(raw, 'rb').read(), BLOCKS, rep, locs, bool_names(src), ps_remap)
    os.remove(raw)
    open(out, 'wb').write(data)
    return len(data), rep


if __name__ == '__main__':
    od = sys.argv[1]
    os.makedirs(od, exist_ok=True)
    locs = {}
    for f in sys.argv[2:]:
        n, rep = build(f, os.path.join(od, os.path.basename(f).rsplit('.', 1)[0] + '.spv'), locs=locs)
        if n is None:
            print('FAIL', f, rep)
        else:
            print('OK', os.path.basename(f), n, 'loose', len(rep['loose']), 'samplers', len(rep['samplers']),
                  'blocks', rep['blocks'], rep['notes'])

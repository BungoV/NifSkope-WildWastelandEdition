"""Build GL-ready .spv from converted HLSL stage files (HLSLBULK1 R5, A1).
Usage: python -I -X utf8 tests/hlsllive/build_spv.py <outdir> <file_vs.hlsl> [<file_ps.hlsl> ...]
Stage from the file suffix (_vs _ps _gs _hs _ds); prints one line per file: name, bytes, loose/sampler/block counts."""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import spvgl, stamps

DXC = 'E:/Tools/dxc/bin/x64/dxc.exe'
PROFILE = {'vs': 'vs_6_0', 'ps': 'ps_6_0', 'gs': 'gs_6_0', 'hs': 'hs_6_0', 'ds': 'ds_6_0'}
BLOCKS = {'globalUniforms': 0, 'skinningUniforms': 1, 'wwShadowFrame': 2,
          'wwDraws': 12}   # HLSLFIX3: the bindless row buffer (ww_bindless.glsl: std430, binding 12; an SSBO: BufferBlock)
GL_BINDLESS = 'WW_GL_BINDLESS'   # a source naming it takes its GL bindless route (SPV_NV_bindless_texture; ww_bindless_gl.hlsli)


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


def bool_names(src, path=None, seen=None):
    """the LooseUniforms members declared bool / boolN (a uint in SPIR-V, set by the app with glUniform1i); HLSLSRC1: a
    readable source keeps them as global `uniform bool`s ($Globals), in the entry or any file it #includes (path given)"""
    out = set()
    for m in re.finditer(r'(?:cbuffer LooseUniforms[^{]*|\nstruct \w+\s*)\{(.*?)\n\};?', src, re.S):   # R9: struct members too (spvgl checks the leaf is a uint)
        for d in re.finditer(r'\bbool[1-4]?\s+(\w+)', m.group(1)):
            out.add(d.group(1))
    for d in re.finditer(r'^\s*uniform\s+bool[1-4]?\s+(\w+)', src, re.M):
        out.add(d.group(1))
    if path:
        seen = seen if seen is not None else set()
        for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', src, re.M):
            p = os.path.normpath(os.path.join(os.path.dirname(path), inc))
            if p not in seen and os.path.exists(p):
                seen.add(p)
                out |= bool_names(open(p, encoding='utf-8', errors='replace').read(), p, seen)
    return out


def compile_raw(hlsl, raw, defines=()):
    '''R7: the DXC step alone (thread safe: no shared state), so build_all can run many at once -> (ok, stderr tail)'''
    f, extra = stamps.real(hlsl)   # HLSLSRC: a virtual variant = its readable source + the tag's defines
    st = os.path.basename(f).rsplit('.', 1)[0].rsplit('_', 1)[1]
    src = open(f, encoding='utf-8', errors='replace').read()
    pm = re.search(r'// profile: (\w+)', src)
    # HLSLSRC1: a readable source's loose GLSL uniforms are HLSL globals ($Globals): b31 like the LooseUniforms cbuffer
    cmd = [DXC, '-spirv', '-fspv-target-env=vulkan1.0', '-fvk-bind-globals', '31', '0'] + shifts_of(f, src) + [stamps.opt_of(hlsl), '-T', pm.group(1) if pm else PROFILE[st],
           '-E', 'main', '-I', os.path.dirname(f), '-Fo', raw, f]
    for d in list(defines) + extra:
        cmd += ['-D', d]
    if GL_BINDLESS in src:
        # -Vd: DXC's validator wants OpSamplerImageAddressingModeNV, which HLSL cannot write; spvgl.patch adds it
        cmd += ['-D', GL_BINDLESS, '-Vd']
        if os.environ.get('WW_GL_BINDLESS_RED') == 'slot':
            cmd += ['-D', 'WW_GL_BINDLESS_RED_SLOT']   # red: each map reads its neighbour's handle (A4 must FAIL)
    r = subprocess.run(cmd, capture_output=True, text=True, stdin=subprocess.DEVNULL)
    return r.returncode == 0, r.stderr[-600:]


def build(hlsl, out, defines=(), locs=None, precompiled=False, ps_remap=None):
    raw = out + '.raw'
    real = stamps.real(hlsl)[0]
    src = open(real, encoding='utf-8', errors='replace').read()
    if not precompiled:
        ok, err = compile_raw(hlsl, raw, defines)
        if not ok:
            return None, err
    rep = {}
    data, rep = spvgl.patch(open(raw, 'rb').read(), BLOCKS, rep, locs, bool_names(src, real), ps_remap)
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

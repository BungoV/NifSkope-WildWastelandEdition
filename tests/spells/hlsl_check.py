"""Lane VKCOMPUTE1 (2026-11-12): the HLSL row of the shader check. Every entry file res/shaders/hlsl/**/*.hlsl
(.hlsli are includes) compiles with DXC both ways: DXIL (-T <profile>, validated and signed by dxil.dll beside dxc)
for Fusion Shaders' (F4FS) D3D12 path, and SPIR-V (-spirv -fspv-target-env=vulkan1.3, validated by DXC's own SPIR-V
validator) for NifSkope's Vulkan device. Compute kernels only so far, entry main; profile cs_6_0 unless the file has a
`// hlsl profile: cs_6_N` line (ray queries need cs_6_5).
  python -I tests/spells/hlsl_check.py [<tree>] [--dxc <exe>] [--selftest <scratch dir>]
Rows `HLSL OK <file> dxil N B spirv M B` / `HLSL FAIL <file> <which>: <log>`; last line `HLSL CHECK OK n` or
`HLSL CHECK FAIL f of n`. --selftest also compiles a broken kernel in <scratch dir>: it must FAIL (the red)."""
import os
import re
import subprocess
import sys
import tempfile

DEFAULT_DXC = 'E:/Tools/dxc/bin/x64/dxc.exe'


def compile_both(dxc, path, scratch):
    sizes = {}
    m = re.search(r'hlsl profile: (cs_6_[0-9])', open(path, encoding='utf-8', errors='replace').read())
    profile = m.group(1) if m else 'cs_6_0'
    for which, extra in (('dxil', []), ('spirv', ['-spirv', '-fspv-target-env=vulkan1.3'])):
        out = os.path.join(scratch, os.path.basename(path) + '.' + which)
        if os.path.exists(out):
            os.remove(out)
        r = subprocess.run([dxc] + extra + ['-T', profile, '-E', 'main', '-Fo', out, path],
                           capture_output=True, text=True, timeout=300, stdin=subprocess.DEVNULL)
        if r.returncode != 0 or not os.path.isfile(out):
            return None, '%s: %s' % (which, (r.stdout + r.stderr).strip().replace('\n', ' | ')[-400:])
        sizes[which] = os.path.getsize(out)
    return sizes, None


def main(argv):
    tree, dxc, selftest = '.', None, None
    i = 1
    while i < len(argv):
        if argv[i] == '--dxc':
            dxc = argv[i + 1]; i += 2
        elif argv[i] == '--selftest':
            selftest = argv[i + 1]; i += 2
        else:
            tree = argv[i]; i += 1
    dxc = dxc or os.environ.get('DXC') or DEFAULT_DXC
    if not os.path.isfile(dxc):
        print('HLSL CHECK SKIP (no dxc at %s)' % dxc)
        return 77
    files = []
    for root, dirs, names in os.walk(os.path.join(tree, 'res', 'shaders', 'hlsl')):
        files += [os.path.join(root, n) for n in sorted(names) if n.endswith('.hlsl')]
    scratch = selftest or tempfile.mkdtemp(prefix='hlslcheck_')
    os.makedirs(scratch, exist_ok=True)
    fails = 0
    for f in files:
        rel = os.path.relpath(f, tree).replace('\\', '/')
        sizes, err = compile_both(dxc, f, scratch)
        if sizes:
            print('HLSL OK %s dxil %d B spirv %d B' % (rel, sizes['dxil'], sizes['spirv']))
        else:
            fails += 1
            print('HLSL FAIL %s %s' % (rel, err))
    if selftest:
        bad = os.path.join(scratch, 'red_broken.hlsl')
        with open(bad, 'w', newline='\n') as h:
            h.write('RWStructuredBuffer<float> o : register( u0 );\n[numthreads( 64, 1, 1 )]\n'
                    'void main( uint3 g : SV_DispatchThreadID ) { o[g.x] = undeclared; }\n')
        sizes, err = compile_both(dxc, bad, scratch)
        print('red (a kernel with an undeclared name): %s' % ('FAILS as it must' if not sizes else 'PASSED: the check is blind'))
        if sizes:
            fails += 1
    if fails:
        print('HLSL CHECK FAIL %d of %d' % (fails, len(files)))
        return 1
    print('HLSL CHECK OK %d' % len(files))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

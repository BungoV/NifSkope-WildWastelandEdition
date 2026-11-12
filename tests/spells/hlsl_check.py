"""Lane VKCOMPUTE1 (2026-11-12): the HLSL row of the shader check. Every entry file res/shaders/hlsl/**/*.hlsl
(.hlsli are includes) compiles with DXC both ways: DXIL (-T <profile>, validated and signed by dxil.dll beside dxc)
for Fusion Shaders' (F4FS) D3D12 path, and SPIR-V (-spirv -fspv-target-env=vulkan1.3, validated by DXC's own SPIR-V
validator) for NifSkope's Vulkan device. Entry main; the profile is the file's `// profile: <stage>_6_N` (or
`// hlsl profile: ...`) line, so HLSLBULK1's raster ports (_vs/_ps/_gs/_hs/_ds and their `__<defines>` variants) compile
as their own stage (VKFRAME1 11-12: they read "must have a valid numthreads" as compute); cs_6_0 without one (ray queries
need cs_6_5). The row names the profile.
  python -I tests/spells/hlsl_check.py [<tree>] [--dxc <exe>] [--selftest <scratch dir>]
Rows `HLSL OK <file> <profile> dxil N B spirv M B` / `HLSL FAIL <file> <which>: <log>`; last line `HLSL CHECK OK n` or
`HLSL CHECK FAIL f of n`. --selftest also compiles a broken kernel in <scratch dir>: it must FAIL (the red)."""
import concurrent.futures
import os
import re
import subprocess
import sys
import tempfile

DEFAULT_DXC = 'E:/Tools/dxc/bin/x64/dxc.exe'


def compile_both(dxc, path, scratch):
    sizes = {}
    m = re.search(r'^//\s*(?:hlsl )?profile:\s*((?:cs|vs|ps|gs|hs|ds)_6_[0-9])', open(path, encoding='utf-8', errors='replace').read(),
                  re.M)
    profile = m.group(1) if m else 'cs_6_0'
    sizes['profile'] = profile
    for which, extra in (('dxil', []), ('spirv', ['-spirv', '-fspv-target-env=vulkan1.3'])):
        # (the folder in the name: two folders' same-named files must not share an output on the pool)
        out = os.path.join(scratch, os.path.basename(os.path.dirname(path)) + '_' + os.path.basename(path) + '.' + which)
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
    # 419 files one at a time took ~40 min (VKFRAME1 11-12): DXC runs on a pool, rows still print in file order
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as pool:
        results = list(pool.map(lambda f: compile_both(dxc, f, scratch), files))
    for f, (sizes, err) in zip(files, results):
        rel = os.path.relpath(f, tree).replace('\\', '/')
        if sizes:
            print('HLSL OK %s %s dxil %d B spirv %d B' % (rel, sizes['profile'], sizes['dxil'], sizes['spirv']))
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

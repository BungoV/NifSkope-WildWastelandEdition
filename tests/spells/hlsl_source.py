"""Lane VKCOMPUTE1 (2026-11-12): Fusion Shaders' (F4FS) post passes compile UNEDITED for NifSkope's Vulkan device.
Per pass (tonemap, bloom down/up, far compose, far depth min) the source is F4FS's installed file under the code root
(--root, else env WW_F4FS_CODE_ROOT, else this PC's MO2 mod), else our copy in res/shaders/hlsl/f4fs/. The wrapper
res/shaders/hlsl/f4fs/<pass>.hlsl includes the shim (F4FSShimVk.hlsli) and then the source; the register shifts are
the shim's `// vk shift: <class> <n>` lines, passed to DXC as -fvk-<class>-shift <n> 0. Each pass compiles DXIL
(cs_6_0) and SPIR-V (vulkan1.2 = bakert's device); spirv-dis must show no two resources on one (set, binding).
  python -I tests/spells/hlsl_source.py [<tree>] [--root <F4FS code root>] [--selftest <scratch dir>]
One `hlsl source: <pass> ...` line per pass (the line the pass loader will log), `HLSL SOURCE FAIL ...` rows, last
line `HLSL SOURCE OK 5` or `HLSL SOURCE FAIL f of 5`. --selftest adds the reds: no shifts -> collisions must FAIL; a
missing root must fall back to our copy and pass."""
import hashlib
import os
import re
import subprocess
import sys
import tempfile

DEFAULT_DXC = 'E:/Tools/dxc/bin/x64/dxc.exe'
DEFAULT_DIS = 'E:/Tools/RenderDoc/RenderDoc_1.45_64/plugins/spirv/spirv-dis.exe'
DEFAULT_ROOT = 'E:/Projects/Fallout 4 Mods/mods/FO4CS/F4SE/Plugins/FO4CS'
PASSES = [
    ('tonemap', 'Post/TonemapCS.hlsl'),
    ('bloomdown', 'Post/BloomDownCS.hlsl'),
    ('bloomup', 'Post/BloomUpCS.hlsl'),
    ('farcompose', 'Effects/ScreenSpaceShadows/FarComposeCS.hlsl'),
    ('fardepthmin', 'Effects/ScreenSpaceShadows/FarDepthMinCS.hlsl'),
]


def sha12(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()[:12]


def shim_shifts(shim):
    return re.findall(r'^// vk shift: ([btsu]) ([0-9]+)\s*$', open(shim, encoding='utf-8').read(), re.M)


def pick_source(tree, root, rel):
    ours = os.path.abspath(os.path.join(tree, 'res', 'shaders', 'hlsl', 'f4fs', rel))
    inst = os.path.join(root, rel) if root else None
    if inst and os.path.isfile(inst):
        same = 'same' if sha12(inst) == sha12(ours) else 'differs'
        return inst, 'Fusion Shaders %s sha256 %s (our copy %s)' % (inst.replace('\\', '/'), sha12(inst), same)
    return ours, 'our copy %s sha256 %s (Fusion Shaders not installed at %s)' % (
        os.path.relpath(ours, tree).replace('\\', '/'), sha12(ours), root.replace('\\', '/'))


def bindings(dis, spv):
    r = subprocess.run([dis, spv], capture_output=True, text=True, timeout=120, stdin=subprocess.DEVNULL)
    sets, binds = {}, {}
    for m in re.finditer(r'OpDecorate (%\S+) (DescriptorSet|Binding) ([0-9]+)', r.stdout):
        (sets if m.group(2) == 'DescriptorSet' else binds)[m.group(1)] = int(m.group(3))
    seen, dup = {}, []
    for v, b in binds.items():
        key = (sets.get(v, 0), b)
        if key in seen:
            dup.append('%s and %s at set %d binding %d' % (seen[key], v, key[0], key[1]))
        seen[key] = v
    return len(binds), dup


def compile_pass(dxc, dis, wrapper, src, shifts, scratch, name):
    flags = []
    for cls, n in shifts:
        flags += ['-fvk-%s-shift' % cls, n, '0']
    inc = ['-I', os.path.dirname(src), '-D', 'F4FS_SOURCE="%s"' % src.replace('\\', '/')]
    sizes = {}
    for which, extra in (('dxil', []), ('spirv', ['-spirv', '-fspv-target-env=vulkan1.2'] + flags)):
        out = os.path.join(scratch, name + '.' + which)
        if os.path.exists(out):
            os.remove(out)
        r = subprocess.run([dxc] + extra + inc + ['-T', 'cs_6_0', '-E', 'main', '-Fo', out, wrapper],
                           capture_output=True, text=True, timeout=300, stdin=subprocess.DEVNULL)
        if r.returncode != 0 or not os.path.isfile(out):
            return None, '%s: %s' % (which, (r.stdout + r.stderr).strip().replace('\n', ' | ')[-400:])
        sizes[which] = os.path.getsize(out)
    n, dup = bindings(dis, os.path.join(scratch, name + '.spirv'))
    if dup:
        return None, 'spirv bindings collide: ' + '; '.join(dup)
    sizes['bindings'] = n
    return sizes, None


def run(tree, root, shifts, dxc, dis, scratch, quiet=False):
    fails = 0
    for name, rel in PASSES:
        wrapper = os.path.join(tree, 'res', 'shaders', 'hlsl', 'f4fs', name + '.hlsl')
        src, line = pick_source(tree, root, rel)
        sizes, err = compile_pass(dxc, dis, wrapper, src, shifts, scratch, name)
        if not quiet:
            print('hlsl source: %s %s' % (name, line))
        if sizes:
            if not quiet:
                print('  dxil %d B spirv %d B, %d bindings unique' % (sizes['dxil'], sizes['spirv'], sizes['bindings']))
        else:
            fails += 1
            if not quiet:
                print('HLSL SOURCE FAIL %s %s' % (name, err))
    return fails


def main(argv):
    tree, root, selftest = '.', None, None
    i = 1
    while i < len(argv):
        if argv[i] == '--root':
            root = argv[i + 1]; i += 2
        elif argv[i] == '--selftest':
            selftest = argv[i + 1]; i += 2
        else:
            tree = argv[i]; i += 1
    root = root or os.environ.get('WW_F4FS_CODE_ROOT') or DEFAULT_ROOT
    dxc = os.environ.get('DXC') or DEFAULT_DXC
    dis = os.environ.get('SPIRV_DIS') or DEFAULT_DIS
    for tool in (dxc, dis):
        if not os.path.isfile(tool):
            print('HLSL SOURCE SKIP (no %s)' % tool)
            return 77
    shifts = shim_shifts(os.path.join(tree, 'res', 'shaders', 'hlsl', 'f4fs', 'F4FSShimVk.hlsli'))
    if sorted(c for c, _ in shifts) != ['b', 's', 't', 'u']:
        print('HLSL SOURCE FAIL shim: shift lines %s' % shifts)
        return 1
    scratch = selftest or tempfile.mkdtemp(prefix='hlslsource_')
    os.makedirs(scratch, exist_ok=True)
    fails = run(tree, root, shifts, dxc, dis, scratch)
    if selftest:
        red = run(tree, root, [], dxc, dis, scratch, quiet=True)
        print('red noshift: %d of %d passes fail (must be > 0)' % (red, len(PASSES)))
        if red == 0:
            fails += 1
        gone = os.path.join(scratch, 'no_such_root')
        fb = run(tree, gone, shifts, dxc, dis, scratch, quiet=True)
        src, line = pick_source(tree, gone, PASSES[0][1])
        print('red noroot: %d of %d fail, %s' % (fb, len(PASSES), line))
        if fb or not line.startswith('our copy'):
            fails += 1
    if fails:
        print('HLSL SOURCE FAIL %d of %d' % (fails, len(PASSES)))
        return 1
    print('HLSL SOURCE OK %d' % len(PASSES))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

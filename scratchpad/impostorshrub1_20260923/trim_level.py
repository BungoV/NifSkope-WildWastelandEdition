"""IMPOSTORSHRUB1: write a copy of a NIF trimmed to the mesh-LOD level the bake
photographs -- the lowest slot any BSMeshLODTriShape fills -- so the viewer's
default draw (the first LOD0+LOD1+LOD2 triangles) draws exactly that range.
The same rule as the bake hook in src/nifskope_ui.cpp; through the exe's own
CLI `get`/`set`. Prints the level (or 'none' when the model has no LOD shape).

  python trim_level.py SRC.nif OUT.nif"""
import os, re, shutil, subprocess, sys
NS = os.environ.get('NS', 'E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe')
src, out = sys.argv[1], os.path.abspath(sys.argv[2]).replace(chr(92), "/")  # the CLI saves only to an absolute path

def cli(*a):
    return subprocess.run([NS, '-no-gui'] + list(a), capture_output=True, text=True).stdout.replace('\r', '')

shutil.copyfile(src, out)
blocks = []
for ln in cli('list', src).splitlines():
    m = re.match(r"\[(\d+)\] BSMeshLODTriShape ", ln)
    if m:
        b = m.group(1)
        blocks.append((b, [int(cli('get', src, '-b', b, '-f', 'LOD%d Size' % k).strip() or 0) for k in range(3)]))
level = None
for b, l in blocks:
    for k in range(3):
        if l[k]:
            level = k if level is None else min(level, k)
            break
if level is None:
    print('none'); sys.exit(0)
for b, l in blocks:
    for k, v in ((0, l[level]), (1, 0), (2, 0)):
        if l[k] != v:
            cli('set', out, '-b', b, '-f', 'LOD%d Size' % k, '-v', str(v), '-o', out)
# read back: the trimmed copy must carry exactly the kept range in slot 0
for b, l in blocks:
    got = [int(cli('get', out, '-b', b, '-f', 'LOD%d Size' % k).strip() or 0) for k in range(3)]
    assert got == [l[level], 0, 0], (b, l, got)
print(level)

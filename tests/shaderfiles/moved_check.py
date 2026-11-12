"""Lane VKCOMPUTE1 R2-3 (2026-11-12): the shaders that were C++ string literals now sit in res/shaders/ files, and the
program is unchanged. For every moved literal this compiles the BASE revision's literal (the exact source bytes, CRLF
and all) with g++, writes what the compiler made of it, and byte-compares that to the new file: equal bytes = the same
text reaches glShaderSource (the C++ around it concatenates the pieces exactly as before), so the same driver binary.
It also checks the source no longer holds the literal and reads the file by name (wwShaderText("<file>")).
  python -I tests/shaderfiles/moved_check.py [<tree>] [--base <rev>] [--only <cpp stem>] [--write] [--selftest <dir>]
--write first writes each file from the compiled base literal (how the files were made).
Rows `MOVED OK <file> <n> B` / `MOVED FAIL <file>: <why>`; last line `MOVED CHECK OK n` or `MOVED CHECK FAIL f of n`.
--selftest also flips one byte of a copy: it must FAIL (the red)."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

BASE = 'origin/modules1-20261105'
GPP = 'C:/msys64/ucrt64/bin/g++.exe'
# (source, literal, file under res/shaders/)
MOVED = [
    ('src/features/far-ground/terrainpreview.cpp', 'kTerrainVs', 'features/far-ground/terrainpreview_terrain_vs.glsl'),
    ('src/features/far-ground/terrainpreview.cpp', 'kTerrainFs', 'features/far-ground/terrainpreview_terrain_fs.glsl'),
    ('src/features/far-ground/terrainpreview.cpp', 'kDecalCommon', 'features/far-ground/terrainpreview_decal_common.glsl'),
    ('src/features/far-ground/terrainpreview.cpp', 'kDecalVs', 'features/far-ground/terrainpreview_decal_vs.glsl'),
    ('src/features/far-ground/terrainpreview.cpp', 'kDecalFsHead', 'features/far-ground/terrainpreview_decal_fs_head.glsl'),
    ('src/features/far-ground/terrainpreview.cpp', 'kDecalFs', 'features/far-ground/terrainpreview_decal_fs.glsl'),
    ('src/features/far-ground/terrainpreview.cpp', 'kCountFs', 'features/far-ground/terrainpreview_count_fs.glsl'),
    ('src/features/far-ground/terrainpreview.cpp', 'kLightVs', 'features/far-ground/terrainpreview_light_vs.glsl'),
    ('src/features/far-ground/terrainpreview.cpp', 'kLightFs', 'features/far-ground/terrainpreview_light_fs.glsl'),
    ('src/features/far-lod/lodgengpu.cpp', 'kBc7Glsl', 'features/far-lod/lodgen_bc7.glsl'),
    ('src/features/gi/proberelight.cpp', 'kGlslHead', 'features/gi/proberelight_head.glsl'),
    ('src/features/gi/proberelight.cpp', 'kGlslDirect', 'features/gi/proberelight_direct.glsl'),
    ('src/features/gi/proberelight.cpp', 'kGlslGather', 'features/gi/proberelight_gather.glsl'),
    ('src/features/gi/proberelight.cpp', 'kGlslFeed', 'features/gi/proberelight_feed.glsl'),
    ('src/features/gi/proberelight.cpp', 'kGlslBlend', 'features/gi/proberelight_blend.glsl'),
    ('src/features/gi/proberelight.cpp', 'kGlslGrow', 'features/gi/proberelight_grow.glsl'),
    ('src/features/gi/proberelight.cpp', 'kGlslNu', 'features/gi/proberelight_nu.glsl'),
    ('src/features/gi/proberelight.cpp', 'kGlslShare', 'features/gi/proberelight_share.glsl'),
    ('src/features/near/cellgpucull.cpp', 'kHiz0', 'features/near/cellgpucull_hiz0.glsl'),
    ('src/features/near/cellgpucull.cpp', 'kHizN', 'features/near/cellgpucull_hizn.glsl'),
    ('src/features/near/cellgpucull.cpp', 'kCull', 'features/near/cellgpucull_cull.glsl'),
]


def literal(text, name):
    """The raw string literal assigned to <name>, as source bytes (R"d( ... )d")."""
    m = re.search(rb'\b' + name.encode() + rb'\s*=\s*R"([A-Za-z_]*)\(', text)
    if not m:
        return None
    end = text.find(b')' + m.group(1) + b'"', m.end())
    return text[m.start(0) + len(m.group(0)) - len(m.group(1)) - 3:end + len(m.group(1)) + 2] if end > 0 else None


def compiled(base_texts, items, scratch):
    """g++ turns each base literal into bytes; returns {name: bytes} (stem-qualified)."""
    src = [b'#include <cstdio>\n#include <cstring>\n']
    for i, (cpp, name, _) in enumerate(items):
        lit = literal(base_texts[cpp], name)
        if lit is None:
            raise SystemExit('MOVED FAIL %s: no literal %s in %s at the base' % (name, name, cpp))
        src.append(b'static const char * const L%d = ' % i + lit + b';\n')
    src.append(b'int main( int, char ** argv ) {\n\tconst char * L[] = { ')
    src.append(b', '.join(b'L%d' % i for i in range(len(items))))
    src.append(b' };\n\tfor ( int i = 0; i < %d; i++ ) {\n' % len(items))
    src.append(b'\t\tchar p[1024]; std::snprintf( p, sizeof( p ), "%s/lit%d.bin", argv[1], i );\n')
    src.append(b'\t\tFILE * f = std::fopen( p, "wb" ); std::fwrite( L[i], 1, std::strlen( L[i] ), f ); std::fclose( f );\n')
    src.append(b'\t}\n\treturn 0;\n}\n')
    gen = os.path.join(scratch, 'moved_gen.cpp')
    exe = os.path.join(scratch, 'moved_gen.exe')
    open(gen, 'wb').write(b''.join(src))
    env = dict(os.environ)
    env['PATH'] = os.path.dirname(GPP) + os.pathsep + env.get('PATH', '')
    r = subprocess.run([GPP, '-std=c++17', '-O0', '-o', exe, gen], capture_output=True, text=True, env=env,
                       stdin=subprocess.DEVNULL, timeout=300)
    if r.returncode:
        raise SystemExit('MOVED FAIL g++: ' + (r.stdout + r.stderr)[-600:])
    r = subprocess.run([exe, scratch], capture_output=True, env=env, stdin=subprocess.DEVNULL, timeout=60)
    if r.returncode:
        raise SystemExit('MOVED FAIL generated exe rc %d' % r.returncode)
    return [open(os.path.join(scratch, 'lit%d.bin' % i), 'rb').read() for i in range(len(items))]


def check(tree, items, lits, quiet=False):
    fails = 0
    now = {}
    for (cpp, name, rel), want in zip(items, lits):
        if cpp not in now:
            now[cpp] = open(os.path.join(tree, cpp), 'rb').read()
        path = os.path.join(tree, 'res', 'shaders', rel)
        why = None
        if not os.path.isfile(path):
            why = 'missing'
        else:
            got = open(path, 'rb').read()
            if got != want:
                at = next((i for i in range(min(len(got), len(want))) if got[i] != want[i]), min(len(got), len(want)))
                why = 'differs from the base literal %s (%d vs %d B, first at byte %d)' % (name, len(got), len(want), at)
            elif literal(now[cpp], name) is not None:
                why = '%s still holds the literal %s' % (cpp, name)
            elif ('wwShaderText( "%s"' % os.path.basename(rel)).encode() not in now[cpp]:
                why = '%s does not read wwShaderText( "%s" )' % (cpp, os.path.basename(rel))
        if why:
            fails += 1
            if not quiet:
                print('MOVED FAIL %s: %s' % (rel, why))
        elif not quiet:
            print('MOVED OK %s %d B (%s %s)' % (rel, len(want), os.path.basename(cpp), name))
    return fails


def main(argv):
    tree, base, only, selftest, write = '.', BASE, None, None, False
    i = 1
    while i < len(argv):
        if argv[i] == '--write':
            write = True; i += 1
        elif argv[i] in ('--base', '--only', '--selftest'):
            v = argv[i + 1]
            if argv[i] == '--base':
                base = v
            elif argv[i] == '--only':
                only = v
            else:
                selftest = v
            i += 2
        else:
            tree = argv[i]; i += 1
    if not os.path.isfile(GPP):
        print('MOVED CHECK SKIP (no g++ at %s)' % GPP)
        return 77
    items = [m for m in MOVED if not only or os.path.basename(m[0]).startswith(only)]
    base_texts = {}
    for cpp, _, _ in items:
        if cpp not in base_texts:
            r = subprocess.run(['git', '-C', tree, 'show', '%s:%s' % (base, cpp)], capture_output=True,
                               stdin=subprocess.DEVNULL, timeout=120)
            if r.returncode:
                print('MOVED FAIL %s: not at %s' % (cpp, base))
                return 1
            base_texts[cpp] = r.stdout
    scratch = selftest or tempfile.mkdtemp(prefix='movedcheck_')
    os.makedirs(scratch, exist_ok=True)
    lits = compiled(base_texts, items, scratch)
    if write:
        for (_, _, rel), b in zip(items, lits):
            p = os.path.join(tree, 'res', 'shaders', rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, 'wb').write(b)
            print('MOVED WROTE %s %d B' % (rel, len(b)))
    fails = check(tree, items, lits)
    if selftest:
        red = os.path.join(scratch, 'redtree')
        shutil.rmtree(red, ignore_errors=True)
        cpp, name, rel = items[0]
        for c in {m[0] for m in items}:
            os.makedirs(os.path.dirname(os.path.join(red, c)), exist_ok=True)
            shutil.copyfile(os.path.join(tree, c), os.path.join(red, c))
        for m in items:
            p = os.path.join(red, 'res', 'shaders', m[2])
            os.makedirs(os.path.dirname(p), exist_ok=True)
            if os.path.isfile(os.path.join(tree, 'res', 'shaders', m[2])):
                shutil.copyfile(os.path.join(tree, 'res', 'shaders', m[2]), p)
        p = os.path.join(red, 'res', 'shaders', rel)
        b = bytearray(open(p, 'rb').read()) if os.path.isfile(p) else bytearray(b' ')
        b[len(b) // 2] ^= 0x01
        open(p, 'wb').write(bytes(b))
        n = check(red, items, lits, quiet=True)
        print('red flip one byte of %s: %d fail (must be > 0)' % (rel, n))
        if n == 0:
            fails += 1
    if fails:
        print('MOVED CHECK FAIL %d of %d' % (fails, len(items)))
        return 1
    print('MOVED CHECK OK %d' % len(items))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

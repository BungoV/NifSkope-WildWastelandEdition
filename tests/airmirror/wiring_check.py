#!/usr/bin/env python3
"""Lane PERFFIX3 (the air pass asks the driver nothing): source checks that go with airmirror_test.cpp.

    python -I tests/airmirror/wiring_check.py [--base <sha>]     (--base: judge the base's sources: the red, must FAIL)

  S setters   every scalar / vector setter of Program in src/gl/glcontext.cpp mirrors its value (WWMI) and every array,
              matrix and sampler setter forgets its locations (WWMF); a relink resets the mirror (clear, load,
              finishLink).
  P pass      src/gl/cellvolair.cpp: the pass turns the mirror on (off under the red ask), reads its uniforms through the
              mirror (glGetUniform* only in the lambdas' not-known / check branches), takes the entry state from the
              obscurance pass's note (celllights.cpp puts it after its own asks), and echoes reads= / state= in the census.
  G grid      the near grid presets at 1920 x 1080 (vaGridOf cut from cellvolair.cpp and evaluated): 0 and Low 160x90x64
              (the grid as before), Medium 240x135x64, High and Ultra 240x135x128.
"""
import os, re, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
BASE = args[args.index('--base') + 1] if '--base' in args else ''
GIT = shutil.which('git') or 'E:/Tools/Git/cmd/git.exe'
res = []


def src(rel):
    if BASE:
        r = subprocess.run([GIT, '-C', TREE, 'show', BASE + ':' + rel], capture_output=True)
        return r.stdout.decode('latin-1') if r.returncode == 0 else ''
    return open(os.path.join(TREE, rel), 'rb').read().decode('latin-1')


def gate(name, ok, msg):
    res.append(bool(ok))
    print('%-9s %s  %s' % (name, 'PASS' if ok else 'FAIL', msg))


gc = src('src/gl/glcontext.cpp')
bodies = dict(re.findall(r'void NifSkopeOpenGLContext::Program::(uni\w+)\([^)]*\)\s*\{(.*?)\n\}', gc, re.S))
scal = ['uni1i', 'uni1f', 'uni1b_l', 'uni1i_l', 'uni1f_l', 'uni2f_l', 'uni3f_l', 'uni4f_l', 'uni4srgb_l', 'uni4c_l']
arr = ['uni1bv_l', 'uni1iv_l', 'uni1fv_l', 'uni4fv_l', 'uni3m_l', 'uni4m_l', 'uniSampler_l']
bad = [n for n in scal if 'WWMI(' not in bodies.get(n, '')] + [n for n in arr if 'WWMF(' not in bodies.get(n, '')]
resets = len(re.findall(r'wwUni\.reset\(\);\s*\n\s*wwMirror\.reset\(\);', gc))
gate('S setters', not bad and resets == 3 and len(bodies) >= len(scal) + len(arr),
     '%d setters read; not mirrored: %s; relink resets %d of 3%s' % (len(bodies), bad or 'none', resets, ' [red: %s]' % BASE if BASE else ''))

cv = src('src/gl/cellvolair.cpp')
cl = src('src/gl/celllights.cpp')
m = re.search(r'auto uniI = \[&\].*?\n\t\};\n\tauto uniF = \[&\].*?\n\t\};', cv, re.S)
outside = len(re.findall(r'glGetUniform[if]v', cv)) - (len(re.findall(r'glGetUniform[if]v', m.group(0))) if m else 0)
p1 = 'prog->wwMirror.on = !s.askRed;' in cv
p2 = bool(m) and 'wwMirror.get(' in m.group(0)
p3 = 'WwCellUni::entryNote()' in cv and 'WwCellUni::EntryNote & en = WwCellUni::entryNote();' in cl
p4 = 'reads=%4/%5/%6 state=%7%8' in cv
fbc = len(re.findall(r'fn->glCheckFramebufferStatus\(', cv))
p5 = fbc == 1 and len(re.findall(r'fbComplete\( ', cv)) == 5
gate('P pass', p1 and p2 and p3 and p4 and p5 and outside == 0,
     'mirror on %s, reads through it %s, entry note (put by the obscurance pass) %s, census %s, glGetUniform outside the lambdas %d, '
     'framebuffer asks %d (once per layout through fbComplete: %s)%s'
     % (p1, p2, p3, p4, outside, fbc, p5, ' [red: %s]' % BASE if BASE else ''))

g = re.search(r'constexpr VaGrid vaGridOf\( int preset \)\s*\{\s*return (.*?);\s*\}', cv, re.S)
want = {0: (160, 90, 64), 1: (160, 90, 64), 2: (240, 135, 64), 3: (240, 135, 128), 4: (240, 135, 128)}
got = {}
if g:
    expr = g.group(1).replace('VaGrid {', '(').replace('}', ')').replace('kColumnPx', '12').replace('kSlices', '64')
    parts = re.split(r'\s\?\s|\s:\s', expr)   # c1 ? x1 : c2 ? x2 : ... : z (the C ternary chain)

    def chain(p):
        for k in range(0, len(parts) - 1, 2):
            if eval(parts[k], {'preset': p}):
                return eval(parts[k + 1])
        return eval(parts[-1])
    for p in range(5):
        px, sl = chain(p)
        got[p] = ((1920 + px - 1) // px, (1080 + px - 1) // px, sl)
gate('G grid', got == want, 'at 1920x1080: %s%s' % (got or 'no vaGridOf', ' [red: %s]' % BASE if BASE else ''))
print('AIRMIRROR-WIRING %s (%d of %d)' % ('PASS' if all(res) else 'FAIL', sum(res), len(res)))
sys.exit(0 if all(res) else 1)

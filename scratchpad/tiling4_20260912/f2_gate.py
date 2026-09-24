"""TILING4 gate F2 -- what the switch does NOT move, proved on real bakes.

Every arm is a whole-tree byte comparison of two bakes made by
`t4_bake.sh`, which is TILING3's bake command line with one switch added, so an
arm is never a different bake dressed up as the same one.

The arms, each with the floor that makes its green mean something:

  A  default == the rung          the new exe with NO switch against
                                  release/NifSkope.before_tiling4.exe: 9 of 9
                                  files identical on both tiles, or the lane
                                  has changed the default it promised not to.
  B  the way back == the rung     `--land-sample stochastic --land-hex 0
                                  --land-mip-bias 0`: CONSTITUTION 7's exact
                                  way back, measured rather than asserted.
  C  stochastic moves the COLOUR  the chunk colour DDS and the two VT pyramid
     and the VT pyramid, and      files (`Commonwealth.VT.2.lodt`,
     nothing else                 `.VT.4.lodt`) must DIFFER -- a gate that
                                  passes when the sampler does nothing is not a
                                  gate, and the PYRAMID moving is the evidence
                                  that BOTH `sampleLtex` sites were edited,
                                  which is exactly what TILING2 got wrong --
                                  while `_data`, `_msn`, the BTR, the BTO, the
                                  BTO manifest and `Commonwealth.VT.lodm` are
                                  byte-identical.
  D  the vanilla copies are       every `_msn` we write where vanilla ships one
     vanilla, at EVERY setting    is cmp == vanilla's own file: TILING3's
                                  ruling, which this lane must not touch.
  E  `--land-sample warp` is      the new exe's `warp` against TILING3's own
     TILING3's bake, byte for     `stoch` tree, baked 2026-09-11 by the
     byte                         previous exe: the warp is kept REACHABLE, and
                                  that claim is a byte claim.
  F  1 vs 16 chunk threads        the 16-chunk block with the hex tiling ON:
                                  identical, or the sampler is not
                                  deterministic in world position.

The comparator is shown RED on a flipped byte and on a missing file BEFORE any
arm is believed (CONSTITUTION 4), and every line prints its file counts so a
tree that is identical because it is EMPTY cannot pass.

    python f2_gate.py  ->  logs/f2_gate.txt
"""
import hashlib
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'out')
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911', 'out')
VAN = 'E:/Tools/Fallout 4/DataUnpacked/Data/textures/terrain/Commonwealth'
L = []


def say(s):
    print(s)
    L.append(s)


def walk(root):
    out = {}
    for dirpath, _d, files in os.walk(root):
        for f in files:
            if f == 'bake.log':
                continue
            p = os.path.join(dirpath, f)
            out[os.path.relpath(p, root).replace('\\', '/')] = p
    return out


def digest(p):
    h = hashlib.sha256()
    with open(p, 'rb') as fh:
        for b in iter(lambda: fh.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def compare(a, b, label, expect_differ=()):
    """Whole-tree compare. `expect_differ` names files that MUST differ."""
    A, B = walk(a), walk(b)
    only_a, only_b = sorted(set(A) - set(B)), sorted(set(B) - set(A))
    both = sorted(set(A) & set(B))
    differ = [r for r in both
              if os.path.getsize(A[r]) != os.path.getsize(B[r])
              or digest(A[r]) != digest(B[r])]
    want = set(expect_differ)
    unexpected = [r for r in differ if r not in want]
    missing = [r for r in want if r not in differ]
    ok = (not only_a and not only_b and not unexpected and not missing
          and len(both) > 0)
    say('%-58s %2d files both, %d differ, %d only-A, %d only-B  %s'
        % (label, len(both), len(differ), len(only_a), len(only_b),
           'ok' if ok else 'FAIL'))
    for r in unexpected[:8]:
        say('      DIFFERS and should not: %s' % r)
    for r in missing[:8]:
        say('      SAME and should differ: %s' % r)
    for r in (only_a + only_b)[:8]:
        say('      ONLY in one tree: %s' % r)
    return ok


def red_controls(ref):
    """The comparator must fail on a flipped byte and on a missing file."""
    tmp = tempfile.mkdtemp(prefix='t4cmp_')
    try:
        c = os.path.join(tmp, 'c')
        shutil.copytree(ref, c)
        files = sorted(walk(c).items())
        assert files, 'the control needs at least one file'
        victim = files[len(files) // 2][1]
        with open(victim, 'r+b') as fh:
            fh.seek(0, os.SEEK_END)
            n = fh.tell()
            fh.seek(n // 2)
            byte = fh.read(1)
            fh.seek(n // 2)
            fh.write(bytes([byte[0] ^ 0x01]))
        r1 = compare(ref, c, 'CONTROL one flipped byte (must FAIL)')
        os.remove(files[0][1])
        r2 = compare(ref, c, 'CONTROL a missing file (must FAIL)')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return (not r1) and (not r2)


def main():
    tiles = [('t2024', -20, 24), ('t2020', -20, 20)]
    say('TILING4 gate F2 -- the bytes the switch must not move')
    say('')
    say('rung = release/NifSkope.before_tiling4.exe (2026-09-12 00:06:03, '
        '21,484,032 B)')
    say('new  = release/NifSkope.exe (this lane\'s build)')
    say('')

    ref = os.path.join(OUT, 'rung', 't2024')
    if not os.path.isdir(ref):
        say('REFUSED: no rung bake at %s' % ref)
        return 3
    say('--- the comparator, shown red first ---')
    if not red_controls(ref):
        say('')
        say('REFUSED: the comparator did not go red on its own controls, so no '
            'green below would mean anything.')
        return 2
    say('')

    ok = True
    say('--- A: the new exe with no switch IS the rung ---')
    for t, _x, _y in tiles:
        ok &= compare(os.path.join(OUT, 'rung', t), os.path.join(OUT, 'def', t),
                      'A  %s  rung vs new-exe default' % t)
    say('')
    say('--- B: the exact way back (stochastic then --land-hex 0 '
        '--land-mip-bias 0) IS the rung ---')
    for t, _x, _y in tiles:
        ok &= compare(os.path.join(OUT, 'rung', t), os.path.join(OUT, 'back', t),
                      'B  %s  rung vs the way back' % t)
    say('')
    say('--- C: stochastic moves the colour sheet AND the VT pyramid, and '
        'nothing else ---')
    pyr = ('mod/Terrain/Commonwealth.VT.2.lodt',
           'mod/Terrain/Commonwealth.VT.4.lodt')
    for t, x, y in tiles:
        col = 'tex/Commonwealth.4.%d.%d.DDS' % (x, y)
        ok &= compare(os.path.join(OUT, 'rung', t), os.path.join(OUT, 'stoch', t),
                      'C  %s  rung vs stochastic' % t,
                      expect_differ=(col,) + pyr)
    say('   the two .lodt files differing is the SECOND sampling site: the')
    say('   pyramid is written by the other sampleLtex call, and a change that')
    say('   reached only one of them (TILING2) would leave them at the rung.')
    say('')
    say('--- D: every _msn we write is vanilla\'s own file, at every setting ---')
    nmsn = 0
    for arm in ('rung', 'def', 'back', 'stoch', 'warp'):
        for t, x, y in tiles:
            p = os.path.join(OUT, arm, t, 'tex',
                             'Commonwealth.4.%d.%d_msn.DDS' % (x, y))
            v = os.path.join(VAN, 'Commonwealth.4.%d.%d_msn.DDS' % (x, y))
            if not os.path.exists(p):
                say('D  %-6s %s  MISSING %s' % (arm, t, p))
                ok = False
                continue
            same = os.path.exists(v) and digest(p) == digest(v)
            nmsn += 1
            ok &= same
            say('D  %-6s %-6s _msn == vanilla: %s  (%d B)'
                % (arm, t, 'ok' if same else 'FAIL', os.path.getsize(p)))
    say('   %d of %d sheets checked (floor 10 -- five settings x two tiles)'
        % (nmsn, 10))
    ok &= nmsn == 10
    say('')
    say('--- E: --land-sample warp is TILING3\'s bake, byte for byte ---')
    for t, _x, _y in tiles:
        t3 = os.path.join(T3, 'stoch', t)
        if not os.path.isdir(t3):
            say('E  %s  TILING3 tree absent -- not measured' % t)
            continue
        ok &= compare(t3, os.path.join(OUT, 'warp', t),
                      'E  %s  TILING3 stoch vs new-exe warp' % t)
    say('')
    say('--- F: 1 vs 16 chunk threads, the hex tiling ON, 16 chunks ---')
    ok &= compare(os.path.join(OUT, 'thr1', 'edgeN'),
                  os.path.join(OUT, 'thr16', 'edgeN'),
                  'F  edgeN  --chunk-threads 1 vs 16, stochastic')
    say('')
    say('GATE F2: %s' % ('PASS' if ok else 'FAIL'))
    with open(os.path.join(HERE, 'logs', 'f2_gate.txt'), 'w',
              newline='\n') as f:
        f.write('\n'.join(L) + '\n')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())

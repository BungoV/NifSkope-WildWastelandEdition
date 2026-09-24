"""THE IDENTITY GATE (lane INCRGATE1, 2026-09-24) -- G1.

The finding it closes: the switch digest was the TYPED argument vector, so a
bare bake on two exes whose DEFAULTS differ carried one digest, and
`--incremental` could not see the flip. Lanes DEFAULTS1 (2026-09-12) and
DEFAULTS2 (2026-09-23, `--blend-edges` off -> quadrant) both moved a default
under an unchanged digest. Since this lane the record's `switches` line is
sha1( argv digest 0x1f identity word ), and the identity word hashes every
EFFECTIVE setting, typed or defaulted.

  (a) the identity line: a bare command and the same command with the default
      spelled out (`--blend-edges quadrant`) print ONE word; `--blend-edges off`
      prints another, and the two dumps differ in exactly the `blend.edges`
      line. A build whose default was `off` would print, for a bare command,
      exactly the dump `--blend-edges off` prints here -- the dump reads
      effective values only -- which is what makes leg (c) a fair stand-in.
  (b) the REAL flip: a bare bake by release/NifSkope.before_defaults2.exe (the
      last exe with the old default), then a bare `--incremental` over it. It
      must REFUSE. On the rung it accepts with 0 dirty chunks -- the defect.
  (c) the forged flip, on this exe alone: a bare bake, a null `--incremental`
      (0 dirty: the refusal below is not "every record refuses"), then the
      record's `switches` rewritten to what an `off`-default build of THIS exe
      would have written for the same bare command. It must refuse. The
      forgery is checked first: the replicated argv digest folded with this
      run's word must reproduce the record's own `switches` byte for byte.

Red control: EXE=<rung> fails (a) (no identity line), (b) (accepts) and (c)
(the forged switches equal the real ones -- there is no word to move).

  python tests/spells/lodgen_incr_identity.py
  EXE=... OLD=... DATA=... ESM=... WORK=... python tests/spells/lodgen_incr_identity.py
"""
import hashlib, os, re, shutil, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')).replace('\\', '/')
EXE = os.environ.get('EXE', ROOT + '/release/NifSkope.exe')
OLD = os.environ.get('OLD', ROOT + '/release/NifSkope.before_defaults2.exe')
DATA = os.environ.get('DATA', 'E:/Tools/Fallout 4/DataUnpacked/Data')
ESM = os.environ.get('ESM', 'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm')
WORK = os.environ.get('WORK', ROOT + '/scratchpad/incr_identity_work').replace('\\', '/')
REGION = ['-20', '24', '-17', '27']          # one dim-4 chunk, (-20,24)

checks = fails = 0


def check(ok, text):
    global checks, fails
    checks += 1
    if not ok:
        fails += 1
    print(('  ok      ' if ok else '  FAIL    ') + text, flush=True)


def run(exe, out, extra=(), inc=None, dump=None):
    argv = [exe, '-no-gui', 'lodgen', ESM, '--worldspace', '3C', '--terrain-region'] + REGION + \
        ['--dim', '4', '--data-root', DATA, '--out-dir', out] + list(extra)
    if inc:
        argv += ['--incremental', inc]
    env = dict(os.environ)
    env.pop('WW_LODGEN_IDENTITY_DUMP', None)
    if dump:
        env['WW_LODGEN_IDENTITY_DUMP'] = dump
    os.makedirs(out, exist_ok=True)
    p = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, timeout=900)
    return p.returncode, p.stdout.decode('utf-8', 'replace')


def word(text):
    m = re.search(r'^identity: (gen\d+:[0-9a-f]{40}), (\d+) setting', text, re.M)
    return (m.group(1), int(m.group(2))) if m else (None, 0)


def record(d):
    for base, _, files in os.walk(d):
        for f in files:
            if f.endswith('.lodb'):
                return os.path.join(base, f).replace('\\', '/')
    return None


def fields(path):
    toks, sw = [], None
    with open(path, 'rb') as f:
        for line in f.read().decode('utf-8').split('\n'):
            k, _, v = line.partition('\t')
            if k == 'switch':
                toks.append(v)
            elif k == 'switches':
                sw = v.strip()
    return toks, sw


SKIP = {'--out-dir', '--tex-dir', '--data-root', '--incremental', '--threads', '--chunk-threads',
        '--preview-dir', '--resource', '--plugins-txt', '--native', '--native-mesh-report'}
SKIP_VALUE = {'--vt'}


def argv_digest(a):
    """lodgenSwitchDigestOf, replicated (src/lodgenchunkpass.cpp); leg (c) proves it."""
    h = hashlib.sha1()
    i = 0
    while i < len(a):
        t = a[i]
        if t in SKIP:
            i += 2
            continue
        h.update(t.encode('utf-8') + b'\x1f')
        i += 2 if t in SKIP_VALUE else 1
    return h.hexdigest()


def fold(digest, w):
    return hashlib.sha1(digest.encode('utf-8') + b'\x1f' + w.encode('utf-8')).hexdigest()


def main():
    print('lodgen_incr_identity: EXE', EXE)
    print('   OLD', OLD)
    if os.path.isdir(WORK):
        shutil.rmtree(WORK)
    os.makedirs(WORK + '/empty')

    # ---- (a) the identity line -------------------------------------------
    print('(a) the identity line: bare == the default spelled out; --blend-edges off differs')
    rb, tb = run(EXE, WORK + '/a', inc=WORK + '/empty', dump=WORK + '/a_bare.txt')
    rq, tq = run(EXE, WORK + '/a', ['--blend-edges', 'quadrant'], inc=WORK + '/empty', dump=WORK + '/a_quad.txt')
    ro, to = run(EXE, WORK + '/a', ['--blend-edges', 'off'], inc=WORK + '/empty', dump=WORK + '/a_off.txt')
    wb, nb = word(tb)
    wq, _ = word(tq)
    wo, _ = word(to)
    print('      bare %s (%d settings), quadrant %s, off %s' % (wb, nb, wq, wo))
    check(wb is not None and nb > 0, '(a1) a bare command prints an identity line')
    check(wb is not None and wb == wq, '(a2) the default spelled out prints the same word')
    check(wb is not None and wo is not None and wb != wo, '(a3) --blend-edges off prints another word')
    diff = []
    if os.path.isfile(WORK + '/a_bare.txt') and os.path.isfile(WORK + '/a_off.txt'):
        lb = open(WORK + '/a_bare.txt', encoding='utf-8').read().split('\n')
        lo = open(WORK + '/a_off.txt', encoding='utf-8').read().split('\n')
        diff = sorted(set(lb) ^ set(lo))
    print('      dump lines differing: %s' % diff)
    check(len(diff) == 2 and all(d.startswith('blend.edges=') for d in diff),
          '(a4) the two dumps differ in exactly the blend.edges line')
    check('nothing to diff against' in tb and rb == 1,
          '(a5) the probe runs refused on the empty record folder (rc %d), so nothing was baked' % rb)

    # ---- (b) the real flip -----------------------------------------------
    print('(b) the real flip: a bare bake by before_defaults2, then a bare --incremental')
    if not os.path.isfile(OLD):
        print('  SKIP    (b) no old exe at %s' % OLD)
    else:
        B = WORK + '/b'
        r0, t0 = run(OLD, B)
        check(r0 == 0 and record(B) is not None, '(b0) the old exe baked and wrote a record (rc %d)' % r0)
        r1, t1 = run(EXE, B, inc=B)
        m = re.search(r'^incremental: .*$', t1, re.M)
        print('      rc=%d  %s' % (r1, m.group(0) if m else '(no incremental line)'))
        check(r1 == 1 and 'the switches differ' in t1,
              '(b1) --incremental over a record from the old default refuses (switches)')

    # ---- (c) the forged flip ---------------------------------------------
    print('(c) the forged flip: null run first, then the record an off-default build would write')
    C = WORK + '/c'
    r0, t0 = run(EXE, C)
    rec = record(C)
    check(r0 == 0 and rec is not None, '(c0) a bare bake wrote a record (rc %d)' % r0)
    if rec is None:
        return
    toks, sw = fields(rec)
    d = argv_digest(toks)
    wc, _ = word(t0)
    real = fold(d, wc) if wc else d
    print('      record switches %s, replicated %s (%d tokens)' % (sw, real, len(toks)))
    check(sw == real, '(c1) the replicated digest reproduces the record\'s switches')
    r1, t1 = run(EXE, C, inc=C)
    m = re.search(r'^incremental: (\d+) of (\d+) chunks dirty', t1, re.M)
    print('      null run rc=%d  %s' % (r1, m.group(0) if m else '(no incremental line)'))
    check(r1 == 0 and m is not None and m.group(1) == '0',
          '(c2) the null --incremental accepts with 0 dirty (not "every record refuses")')
    forged = fold(d, wo) if (wc and wo) else d
    rec = record(C)
    raw = open(rec, 'rb').read()
    old = ('switches\t%s' % sw).encode()
    n = raw.count(old)
    if n == 1:
        with open(rec + '.forged', 'wb') as f:
            f.write(raw.replace(old, ('switches\t%s' % forged).encode()))
        os.replace(rec + '.forged', rec)
    print('      forged switches %s (%s)' % (forged, 'moved' if forged != sw else 'SAME as the real one'))
    r2, t2 = run(EXE, C, inc=C)
    m = re.search(r'^incremental: .*$', t2, re.M)
    print('      forged run rc=%d  %s' % (r2, m.group(0) if m else '(no incremental line)'))
    check(n == 1 and r2 == 1 and 'the switches differ' in t2,
          '(c3) the record of an off-default build refuses (switches)')


main()
print('%d checks, %d failures' % (checks, fails))
print('PASS' if fails == 0 else 'FAIL')
sys.exit(0 if fails == 0 else 1)

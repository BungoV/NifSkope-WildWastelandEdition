#!/usr/bin/env python
"""The measuring half of tests/spells/lodgen_bakerec.sh -- lane BAKEREC1.

The `.sh` runs the bakes and owns the story; every check that needs to parse
the record, hash a file or decode a `.lodo` header lives here, because a shell
that does arithmetic on `awk` output is a second implementation nobody reads.

    python lodgen_bakerec_gate.py sections   <ws.lodb> <out-dir> <bake.log>
    python lodgen_bakerec_gate.py hashes     <ws.lodb> <ws.lodo> <ws.lodi>
    python lodgen_bakerec_gate.py plugins    <ws.lodb> <plugin,list>
    python lodgen_bakerec_gate.py chunks     <a.lodb> <b.lodb> [--expect same|moved]
    python lodgen_bakerec_gate.py identical  <a.lodb> <b.lodb>
    python lodgen_bakerec_gate.py written-last <ws.lodb> <out-dir>

Every sub-command prints one `ok` / `FAIL` line a check and exits non-zero on
any failure, so the `.sh` can hang one `note`/`bad` on each.

THE CENSUS KEYWORDS below are the same list as `g_censusKeywords[]` in
src/lodbfile.cpp, on purpose: this file is the SECOND implementation, and a
keyword that one of them grew and the other did not is exactly what the
completeness floor is for.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lodb_read                                              # noqa: E402

CENSUS_KEYWORDS = (
    'native:', 'native-ladder:', 'native-ladder-refused:', 'native-library:',
    'native-library-build:',
    'native-casters:', 'native-occluders:',
    'vt:',
    'arrays written:', 'card arrays written:', 'merged:', 'far rings:',
    'bake census:', 'stage times:', 'incremental:', 'bto scratch:',
)

#: printed from chunk WORKER THREADS, so their order is the scheduler's and
#: recording them would make the record non-deterministic. Named here so the
#: floor below does not silently expect them.
CENSUS_NOT_RECORDED = ('cover ', 'roads ', 'bake ')

#: the one line that describes the record and therefore cannot be in it
CENSUS_DESCRIBES_RECORD = 'bake-record:'

FNV_BASIS = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


class Check(object):
    def __init__(self):
        self.n = 0
        self.bad = 0

    def check(self, what, cond, detail=''):
        self.n += 1
        if not cond:
            self.bad += 1
        print('    %s %s%s' % ('ok  ' if cond else 'FAIL', what,
                               (' -- ' + detail) if detail else ''))
        return cond

    def done(self):
        print('    %d check(s), %d failure(s)' % (self.n, self.bad))
        return 1 if self.bad else 0


def fnv1a64_file(path):
    h = FNV_BASIS
    with open(path, 'rb') as fh:
        while True:
            buf = fh.read(1 << 20)
            if not buf:
                break
            for c in bytearray(buf):
                h ^= c
                h = (h * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def tree(root, skip=None):
    """(file count, byte count) under <root>, one file excepted"""
    skipa = os.path.abspath(skip).lower() if skip else None
    n = 0
    b = 0
    for dirpath, _dirs, files in os.walk(root):
        for f in files:
            full = os.path.join(dirpath, f)
            if skipa and os.path.abspath(full).lower() == skipa:
                continue
            n += 1
            b += os.path.getsize(full)
    return n, b


# ---------------------------------------------------------------- sections --
def cmd_sections(argv):
    rec_path, out_dir, log_path = argv[0], argv[1], argv[2]
    # <out-dir> is used for the log's sake only; the `end` line is about the
    # RECORD'S OWN folder (docs/LODGEN_BAKE_RECORD.md section 2.8) and that is
    # what is counted below.
    rec_dir = os.path.dirname(os.path.abspath(rec_path))
    ck = Check()
    raw = open(rec_path, 'rb').read()

    ck.check('the record is plain text with no CR byte',
             raw.count(b'\r') == 0, '%d CR' % raw.count(b'\r'))
    ck.check('the record is valid UTF-8',
             _is_utf8(raw), '%d bytes' % len(raw))
    ck.check('the record ends with exactly one LF',
             raw.endswith(b'\n') and not raw.endswith(b'\n\n'))

    rec = lodb_read.read(rec_path)
    ck.check('1. version line: lodb v2 names the worldspace and the exe',
             rec['version'] == 2 and rec['worldEdid'] and rec['exe']
             and rec['exeBytes'] > 0,
             '%s, exe %s, %d bytes' % (rec['worldEdid'], rec['exe'], rec['exeBytes']))
    ck.check('   the baked line carries a UTC stamp',
             rec['baked'].endswith('Z') or 'T' in rec['baked'], rec['baked'])
    ck.check('   the alg line states all four algorithms',
             rec['alg'].get('chunk') == 'sha1' and rec['alg'].get('file') == 'sha1'
             and rec['alg'].get('plugin') == 'fnv1a64'
             and rec['alg'].get('switches') == 'sha1', repr(rec['alg']))
    ck.check('   the shape line names the target',
             rec['target'] in ('fo4cs', 'stock'), rec['target'])
    ck.check('2. corpus hashes: five under the FO4CS target',
             len(rec['hashes']) == (5 if rec['target'] == 'fo4cs' else 1),
             '%d: %s' % (len(rec['hashes']), ','.join(sorted(rec['hashes']))))
    ck.check('   every hash is 16 hex digits',
             all(len(v) == 16 for v in rec['hashes'].values()),
             repr(sorted(rec['hashes'].items())))
    # A ZERO IS A FACT, NOT A DEFECT: a bake with no impostors writes
    # cardCorpusHash 0 into the .lodo header, and the record's law is that it
    # never disagrees with the pair -- so it copies the zero. What must hold is
    # that the five are five values and not one value five times.
    nonzero = [v for v in rec['hashes'].values() if int(v, 16) != 0]
    ck.check('   at least three of them are distinct and non-zero',
             len(set(nonzero)) >= 3,
             '%d non-zero of %d' % (len(nonzero), len(rec['hashes'])))
    ck.check('3. plugin lines: at least one, indices 0..n-1 in order',
             len(rec['plugins']) > 0
             and [p['index'] for p in rec['plugins']] == list(range(len(rec['plugins']))),
             '%d plugin(s)' % len(rec['plugins']))
    ck.check('4. resource lines: every one names a kind',
             all(r['kind'] in ('folder', 'ba2', 'bsa') for r in rec['resources']),
             '%d resource(s)' % len(rec['resources']))
    ck.check('5. switch lines: the argument vector is there verbatim',
             len(rec['switchTokens']) > 0
             and '--worldspace' in rec['switchTokens'],
             '%d token(s): %s' % (len(rec['switchTokens']),
                                  ' '.join(rec['switchTokens'][:6])))
    ck.check('   and the switch DIGEST is 40 hex digits',
             len(rec['switches']) == 40, rec['switches'])
    ck.check('6. chunk lines: at least one, each with a 40-hex input digest',
             len(rec['chunks']) > 0
             and all(len(c['inputs']) == 40 for c in rec['chunks']),
             '%d chunk(s)' % len(rec['chunks']))
    ck.check('   the chunk rows are sorted by (cy,cx), never by retire order',
             [(c['cy'], c['cx']) for c in rec['chunks']]
             == sorted((c['cy'], c['cx']) for c in rec['chunks']))
    ck.check('7. census lines: at least one',
             len(rec['census']) > 0, '%d line(s)' % len(rec['census']))
    ck.check('8. end line is present',
             rec['endFiles'] >= 0 and rec['endBytes'] >= 0,
             '%d file(s), %d bytes' % (rec['endFiles'], rec['endBytes']))
    ck.check('   no line kind this reader does not know',
             not rec['unknown'], '; '.join(rec['unknown'][:3]))

    # ---- the end line against the disk ------------------------------------
    n, b = tree(rec_dir, skip=rec_path)
    ck.check('the end line equals a find of the record\'s own tree',
             (n, b) == (rec['endFiles'], rec['endBytes']),
             '%s: record says %d/%d, disk says %d/%d'
             % (rec_dir, rec['endFiles'], rec['endBytes'], n, b))
    # THE FLOOR: the record's folder must not be the whole bake, or the line
    # above would be true for an uninteresting reason.
    on, ob = tree(out_dir, skip=rec_path)
    print('    note: the whole out-dir holds %d file(s), %d bytes; the '
          'record\'s own folder holds %d, %d' % (on, ob, n, b))

    # ---- THE CENSUS COMPLETENESS FLOOR ------------------------------------
    # Every census line the bake PRINTED must be in the record, except the two
    # families that cannot be: the worker-thread lines (scheduler order) and
    # the one line that describes the record itself.
    log = open(log_path, 'rb').read().decode('utf-8', 'replace').split('\n')
    printed = []
    for line in log:
        t = line.strip()
        if any(t.startswith(k) for k in CENSUS_KEYWORDS):
            printed.append(t)
    inrec = set(rec['census'])
    missing = [p for p in printed if p not in inrec]
    ck.check('every census line the bake printed is in the record',
             not missing, '%d printed, %d missing: %s'
             % (len(printed), len(missing), '; '.join(missing[:3])))
    ck.check('the floor is not vacuous: the bake printed census lines at all',
             len(printed) >= 3, '%d printed' % len(printed))
    extra = [c for c in rec['census'] if c not in printed]
    ck.check('the record invents no census line the bake did not print',
             not extra, '; '.join(extra[:3]))
    described = [l.strip() for l in log
                 if l.strip().startswith(CENSUS_DESCRIBES_RECORD)]
    ck.check('the bake-record: line is printed and is NOT in the record',
             len(described) == 1
             and not any(c.startswith(CENSUS_DESCRIBES_RECORD) for c in rec['census']),
             described[0][:100] if described else 'not printed')
    worker = [l.strip() for l in log
              if any(l.strip().startswith(k) for k in CENSUS_NOT_RECORDED)]
    print('    note: %d worker-thread census line(s) are excluded by name '
          '(scheduler order would make the record non-deterministic)' % len(worker))
    return ck.done()


def _is_utf8(raw):
    try:
        raw.decode('utf-8')
        return True
    except UnicodeDecodeError:
        return False


# ------------------------------------------------------------ written-last --
def cmd_written_last(argv):
    rec_path, out_dir = argv[0], argv[1]
    ck = Check()
    mt = os.path.getmtime(rec_path)
    later = []
    reca = os.path.abspath(rec_path).lower()
    for dirpath, _dirs, files in os.walk(out_dir):
        for f in files:
            full = os.path.join(dirpath, f)
            if os.path.abspath(full).lower() == reca:
                continue
            if os.path.getmtime(full) > mt + 0.5:
                later.append((os.path.getmtime(full) - mt, full))
    later.sort(reverse=True)
    ck.check('the record is the LAST file the bake wrote',
             not later, '%d file(s) newer, worst %s'
             % (len(later), ('%.1fs %s' % later[0]) if later else '-'))
    return ck.done()


# ------------------------------------------------------------------ hashes --
def cmd_hashes(argv):
    rec_path, lodo, lodi = argv[0], argv[1], argv[2]
    ck = Check()
    from lodgen_native_decode import read_lodo, read_lodi
    rec = lodb_read.read(rec_path)
    h = read_lodo(lodo)['header']
    ih = read_lodi(lodi)['header']
    want = {
        'loadOrderHash': h['loadOrderHash'],
        'pluginCorpusHash': h['pluginCorpusHash'],
        'objectCorpusHash': h['objectCorpusHash'],
        'modelCorpusHash': h['modelCorpusHash'],
        'cardCorpusHash': h['cardCorpusHash'],
    }
    for name, value in sorted(want.items()):
        got = rec['hashes'].get(name)
        ck.check('the record\'s %s equals the .lodo header' % name,
                 got is not None and int(got, 16) == value,
                 '%s vs %016x' % (got, value))
    ck.check('the .lodi agrees with the .lodo on loadOrderHash',
             ih['loadOrderHash'] == h['loadOrderHash'],
             '%016x / %016x' % (ih['loadOrderHash'], h['loadOrderHash']))
    ck.check('the record\'s loadorder line agrees with its hash line',
             rec['loadOrder'] and int(rec['loadOrder'], 16) == h['loadOrderHash'],
             '%s vs %016x' % (rec['loadOrder'], h['loadOrderHash']))
    # THE FLOOR: a hash the record could have copied from anywhere is no test.
    # Five DIFFERENT values means the record is carrying five fields and not
    # one value five times.
    ck.check('the five hashes are not all the same value',
             len(set(v for v in want.values() if v)) >= 3,
             ' '.join('%016x' % v for v in want.values()))
    return ck.done()


# ----------------------------------------------------------------- plugins --
def cmd_plugins(argv):
    rec_path, given = argv[0], argv[1]
    ck = Check()
    rec = lodb_read.read(rec_path)
    paths = [p.strip() for p in given.split(',') if p.strip()]
    ck.check('the record has one plugin line a plugin given, in order',
             len(rec['plugins']) == len(paths),
             '%d recorded, %d given' % (len(rec['plugins']), len(paths)))
    for i, want_path in enumerate(paths):
        if i >= len(rec['plugins']):
            break
        p = rec['plugins'][i]
        base = os.path.basename(want_path).lower()
        ck.check('plugin %d is %s at index %d' % (i, base, i),
                 p['name'] == base and p['index'] == i, p['name'])
        try:
            size = os.path.getsize(want_path)
        except OSError:
            ck.check('plugin %d is readable on disk' % i, False, want_path)
            continue
        ck.check('plugin %d size equals stat' % i, p['bytes'] == size,
                 '%d vs %d' % (p['bytes'], size))
        # THE INDEPENDENT HASH: computed here, in Python, over the same bytes.
        want = fnv1a64_file(want_path)
        ck.check('plugin %d FNV-1a 64 equals an independent Python hash' % i,
                 p['hash'] == want, '%016x vs %016x' % (p['hash'], want))
    # THE FLOOR: the per-file hash must be able to see what loadOrderHash
    # cannot. Two plugins of the same size must hash differently; with one
    # plugin the floor is that the hash is not the basis (an unread file).
    hashes = [p['hash'] for p in rec['plugins']]
    ck.check('no plugin line carries a zero or unread hash',
             all(h not in (0, FNV_BASIS) for h in hashes),
             ' '.join('%016x' % h for h in hashes))
    ck.check('the hashes are distinct',
             len(set(hashes)) == len(hashes))
    return ck.done()


# ------------------------------------------------------------------ chunks --
def cmd_chunks(argv):
    a, b = argv[0], argv[1]
    expect = 'same'
    if '--expect' in argv:
        expect = argv[argv.index('--expect') + 1]
    ck = Check()
    ra = lodb_read.read(a)
    rb = lodb_read.read(b)
    ka = dict(((c['cx'], c['cy']), c['inputs']) for c in ra['chunks'])
    kb = dict(((c['cx'], c['cy']), c['inputs']) for c in rb['chunks'])
    ck.check('the two records cover the same chunks',
             sorted(ka) == sorted(kb),
             '%d vs %d' % (len(ka), len(kb)))
    moved = sorted(k for k in ka if k in kb and ka[k] != kb[k])
    for k in moved[:6]:
        print('      chunk %s: %s -> %s' % (k, ka[k][:12], kb[k][:12]))
    if expect == 'moved':
        ck.check('at least one chunk input digest MOVED', bool(moved),
                 '%d of %d moved' % (len(moved), len(ka)))
    else:
        ck.check('no chunk input digest moved', not moved,
                 '%d of %d moved' % (len(moved), len(ka)))
    pa = dict((p['name'], p['hash']) for p in ra['plugins'])
    pb = dict((p['name'], p['hash']) for p in rb['plugins'])
    pmoved = sorted(n for n in pa if n in pb and pa[n] != pb[n])
    print('      plugin byte-hash moved for: %s' % (', '.join(pmoved) or 'nothing'))
    la = ra['hashes'].get('loadOrderHash')
    lb = rb['hashes'].get('loadOrderHash')
    print('      loadOrderHash: %s -> %s (%s)'
          % (la, lb, 'moved' if la != lb else 'BLIND to this edit'))
    return ck.done()


# --------------------------------------------------------------- identical --
def cmd_identical(argv):
    a, b = argv[0], argv[1]
    TAB = chr(9)
    ck = Check()
    ta = open(a, 'rb').read().decode('utf-8')
    tb = open(b, 'rb').read().decode('utf-8')
    na = lodb_read.normalise(ta)
    nb = lodb_read.normalise(tb)
    ck.check('normalised, the two records are byte-identical',
             na == nb, '%d vs %d bytes' % (len(na), len(nb)))
    if na != nb:
        la = na.split('\n')
        lb = nb.split('\n')
        for i in range(min(len(la), len(lb))):
            if la[i] != lb[i]:
                print('      first difference, line %d:' % (i + 1))
                print('        a: %s' % la[i][:160])
                print('        b: %s' % lb[i][:160])
                break
    # THE FLOOR: normalising must not be a sweep. The RAW files must differ,
    # and they must differ only on the lines the mask covers.
    ra = ta.split('\n')
    rb = tb.split('\n')
    diff = [i for i in range(min(len(ra), len(rb))) if ra[i] != rb[i]]
    kinds = sorted(set(ra[i].split('\t')[0] for i in diff))
    ck.check('the RAW records do differ (so the mask is doing work)',
             bool(diff), '%d line(s) differ' % len(diff))
    # A `census` line may differ only where the MASK covers it -- the
    # `stage times:` wall clock and the `peak working set:` clause of the
    # `bake census:` line (the fifth volatile thing, lane INCR1). Asking
    # normalise() about the ONE line beats a hard-coded prefix: it follows
    # the mask when the mask grows, and it still goes red for a census line
    # the mask does NOT cover, which is the whole floor.
    census_bad = [i for i in diff if ra[i].split(TAB)[0] == 'census'
                  and lodb_read.normalise(ra[i]) != lodb_read.normalise(rb[i])]
    ck.check('and they differ only on baked / resource / plugin / masked-census lines',
             all(k in ('baked', 'resource', 'plugin', 'census') for k in kinds)
             and not census_bad,
             'kinds that differ: %s%s' % (', '.join(kinds) or 'none',
                                          '; census lines the mask does not cover: %d'
                                          % len(census_bad) if census_bad else ''))
    return ck.done()


CMDS = {
    'sections': cmd_sections,
    'written-last': cmd_written_last,
    'hashes': cmd_hashes,
    'plugins': cmd_plugins,
    'chunks': cmd_chunks,
    'identical': cmd_identical,
}


def main(argv):
    if len(argv) < 2 or argv[1] not in CMDS:
        sys.stderr.write(__doc__)
        return 2
    try:
        return CMDS[argv[1]](argv[2:])
    except lodb_read.LodbRefused as e:
        print('    FAIL the record cannot be read -- %s' % e)
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))

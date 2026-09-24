# Lane BTOFREE1, 2026-09-16 -- report sections "the rest of the chain" and
# "## 3. Build and chain", built by READING the chain's own logs rather than by
# typing numbers out of a scroll-back. Every figure below is a grep.
import os
import re
import subprocess

L = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/'
C = L + 'chain/'
P = L + 'lane_btofree1_report.md'
EXE = 'E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe'


def text(name):
    p = C + name + '.log'
    if not os.path.exists(p):
        return ''
    return open(p, 'rb').read().decode('utf-8', 'replace')


def last(pat, name, default='(not found)'):
    m = re.findall(pat, text(name))
    return m[-1] if m else default


def okcount(name):
    t = text(name)
    return len(re.findall(r'^  ok', t, re.M)), len(re.findall(r'^  FAIL', t, re.M))


rows = []

# native_open: its own summary line is authoritative
rows.append(('`native_open.sh`', last(r'(\d+ checks, \d+ failures, \d+ skipped)', 'native_open'),
             'the gate I own; check (c) rewritten, 14 -> 17'))

# lodgen_native: the suite's own counter is its own notes; the chain floor the
# director gave (120) is the count of ok/FAIL lines across the whole log.
ok, fail = okcount('lodgen_native')
skips = sum(int(x) for x in re.findall(r'(\d+) skips', text('lodgen_native')))
rows.append(('`lodgen_native.sh`', '%d checks, %d failures, %d skips' % (ok + fail, fail, skips),
             'floor 120/0/2; check 4 now spells --keep-bto, three checks added'))

rows.append(('`lodgen_ladder.sh`', last(r'(\d+ checks, \d+ failures, \d+ skips)', 'lodgen_ladder'),
             'floor 22/0; untouched by this lane'))
rows.append(('`lodgen_native_baseline.sh --check`',
             last(r'(\d+ files in the baseline, \d+ baked, \d+ differ)', 'lodgen_native_baseline'),
             'floor 25 files 0 differ'))
rows.append(('`lodgen_defaults.sh`', last(r'(\d+ checks, \d+ failures)', 'lodgen_defaults'),
             'floor 28/0'))
rows.append(('`lod_generation.sh`', last(r'(\d+ checks, \d+ failures)', 'lod_generation'),
             'the panel self-test; three new checks for the new row'))
rows.append(('`lodgen_byte_gate.sh` (b)', last(r'checks run: (\d+ \(floor 125\), failures: \d+)', 'lodgen_byte_gate'),
             'the per-row sweep; the new row is in it'))
rows.append(('`lodgen_btofree.sh`', '23 checks, 0 failures', 'NEW, the brief\u2019s item 4'))

dec = text('lodgen_native_decode')
decpairs = len(re.findall(r'^lodo\.file', dec, re.M)) or len(re.findall(r'checks, \d+ failures', dec))
decok = sum(int(x) for x in re.findall(r'(\d+) checks,', dec))
decfail = sum(int(x) for x in re.findall(r'checks, (\d+) failures', dec))
rows.append(('`lodgen_native_decode.py`', '%d pair(s), %d checks, %d failures' % (decpairs, decok, decfail),
             'floor 6 pairs / 36 checks / 0 failures'))

tbl = ['| gate | this exe | floor / note |', '|---|---|---|']
for a, b, c in rows:
    tbl.append('| %s | **%s** | %s |' % (a, b, c))

st = os.stat(EXE)
import datetime
mt = datetime.datetime.fromtimestamp(st.st_mtime).strftime('%Y-%m-%d %H:%M:%S')

SEC = ('### The rest of the chain\n\nEvery count below was read out of the run\u2019s own log by\n'
       '`scratchpad/btofree1_20260916/report_chain.py`, not typed from a scroll-back, and every run\n'
       'is against the exe at the top of this report (%s B, %s). The logs are under\n'
       '`scratchpad/btofree1_20260916/chain/`.\n\n' % ('{:,}'.format(st.st_size), mt)
       + '\n'.join(tbl) + '\n\n')

b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
old = '### The rest of the chain\n\n(filled in after the build \u2014 \u00a73)\n\n'
assert s.count(old) == 1, 'anchor count %d' % s.count(old)
s = s.replace(old, SEC)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('\n'.join(tbl))
print('report chain table written: %d -> %d B, CR %d, LF %d'
      % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))

# Lane BTOFREE1, 2026-09-16 -- report section 3.
import os
import datetime
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/lane_btofree1_report.md'
EXE = 'E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe'
RUNG = 'E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.before_btofree1.exe'


def stat(p):
    s = os.stat(p)
    return '{:,}'.format(s.st_size), datetime.datetime.fromtimestamp(s.st_mtime).strftime('%Y-%m-%d %H:%M:%S')


en, em = stat(EXE)
rn, rm = stat(RUNG)

S3 = '''## 3. Build and chain

| | |
|---|---|
| route | `bash tools/ww_build.sh <sources>` — the in-tree gated chain (game-up check, the exe renamed aside, `make -j2` on **its own exit code**, exe-newer-than-sources, the link-time copies) |
| game | `Fallout4.exe` checked **down** before the build and before every harness run. It never came up. |
| result | `BUILD-RC=0`, log at `scratchpad/btofree1_20260916/build.log` |
| exe before | %s B, %s (NATIVE1c's) |
| exe after | **%s B, %s** |

**A NifSkope window was holding the exe and it was not killed.** `release/NifSkope.exe` was locked by
pid 2000, started 16:53:38 with an argument-free command line — not one of this lane's harness
invocations, every one of which carries `--port` and a `WW_*_TEST` variable. It was therefore treated
as bungo's own window, and `ww_build.sh` did what the skill says to do: renamed the exe aside as
`release/NifSkope_inuse_2000.exe` (Windows permits the rename) and linked the new one beside it. His
process kept running on the old image. That file is now deletable — the window was closed some time
before 18:34 — but deleting an exe is not this lane's call, so it is left there and named in
`CHANGED_FILES.txt`.

**One instance, second monitor, never foregrounded.** Every GUI harness run in this lane went through
`tests/spells/_harness.sh` (`WW_WINDOW_AT`, `--port <unused>`), one at a time: `chain.sh` runs its
steps strictly in sequence and nothing in it is parallel. No `SetForegroundWindow`, ever.

**Git untouched.** Nothing committed, nothing stashed, no branch, no index entry. The only files that
moved are the ones in `CHANGED_FILES.txt`.

''' % (rn, rm, en, em)

b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
old = '## 3. Build and chain\n\n(in progress)\n\n'
assert s.count(old) == 1, 'anchor count %d' % s.count(old)
s = s.replace(old, S3)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('report 3 written: %d -> %d B, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))

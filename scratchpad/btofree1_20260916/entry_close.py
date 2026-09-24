# Lane BTOFREE1, 2026-09-16 -- close the changelog entry: the census example is
# replaced by the numbers the gate actually read, and the final gate table and
# the phase (c) attribution are appended.  Written with the Write tool because a
# heredoc eats backslashes and apostrophes (MISTAKES.md entry 5, today).
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/WW_CHANGES_ENTRY.md'

OLD_CENSUS = '''```
bto built in scratch <dir>, 4 chunk(s), 4 dropped, <n> bytes freed   <- the default
bto built in the mod folder, 4 chunk(s), 0 dropped, 0 bytes freed    <- --keep-bto
```

and the command line prints its own line beside it:

```
bto scratch: 4 chunk(s) built in <dir>, 4 removed, 4 manifest sidecar(s) kept, <n> bytes freed
```
'''

NEW_CENSUS = '''```
bto built in scratch <dir>, 1 chunk(s), 1 dropped, 860743 bytes freed   <- the default
bto built in the mod folder, 1 chunk(s), 0 dropped, 0 bytes freed       <- --keep-bto
```

and the command line prints its own line beside it, for the default only:

```
bto scratch: 1 chunk(s) built in <dir>, 1 removed, 1 manifest sidecar(s) kept, 860743 bytes freed
```

Those are the real numbers off the gate's own one-chunk bake of Commonwealth
(-20,24) at dim 4; a bigger bake prints bigger ones the same way.
'''

TAIL = '''
### What the numbers came out at

Built 2026-09-16 17:11:17, `release/NifSkope.exe` 22,356,992 bytes. Every count
below was read out of that run's own log.

| gate | result |
|---|---|
| `tests/spells/lodgen_btofree.sh` | **23 checks, 0 failures** (new) |
| `tests/spells/native_open.sh` | **17 checks, 0 failures, 2 skipped** (was 14 checks and never green) |
| `tests/spells/lodgen_native.sh` | **125 checks, 0 failures, 2 skips** |
| `tests/spells/lodgen_ladder.sh` | **22 checks, 0 failures, 0 skips** |
| `tests/spells/lodgen_native_baseline.sh --check` | **25 files, 25 baked, 0 differ** |
| `tests/spells/lodgen_defaults.sh` | **28 checks, 0 failures** |
| `tests/spells/lod_generation.sh` (the panel self-test) | **124 checks, 0 failures** |
| `tests/spells/lodgen_byte_gate.sh` (b), the per-row sweep | **128 checks, 0 failures** (floor 125) |
| `tools/lodgen_native_decode.py` | **6 pairs, 36 checks, 0 failures** |

`lodgen_native.sh` check 5 compares the stock output with and without `--native`.
It now compares **25 stock files, 0 differ** and hands the `.lodb` ledger to a new
field-by-field comparator, `tests/spells/lodgen_btofree_ledger.py`, which asks that
the two ledgers differ **only** in the command-line digest and that every recorded
chunk digest is alike. A byte compare of the ledger could not stay honest once one
of the two runs spells `--keep-bto`, because the switch is deliberately inside the
digest -- it decides what is on disk -- and loosening the check to skip the file
would have skipped the chunk digests with it.

### Two differences phase (c) reports that this change did not cause

`lodgen_byte_gate.sh` phase (c) compares the panel's tree with the command line's.
It reads **15 identical, 2 differ**: `Commonwealth.4.-20.24.DDS` (174,888 bytes on
both sides, payload different from byte 129 on) and `Commonwealth.lodi` (41,638
bytes on both sides). The previous exe, run through the same gate, reports the
**same two files** plus the `.BTO` this change removes -- 14 identical, 3 differ --
and the panel's own tree is byte-identical across the two exes on all fourteen
surviving files. The pair is a front-end divergence that predates this work and is
left where it is, named rather than absorbed.
'''

b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')
assert s.count(OLD_CENSUS) == 1, 'census anchor %d' % s.count(OLD_CENSUS)
s = s.replace(OLD_CENSUS, NEW_CENSUS)
old_last = 'Closes `docs/FO4CS_IMPROVED_LOD_PLAN.md` \u00a75 row 7 and rules \u00a76 (k).'
assert s.count(old_last) == 1, 'tail anchor %d' % s.count(old_last)
s = s.replace(old_last, TAIL.strip('\n') + '\n\n' + old_last)
nb = s.encode('utf-8')
assert nb.count(b'\r') == 0
open(P, 'wb').write(nb)
print('WW_CHANGES_ENTRY.md: %d -> %d B, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))

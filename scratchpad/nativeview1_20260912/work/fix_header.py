P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/'
     'lane_nativeview1_report.md')
s = open(P, 'rb').read().decode('utf-8')

old = """Status at 2026-09-12 18:5x: **STOPPED ON THE GAME GATE, not finished.** `Fallout4.exe`
(pid 55208, ~7.7 GB working set and growing) came up while the lane was running.
The brief's header rule is `tasklist | grep -i Fallout4` before every build and
every exe launch, up = stop, PENDING.md (CONSTITUTION 6). Everything that needs
a build or a window is therefore owed and is written down step by step in
`scratchpad/nativeview1_20260912/PENDING.md`. Everything that needed neither was
finished and is below.

The code for all three routes is WRITTEN, COMPILED and LINKED (build at
18:46:50, below). Two of the four gates' legs are measured; the rest need
windows.
"""

new = """Status at 2026-09-12 20:1x: **DONE, with one gate RED and no claim that it is
not.** All three routes are written, compiled, linked and exercised end to end;
the harness runs whole; the pictures are taken. `tests/spells/native_open.sh`
reports **17 checks, 1 failures, 0 skipped** — the failure is gate (c), at the
number the brief pre-registered, with its cause measured and its refuter firing.
Nothing was sent to bungo; the director sends.

The lane stopped once in the middle, at 18:5x, because `Fallout4.exe` came up
(pid 55208) and the rule is no builds and no windows while he is playing. It
resumed from `PENDING.md` on the director's word at 19:28 with the game down, and
every build and every launch after that had its own `tasklist` check whose answer
was read first.

Three builds, three relinks; the exe on disk is **19:45:31, 22,275,072 B,
`BUILD-RC=0`**.
"""

assert s.count(old) == 1, 'header anchor %d' % s.count(old)
s = s.replace(old, new)
open(P, 'wb').write(s.encode('utf-8'))
print('report', len(s))

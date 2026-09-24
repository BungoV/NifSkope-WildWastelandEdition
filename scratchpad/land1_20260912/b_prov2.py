"""The provenance block again, after the digest fix. Every number re-read.

A third exe exists now, and saying so is the point of the block: the INCR1 rows
were stamped against 08:42:33, two harnesses then found a defect in the switch
digest, and the rows that moved are re-found against 09:21:04.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_TERRAIN_VT.md'

OLD_STAMP = """**The INCR1 rows below and the `--incremental` \u00a75 row** were stamped against the
exe carrying BOTH parts (08:42:33, 21,935,616 bytes, sha1
`1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`, objects
`GeneratedFiles/.obj/lodgen.o` and `nifcli.o` both 08:42:31 -- the object
timestamps are the check, because `exe -nt src` is satisfied by a link the other
translation unit triggered). Their numbers come from the byte-identity gate's
own bakes under `scratchpad/land1_20260912/out/b3/`, and the contract itself is
`docs/LODGEN_LEDGER_FORMAT.md`, not this file. Note that the 2.5g line numbers
above were re-found, not carried over: Part B moved two of them
(`lodgen.cpp:6297` -> `6304`, `nifcli.cpp:6175` -> `6516`), which is exactly the
failure mode stamping the anchor TEXT beside the number exists to catch."""

NEW_STAMP = """**The INCR1 rows below and the `--incremental` \u00a75 row** were measured against the
exe carrying BOTH parts (08:42:33, 21,935,616 bytes, sha1
`1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`): gates B2, B3, B4 and B5 and the
picture are that exe's bakes, under `scratchpad/land1_20260912/out/b3/`.

**A THIRD exe ships** (09:21:04, 21,935,616 bytes, sha1
`0e5b65d69f4cb532b7f2f24b8a8caa381fe8f3fb`, object
`GeneratedFiles/.obj/nifcli.o` 09:20:58 -- the object timestamp is the check,
because `exe -nt src` is satisfied by a link the other translation unit
triggered; `lodgen.o` is 08:32:06 and unchanged, no `lodgen.cpp` edit having
happened since). It differs from 08:42:33 in the switch digest ONLY, and it
exists because the harness chain on 08:42:33 came back with two reds this lane
had introduced: `lodgen_roads.sh` R1 (two `--no-roads` runs are byte-identical)
and `lodgen_native.sh` check 5 (the stock bake is byte-identical with and
without `--native`). Both were the ledger's `switches` field digesting a
DESTINATION PATH. `--vt` now keeps its token and loses its value, `--native` and
`--native-mesh-report` leave the digest entirely, and `--native` gains the
whole-region refusal it always needed -- see `docs/LODGEN_LEDGER_FORMAT.md`
sections 3 and 4, which are the contract, not this file. The B2-B5 numbers were
NOT re-measured against 09:21:04; none of their argument vectors contains
`--vt`, `--native` or `--incremental --native`, so none of their digests moved,
and that is a reasoned carry-forward stated rather than hidden. The line numbers
and hashes below ARE re-read against 09:21:04's sources.

Note that the 2.5g line numbers above were re-found, not carried over: Part B
moved two of them (`lodgen.cpp:6297` -> `6304`, `nifcli.cpp:6175` -> `6516`),
which is exactly the failure mode stamping the anchor TEXT beside the number
exists to catch."""

OLD_HASH = """| `src/nifcli.cpp` | `1b1a696ee324c8eb` | 335,580 | 7350 |"""
NEW_HASH = """| `src/nifcli.cpp` | `7fa7b42e6a0dba83` | 338,407 | 7405 |"""

ROWS = (
	("| INCR1 the switch digest, and the skip list it drops -- `--vt`, `--native` and `--vanilla-lod-root` are NOT on it | `nifcli.cpp:2498, 2525` | `static const char * const gLgSwitchSkip[] = {` |",
	 "| INCR1 the switch digest and its TWO skip lists -- token+value dropped, and token kept with the value dropped | `nifcli.cpp:2532, 2552, 2557` | `static const char * const gLgSwitchSkip[] = {` / `static const char * const gLgSwitchSkipValue[] = {` |\n"
	 "| INCR1 `--vt` keeps its token and loses its path, which is what made two identical `--no-roads` bakes write two different ledgers | `nifcli.cpp:2569` | `h.addData( a.at( i ).toUtf8() );   /* the flag, never its path */` |"),
	("| INCR1 the four refusals, each naming itself and exiting before a byte is written | `nifcli.cpp:3681, 3690, 3700, 3727` | `refused: --incremental has nothing to diff against -- ` |",
	 "| INCR1 the five refusals, each naming itself and exiting before a byte is written | `nifcli.cpp:3717, 3730, 3736, 3763, 3783` | `refused: --incremental has nothing to diff against -- ` |"),
	("| INCR1 the whole-region refusal names THREE passes, not five: the merge and the far-ring simplify are per-file loops and are not refused | `nifcli.cpp:3727` | `refused: --atlas, --arrays and --impostors each build ONE region-wide ` |",
	 "| INCR1 the whole-region refusal names FOUR passes, not five: the merge and the far-ring simplify are per-file loops and are not refused | `nifcli.cpp:3763, 3783` | `refused: --atlas, --arrays and --impostors each build ONE region-wide ` / `refused: --native builds ONE .lodo/.lodi pair for the whole region ` |\n"
	 "| INCR1 why `--native` is refused: the pair is collected INSIDE the chunk pass, one placement a reference and one lighting sample a vertex | `lodgen.cpp:3784, 4069` | `if ( lodgenNativeActive() ) {` / `if ( lodgenNativeActive() && opts.identity )` |"),
	("| INCR1 the one-cell neighbour widening, applied on top of the digest's own ring | `nifcli.cpp:3791` | `if ( j.cx <= dx + d && dx <= j.cx + d && j.cy <= dy + d && dy <= j.cy + d ) {` |",
	 "| INCR1 the one-cell neighbour widening, applied on top of the digest's own ring | `nifcli.cpp:3846` | `if ( j.cx <= dx + d && dx <= j.cx + d && j.cy <= dy + d && dy <= j.cy + d ) {` |"),
	("| INCR1 the census line, printed every run | `nifcli.cpp:3804` | `\"%6 by neighbour)\" )` |",
	 "| INCR1 the census line, printed every run | `nifcli.cpp:3859` | `\"%6 by neighbour)\" )` |"),
	("| INCR1 the ledger is written LAST, AFTER the merge has rewritten every `.BTO` -- the defect gate B3 caught | `nifcli.cpp:4060, 4113` | `THE LEDGER GOES HERE, LAST` |",
	 "| INCR1 the ledger is written LAST, AFTER the merge has rewritten every `.BTO` -- the defect gate B3 caught | `nifcli.cpp:4115, 4177` | `THE LEDGER GOES HERE, LAST` |"),
)


def main():
	b = open(P, 'rb').read()
	assert b.count(b'\x0d') == 0, 'doc is not LF-only'
	s = b.decode('utf-8')
	pairs = [(OLD_STAMP, NEW_STAMP, 'stamp'), (OLD_HASH, NEW_HASH, 'hash row')]
	pairs += [(o, n, 'row %d' % i) for i, (o, n) in enumerate(ROWS)]
	for old, new, label in pairs:
		c = s.count(old)
		assert c == 1, '%s matched %d times' % (label, c)
		s = s.replace(old, new)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('LODGEN_TERRAIN_VT.md %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

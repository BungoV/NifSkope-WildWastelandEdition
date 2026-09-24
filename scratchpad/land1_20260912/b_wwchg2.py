"""WW_CHANGES_ENTRY.md: the refusal list gained a fourth arm, and the lane's own
two reds get their own subsection rather than being folded into the inherited
one. A reader must not have to infer which reds were mine.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/WW_CHANGES_ENTRY.md'

OLD_REF = """**It refuses rather than guessing.** No ledger, a different region, different
switches, or `--atlas`/`--arrays`/`--impostors` on the command line: each stops
the run with a sentence naming which and what to do instead, having written
nothing. Those three each build ONE product out of the whole region and there is
no honest way to build them from a quarter of it. An `--incremental` that
promoted itself to a full bake would have lied to somebody watching a clock; one
that silently did half the work would have lied worse."""

NEW_REF = """**It refuses rather than guessing.** No ledger, a different region, different
switches, or `--atlas`/`--arrays`/`--impostors`/`--native` on the command line:
each stops the run with a sentence naming which and what to do instead, having
written nothing. Those four each build ONE product out of the whole region and
there is no honest way to build them from a quarter of it. An `--incremental`
that promoted itself to a full bake would have lied to somebody watching a
clock; one that silently did half the work would have lied worse.

`--native` is the arm of that refusal you would never have caught by looking.
A quarter-sized atlas **looks** like a quarter-sized atlas; a `.lodo`/`.lodi`
pair built from a quarter of a region loads, verifies, matches its own three
staleness hashes and is simply missing most of the worldspace. It was reachable
until the harness chain sent me back to the switch digest."""

OLD_TAIL = """The contract, the dependency map, the switch-digest skip list and the full
refusal list are in the new `docs/LODGEN_LEDGER_FORMAT.md`."""

NEW_TAIL = """The contract, the dependency map, the switch-digest skip lists and the full
refusal list are in the new `docs/LODGEN_LEDGER_FORMAT.md`.

### Two reds this lane made, and cleared

Writing a ledger into the out-dir of **every** bake changes the meaning of every
gate in the tree that says *two bakes of this are byte-identical*, and two of
them said so:

* `tests/spells/lodgen_roads.sh` R1 bakes `--no-roads` twice into two
  directories and compares every byte. The two commands differ only in
  `--vt <dir>`, and the ledger was digesting that path.
* `tests/spells/lodgen_native.sh` check 5 asserts the stock output is identical
  with and without `--native`. It is, except for a ledger recording whether a
  `.lodo` was written beside it.

Both came from one line of reasoning, written into the docs in my own words and
wrong: *a flag that changes the output belongs in the digest even when it does
not change the inputs*. The ledger's promise is about the chunks it tracks, so
the test is narrower -- **can this flag make a tracked chunk stale?** `--vt`
can, and its argument cannot, so the flag now keeps its token in the digest and
loses its path. `--native` cannot, so it leaves the digest and is refused
outright instead. Both harnesses are green, and the fix is what turned up the
`--native` refusal above.

The tree's other byte-identity gates were then swept for the same class rather
than waited on: `lodgen_farring`, `lodgen_texture_arrays`,
`lodgen_native_baseline` and `lod_channel_preview`."""


def main():
	b = open(P, 'rb').read()
	assert b.count(b'\x0d') == 0, 'entry is not LF-only'
	s = b.decode('utf-8')
	for old, new, label in ((OLD_REF, NEW_REF, 'refusal para'),
							(OLD_TAIL, NEW_TAIL, 'tail')):
		n = s.count(old)
		assert n == 1, '%s matched %d times' % (label, n)
		s = s.replace(old, new)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('WW_CHANGES_ENTRY.md %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

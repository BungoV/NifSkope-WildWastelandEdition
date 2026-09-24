"""One more MISTAKES.md entry, newest at the top (CONSTITUTION rule 2)."""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/MISTAKES_ENTRIES.md'

ANCHOR = """## 2026-09-12 \u2014 The test edit never reached the thing under test, and the arm said PASS"""

NEW = """## 2026-09-12 \u2014 A new file in an old directory broke two harnesses that had never heard of it

`--incremental` writes a ledger, `<out-dir>/<WS>.lodb`, on **every** bake,
whether or not anybody asked for incremental anything. That is the right design
-- you cannot diff against a ledger nobody wrote -- and it quietly changed the
contract of every gate in the tree that says *two bakes of this are
byte-identical*. Two of them went red on the very next chain:

* `lodgen_roads.sh` **R1**: two `--no-roads` runs, `1 of 10 differ`,
  `./obj/Commonwealth.lodb`. The harness gives each run its own directory, so
  the commands differ only in `--vt <dir>`.
* `lodgen_native.sh` **check 5**: the stock bake is byte-identical with and
  without `--native`, `1 of 26 differ`, the same file.

Both are the ledger's `switches` field, and both are the same error of judgement
written down in `docs/LODGEN_LEDGER_FORMAT.md` in my own words: *"A flag that
changes the OUTPUT belongs in the digest even when it does not change the
INPUTS."* It reads like conservatism. It is not, in two ways:

1. **`--vt <dir>` is a flag AND a destination.** The flag changes the picture,
   so the token belongs in the digest; the argument is a place to put the
   pyramid, exactly like `--out-dir`'s. One skip list could only take both or
   neither. There are two lists now.
2. **The ledger's promise is about the chunks it tracks**, not about every file
   the process happens to write. `--native` writes a `.lodo`/`.lodi` pair into
   its own directory and cannot make a chunk stale, so digesting it bought
   nothing and cost a gate.

The conservatism argument is the seductive part: over-rebaking *is* free of
correctness risk, so "when in doubt, digest it" feels safe. It is not free --
it is paid in false refusals nobody can explain, and here it was paid in two
harness reds that look exactly like a broken bake.

**The compensating find.** Fixing (2) turned up a defect the gate had not:
`--native` collects one placement per drawn reference and one lighting sample
per vertex **inside the chunk pass**, so `--incremental --native` would have
written a pair covering only the dirty chunks. Unlike a quarter-sized atlas,
that pair loads, passes `--native-verify --native-verify-corpus`, matches its
three staleness hashes and is simply missing most of the worldspace. It is now
refused with `--atlas`, `--arrays` and `--impostors`.

**When a feature adds a file to an existing output directory, the regression
surface is every byte-identity gate in the tree, not the feature's own gate.**
Grep the harnesses for `cmp` and `byte-identical` before the build, not after
the chain.

---

"""


def main():
	b = open(P, 'rb').read()
	s = b.decode('utf-8')
	n = s.count(ANCHOR)
	assert n == 1, 'anchor matched %d times' % n
	s = s.replace(ANCHOR, NEW + ANCHOR)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('MISTAKES_ENTRIES.md %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

"""docs/LODGEN_LEDGER_FORMAT.md sections 3 and 4, corrected by two harnesses.

Section 3 said a flag that changes the OUTPUT belongs in the digest even when
it does not change the INPUTS. Two shipped gates disagreed within the hour.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_LEDGER_FORMAT.md'

OLD3 = """**The skip list.** A flag is on it only if it passes all three tests: it names
*where* files go, or *how many* threads carry them, or *which file* an asset is
read from \u2014 and that last only because the ledger digests the asset's **bytes**
through the same `lodgenReadAsset()` the bake uses, so moving a mod folder still
dirties every chunk whose assets changed under it.

```
--out-dir  --tex-dir  --data-root  --incremental
--threads  --chunk-threads  --preview-dir
--resource --plugins-txt
```

`--threads` being on that list is a **claim**: BAKEPERF1's pass retires every
job on the calling thread in job order, so the worker count cannot reach a
byte. If that ever stops being true, this line is the bug.

`--vt`, `--native` and `--vanilla-lod-root` were on the skip list for one
afternoon of 2026-09-12 and are not any more. `--vt` makes the chunk sheets come
from the pyramid instead of the stock per-chunk composite: a different picture
from the same inputs. A flag that changes the **output** belongs in the digest
even when it does not change the **inputs**, because the ledger's promise is
about the files on disk.
"""

NEW3 = """**Two lists, because a flag can be two things at once.** The test is narrower
than \"cannot reach an output byte\": it is **cannot make a TRACKED CHUNK OUTPUT
stale**. The ledger tracks the per-chunk `.BTO`/`.BTR`/`.DDS` files it lists and
nothing else, so a flag that writes a separate product into a separate directory
is not its business.

**Token and value both dropped.** The flag names *where* files go, or *how many*
threads carry them, or *which file* an asset is read from \u2014 and that last only
because the ledger digests the asset's **bytes** through the same
`lodgenReadAsset()` the bake uses, so moving a mod folder still dirties every
chunk whose assets changed under it.

```
--out-dir  --tex-dir  --data-root  --incremental
--threads  --chunk-threads  --preview-dir
--resource --plugins-txt
--native   --native-mesh-report
```

**Token kept, value dropped.** The flag changes the picture; its argument is
only a place to put files.

```
--vt
```

`--threads` being on the first list is a **claim**: BAKEPERF1's pass retires
every job on the calling thread in job order, so the worker count cannot reach a
byte. If that ever stops being true, this line is the bug.

### The rule this section got wrong for one afternoon

`--vt`, `--native` and `--vanilla-lod-root` were skipped entirely, then digested
entirely, on the reasoning that **a flag that changes the output belongs in the
digest even when it does not change the inputs**. Two shipped gates said no
within the hour, and both were right:

* `tests/spells/lodgen_roads.sh` **R1** bakes `--no-roads` twice into
  `roadOff/` and `roadOff2/` and compares every byte. The two commands differ
  only in `--vt <dir>` \u2014 a destination, exactly like `--out-dir` \u2014 so
  digesting the path made two identical bakes write two different ledgers.
  `--vt` keeps its token (the pyramid really is a different picture from the
  stock per-chunk composite) and loses its value.
* `tests/spells/lodgen_native.sh` **check 5** asserts the stock bake is
  byte-identical with and without `--native`. It is, except for a ledger
  recording whether a `.lodo` was written beside it. `--native` goes to its own
  directory and cannot touch a chunk, so it leaves the digest \u2014 and the run
  that *would* be wrong is refused outright (section 4).

The surviving rule: **digest what can make a tracked chunk stale; refuse what
builds a region-wide product; ignore the rest.** `--vanilla-lod-root` stays
digested, token and value, because its argument is an INPUT root and no gate
compares two bakes across it.
"""

OLD4 = """| `LODGEN_INCR_WHOLE_REGION` | `--atlas`, `--arrays` or `--impostors` is asked for | do those in a separate full pass over the finished chunks (section 2). The merge and the far-ring simplify are **not** on this list |"""

NEW4 = """| `LODGEN_INCR_WHOLE_REGION` | `--atlas`, `--arrays`, `--impostors` or `--native` is asked for | do those in a separate full pass over the finished chunks (section 2). The merge and the far-ring simplify are **not** on this list |"""

OLD4B = """An `--incremental` run that promoted itself to a full bake would have lied to
an operator watching a clock. One that silently did half the work would have
lied worse."""

NEW4B = """An `--incremental` run that promoted itself to a full bake would have lied to
an operator watching a clock. One that silently did half the work would have
lied worse.

`--native` was added to the whole-region refusal on 2026-09-12 and it is the
one arm of that refusal whose absence would not have shown in the output tree.
An atlas built from a fraction of a region **looks** like a quarter of an
atlas; a `.lodo`/`.lodi` pair built from a fraction of one loads, passes
`--native-verify --native-verify-corpus`, matches its own three staleness
hashes, and is simply missing most of the worldspace. The pass collects one
`NativePlacement` per drawn reference and one lighting sample per vertex
**inside the chunk pass** (`src/lodgen.cpp:3784` and `:4069`), so it can only
ever see the chunks that were rebaked."""


def main():
	b = open(P, 'rb').read()
	assert b.count(b'\x0d') == 0, 'doc is not LF-only'
	s = b.decode('utf-8')
	for old, new, label in ((OLD3, NEW3, 'section 3'),
							(OLD4, NEW4, 'refusal row'),
							(OLD4B, NEW4B, 'refusal tail')):
		n = s.count(old)
		assert n == 1, '%s matched %d times' % (label, n)
		s = s.replace(old, new)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('LODGEN_LEDGER_FORMAT.md %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

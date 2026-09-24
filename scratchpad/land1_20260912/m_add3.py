"""The ninth entry, newest at the top (CONSTITUTION rule 2), plus the one line
of B7 that the harness chain made false.
"""
import sys

M = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/MISTAKES_ENTRIES.md'
R = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/lane_land1_report.md'

ANCHOR = """## 2026-09-12 \u2014 A new file in an old directory broke two harnesses that had never heard of it"""

NEW = """## 2026-09-12 \u2014 The build gate passed, the exe was newer than every source, and six objects were stale

`tools/ww_build.sh` gates on **the exe being newer than the sources**, which is
already the careful version of the rule -- this lane has been checking object
timestamps all day precisely because `exe -nt src` is satisfied by a link that
the *other* translation unit triggered.

It still let a stale exe through. Six files arrived in the tree from another
lane between two builds, copied with their **mtimes preserved** (08:26-08:46).
The exe was 09:21:04. Every source was older than the exe, so the gate passed,
`make` printed nothing, and the objects told the truth:

```
GeneratedFiles/.obj/animdopesheet.o   04:34:58
src/animdopesheet.cpp                 08:32:23
```

`make -n` wanted six translation units. The exe on disk did not contain a line
of what had just landed in the tree, and nothing in the build output said so.

**An exe newer than a source file is not an exe built from it.** A copy that
preserves timestamps defeats every mtime gate in the chain at once -- the
build's, the harness runner's, and the reader's. The cheap check is `make -n`,
which costs two seconds and answers the actual question (*is there anything
left to compile?*) instead of a proxy for it. Run it after the build, not only
before, and run it again whenever a tree is shared with a lane that merges.

---

"""

OLD_B7 = """3. **No `--vt` arm.** `--vt` is reasoned into the switch digest because it
   changes the output from the same inputs, but no `--vt` bake was compared."""

NEW_B7 = """3. ~~**No `--vt` arm.**~~ **Corrected at B8.** This said `--vt` was reasoned
   into the switch digest and never baked. It was baked, by a harness this
   section had not thought to consult: `lodgen_roads.sh` R1 compares two
   `--no-roads` bakes that differ only in their `--vt` directory, and it went
   red. `--vt`'s token is in the digest and its path is not, and R1 is now the
   arm that holds that open. What is STILL not measured is an `--incremental`
   run across a change of `--vt` state, and no arm asserts the new `--native`
   whole-region refusal fires -- that refusal is reasoned from the collection
   sites, not gated."""


def main():
	b = open(M, 'rb').read()
	s = b.decode('utf-8')
	n = s.count(ANCHOR)
	assert n == 1, 'anchor matched %d times' % n
	s = s.replace(ANCHOR, NEW + ANCHOR)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0
	open(M, 'wb').write(out)
	print('MISTAKES_ENTRIES.md %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))

	rb = open(R, 'rb').read()
	rs = rb.decode('utf-8')
	n = rs.count(OLD_B7)
	assert n == 1, 'B7 item matched %d times' % n
	rs = rs.replace(OLD_B7, NEW_B7)
	ro = rs.encode('utf-8')
	assert ro.count(b'\x0d') == 0
	open(R, 'wb').write(ro)
	print('report %d -> %d bytes, CR %d' % (len(rb), len(ro), ro.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

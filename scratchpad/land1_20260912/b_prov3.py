"""A fourth exe, and the reason it exists is not this lane's code."""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_TERRAIN_VT.md'

OLD = """**A THIRD exe ships** (09:21:04, 21,935,616 bytes, sha1
`0e5b65d69f4cb532b7f2f24b8a8caa381fe8f3fb`, object
`GeneratedFiles/.obj/nifcli.o` 09:20:58 -- the object timestamp is the check,
because `exe -nt src` is satisfied by a link the other translation unit
triggered; `lodgen.o` is 08:32:06 and unchanged, no `lodgen.cpp` edit having
happened since). It differs from 08:42:33 in the switch digest ONLY, and it
exists because the harness chain on 08:42:33 came back with two reds this lane
had introduced:"""

NEW = """**A THIRD exe** (09:21:04, 21,935,616 bytes, sha1
`0e5b65d69f4cb532b7f2f24b8a8caa381fe8f3fb`, object
`GeneratedFiles/.obj/nifcli.o` 09:20:58 -- the object timestamp is the check,
because `exe -nt src` is satisfied by a link the other translation unit
triggered; `lodgen.o` is 08:32:06 and unchanged, no `lodgen.cpp` edit having
happened since) differs from 08:42:33 in the switch digest ONLY, and exists
because the harness chain on 08:42:33 came back with two reds this lane had
introduced:"""

OLD2 = """sections 3 and 4, which are the contract, not this file. The B2-B5 numbers were
NOT re-measured against 09:21:04; none of their argument vectors contains
`--vt`, `--native` or `--incremental --native`, so none of their digests moved,
and that is a reasoned carry-forward stated rather than hidden. The line numbers
and hashes below ARE re-read against 09:21:04's sources."""

NEW2 = """sections 3 and 4, which are the contract, not this file.

**THE SHIPPING EXE IS A FOURTH** (09:32:37, 21,951,488 bytes, sha1
`3e1914a0637b66f438d873e0230b1e8c04d7c806`), and NOT for anything in this lane:
six files from another lane's merge arrived in the tree between 09:21 and 09:31
with their **mtimes preserved** (08:26-08:46), so `tools/ww_build.sh`'s
exe-newer-than-sources gate passed over a tree whose objects were stale --
`animdopesheet.o` was 04:34:58 against an `animdopesheet.cpp` of 08:32:23, and
`make -n` wanted six translation units. **An exe newer than a source file is not
an exe built from it**, and a copy that preserves timestamps defeats every
mtime gate at once. The rebuild touched `animdopesheet`, `animworkspace`,
`animworkspacetest`, `hkxanimuitest`, `nifskope` and `nifskope_ui`; `nifcli.o`
is 09:20:58 and `lodgen.o` 08:32:06 in BOTH exes, so **no lodgen code differs
between 09:21:04 and 09:32:37**. `lodgen_roads`, `lodgen_native`,
`lodgen_native_baseline`, `lodgen_identity` and `animws` were re-run on the
shipping exe regardless.

The B2-B5 numbers were NOT re-measured against either later exe; none of their
argument vectors contains `--vt`, `--native` or `--incremental --native`, so
none of their digests moved, and that is a reasoned carry-forward stated rather
than hidden. The line numbers and hashes below ARE re-read against the shipping
sources."""


def main():
	b = open(P, 'rb').read()
	assert b.count(b'\x0d') == 0, 'doc is not LF-only'
	s = b.decode('utf-8')
	for old, new, label in ((OLD, NEW, 'third'), (OLD2, NEW2, 'fourth')):
		n = s.count(old)
		assert n == 1, '%s matched %d times' % (label, n)
		s = s.replace(old, new)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('LODGEN_TERRAIN_VT.md %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

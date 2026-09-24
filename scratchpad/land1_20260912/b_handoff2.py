"""HANDOFF_BLOCK.md, brought to the shipping exe and the two reds cleared."""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/HANDOFF_BLOCK.md'

OLD_TREE = """- `tests/spells/lodgen_roads.sh` \u2014 the inherited R5 red cleared (MARG 0.8 \u2192
  0.72, derivation recorded in the file). Suite reads 11 checks / 0 failures.
- NOTHING committed. `git stash` never run."""

NEW_TREE = """- `tests/spells/lodgen_roads.sh` \u2014 the inherited R5 red cleared (MARG 0.8 \u2192
  0.72, derivation recorded in the file). Suite reads 11 checks / 0 failures.
- `tests/spells/lodgen_native_baseline.sh` \u2014 `*.LODB` added to the excluded set
  beside `*.BTR`, with the reasoning in the header. The frozen baseline was NOT
  re-written: it is a list from ONE NAMED BUILD of 2026-09-10 and re-freezing it
  from a mid-lane exe would bless every other lane's drift since.
- NOTHING committed. `git stash` never run."""

OLD_EXE = """### Exe on disk
- `release/NifSkope.exe` \u2014 2026-09-12 08:42:33, 21,935,616 B,
  sha1 `1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`, carrying BOTH parts.
  Objects `GeneratedFiles/.obj/lodgen.o` and `nifcli.o` verified rebuilt
  alongside it (08:42:31), which is the check that matters \u2014 `exe -nt src` is
  satisfied by a link the *other* file triggered."""

NEW_EXE = """### Exe on disk
- `release/NifSkope.exe` \u2014 2026-09-12 **09:32:37, 21,951,488 B,
  sha1 `3e1914a0637b66f438d873e0230b1e8c04d7c806`**, carrying both parts and the
  switch-digest fix. `GeneratedFiles/.obj/nifcli.o` 09:20:58 and `lodgen.o`
  08:32:06 \u2014 the object timestamps are the check, because `exe -nt src` is
  satisfied by a link the *other* file triggered.
- Two earlier exes are named in the report and in the `docs/LODGEN_TERRAIN_VT.md`
  provenance block: 08:42:33 (`1e4e2c5c\u2026`), which is the exe gates B2\u2013B5 and
  the picture were measured on, and 09:21:04 (`0e5b65d6\u2026`), the switch-digest
  fix. **No lodgen code differs between 09:21:04 and the shipping exe**: the
  last build exists only because six files from another lane's merge arrived
  with their mtimes preserved (08:26\u201308:46), so the exe-newer-than-sources gate
  passed over stale objects \u2014 `animdopesheet.o` was 04:34:58 against a
  `.cpp` of 08:32:23. **An exe newer than a source file is not an exe built from
  it**; `make -n` is the two-second check that says so."""

OLD_F2 = """2. **Part B's refusal list, written from the dependency map before the code,
   refused the default command.** The merge is on by default and the map named
   it a whole-region pass; reading the function proved it is a per-file loop
   with no cross-file state. The feature was 100 % unusable and the refusal
   looked like caution. Both this and a ledger-timing defect were found by the
   byte-identity gate, not by reading \u2014 see `MISTAKES_ENTRIES.md`."""

NEW_F2 = """2. **Part B's refusal list, written from the dependency map before the code,
   refused the default command.** The merge is on by default and the map named
   it a whole-region pass; reading the function proved it is a per-file loop
   with no cross-file state. The feature was 100 % unusable and the refusal
   looked like caution. Both this and a ledger-timing defect were found by the
   byte-identity gate, not by reading \u2014 see `MISTAKES_ENTRIES.md`.
3. **A ledger in every out-dir changes the contract of every byte-identity gate
   in the tree.** `lodgen_roads` R1 and `lodgen_native` check 5 both went red on
   the ledger's `switches` field, because the digest was eating a DESTINATION
   PATH (`--vt <dir>`) and a flag that cannot make a chunk stale (`--native`).
   The digest has two skip lists now \u2014 token+value, and token-kept/value-dropped
   \u2014 and both harnesses are green. Fixing it found a live defect the gates had
   not: `--incremental --native` would have written a `.lodo`/`.lodi` pair
   covering only the dirty chunks, and unlike a quarter-sized atlas **that pair
   loads, verifies and matches its own staleness hashes**. `--native` is now
   refused with `--atlas`, `--arrays` and `--impostors`."""

OLD_RED = """- The VT pyramid path (`--vt`) was reasoned about (it changes the output from
  the same inputs, so it is in the switch digest) but no `--vt` arm was baked."""

NEW_RED = """- The VT pyramid path (`--vt`): its TOKEN is in the switch digest and its path
  is not, and `lodgen_roads.sh` R1 now exercises exactly that (two `--no-roads`
  bakes into different `--vt` directories, byte-identical). What is still not
  baked is an `--incremental` arm that toggles `--vt` itself.
- `--native` is refused under `--incremental` by reading the collection sites,
  not by a gate arm: no arm asserts the refusal fires. It is the cheapest arm
  the next lane could add, after the `assets` one."""

OLD_DOC = """`scratchpad/land1_20260912/` \u2014 `WW_CHANGES_ENTRY.md` (one entry, a subsection
per part), `MISTAKES_ENTRIES.md` (six entries), `CHANGED_FILES.txt`, this
block."""

NEW_DOC = """`scratchpad/land1_20260912/` \u2014 `WW_CHANGES_ENTRY.md` (one entry, a subsection
per part), `MISTAKES_ENTRIES.md` (nine entries), `CHANGED_FILES.txt`, this
block."""


def main():
	b = open(P, 'rb').read()
	assert b.count(b'\x0d') == 0, 'block is not LF-only'
	s = b.decode('utf-8')
	for old, new, label in ((OLD_TREE, NEW_TREE, 'tree'), (OLD_EXE, NEW_EXE, 'exe'),
							(OLD_F2, NEW_F2, 'finding 2'), (OLD_RED, NEW_RED, 'reds'),
							(OLD_DOC, NEW_DOC, 'documents')):
		n = s.count(old)
		assert n == 1, '%s matched %d times' % (label, n)
		s = s.replace(old, new)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('HANDOFF_BLOCK.md %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

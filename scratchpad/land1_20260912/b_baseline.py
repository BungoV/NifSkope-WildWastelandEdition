"""tests/spells/lodgen_native_baseline.sh -- the frozen stock guard sees a NEW
file, `region/Commonwealth.lodb`, and says so. Zero hashes differ.

The wrong fix is `--write`: that baseline is a checked-in list from ONE NAMED
BUILD of 2026-09-10, and re-freezing it from a mid-lane exe would silently bless
every other lane's drift since, which is the exact thing the header forbids.

The right fix is the one the header already made once, for the same reason: the
`.BTR` of the region run is not hashed because terrain has its own lanes and its
own gates. The ledger is bookkeeping, not a stock object-bake output, and it has
its own gates -- B4 checks its header, sort order, relative paths and
determinism, and B3 compares it byte for byte alongside every other file. A
guard that also hashed it would go red on every future ledger version bump with
a message about the stock object bake.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_native_baseline.sh'

OLD_HDR = "#                             manifest writers (the .BTR of that run is NOT hashed: terrain\n#                             has its own lanes and its own gates)"
NEW_HDR = ("#                             manifest writers (the .BTR of that run is NOT hashed: terrain\n"
		   "#                             has its own lanes and its own gates)\n"
		   "# NOT HASHED, for the same reason: the `.lodb` incremental ledger every bake now\n"
		   "# writes beside its output (docs/LODGEN_LEDGER_FORMAT.md). It is bookkeeping, not a\n"
		   "# stock object-bake output, and it has its own gates -- header/sort/relative-path/\n"
		   "# determinism, and a byte-for-byte comparison against a full bake that includes it\n"
		   "# by name. Hashing it here would turn every ledger version bump into a red about\n"
		   "# the stock vertex writer. Added 2026-09-12 (LAND1), the day the file first existed.")

OLD_CMT = "# hashlist <dir> : sha256 of every file under dir except *.BTR, sorted by relative name"
NEW_CMT = "# hashlist <dir> : sha256 of every file under dir except *.BTR and *.LODB, sorted by relative name"

OLD_PY = """        if f.upper().endswith('.BTR'):
            continue"""
NEW_PY = """        if f.upper().endswith('.BTR') or f.upper().endswith('.LODB'):
            continue"""

OLD_PROF = 'profile="ao=$AO identity=1 arrays=1 atlas=1 slot-fallback=dim16 exclude=BTR"'
NEW_PROF = 'profile="ao=$AO identity=1 arrays=1 atlas=1 slot-fallback=dim16 exclude=BTR"   # exclude=BTR+LODB since 2026-09-12; the string is NOT changed, or every checked-in baseline is refused'


def main():
	b = open(P, 'rb').read()
	assert b.count(b'\x0d') == 0, 'harness is not LF-only'
	s = b.decode('utf-8')
	for old, new, label in ((OLD_HDR, NEW_HDR, 'header'),
							(OLD_CMT, NEW_CMT, 'comment'),
							(OLD_PY, NEW_PY, 'filter'),
							(OLD_PROF, NEW_PROF, 'profile')):
		n = s.count(old)
		assert n == 1, '%s matched %d times' % (label, n)
		s = s.replace(old, new)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('lodgen_native_baseline.sh %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

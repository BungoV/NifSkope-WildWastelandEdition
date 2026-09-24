"""CHANGED_FILES.txt, re-measured against the SHIPPING exe. No number carried."""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/CHANGED_FILES.txt'

OLD_HDR = """Sizes/hashes taken at 08:49 against the exe of 08:42:33
(21,935,616 B, sha1 1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968).
All seven files are LF-only: CR count 0, measured with Python byte counts."""

NEW_HDR = """Sizes/hashes RE-MEASURED at 09:36 against the shipping exe of 09:32:37
(21,951,488 B, sha1 3e1914a0637b66f438d873e0230b1e8c04d7c806). The earlier
08:42:33 measurement is superseded, not amended: four of the eight files moved
after it.
All eight files are LF-only: CR count 0, measured with Python byte counts."""

OLD_N = """    B   src/nifcli.cpp                               335580     0   7350  d74047455fc3
          --incremental: the diff, the one-cell neighbour widening, the four
          refusals, the census line, and the ledger write -- which goes LAST,
          after the merge has rewritten every .BTO. The retire lambda also
          names the three per-chunk tex sheets among a chunk's outputs."""

NEW_N = """    B   src/nifcli.cpp                               338407     0   7405  b893c4c6a2f6
          --incremental: the diff, the one-cell neighbour widening, the FIVE
          refusals, the census line, and the ledger write -- which goes LAST,
          after the merge has rewritten every .BTO. The retire lambda also
          names the three per-chunk tex sheets among a chunk's outputs.
          The switch digest has TWO skip lists: gLgSwitchSkip (token and value
          dropped, --native and --native-mesh-report now on it) and
          gLgSwitchSkipValue (token kept, value dropped: --vt). --native joins
          --atlas/--arrays/--impostors on the whole-region refusal."""

OLD_D = """  A B   docs/LODGEN_TERRAIN_VT.md                    158666     0   2435  c8cbf39801fd
          four new rows in the \u00a75 flag table; the LAND1 section under \u00a78."""

NEW_D = """  A B   docs/LODGEN_TERRAIN_VT.md                    164170     0   2498  88e898ee1923
          four new rows in the \u00a75 flag table; the LAND1 section under \u00a78, now
          naming all FOUR exes this lane built and which numbers came off which."""

OLD_R = """    -   tests/spells/lodgen_roads.sh                   8399     0    171  6182df63d716
          INHERITED RED, cleared: the R5 road-colour margin 0.8 -> 0.72, with
          the derivation written into the file. Suite 11 checks / 0 failures."""

NEW_R = """    -   tests/spells/lodgen_roads.sh                   8399     0    171  6182df63d716
          INHERITED RED, cleared: the R5 road-colour margin 0.8 -> 0.72, with
          the derivation written into the file. Suite 11 checks / 0 failures.
          (R1 also went red mid-lane and is NOT a change to this file: it was
          the switch digest eating --vt's path, fixed in nifcli.cpp.)
    B   tests/spells/lodgen_native_baseline.sh         9139     0    199  075312319ece
          *.LODB added to the excluded set beside *.BTR, with the reasoning in
          the header: the ledger is bookkeeping, not a stock object-bake output,
          and it has its own gates. The FROZEN BASELINE FILE
          (tests/baselines/stock_baseline.sha256) is deliberately NOT rewritten
          and the exclude=BTR profile string is deliberately unchanged, or every
          checked-in baseline is refused. Reads 25 of 25, 0 differ."""

OLD_L = """    B   docs/LODGEN_LEDGER_FORMAT.md                  11497     0    227  8ca2ee133601"""
NEW_L = """    B   docs/LODGEN_LEDGER_FORMAT.md                  13582     0    268  1cde529801ab"""

OLD_W = """  Part B: b_esmedit.py b2_refusals.sh b3_identity.sh b3_compare.py b5_times.py
  Rollback rung: NifSkope.partA_prerelink.exe (07:42:22, Part A only)."""

NEW_W = """  Part B: b_esmedit.py b_pickref.py b2_refusals.sh b3_identity.sh b3_compare.py
          b4_ledger.py b5_times.py b_pics.py b8_chain.sh b8b_chain.sh
          b8c_chain.sh b8d_chain.sh b_digestfix.py b_docfix.py b_prov2.py
          b_prov3.py b_baseline.py b_handoff2.py b_wwchg2.py m_add.py m_add2.py
  Rollback rung: NifSkope.partA_prerelink.exe (07:42:22, Part A only)."""


def main():
	b = open(P, 'rb').read()
	assert b.count(b'\x0d') == 0, 'list is not LF-only'
	s = b.decode('utf-8')
	for old, new, label in ((OLD_HDR, NEW_HDR, 'header'), (OLD_N, NEW_N, 'nifcli'),
							(OLD_D, NEW_D, 'terrain doc'), (OLD_R, NEW_R, 'roads'),
							(OLD_L, NEW_L, 'ledger doc'), (OLD_W, NEW_W, 'working files')):
		n = s.count(old)
		assert n == 1, '%s matched %d times' % (label, n)
		s = s.replace(old, new)
	out = s.encode('utf-8')
	assert out.count(b'\x0d') == 0, 'CR introduced'
	open(P, 'wb').write(out)
	print('CHANGED_FILES.txt %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
	return 0


if __name__ == '__main__':
	sys.exit(main())

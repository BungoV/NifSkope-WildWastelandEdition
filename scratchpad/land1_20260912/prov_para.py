import os
import sys

DOC = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_TERRAIN_VT.md'

OLD = """Section 2.5g is new and three rows were added to \u00a75. Its numbers come from bakes
into `scratchpad/land1_20260912/out/` made by `release/NifSkope.exe` (07:42:22,
21,861,376 bytes, sha1 `902223bd99dba4bfaf5d621fe36eb12fc4cf0272`). The INCR1 rows below, and
the --incremental row of section 5, were stamped against the exe that carries BOTH of this lane's parts (08:42:33,
21,935,616 bytes, sha1 `1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`, objects
`GeneratedFiles/.obj/lodgen.o` and `nifcli.o` both 08:42:31) -- 119 sweep
bakes plus the gate bakes, every one rc=0 and every one carrying
`--road-detail 1` -- graded by lane TILING4's instruments imported UNCHANGED
(`h1_sweep.py`, `t4_gates.py`, `t4_lib.py`, `f3_full.py`, `pool.json`) and by
this lane's own `a2_msn.py`, `a3_gate.py`, `a4_gate.py`, `a5_controls.py`,
`a6_score.py`, `a_pics.py` with the logs beside them under `logs/`. The macro
heights are read from `scratchpad/mountains_20260907/land_all.bin`, the
whole-worldspace VHGT dump written by `--dump-land`, which is the same data
lodgen itself reads. No number here is copied forward: each was re-read off the
artefact it describes. Every line below was found again from its own anchor text
against the sources stamped here (`ww-contract-provenance` step 3)."""

NEW = """This lane shipped two independent features and they were stamped against two
different exes, which is said here rather than smoothed over.

**Section 2.5g (`--land-guide`) and its three \u00a75 rows** come from bakes into
`scratchpad/land1_20260912/out/` made by `release/NifSkope.exe` (07:42:22,
21,861,376 bytes, sha1 `902223bd99dba4bfaf5d621fe36eb12fc4cf0272`) -- 119 sweep
bakes plus the gate bakes, every one rc=0 and every one carrying
`--road-detail 1` -- graded by lane TILING4's instruments imported UNCHANGED
(`h1_sweep.py`, `t4_gates.py`, `t4_lib.py`, `f3_full.py`, `pool.json`) and by
this lane's own `a2_msn.py`, `a3_gate.py`, `a4_gate.py`, `a5_controls.py`,
`a6_score.py`, `a_pics.py` with the logs beside them under `logs/`. The macro
heights are read from `scratchpad/mountains_20260907/land_all.bin`, the
whole-worldspace VHGT dump written by `--dump-land`, which is the same data
lodgen itself reads.

**The INCR1 rows below and the `--incremental` \u00a75 row** were stamped against the
exe carrying BOTH parts (08:42:33, 21,935,616 bytes, sha1
`1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`, objects
`GeneratedFiles/.obj/lodgen.o` and `nifcli.o` both 08:42:31 -- the object
timestamps are the check, because `exe -nt src` is satisfied by a link the other
translation unit triggered). Their numbers come from the byte-identity gate's
own bakes under `scratchpad/land1_20260912/out/b3/`, and the contract itself is
`docs/LODGEN_LEDGER_FORMAT.md`, not this file. Note that the 2.5g line numbers
above were re-found, not carried over: Part B moved two of them
(`lodgen.cpp:6297` -> `6304`, `nifcli.cpp:6175` -> `6516`), which is exactly the
failure mode stamping the anchor TEXT beside the number exists to catch.

No number here is copied forward: each was re-read off the artefact it
describes. Every line below was found again from its own anchor text against the
sources stamped here (`ww-contract-provenance` step 3)."""


def main():
    b = open(DOC, 'rb').read()
    s = b.decode('utf-8')
    n = s.count(OLD)
    assert n == 1, 'paragraph matched %d times' % n
    s = s.replace(OLD, NEW)
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(DOC, 'wb').write(out)
    print('%d -> %d bytes, CR 0, LF %d' % (len(b), len(out), out.count(b'\n')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

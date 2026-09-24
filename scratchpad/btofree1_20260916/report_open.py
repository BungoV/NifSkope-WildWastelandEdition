# Lane BTOFREE1, 2026-09-16 -- an OPEN item written into the report the moment
# it was found, not when it was resolved. If the lane were interrupted here this
# paragraph is what the next reader needs.
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/lane_btofree1_report.md'

NOTE = '''### OPEN at 18:52 \u2014 `lodgen_byte_gate.sh` phase (c), two files

Phase (b) reads **128 checks, 0 failures** (floor 125) and the new panel row is in the sweep with a
verdict of its own: `row keepBto false -> true: bytes MOVED (15 files, 266,282,955 bytes)`, which is
the gate saying the row reaches the output rather than me saying it.

Phase (c) \u2014 the panel's tree against the command line's, file by name \u2014 reads
**15 identical, 2 differ, 0 missing**, and the two are `Commonwealth.4.-20.24.DDS` (174,888 B both
sides, and the whole payload differs from byte 129 on, 123,515 differing bytes) and
`Commonwealth.lodi` (41,638 B both sides). The `.BTO` leg itself is green: *dropped by both*, no
scratch folder on either side, and the manifest sidecar byte-identical.

What is already measured, and rules half of it out: **the command line's side of both files is
byte-identical to the rung's.** `lodgen_btofree.sh` leg (a) compares this exe's CLI bake with the
rung's CLI bake file by file and reads 13 identical / 0 differ, `Commonwealth.4.-20.24.DDS` and
`Commonwealth.lodi` among them. So whatever phase (c) is seeing is on the PANEL side, or is a
difference between the two front ends that predates this lane.

I am not guessing which. The rung exe is being run through the same `PHASES=bc` now, with this
exe's panel tree copied aside at `chain/panel_base_newexe/` first, so the two panel trees can be
compared directly and the rung's own phase (c) verdict read. Whichever way it falls, the answer goes
in this section with the numbers.

'''

b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
old = '## 3. Build and chain\n'
assert s.count(old) == 1, 'anchor count %d' % s.count(old)
s = s.replace(old, NOTE + old)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('open note written: %d -> %d B, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))

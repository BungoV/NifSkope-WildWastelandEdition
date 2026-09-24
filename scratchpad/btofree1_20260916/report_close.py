# Lane BTOFREE1, 2026-09-16 -- the OPEN item of 18:52, closed at 19:39 by
# running the SAME gate on the rung exe and by comparing the two panel trees
# file by file. Both answers are measurements, neither is an argument.
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/lane_btofree1_report.md'

NEW = '''### `lodgen_byte_gate.sh` phases (b) and (c) \u2014 (b) 128/0, (c) two differences that are not mine

**(b), the per-row sweep: 128 checks, 0 failures** against a floor of 125 (the rung reads exactly
125, so the new row raised it by three). The new row gets a verdict of its own out of the sweep
rather than out of me: `row keepBto false -> true: bytes MOVED (15 files, 266,282,955 bytes,
58,569 ms)`.

**(c), the panel's tree against the command line's, file by name: 15 identical, 2 differ, 0
missing.** The `.BTO` leg is green \u2014 *dropped by both*, no scratch folder on either side, and the
manifest sidecar byte-identical \u2014 and the two that differ are `Commonwealth.4.-20.24.DDS`
(174,888 B both sides, whole payload different from byte 129 on, 123,515 differing bytes) and
`Commonwealth.lodi` (41,638 B both sides).

**Neither is this lane\u2019s.** I ran the rung exe \u2014 the 15:53:30 build, before a line of this work \u2014
through the same `PHASES=bc`, and its phase (c) reads:

    DIFFERS: Commonwealth.4.-20.24.DDS (174888 vs 174888 bytes)
    DIFFERS: Commonwealth.lodi (41638 vs 41638 bytes)
    panel vs command line: 14 identical, 3 differ, 0 missing

The same two files, on an exe that has never heard of a scratch folder. (Its third difference is
`STILL THERE: Commonwealth.4.-20.24.BTO (panel yes, cli yes)` \u2014 the old exe run through the new
gate, which is the row doing its job.) So the count went **14 identical / 3 differ \u2192 15 identical /
2 differ**: this lane removed one of the three and touched neither of the others.

**And the panel\u2019s own output did not move by a byte.** I copied this exe\u2019s panel tree aside before
the rung run (`chain/panel_base_newexe/`) and compared the two file by file:

    PANEL rung vs new (the .BTO excluded): 14 identical, 0 differ, 0 only-rung, 0 only-new
    no scratch folder in either panel tree

Fourteen files, byte for byte, including both files phase (c) complains about. The **only** change to
what a panel FO4CS bake leaves on disk is the 860,743-byte chunk that is gone.

**Whose the two are.** I did not chase them, because the brief tells me not to take red that is not
mine and because neither is reachable from anything this lane touched. What is measurable about them,
for whoever does pick them up: the `.lodi` is NATIVE1c\u2019s file (v3 \u2192 v5 the same day, `--library
near` the same day) and the chunk diffuse is the one land texture of the three that moved, with
`_data` and `_msn` byte-identical, which points at the cover/land pass rather than at the chunk
writer. Phase (c) was already failing on the rung; it is failing less now.

'''

b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
i = s.find('### OPEN at 18:52')
j = s.find('## 3. Build and chain')
assert i > 0 and j > i, 'anchors %d %d' % (i, j)
s = s[:i] + NEW + s[j:]
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('open note closed: %d -> %d B, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))

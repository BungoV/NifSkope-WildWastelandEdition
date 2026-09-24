# Lane BTOFREE1, 2026-09-16 -- the new gate's own section in report part 2.
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/lane_btofree1_report.md'

SEC = '''### The gate the brief asked for: `tests/spells/lodgen_btofree.sh` — 23 checks, 0 failures, PASS

New file, 13,839 B, LF-only. Five bakes of the same one chunk — the rung's FO4CS bake, the rung's
stock bake, this exe's FO4CS default, this exe's `--keep-bto`, this exe's stock — at
`REGION="-20 24 -19 25" DIM=4`, both taken from the environment so a lane that wants a wider
statement gets it without editing the gate. Run 17:20:33 → 17:24:27 on the 17:11:17 exe; full log at
`scratchpad/btofree1_20260916/lodgen_btofree.log`, the five trees left on disk under
`btofree_work/`.

**(a) the FO4CS default leaves our types and nothing else.** `.BTO` count under the mod folder
**0**; no `lodgen_bto_scratch` folder behind it; manifest sidecars kept **1**; the extension set
under the whole mod folder reads `btr dds lodb lodi lodm lodo txt` — measured, not asserted.
**The refuter**: the rung's identical bake wrote **1** `.BTO` here, so the three checks above are not
congratulating themselves on an empty folder. Against the rung, **13 files identical, 0 differ,
0 only on either side**, with `Commonwealth.lodo` (225,399,755 B) and `Commonwealth.lodi` (41,638 B)
called out by name.

**(b) `--keep-bto` is the exact way back.** 1 chunk in the mod folder, no scratch folder ever
created, and **14 files identical, 0 differ** against the rung — the chunk included. The way back is
byte-for-byte, not approximately.

**(c) the stock target does not move.** 1 `.BTO`, no scratch folder, **13 files identical,
0 differ** against the rung's stock bake. That is the brief's hard gate and it is green on the whole
tree, not a sample.

**(d) the census clause is written AND it moves.** Read off the three bakes:

    default   : bto built in scratch <...>/drop/lodgen_bto_scratch, 1 chunk(s), 1 dropped, 860743 bytes freed
    --keep-bto: bto built in the mod folder, 1 chunk(s), 0 dropped, 0 bytes freed
    stock     : bto built in the mod folder, 1 chunk(s), 0 dropped, 0 bytes freed

A counter that is only ever zero is not a counter, so the gate asks the dropped count and the
bytes-freed count to **move off zero** (1 and 860,743) on the default bake and to **read zero** on
the way back. The CLI's own `bto scratch:` line is required present for the drop bake and required
**absent** for `--keep-bto`.

**The two legs that are not `cmp`, and why.** `Commonwealth.lodb` is the ledger, and it is compared
field by field by `tests/spells/lodgen_btofree_ledger.py` (new, 4,285 B, LF-only) instead of byte by
byte, because on this file "the rung's bytes" is the wrong question twice over and both times the
ledger is doing its job:

* **the drop bake** (rung 6 output rows, 1 of them `.BTO`; this bake 5 rows, 0 of them `.BTO`) must
  NOT record a chunk it deleted — digesting a file that is about to be deleted is the defect that
  once forced full rebakes. The comparator checks the rung DID carry `.BTO` rows (non-vacuous), that
  this bake carries none, that **every other recorded file matches digest for digest**, and that the
  manifest row survives **with the digest it had before the move**.
* **the `--keep-bto` bake** differs from the rung at one character, 632: the `switches` field,
  `6f9a266c179d` → `deaac40aee43`. That is a digest of the command line and `--keep-bto` is one more
  token on it. It MUST move, or an `--incremental` run would reuse chunks baked by a command line
  that asked for different files on disk. So the gate asks for the difference rather than excusing
  it: identical once `switches` is removed, AND `switches` changed.

I found both of these as failures — the gate read 21 checks, 2 failures on its first run against
this exe — and neither was fixed by loosening a comparison. The ledger legs say more than the sweep
they replaced, and they are the reason the count went 21 → 23.

'''

b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
old = '### The rest of the chain\n'
assert s.count(old) == 1, 'anchor count %d' % s.count(old)
s = s.replace(old, SEC + old)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('report 2b written: %d -> %d B, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))

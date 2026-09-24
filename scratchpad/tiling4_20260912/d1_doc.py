"""TILING4 -- the correction to the `docs/LODGEN_TERRAIN_VT.md` amendment.

`d0_doc.py` wrote 2.5e with the counts from `offline_bake.py`'s fourteen-sheet
sweep (6 of 7 and 6 of 7 on the repeat, 2 of 7 vs 7 of 7 on the swirl).
`f3_full.sh` then baked all fourteen sheets with the real exe in three arms and
`f3_full.py` scored them with the same instruments: the product reads 4 of 7 and
5 of 7 on the repeat, and TILING3's warp is the BETTER of the two there.  A
shipped document does not keep a modelled number when the measured one exists,
so both paragraphs are replaced and the provenance list gains the two scripts.

Every anchor must occur EXACTLY once.  `--check` counts and writes nothing.
The file is LF-only; it is read and written as bytes and the CR count is
asserted unchanged (0) on the way out.

    python d1_doc.py --check
    python d1_doc.py
"""
import hashlib
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))
DOC = os.path.join(ROOT, 'docs', 'LODGEN_TERRAIN_VT.md')

A_OLD = '''**It is NOT the default, and the number that decides that is the repeat.** On
the fourteen shipped sheets of this lane's frozen selection/validation split the
hex tiling passes the repeat law on **6 of 7 and 6 of 7** -- (-4,-20) reads
0.308 and (-12,-20) reads 0.279 against the 0.264 absolute ceiling, +17 % and
+6 % over. The hex offsets break the phase BETWEEN tiles; they do nothing to
the land texture's own 10.667-texel period INSIDE one tap, which is what those
two sheets are carrying. A capped warp on top (strain 0.5, the two operators
composed) was built and measured: it buys **no** repeat at all -- 6 of 7 with
it and 6 of 7 without -- and costs the swirl, 7 of 7 down to 2 of 7.
'''

A_NEW = '''**It is NOT the default, and the number that decides that is the repeat.** The
fourteen shipped sheets of this lane's frozen selection/validation split were
baked by the real exe in three arms and scored by one piece of code
(`f3_full.sh`, `f3_full.py`): the hex tiling passes the repeat law on **4 of 7
and 5 of 7**. Four sheets miss on the amplitude -- (-20,20) at 0.623 against
that sheet's own 0.366 no-repeat control, and (-4,-20) 0.338, (4,-24) 0.324,
(-12,-20) 0.301 against the 0.264 absolute ceiling -- and (-36,-20) misses on
the RATIO law although its amplitude falls by a factor of twenty there, 1.501 to
0.073: that law divides the amplitude by the sheet's own no-repeat floor, and
when the amplitude collapses the floor collapses with it (the warp fails the
same sheet the same way, ratio 0.686 against 0.595). The hex offsets break the
phase BETWEEN tiles; they do nothing to the land texture's own 10.667-texel
period INSIDE one tap, which is what the four amplitude sheets are carrying.
**On the product TILING3's warp passes the repeat on more sheets than the hex
tiling does, 11 of 14 against 9 of 14**, so replacing it is a trade and not a
free improvement. The shipped default passes on **0 of 14** (0.531 to 1.618),
which is the defect of 2.5a restated on fourteen sheets. A capped warp composed
on top of the tiling (strain 0.5) was swept offline: it bought no repeat at all
and cost the swirl, so it was not built into the shipped path.
'''

B_OLD = '''**What it does buy**, on the same fourteen sheets and the same bakes: the swirl
reading -- the structure-tensor orientation coherence of the 1-5 texel grain
over each sheet's own phase-twin floor, the repeat notched out -- goes from
**2 of 7 passing** under the warp to **7 of 7 and 7 of 7**, at the same repeat
count and with every sheet inside 20 % of the previous build's grain. On chunk
(-20,24), whole sheet, from the real bakes: repeat **0.148** against vanilla's
0.201 and the warp's 0.183, swirl r **1.113** against vanilla's 1.994 and the
warp's 2.221, grain 3.834 against vanilla's 4.476.
'''

B_NEW = '''**What it does buy**, on the same fourteen real bakes: the swirl reading -- the
structure-tensor orientation coherence of the 1-5 texel grain over each sheet's
own phase-twin floor, the repeat notched out -- passes on **13 of 14** sheets
under the hex tiling against **7 of 14** under the warp. The warp reads 2.048
to 2.737 on every one of the fourteen; the hex tiling reads 0.859 to 2.084, and
its one red sheet, (-20,20), is a sheet where the shipped default is already
red and where the hex tiling reads BELOW it (2.084 against the default's 2.186,
ceiling 2.021) -- so on fourteen shipped sheets the hex tiling does not make a
single sheet's swirl worse than the build it replaces. Per-sheet grain stays
within 20 % of that build's on **14 of 14** sheets, against the warp's **1 of
14** (the warp's -1.00 mip bias is what does that; the tiling needs -0.22). On
chunk (-20,24), whole sheet: repeat **0.148** against vanilla's 0.201 and the
warp's 0.183, swirl r **1.113** against vanilla's 1.994 and the warp's 2.221,
grain 3.834 against vanilla's 4.476.
'''

P_OLD = ("(`s1e_law.py` the swirl law, `h1_sweep.py`, `h2_rescore.py`, "
         "`h3_sweep.py`,\n`h4_pick.py`, `h4b_flip.py`, `t4_gates.py`, "
         "`f2_gate.py`, `f3_real.py`,\n`hex_parity.py`) with the logs beside "
         "them.")
P_NEW = ("(`s1e_law.py` the swirl law, `h1_sweep.py`, `h2_rescore.py`, "
         "`h3_sweep.py`,\n`h4_pick.py`, `h4b_flip.py`, `t4_gates.py`, "
         "`f2_gate.py`, `f3_real.py`,\n`f3_full.sh` + `f3_full.py` -- all "
         "fourteen sheets baked by the real exe in\nthree arms, which is where "
         "every count in 2.5e comes from -- and\n`hex_parity.py`) with the logs "
         "beside them.")


def main():
    check = '--check' in sys.argv
    b = open(DOC, 'rb').read()
    cr0 = b.count(b'\r')
    t = b.decode('utf-8')
    edits = [('2.5e the repeat paragraph', A_OLD),
             ('2.5e the what-it-buys paragraph', B_OLD),
             ('the provenance script list', P_OLD)]
    bad = cr0 != 0
    for name, anchor in edits:
        n = t.count(anchor)
        print('%-34s %d (want 1)%s' % (name, n, '' if n == 1 else '   REFUSED'))
        bad |= n != 1
    if t.count('4 of 7\nand 5 of 7'):
        print('ALREADY APPLIED')
        bad = True
    if bad:
        print('REFUSED: nothing written')
        return 2
    if check:
        print('--check: counts only, nothing written')
        return 0
    t = t.replace(A_OLD, A_NEW, 1).replace(B_OLD, B_NEW, 1)
    t = t.replace(P_OLD, P_NEW, 1)
    out = t.encode('utf-8')
    assert out.count(b'\r') == 0, 'CR count moved'
    open(DOC, 'wb').write(out)
    print('wrote %s  %d -> %d bytes, sha256 %s'
          % (DOC, len(b), len(out), hashlib.sha256(out).hexdigest()[:16]))
    return 0


if __name__ == '__main__':
    sys.exit(main())

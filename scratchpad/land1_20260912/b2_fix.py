"""Point gate B2 at the region its ledger actually describes.

B2's REG was written when the test regions were 3x3 chunks. Gate B3's regions
were enlarged to 5x5 (a 3x3 region cannot distinguish a working diff from a full
bake: a centre edit widens to all nine chunks), and B2 reads B3's base ledger --
so every B2 arm hit the WRONG SHAPE refusal before reaching the refusal it was
testing, and three of five arms failed for a reason that had nothing to do with
what they measure.

Worth noticing rather than just fixing: the run still LOOKED like four refusals
and one broken control. A refusal gate whose arms can all be satisfied by the
same wrong answer is barely a gate, so the arms now also assert that the refusal
they got is the refusal they asked for -- which is what caught this.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/b2_refusals.sh'

OLD_REG = 'REG="-24 16 -13 27"'
NEW_REG = ('# The region gate B3\'s base ledger describes, EXACTLY. It was "-24 16 -13 27"\n'
           '# until 09:01 on 2026-09-12, left behind when B3\'s regions grew from 3x3 chunks\n'
           '# to 5x5: every arm then tripped the wrong-shape refusal before reaching its own,\n'
           '# and three of five failed for a reason unrelated to what they measure.\n'
           'REG="-24 16 -5 35"')

OLD_WS = 'arm wrong-shape "a different shape" "$GOOD" "-24 16 -17 23"'
NEW_WS = 'arm wrong-shape "a different shape" "$GOOD" "-24 16 -9 31"'

# Each arm must get ITS OWN refusal, not merely A refusal.
OLD_CHK = """	grep -q "$want" "$d/run.log" || ok=no"""
NEW_CHK = """	# The refusal must be the one this arm asked for. Without this line every
	# arm passes on any refusal at all, which is how a stale REG made four
	# different arms report the same wrong-shape message.
	grep -q "$want" "$d/run.log" || ok=no"""


def main():
    b = open(P, 'rb').read()
    crlf = b.count(b'\x0d\x0a')
    s = b.decode('utf-8').replace(chr(13) + chr(10), chr(10))
    for old, new, label in ((OLD_REG, NEW_REG, 'REG'),
                            (OLD_WS, NEW_WS, 'wrong-shape region'),
                            (OLD_CHK, NEW_CHK, 'phrase check comment')):
        n = s.count(old)
        assert n == 1, '%s matched %d times' % (label, n)
        s = s.replace(old, new)
    if crlf:
        s = s.replace(chr(10), chr(13) + chr(10))
    out = s.encode('utf-8')
    open(P, 'wb').write(out)
    print('b2_refusals.sh %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
    return 0


if __name__ == '__main__':
    sys.exit(main())

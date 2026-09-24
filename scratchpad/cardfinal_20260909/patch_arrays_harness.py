#!/usr/bin/env python3
"""The half-aux saving in tests/spells/lodgen_card_arrays.sh, as the law rather
than as a remembered percentage.

The check pinned the payload ratio to "within two points of 46.4%", a number
measured on 2026-09-06 under a different mip law.  Two laws have moved since: the
aux sheets' chain comes down with their gap (2026-09-09 morning) and the whole
chain lost a level (2026-09-09 evening), so on this fixture the base colour keeps
two levels while the halved sheets keep one, and the ratio is 42.9%.  Nothing is
wrong; the constant was stale.

Replaced by the exact bytes both runs must have, derived from the same law the
files are written under, with the ratio printed as information.  That is stricter
than the two-point band it replaces -- it fails on a single byte -- and it cannot
go stale.
"""
import sys

P = 'tests/spells/lodgen_card_arrays.sh'
s = open(P, encoding='utf-8').read()


def rep(old, new):
    global s
    n = s.count(old)
    if n != 1:
        print('anchor matched %d times: %r' % (n, old[:70]))
        sys.exit(1)
    s = s.replace(old, new)


rep("""    payFull = fullTotal - 4 * 148
    payHalf = halfTotal - 4 * 148
    print('  payload alone: %d -> %d = %.1f%% (the panel claims 46%%)' % (payFull, payHalf, 100.0 * payHalf / payFull))
    check('the payload fell to within two points of the claimed 46%%',
          abs(100.0 * payHalf / payFull - 46.4) < 2.0)""",
    """    payFull = fullTotal - 4 * 148
    payHalf = halfTotal - 4 * 148
    # THE EXPECTED BYTES, from the law both runs are written under, rather than
    # from a remembered percentage. The full run is four sheets at the class with
    # the gap's own chain; the half-aux run keeps the base colour there and takes
    # the other three to half of each side with the chain their halved gap buys.
    # A percentage pinned to a two-point band stood here until 2026-09-09 evening
    # and had gone stale twice over: the aux chain came down with the gap that
    # morning, and the whole chain lost a level that evening (bungo: ship one mip
    # fewer), so 46.4% became 42.9% with nothing having gone wrong.
    AUXMIPS = max(1, MIPS - 1)          # log2(gap / 2), the halved sheets' own gap
    wantFull = 4 * 148 + 2 * (3 * chain(SW, SH, True, MIPS) + chain(SW, SH, False, MIPS))
    wantHalf = 4 * 148 + 2 * (chain(SW, SH, True, MIPS)
                              + 2 * chain(AW, AH, True, AUXMIPS) + chain(AW, AH, False, AUXMIPS))
    print('  payload alone: %d -> %d = %.1f%% (the law wants %d and %d bytes with their headers)'
          % (payFull, payHalf, 100.0 * payHalf / payFull, wantFull, wantHalf))
    check('the four arrays are EXACTLY the bytes the class, the gap and auxDiv account for, both ways',
          fullTotal == wantFull and halfTotal == wantHalf)
    # and the saving is real: the halved run must be well under the full one, or
    # the check above would pass on a --card-half-aux that did nothing
    check('--card-half-aux actually halves something (the payload is under two thirds of the full run)',
          payHalf < 0.67 * payFull)""")

open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('written; CR %d LF %d' % (b.count(b'\r'), b.count(b'\n')))

#!/usr/bin/env python
"""Lane NEAR1 (2026-09-26): the independent decoder's half of the .lodo v7 / .lodi v11 bump.

Reads a NEAR library pair and a FAR pair (both as written by the exe), checks what each must be,
then corrupts ONE thing at a time -- re-signing every CRC it would otherwise trip -- and requires the
decoder (tests/spells/lodgen_native_decode.py) to refuse it BY NAME. A mutation refused only by a CRC
proves nothing, so each case names the substring its refusal must contain; each case has a control
(the same file re-signed with no change) that must be accepted.

    python near_format_selftest.py <near dir> <far dir>
      <near dir>  holds <World>.near.lodo/.lodi (a bake with an initially-disabled placement: lodi v11)
      <far dir>   holds one far <World>.lodo/.lodi pair from a --native bake (lodo v6)

Prints one line per check, then `N checks, M failures` and RESULT PASS/FAIL (exit 1 on a failure).
"""
import glob
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lodgen_native_decode as dec        # noqa: E402
from lodgen_native_mutate import get, put, resign_lodo   # noqa: E402

TMP = tempfile.mkdtemp(prefix='near_fmt_')
results = []


def check(name, ok, detail=''):
    results.append(ok)
    print('  %s %s%s' % ('ok  ' if ok else 'FAIL', name, (' -- ' + detail) if detail else ''))


def decode(kind, b):
    p = os.path.join(TMP, 'x.' + kind)
    with open(p, 'wb') as f:
        f.write(bytes(b))
    try:
        return (dec.read_lodo if kind == 'lodo' else dec.read_lodi)(p), None
    except dec.Refusal as e:
        return None, str(e)


def refused(name, kind, b, want):
    got, why = decode(kind, b)
    check(name, got is None and want.lower() in (why or '').lower(),
          'refused: %s' % why if why else 'ACCEPTED')


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    near_o = glob.glob(os.path.join(sys.argv[1], '*.near.lodo'))[0]
    near_i = near_o[:-1] + 'i'
    far_o = [p for p in glob.glob(os.path.join(sys.argv[2], '*.lodo')) if not p.endswith('.near.lodo')][0]
    far_i = far_o[:-1] + 'i'
    NO = bytearray(open(near_o, 'rb').read())
    NI = bytearray(open(near_i, 'rb').read())
    FO = bytearray(open(far_o, 'rb').read())
    FI = bytearray(open(far_i, 'rb').read())

    # ---- what each file must be
    L, why = decode('lodo', NO)
    check('near .lodo decodes, version 7, NEAR flag set', L is not None and L['header']['version'] == 7
          and L['header']['flags'] & 16 != 0, why or 'version %d flags 0x%x' % (L['header']['version'], L['header']['flags']))
    feats = sorted({m['reserved'] for m in L['materials']}) if L else []
    check('near materials carry feature bytes within 0x1F, some non-zero', bool(feats) and max(feats) <= 0x1F
          and max(feats) > 0, 'distinct feature bytes %s' % feats)
    T, why = decode('lodi', NI)
    dis = sum(1 for r in T['instances'] if r['flags'] & 256) if T else -1
    check('near .lodi decodes, version 11, bit 8 on some instance', T is not None
          and T['header']['version'] == 11 and dis > 0, why or 'version %d, %d disabled' % (T['header']['version'], dis))
    F, why = decode('lodo', FO)
    check('far .lodo decodes, version 6, no NEAR flag, every material reserved byte 0', F is not None
          and F['header']['version'] == 6 and not F['header']['flags'] & 16
          and all(m['reserved'] == 0 for m in F['materials']), why or '')
    G, why = decode('lodi', FI)
    check('far .lodi decodes, no instance with bit 8, version below 11', G is not None
          and G['header']['version'] < 11 and not any(r['flags'] & 256 for r in G['instances']),
          why or 'version %d' % G['header']['version'])

    # ---- controls: re-signing with no change is accepted (so the refusals below are the rules)
    b = bytearray(NO); resign_lodo(b)
    check('control: near .lodo re-signed unchanged is accepted', decode('lodo', b)[0] is not None)
    b = bytearray(FO); resign_lodo(b)
    check('control: far .lodo re-signed unchanged is accepted', decode('lodo', b)[0] is not None)

    offMat = get(NO, 0x88, 'Q')[0]
    offMatF = get(FO, 0x88, 'Q')[0]
    # ---- .lodo refusals
    b = bytearray(NO); put(b, 0x08, 'I', get(b, 0x08, 'I')[0] & ~16); resign_lodo(b)
    refused('near .lodo, NEAR flag cleared on version 7', 'lodo', b, 'NEAR flag')
    b = bytearray(NO); put(b, 0x04, 'I', 6)          # the version word is outside headerCrc32
    refused('near .lodo relabelled version 6 (NEAR flag kept)', 'lodo', b, 'NEAR flag')
    b = bytearray(NO); b[offMat + 7] |= 0x20; resign_lodo(b)
    refused('near .lodo, material features bit 5 set', 'lodo', b, 'features')
    b = bytearray(FO); put(b, 0x08, 'I', get(b, 0x08, 'I')[0] | 16); resign_lodo(b)
    refused('far .lodo, NEAR flag set on version 6', 'lodo', b, 'NEAR flag')
    b = bytearray(FO); put(b, 0x04, 'I', 7)
    refused('far .lodo relabelled version 7 without the NEAR flag', 'lodo', b, 'NEAR flag')
    b = bytearray(FO); b[offMatF + 7] = 1; resign_lodo(b)
    refused('far .lodo (v6), a material features byte set', 'lodo', b, 'reserved')
    # ---- .lodi refusals (the version word is outside headerCrc32: a relabel needs no re-sign)
    b = bytearray(NI); put(b, 0x04, 'I', 10)
    refused('near .lodi relabelled version 10 with bit 8 on an instance', 'lodi', b, 'reserved flags')
    b = bytearray(NI); put(b, 0x04, 'I', 12)
    refused('near .lodi relabelled version 12', 'lodi', b, 'version 12')

    bad = results.count(False)
    print('%d checks, %d failures' % (len(results), bad))
    print('RESULT %s' % ('PASS' if not bad else 'FAIL'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())

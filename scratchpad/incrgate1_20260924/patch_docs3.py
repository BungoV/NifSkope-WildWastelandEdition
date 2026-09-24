import os
R = r'E:\Projects\NifskopeWWE-incrgate1'


def rep(s, a, b):
    assert s.count(a) == 1, (a[:70], s.count(a))
    return s.replace(a, b)


def patch(rel, pairs):
    p = os.path.join(R, rel)
    src = open(p, 'rb').read()
    cr0 = src.count(b'\r')
    for a, b in pairs:
        src = rep(src, a.encode('utf-8'), b.encode('utf-8'))
    assert src.count(b'\r') == cr0
    open(p + '.tmp', 'wb').write(src)
    os.replace(p + '.tmp', p)
    print('patched', rel)


patch('docs/LODGEN_NATIVE_LODO_LODI.md', [
    ('written correctly. A consumer that bins placements by cell uses the stated cell,\n'
     'never `floor(position / 4096)`.\n',
     'written correctly. A consumer that bins placements by cell uses the stated cell,\n'
     'never `floor(position / 4096)`.\n\n'
     '**Measured on the downtown-Boston pair** (lane INCRGATE1, 2026-09-24,\n'
     '`tests/spells/lodgen_native.sh` leg 13b, region (0,\u221212)..(11,\u22121) dim 4, 33,123\n'
     'placements, 280 occluder boxes). The independent decoder reads the pair,\n'
     '6 checks, 0 failures. **14** instances sit in the band, worst **0.062501 u** from their\n'
     'cell line, all in chunk 6, the first at instance 3358. With the band set to 0 the\n'
     'decoder refuses at that instance by name, which is the leg\'s red control. The\n'
     'decoder\'s band is `step/2 + 16384 \u00b7 2^-23` \u2248 0.126955 u, one float ulp\n'
     'narrower than the reader\'s constant above. Both hold the measured worst with\n'
     'room to spare.\n'),
])
patch('docs/FO4CS_IMPROVED_LOD_PLAN.md', [
    ('| 6 | **the decoder\'s cell rule.** **Status 2026-09-23:**',
     '| 6 | **DONE 2026-09-24, lane INCRGATE1: the run is recorded and NATIVE 4.1 carries the band.** '
     '`tests/spells/lodgen_native.sh` leg 13b decodes the downtown-Boston pair (region 0 -12 11 -1, 33,123 '
     'placements, 280 boxes) with `lodgen_native_decode.py`: rc 0, **`cellQuantAmbiguous 14`**, worst '
     '**0.062501 u** inside the 0.126955 u band (first at instance 3358, beside the 3359 of the original '
     'refusal). Red control: the decoder with the band at 0 refuses that pair by name. Note: the decoder\'s '
     'band is one float ulp narrower than `LODI_CELL_QUANT_TOL` (0.12890 u); both hold the measured worst. '
     'Kept for the record: **the decoder\'s cell rule.** **Status 2026-09-23:**'),
])
